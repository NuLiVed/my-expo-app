// SHELVY_ESP32_FINALACT2.ino
// Same as FINALACT but with a RELAY CLICK SELF-TEST on boot.
//
// ON POWER-UP: the relays click ON/OFF 6 times (you WILL hear it) to prove
// the relay physically switches. THEN it runs the normal 27.5C control.
//
// If you hear the clicks on boot -> relay + coil + 12V are all good.
// If you hear NO clicks on boot  -> it is HARDWARE: the coil ground (GND) is
//   not connected to 12V(-), so the coil can't energise. Fix the wiring, not code.
//
// WIRING:
//   Relay:  IN1->GPIO32  IN2->GPIO33  VCC->5V  GND->ESP32 GND
//           JD-VCC->12V(+)  (jumper OFF)   the single GND -> ESP32 GND AND 12V(-)
//           load on COM + NC (cooling ON by default)   active-LOW board
//   Sensor: DFR0588 analog SHT30 — VCC->3V3  T->GPIO34  RH->GPIO35  GND->GND

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

struct Reading { uint64_t ts; float temp; float hum; float vTemp; float vHum; };

// ============================================================
// 1. WIRING CONFIG
// ============================================================
const bool RELAY_BOARD_ACTIVE_LOW = true;
const bool RELAY_NC[2] = { true, true };   // load on NC -> cooling ON by default
const int PIN_PELTIER_A = 32;
const int PIN_PELTIER_B = 33;
const int RELAY_PIN[2] = { PIN_PELTIER_A, PIN_PELTIER_B };
const int PIN_TEMP_ADC = 34;
const int PIN_HUMI_ADC = 35;

// ============================================================
// 2. SENSOR CONVERSION
// ============================================================
const float V_OUT_MIN = 0.30f;
const float V_OUT_MAX = 2.70f;
const float TEMP_RANGE_MIN = -40.0f, TEMP_RANGE_MAX = 125.0f;
const float HUMI_RANGE_MIN =   0.0f, HUMI_RANGE_MAX = 100.0f;

const float TEMP_CAL_OFFSET = -7.0f;   // re-synced: read ~real temp
const float HUMI_CAL_OFFSET = 1.0f;

float voltsToTempC(float v) {
  return TEMP_RANGE_MIN + (v - V_OUT_MIN) * (TEMP_RANGE_MAX - TEMP_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN) + TEMP_CAL_OFFSET;
}
float voltsToHumidity(float v) {
  return HUMI_RANGE_MIN + (v - V_OUT_MIN) * (HUMI_RANGE_MAX - HUMI_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN) + HUMI_CAL_OFFSET;
}
const float V_VALID_MIN = 0.15f;
const float V_VALID_MAX = 2.95f;
const int ADC_SAMPLES = 41;
const int ADC_TRIM    = 8;

// ============================================================
// 3. CONTROL
// ============================================================
const float    TEMP_SETPOINT_C  = 27.5;
const uint32_t RELAY_OFF_MS     = 30000;
const int      SENSOR_FAIL_LIMIT = 5;

// ============================================================
// 4. NETWORK
// ============================================================
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred WIFI_NETWORKS[] = {
  { "Bawal  Connect!", "@Cute@@KamE" },
  { "pd-shelvy",       "12345678" },
  { "shelvyapp",       "PUT_SHELVYAPP_PASSWORD_HERE" },
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
bool     g_loadOn[2] = { false, false };
bool     g_resting = false;
uint32_t g_restStartMs = 0;
enum Phase { PH_IDLE = 0, PH_REST = 1, PH_COOLING = 2, PH_FAULT = 3 };
const char* PHASE_NAME[] = { "IDLE", "REST", "COOLING", "FAULT" };
Phase g_phase = PH_IDLE;

// ============================================================
// 6. RELAY
// ============================================================
int levelForLoad(int idx, bool loadOn) {
  bool energize = RELAY_NC[idx] ? !loadOn : loadOn;
  if (RELAY_BOARD_ACTIVE_LOW) return energize ? LOW : HIGH;
  return energize ? HIGH : LOW;
}
void relayInit() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(RELAY_PIN[i], levelForLoad(i, true));
    pinMode(RELAY_PIN[i], OUTPUT);
    digitalWrite(RELAY_PIN[i], levelForLoad(i, true));
    g_loadOn[i] = true;
  }
}
void setBoth(bool on) {
  for (int i = 0; i < 2; i++) {
    g_loadOn[i] = on;
    digitalWrite(RELAY_PIN[i], levelForLoad(i, on));
  }
}

// ---- CLICK SELF-TEST (drives the coils directly, ignoring NC/NO) ----
void relaySelfTest() {
  Serial.println("\n[SELFTEST] Listen! Clicking relays 6x...");
  for (int c = 0; c < 6; c++) {
    // ENERGIZE both coils (active-low = LOW)
    digitalWrite(RELAY_PIN[0], LOW);
    digitalWrite(RELAY_PIN[1], LOW);
    Serial.printf("[SELFTEST] click %d - ENERGIZED\n", c + 1);
    delay(800);
    // DE-ENERGIZE both coils
    digitalWrite(RELAY_PIN[0], HIGH);
    digitalWrite(RELAY_PIN[1], HIGH);
    Serial.printf("[SELFTEST] click %d - released\n", c + 1);
    delay(800);
  }
  Serial.println("[SELFTEST] done. If you heard 12 clicks, the relay WORKS.");
  Serial.println("[SELFTEST] If SILENT -> coil GND not on 12V(-). Fix wiring.\n");
}

// ============================================================
// 7. SENSOR TASK
// ============================================================
uint64_t nowMs() {
  time_t sec = time(nullptr);
  if (sec > 1700000000) return (uint64_t)sec * 1000ULL;
  return 0;
}
float readVolts(int pin) {
  uint16_t buf[64];
  int n = ADC_SAMPLES; if (n > 64) n = 64;
  for (int i = 0; i < n; i++) { buf[i] = (uint16_t)analogReadMilliVolts(pin); delayMicroseconds(200); }
  for (int i = 1; i < n; i++) { uint16_t k = buf[i]; int j = i - 1; while (j >= 0 && buf[j] > k) { buf[j+1] = buf[j]; j--; } buf[j+1] = k; }
  int lo = ADC_TRIM, hi = n - ADC_TRIM; if (hi <= lo) { lo = 0; hi = n; }
  uint32_t sum = 0; int cnt = 0;
  for (int i = lo; i < hi; i++) { sum += buf[i]; cnt++; }
  return (sum / (float)cnt) / 1000.0f;
}
void TaskSensors(void* pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    float vT = readVolts(PIN_TEMP_ADC);
    float vH = readVolts(PIN_HUMI_ADC);
    bool ok = (vT >= V_VALID_MIN && vT <= V_VALID_MAX && vH >= V_VALID_MIN && vH <= V_VALID_MAX);
    float t = voltsToTempC(vT);
    float h = voltsToHumidity(vH);
    if (h < 0) h = 0; if (h > 100) h = 100;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (ok) { g_temp = t; g_hum = h; g_sensorOk = true; g_sensorFailCount = 0; }
      else { g_sensorFailCount++; if (g_sensorFailCount >= SENSOR_FAIL_LIMIT) g_sensorOk = false; }
      xSemaphoreGive(stateMutex);
    }
    if (ok) {
      Reading r = { nowMs(), t, h, vT, vH };
      xQueueSend(qReadings, &r, 0);
      Serial.printf("[SENSOR] T=%.2f C  RH=%.2f %%\n", t, h);
    } else {
      Serial.printf("[SENSOR] out of range vT=%.3f vH=%.3f (fail %d)\n", vT, vH, g_sensorFailCount);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

// ============================================================
// 8. CONTROL TASK (27.5C, 30s off)
// ============================================================
void TaskControl(void* pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    float t, h; bool ok, enabled;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      t = g_temp; h = g_hum; ok = g_sensorOk; enabled = g_systemEnabled;
      xSemaphoreGive(stateMutex);
    } else { vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS)); continue; }

    if (!ok || !enabled) {
      g_phase = ok ? PH_IDLE : PH_FAULT;
      setBoth(false); g_resting = false;
      vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS)); continue;
    }

    if (g_resting) {
      setBoth(false); g_phase = PH_REST;
      if (millis() - g_restStartMs >= RELAY_OFF_MS) g_resting = false;
    } else {
      if (t <= TEMP_SETPOINT_C) {
        setBoth(false); g_resting = true; g_restStartMs = millis(); g_phase = PH_REST;
        Serial.printf("[CTRL] reached %.1fC -> OFF 30s (CLICK)\n", TEMP_SETPOINT_C);
      } else {
        setBoth(true); g_phase = PH_COOLING;
      }
    }
    Serial.printf("[CTRL] %-8s T=%.2f | P1=%d P2=%d\n", PHASE_NAME[g_phase], t, g_loadOn[0], g_loadOn[1]);
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
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
  bool was = false;
  for (;;) {
    if (wifiMulti.run(8000) == WL_CONNECTED) {
      if (!was) { Serial.print("[WIFI] "); Serial.println(WiFi.SSID()); configTime(0,0,"pool.ntp.org","time.nist.gov"); was = true; }
    } else { was = false; Serial.println("[WIFI] searching..."); }
    vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
  }
}
bool postReading(const Reading& r) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http; String url = String(BACKEND_URL) + "/api/readings/" + DEVICE_ID;
  WiFiClientSecure sc; bool began;
  if (isHttpsBackend()) { sc.setInsecure(); began = http.begin(sc, url); } else began = http.begin(url);
  if (!began) return false;
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", DEVICE_SECRET);
  StaticJsonDocument<384> doc;
  doc["ts"]=r.ts; doc["temperature"]=r.temp; doc["humidity"]=r.hum;
  doc["phase"]=PHASE_NAME[g_phase]; doc["peltier1"]=g_loadOn[0]; doc["peltier2"]=g_loadOn[1];
  String body; serializeJson(doc, body);
  int code = http.POST(body); http.end();
  return (code >= 200 && code < 300);
}
void TaskPublisher(void* pv) {
  TickType_t last = xTaskGetTickCount(); Reading r;
  for (;;) {
    bool got = false;
    while (xQueueReceive(qReadings, &r, 0) == pdTRUE) got = true;
    if (got) postReading(r);
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
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_TEMP_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_HUMI_ADC, ADC_11db);

  relaySelfTest();   // <-- clicks the relays 6x on boot so you HEAR it

  qReadings  = xQueueCreate(10, sizeof(Reading));
  stateMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(TaskWiFi,      "WiFi",     4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensors,   "Sensors",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl,   "Control",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskPublisher, "Publish", 12288, NULL, 1, NULL, 0);
}
void loop() { vTaskDelay(portMAX_DELAY); }
