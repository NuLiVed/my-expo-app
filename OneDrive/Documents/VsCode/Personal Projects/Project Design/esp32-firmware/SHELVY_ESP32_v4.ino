// SHELVY_ESP32_v4.ino
// Bread storage chamber controller
//
// CHANGES vs v3:
//   - Publishes to the DEPLOYED backend over HTTPS (Vercel) instead of a LAN IP.
//     Set BACKEND_URL below. No port is appended for https URLs.
//   - Timestamp fix: readings are only stamped with epoch time after NTP sync;
//     before sync we send ts=0 and the backend stamps server time instead
//     (backend sanitizes any non-epoch ts, so ordering in Firebase stays correct).
//   - WiFiClientSecure with setInsecure() for TLS (no cert pinning; fine for demo).
//
// ============================================================
// HARDWARE (unchanged from v3)
// ============================================================
//   Board   : ESP32 Dev Module (esp32 core 3.3.11)
//   Sensor  : DFRobot DFR0588 Gravity ANALOG SHT30
//             - GND -> GND, VCC -> 3.3V
//             - T   -> GPIO 34   (ADC1)
//             - RH  -> GPIO 35   (ADC1)
//   Relay 1 : GPIO 33, COM+NC -> Peltier 1
//   Relay 3 : GPIO 26, COM+NC -> Peltier 2
//   Fan A   : constant 12V, PWM blue wire -> GPIO 18
//   Fan B   : constant 12V, PWM blue wire -> GPIO 19
//   Shared ground is mandatory: PSU ground and ESP32 GND joined.
//
// ============================================================
// API CONTRACT
// ============================================================
//   POST {BACKEND_URL}/api/readings/{DEVICE_ID}
//   Header: x-device-secret: <DEVICE_SECRET>
//   Body (JSON), sent every 5 seconds:
//   { "ts": 1751234567890, "temperature": 23.42, "humidity": 69.81,
//     "phase": "MAINTAIN", "peltier1": true, "peltier2": false,
//     "fan1_duty": 255, "fan2_duty": 90 }
// ============================================================

#include <WiFi.h>
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
const bool RELAY_NC[2] = { true, true };

const int PIN_PELTIER_A = 33;
const int PIN_PELTIER_B = 26;
const int RELAY_PIN[2] = { PIN_PELTIER_A, PIN_PELTIER_B };

const int PIN_FAN_A_PWM = 18;
const int PIN_FAN_B_PWM = 19;
const int FAN_PWM_PIN[2] = { PIN_FAN_A_PWM, PIN_FAN_B_PWM };

const int PIN_TEMP_ADC = 34;
const int PIN_HUMI_ADC = 35;

// ============================================================
// 2. DFR0588 CONVERSION  (0.3 - 2.7 V linear output)
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

const uint32_t PELTIER_MIN_STATE_MS = 60000;
const uint32_t PELTIER_ALTERNATE_MS = 180000;

const uint32_t FAN_PWM_FREQ = 25000;
const uint8_t  FAN_PWM_BITS = 8;
const uint8_t  FAN_DUTY_FULL = 255;
const uint8_t  FAN_DUTY_IDLE = 90;
const uint32_t FAN_OVERRUN_MS = 60000;

const int SENSOR_FAIL_LIMIT = 5;

// ============================================================
// 4. NETWORK  <<< EDIT THIS BLOCK >>>
// ============================================================

const char* WIFI_SSID = "Bawal  Connect!";   // NOTE: two spaces
const char* WIFI_PASS = "@Cute@@KamE";

// Deployed backend. Use the full https URL, NO trailing slash, NO port.
// For local bench testing use e.g. "http://192.168.100.16:4000".
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

uint32_t g_peltierOffAt[2] = { 0, 0 };
uint8_t  g_fanDuty[2]      = { FAN_DUTY_IDLE, FAN_DUTY_IDLE };

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
// 7. FAN CONTROL (unchanged)
// ============================================================

void setFanDuty(int idx, uint8_t duty) {
  g_fanDuty[idx] = duty;
  ledcWrite(FAN_PWM_PIN[idx], duty);
}

void updateFans() {
  for (int i = 0; i < 2; i++) {
    uint8_t duty;
    if (g_loadOn[i]) {
      duty = FAN_DUTY_FULL;
      g_peltierOffAt[i] = 0;
    } else {
      if (g_peltierOffAt[i] == 0) g_peltierOffAt[i] = millis();
      bool overrun = (millis() - g_peltierOffAt[i]) < FAN_OVERRUN_MS;
      duty = overrun ? FAN_DUTY_FULL : FAN_DUTY_IDLE;
    }
    setFanDuty(i, duty);
  }
}

// ============================================================
// 8. SENSOR TASK
// ============================================================

// Returns epoch-ms only when NTP has synced, otherwise 0.
// The backend replaces ts=0 (or any non-epoch value) with server time,
// so pre-sync readings no longer corrupt time-ordering in Firebase.
uint64_t nowMs() {
  time_t sec = time(nullptr);
  if (sec > 1700000000) return (uint64_t)sec * 1000ULL;  // > Nov 2023 = synced
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
// 9. CONTROL TASK (unchanged)
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
      setFanDuty(0, FAN_DUTY_IDLE);
      setFanDuty(1, FAN_DUTY_IDLE);
      Serial.printf("[CTRL] %s - peltiers off, fans idle\n", PHASE_NAME[g_phase]);
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

    updateFans();

    Serial.printf("[CTRL] %-8s T=%.2f RH=%.2f | P1=%d P2=%d | F1=%d F2=%d\n",
                  PHASE_NAME[g_phase], t, h,
                  g_loadOn[0], g_loadOn[1], g_fanDuty[0], g_fanDuty[1]);

    vTaskDelayUntil(&last, pdMS_TO_TICKS(CONTROL_INTERVAL_MS));
  }
}

// ============================================================
// 10. WIFI + PUBLISHER
// ============================================================

bool isHttpsBackend() {
  return strncmp(BACKEND_URL, "https://", 8) == 0;
}

void TaskWiFi(void* pv) {
  WiFi.mode(WIFI_STA);
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      Serial.print("[WIFI] connecting");
      uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(400));
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WIFI] IP: "); Serial.println(WiFi.localIP());
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      } else {
        Serial.println("[WIFI] connect failed - check SSID is 2.4GHz");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
  }
}

bool postReading(const Reading& r) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String(BACKEND_URL) + "/api/readings/" + DEVICE_ID;
  bool began;

  // TLS client must stay alive for the whole request, hence function scope.
  WiFiClientSecure secureClient;
  if (isHttpsBackend()) {
    secureClient.setInsecure();          // skip cert validation (demo-grade TLS)
    began = http.begin(secureClient, url);
  } else {
    began = http.begin(url);
  }
  if (!began) return false;

  http.setTimeout(10000);                // Vercel cold starts can take a few seconds
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", DEVICE_SECRET);

  StaticJsonDocument<512> doc;
  // ts=0 before NTP sync -> backend stamps server time (see nowMs()).
  doc["ts"]          = r.ts;
  doc["temperature"] = r.temp;
  doc["humidity"]    = r.hum;
  doc["phase"]       = PHASE_NAME[g_phase];
  doc["peltier1"]    = g_loadOn[0];
  doc["peltier2"]    = g_loadOn[1];
  doc["fan1_duty"]   = g_fanDuty[0];
  doc["fan2_duty"]   = g_fanDuty[1];

  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  http.end();
  if (!(code >= 200 && code < 300)) {
    Serial.printf("[HTTP] POST %s -> %d\n", url.c_str(), code);
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
// 11. SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  relayInit();
  Serial.println("[BOOT] relays initialised to OFF");

  for (int i = 0; i < 2; i++) {
    ledcAttach(FAN_PWM_PIN[i], FAN_PWM_FREQ, FAN_PWM_BITS);
    setFanDuty(i, FAN_DUTY_IDLE);
  }
  Serial.println("[BOOT] fan PWM ready at 25 kHz");

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_TEMP_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_HUMI_ADC, ADC_11db);
  Serial.println("[BOOT] ADC configured for DFR0588 analog outputs");

  qReadings  = xQueueCreate(10, sizeof(Reading));
  stateMutex = xSemaphoreCreateMutex();

  // HTTPS/TLS needs more stack than plain HTTP — publisher gets 12 kB.
  xTaskCreatePinnedToCore(TaskWiFi,      "WiFi",     4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensors,   "Sensors",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl,   "Control",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskPublisher, "Publish", 12288, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
