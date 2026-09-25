// SHELVY_ESP32_FINAL2.ino
// Bread storage chamber controller — FINAL 2 (CALIBRATED 2026-09-23).
//
// WHAT CHANGED vs FINAL1:
//   - HUMI_CAL_OFFSET recalibrated 8.0 -> 6.75 against a stable FY reference on
//     validation day (2026-09-23). Nothing else changed.
//
//   Calibration point (chamber at ambient, both sensors settled, side by side):
//       app humidity 87.25% (old offset 8.0)  vs  FY reference 86%  => shift -1.25
//       new HUMI_CAL_OFFSET = 8.0 - 1.25 = 6.75  -> app now reads ~86%
//   TEMP left at -2.5 (already 2-point validated at 31.6C & 27.2C on a working
//   FY-12; do NOT override a 2-point calibration with a single settling point).
//
// WHAT CHANGED vs v3_cloud:
//   1. Fixed 0.3-2.7V conversion, CALIBRATED against the FY-12 reference.
//      >>> Power the sensor from the 3V3 pin. <<<
//   2. readVolts() uses a RIPPLE-REJECTING TRIMMED MEAN to fight noise.
//   3. Calibration offsets (TEMP_CAL_OFFSET / HUMI_CAL_OFFSET).
//
// HARDWARE
//   Board  : ESP32 Dev Module (esp32 core 3.3.11)
//   Sensor : DFRobot DFR0588 Gravity ANALOG SHT30
//            GND->GND · VCC->3V3 · T->GPIO34 · RH->GPIO35
//   Relay 1: GPIO 33   Relay 3: GPIO 26

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

struct Reading {
  uint64_t ts;
  float temp;
  float hum;
  float vTemp;
  float vHum;
};

// ============================================================
// 1. WIRING CONFIG
// ============================================================
const bool RELAY_BOARD_ACTIVE_LOW = true;
const bool RELAY_NC[2] = { false, false };  // Peltiers wired to COM + NO (verified by continuity: NO=right, NC=left)
const int PIN_PELTIER_A = 33;
const int PIN_PELTIER_B = 26;
const int RELAY_PIN[2] = { PIN_PELTIER_A, PIN_PELTIER_B };
const int PIN_TEMP_ADC = 34;
const int PIN_HUMI_ADC = 35;

// ============================================================
// 2. DFR0588 CONVERSION (fixed 0.3-2.7V window, calibrated to FY reference)
// ============================================================
const float V_OUT_MIN = 0.30f;
const float V_OUT_MAX = 2.70f;
const float TEMP_RANGE_MIN = -40.0f, TEMP_RANGE_MAX = 125.0f;
const float HUMI_RANGE_MIN =   0.0f, HUMI_RANGE_MAX = 100.0f;

// FINAL 2 calibration vs FY reference (2026-09-23, validation day):
//   temp   raw check ~30.6C  vs FY ~31.5C  => kept -2.5 (2-point validated earlier)
//   humid  raw ~87.25%       vs FY 86%     => -1.25  ->  HUMI_CAL_OFFSET = 6.75
const float TEMP_CAL_OFFSET = -2.8f;   // add to temperature reading (exact-synced to FY: SHT30 26.3C -> FY 26.0C)
const float HUMI_CAL_OFFSET = 1.0f;    // add to humidity reading (lowered 3.75->1.0: app ~73% was too high vs FY ~70% at operating point)

float voltsToTempC(float v) {
  return TEMP_RANGE_MIN +
         (v - V_OUT_MIN) * (TEMP_RANGE_MAX - TEMP_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN) +
         TEMP_CAL_OFFSET;
}

float voltsToHumidity(float v) {
  return HUMI_RANGE_MIN +
         (v - V_OUT_MIN) * (HUMI_RANGE_MAX - HUMI_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN) +
         HUMI_CAL_OFFSET;
}

const float V_VALID_MIN = 0.15f;
const float V_VALID_MAX = 2.95f;

// --- NOISE FILTER (trimmed mean) ---
const int ADC_SAMPLES = 41;
const int ADC_TRIM    = 8;

// ============================================================
// 3. CONTROL SETPOINTS
// ============================================================
const float TEMP_SETPOINT_C = 24.0;   // maintaining midpoint
const float TEMP_PULLDOWN_C = 25.0;   // BOTH ON above 25C -> at 25C ONE system turns OFF (first switch, no long wait)
const float TEMP_STOP_C     = 23.0;   // BOTH OFF only at 23C (floor, rarely reached)
                                      // 23-25C = ONE system alternates -> FLUCTUATION IS ACTIVE AT 24 (not idle, no waiting for 23)
const float HUMI_SETPOINT_PCT = 70.0;
const uint32_t PELTIER_MIN_STATE_MS = 45000;   // min 45s in a state before switching -> the "interval before it turns on again"
const uint32_t PELTIER_ALTERNATE_MS = 60000;   // in MAINTAIN, swap the active system every 60s (visible/audible alternation)
const int SENSOR_FAIL_LIMIT = 5;

// ============================================================
// 4. NETWORK
// ============================================================
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred WIFI_NETWORKS[] = {
  { "Bawal  Connect!", "@Cute@@KamE" },
  { "pd-shelvy",       "12345678" },
  { "shelvyapp",       "PUT_SHELVYAPP_PASSWORD_HERE" }, // <<< type shelvyapp password
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
WiFiMulti wifiMulti;

const char* BACKEND_URL   = "https://shelvy-backend.vercel.app";
const char* DEVICE_ID     = "esp32-01";
const char* DEVICE_SECRET = "dev-device-secret-please-change";

const uint32_t SAMPLE_INTERVAL_MS  = 2000;
const uint32_t CONTROL_INTERVAL_MS = 2000;
const uint32_t PUBLISH_INTERVAL_MS = 5000;
const uint32_t WIFI_RETRY_MS       = 3000;

// ============================================================
// 5. STATE
// ============================================================
QueueHandle_t qReadings;
SemaphoreHandle_t stateMutex;
volatile float g_temp = NAN;
volatile float g_hum  = NAN;
volatile bool  g_sensorOk = false;
volatile bool  g_systemEnabled = true;
int g_sensorFailCount = 0;
bool     g_loadOn[2]       = { false, false };
uint32_t g_lastChangeMs[2] = { 0, 0 };
int      g_activePeltier   = 0;
uint32_t g_lastAlternateMs = 0;
enum Phase { PH_IDLE = 0, PH_MAINTAIN = 1, PH_PULLDOWN = 2, PH_FAULT = 3 };
const char* PHASE_NAME[] = { "IDLE", "MAINTAIN", "PULLDOWN", "FAULT" };
Phase g_phase = PH_IDLE;

// ============================================================
// 6. RELAY ABSTRACTION
// ============================================================
int levelForLoad(int idx, bool loadOn) {
  bool energize = RELAY_NC[idx] ? !loadOn : loadOn;
  if (RELAY_BOARD_ACTIVE_LOW) return energize ? LOW : HIGH;
  return energize ? HIGH : LOW;
}
void relayInit() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(RELAY_PIN[i], levelForLoad(i, false));
    pinMode(RELAY_PIN[i], OUTPUT);
    digitalWrite(RELAY_PIN[i], levelForLoad(i, false));
    g_loadOn[i] = false;
    g_lastChangeMs[i] = millis();
  }
}
bool setLoad(int idx, bool on, uint32_t minStateMs) {
  if (g_loadOn[idx] == on) return false;
  if (millis() - g_lastChangeMs[idx] < minStateMs) return false;
  g_loadOn[idx] = on;
  g_lastChangeMs[idx] = millis();
  digitalWrite(RELAY_PIN[idx], levelForLoad(idx, on));
  return true;
}
void forceAllOff() {
  for (int i = 0; i < 2; i++) {
    g_loadOn[i] = false;
    g_lastChangeMs[i] = millis();
    digitalWrite(RELAY_PIN[i], levelForLoad(i, false));
  }
}

// ============================================================
// 7. SENSOR TASK
// ============================================================
uint64_t nowMs() {
  time_t sec = time(nullptr);
  if (sec > 1700000000) return (uint64_t)sec * 1000ULL;
  return 0;
}

// RIPPLE-REJECTING TRIMMED MEAN
float readVolts(int pin) {
  uint16_t buf[64];
  int n = ADC_SAMPLES;
  if (n > 64) n = 64;
  for (int i = 0; i < n; i++) {
    buf[i] = (uint16_t)analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  for (int i = 1; i < n; i++) {
    uint16_t key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = key;
  }
  int lo = ADC_TRIM;
  int hi = n - ADC_TRIM;
  if (hi <= lo) { lo = 0; hi = n; }
  uint32_t sum = 0;
  int cnt = 0;
  for (int i = lo; i < hi; i++) { sum += buf[i]; cnt++; }
  return (sum / (float)cnt) / 1000.0f;
}

void TaskSensors(void* pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    float vT = readVolts(PIN_TEMP_ADC);
    float vH = readVolts(PIN_HUMI_ADC);
    bool ok = (vT >= V_VALID_MIN && vT <= V_VALID_MAX &&
               vH >= V_VALID_MIN && vH <= V_VALID_MAX);
    float t = voltsToTempC(vT);
    float h = voltsToHumidity(vH);
    if (h < 0)   h = 0;
    if (h > 100) h = 100;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (ok) {
        g_temp = t; g_hum = h;
        g_sensorOk = true;
        g_sensorFailCount = 0;
      } else {
        g_sensorFailCount++;
        if (g_sensorFailCount >= SENSOR_FAIL_LIMIT) g_sensorOk = false;
      }
      xSemaphoreGive(stateMutex);
    }
    if (ok) {
      Reading r = { nowMs(), t, h, vT, vH };
      xQueueSend(qReadings, &r, 0);
      Serial.printf("[SENSOR] vT=%.3fV vH=%.3fV -> T=%.2f C  RH=%.2f %%\n", vT, vH, t, h);
    } else {
      Serial.printf("[SENSOR] out of range vT=%.3f vH=%.3f (fail %d) - check wiring\n",
                    vT, vH, g_sensorFailCount);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

// ============================================================
// 8. CONTROL TASK
// ============================================================
void TaskControl(void* pv) {
  TickType_t last = xTaskGetTickCount();
  g_lastAlternateMs = millis();
  for (;;) {
    float t, h; bool ok, enabled;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      t = g_temp; h = g_hum;
      ok = g_sensorOk; enabled = g_systemEnabled;
      xSemaphoreGive(stateMutex);
    } else {
      vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
      continue;
    }
    if (!ok || !enabled) {
      g_phase = ok ? PH_IDLE : PH_FAULT;
      forceAllOff();
      Serial.printf("[CTRL] %s - all loads off\n", PHASE_NAME[g_phase]);
      vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
      continue;
    }
    if (t >= TEMP_PULLDOWN_C) {
      g_phase = PH_PULLDOWN;
      setLoad(0, true, PELTIER_MIN_STATE_MS);
      setLoad(1, true, PELTIER_MIN_STATE_MS);
      g_lastAlternateMs = millis();
    } else if (t > TEMP_STOP_C) {
      g_phase = PH_MAINTAIN;
      if (millis() - g_lastAlternateMs >= PELTIER_ALTERNATE_MS) {
        g_activePeltier = 1 - g_activePeltier;
        g_lastAlternateMs = millis();
        Serial.printf("[CTRL] alternating -> peltier %d\n", g_activePeltier + 1);
      }
      setLoad(g_activePeltier,     true,  PELTIER_MIN_STATE_MS);
      setLoad(1 - g_activePeltier, false, PELTIER_MIN_STATE_MS);
    } else {
      g_phase = PH_IDLE;
      setLoad(0, false, PELTIER_MIN_STATE_MS);
      setLoad(1, false, PELTIER_MIN_STATE_MS);
    }
    Serial.printf("[CTRL] %-8s T=%.2f RH=%.2f | P1=%d P2=%d\n",
                  PHASE_NAME[g_phase], t, h, g_loadOn[0], g_loadOn[1]);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
  }
}

// ============================================================
// 9. WIFI + PUBLISHER
// ============================================================
bool isHttpsBackend() { return strncmp(BACKEND_URL, "https://", 8) == 0; }

void TaskWiFi(void* pv) {
  WiFi.mode(WIFI_STA);
  wifi_country_t country = { "PH", 1, 13, 0, WIFI_COUNTRY_POLICY_MANUAL };
  esp_wifi_set_country(&country);
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
    wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
  }
  bool wasConnected = false;
  for (;;) {
    if (wifiMulti.run(8000) == WL_CONNECTED) {
      if (!wasConnected) {
        Serial.print("[WIFI] connected to "); Serial.print(WiFi.SSID());
        Serial.print("  IP: "); Serial.println(WiFi.localIP());
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        wasConnected = true;
      }
    } else {
      wasConnected = false;
      Serial.println("[WIFI] no known network found - check hotspot ON and 2.4GHz");
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
  }
}

bool postReading(const Reading& r) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String(BACKEND_URL) + "/api/readings/" + DEVICE_ID;
  bool began;
  WiFiClientSecure secureClient;
  if (isHttpsBackend()) {
    secureClient.setInsecure();
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(url);
  }
  if (!began) return false;
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", DEVICE_SECRET);
  StaticJsonDocument<384> doc;
  doc["ts"]          = r.ts;
  doc["temperature"] = r.temp;
  doc["humidity"]    = r.hum;
  doc["phase"]       = PHASE_NAME[g_phase];
  doc["peltier1"]    = g_loadOn[0];
  doc["peltier2"]    = g_loadOn[1];
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  if (!(code >= 200 && code < 300)) Serial.printf("[HTTP] POST -> %d\n", code);
  return (code >= 200 && code < 300);
}

void TaskPublisher(void* pv) {
  TickType_t last = xTaskGetTickCount();
  Reading r;
  for (;;) {
    bool got = false;
    while (xQueueReceive(qReadings, &r, 0) == pdTRUE) got = true;
    if (got) {
      if (postReading(r)) Serial.println("[HTTP] reading sent");
      else                Serial.println("[HTTP] send failed");
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
  }
}

// ============================================================
// 10. SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  relayInit();
  Serial.println("[BOOT] relays initialised to OFF");
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_TEMP_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_HUMI_ADC, ADC_11db);
  Serial.println("[BOOT] ADC configured for DFR0588 analog outputs");
  qReadings  = xQueueCreate(10, sizeof(Reading));
  stateMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(TaskWiFi,      "WiFi",     4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensors,   "Sensors",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl,   "Control",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskPublisher, "Publish", 12288, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
