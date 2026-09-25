// SHELVY_ESP32_v3_cloud.ino
// Bread storage chamber controller — YOUR v3 with ONLY the network part fixed.
//
// WHAT CHANGED vs the v3 you pasted (everything else is identical):
//   1. Sends to the DEPLOYED cloud backend over HTTPS
//      (https://shelvy-backend.vercel.app) instead of a laptop LAN IP that
//      has no server running.
//   2. nowMs() sends ts=0 until NTP syncs; the backend stamps server time,
//      so pre-sync readings no longer corrupt the data log.
//   3. Publisher task stack raised to 12 kB (TLS needs more than 4 kB).
//
// ============================================================
// HARDWARE (unchanged)
// ============================================================
//   Board   : ESP32 Dev Module (esp32 core 3.3.11)
//   Sensor  : DFRobot DFR0588 Gravity ANALOG SHT30
//             - GND -> GND, VCC -> 3.3V
//             - T   -> GPIO 34   (ADC1)
//             - RH  -> GPIO 35   (ADC1)
//   Relay 1 : GPIO 33, COM+NC -> Peltier 1 + fans + brushless fans
//   Relay 3 : GPIO 26, COM+NC -> Peltier 2 + fans2 + brushless fans2
//   Relay channels 2 and 4 unused.
//   No humidifier fitted - humidity is measured and logged only.
// ============================================================

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"   // for the channel-13 country fix (phone hotspot uses ch13)
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
// 1. WIRING CONFIG (unchanged)
// ============================================================

const bool RELAY_BOARD_ACTIVE_LOW = true;
const bool RELAY_NC[2] = { true, true };

const int PIN_PELTIER_A = 33;   // relay channel 1
const int PIN_PELTIER_B = 26;   // relay channel 3
const int RELAY_PIN[2] = { PIN_PELTIER_A, PIN_PELTIER_B };

const int PIN_TEMP_ADC = 34;
const int PIN_HUMI_ADC = 35;

// ============================================================
// 2. DFR0588 CONVERSION (unchanged)
// ============================================================

const float V_OUT_MIN = 0.30f;
const float V_OUT_MAX = 2.70f;

const float TEMP_RANGE_MIN = -40.0f, TEMP_RANGE_MAX = 125.0f;
const float HUMI_RANGE_MIN =   0.0f, HUMI_RANGE_MAX = 100.0f;

float voltsToTempC(float v) {
  return TEMP_RANGE_MIN +
         (v - V_OUT_MIN) * (TEMP_RANGE_MAX - TEMP_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN);
}

float voltsToHumidity(float v) {
  return HUMI_RANGE_MIN +
         (v - V_OUT_MIN) * (HUMI_RANGE_MAX - HUMI_RANGE_MIN) / (V_OUT_MAX - V_OUT_MIN);
}

const float V_VALID_MIN = 0.15f;
const float V_VALID_MAX = 2.95f;
const int   ADC_SAMPLES = 16;

// ============================================================
// 3. CONTROL SETPOINTS (unchanged)
// ============================================================

const float TEMP_SETPOINT_C = 25.0;
const float TEMP_PULLDOWN_C = 26.0;
const float TEMP_STOP_C     = 24.0;

const float HUMI_SETPOINT_PCT = 70.0;   // monitored only, no humidifier

const uint32_t PELTIER_MIN_STATE_MS = 60000;    // 60 s
const uint32_t PELTIER_ALTERNATE_MS = 180000;   // 3 min

const int SENSOR_FAIL_LIMIT = 5;

// ============================================================
// 4. NETWORK  <<< THE ONLY REAL CHANGE >>>
// ============================================================

// Known networks — the ESP32 auto-joins whichever it finds (strongest first).
// All networks must be 2.4 GHz (ESP32 cannot see 5 GHz). On the phone hotspot,
// set: Settings > Portable hotspot > AP band > 2.4 GHz.
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred WIFI_NETWORKS[] = {
  { "Bawal  Connect!", "@Cute@@KamE" },     // home (NOTE: two spaces)
  { "Redmi Note 14 5G", "TYPE_HOTSPOT_PASSWORD_HERE" }, // defense-day phone hotspot <<< TYPE YOUR HOTSPOT PASSWORD, KEEP THE QUOTES
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]);
WiFiMulti wifiMulti;

// Cloud backend (works from ANY network). Full https URL, no port, no
// trailing slash. For local bench tests use e.g. "http://192.168.100.16:4000".
const char* BACKEND_URL   = "https://shelvy-backend.vercel.app";
const char* DEVICE_ID     = "esp32-01";
const char* DEVICE_SECRET = "dev-device-secret-please-change";

const uint32_t SAMPLE_INTERVAL_MS  = 2000;
const uint32_t CONTROL_INTERVAL_MS = 2000;
const uint32_t PUBLISH_INTERVAL_MS = 5000;
const uint32_t WIFI_RETRY_MS       = 3000;

// ============================================================
// 5. STATE (unchanged)
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
// 6. RELAY ABSTRACTION (unchanged)
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

// CHANGED: returns epoch-ms only after NTP has synced, otherwise 0.
// The backend replaces ts=0 with server time, keeping the log ordered.
uint64_t nowMs() {
  time_t sec = time(nullptr);
  if (sec > 1700000000) return (uint64_t)sec * 1000ULL;  // synced (> Nov 2023)
  return 0;
}

float readVolts(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(200);
  }
  return (sum / (float)ADC_SAMPLES) / 1000.0f;
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
      Serial.printf("[SENSOR] vT=%.3fV vH=%.3fV -> T=%.2f C  RH=%.2f %%\n",
                    vT, vH, t, h);
    } else {
      Serial.printf("[SENSOR] out of range vT=%.3f vH=%.3f (fail %d) - check wiring\n",
                    vT, vH, g_sensorFailCount);
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

// ============================================================
// 8. CONTROL TASK (unchanged)
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
// 9. WIFI + PUBLISHER  <<< CHANGED: HTTPS to cloud >>>
// ============================================================

bool isHttpsBackend() {
  return strncmp(BACKEND_URL, "https://", 8) == 0;
}

void TaskWiFi(void* pv) {
  WiFi.mode(WIFI_STA);
  // Channel-13 fix: the phone hotspot broadcasts on ch13, but the default
  // country config only scans ch1-11, so the ESP32 would never see it.
  wifi_country_t country = { "PH", 1, 13, 0, WIFI_COUNTRY_POLICY_MANUAL };
  esp_wifi_set_country(&country);
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
    wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
  }
  bool wasConnected = false;
  for (;;) {
    // wifiMulti scans for ANY known network and joins the strongest one.
    if (wifiMulti.run(8000) == WL_CONNECTED) {
      if (!wasConnected) {
        Serial.print("[WIFI] connected to "); Serial.print(WiFi.SSID());
        Serial.print("  IP: "); Serial.println(WiFi.localIP());
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        wasConnected = true;
      }
    } else {
      wasConnected = false;
      Serial.println("[WIFI] no known network found - check hotspot is ON and set to 2.4GHz");
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
    secureClient.setInsecure();          // demo-grade TLS (no cert pinning)
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(url);
  }
  if (!began) return false;

  http.setTimeout(10000);                // allow for Vercel cold starts
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", DEVICE_SECRET);

  StaticJsonDocument<384> doc;
  doc["ts"]          = r.ts;             // 0 before NTP sync -> server stamps
  doc["temperature"] = r.temp;
  doc["humidity"]    = r.hum;
  doc["phase"]       = PHASE_NAME[g_phase];
  doc["peltier1"]    = g_loadOn[0];
  doc["peltier2"]    = g_loadOn[1];

  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  if (!(code >= 200 && code < 300)) {
    Serial.printf("[HTTP] POST -> %d\n", code);
  }
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
// 10. SETUP (publisher stack raised for TLS)
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
