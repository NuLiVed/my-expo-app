// main.ino
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// --- WiFi ---
const char* WIFI_SSID = "VIRUS";
const char* WIFI_PASS = "2345678";

// --- Backend REST ---
const char* BACKEND_HOST = "192.168.38.33";
const uint16_t BACKEND_PORT = 4000;
const bool USE_HTTPS = false;
const char* DEVICE_ID = "esp32-01";
const char* DEVICE_SECRET = "dev-device-secret-please-change";

// --- Relay pins (active HIGH) ---
const int RELAY_PELTIER_PIN    = 25;
const int RELAY_HUMIDIFIER_PIN = 26;

// --- Analog sensor pins ---
const int TEMP_ADC_PIN = 34;
const int HUMI_ADC_PIN = 35;

// --- ADC calibration ---
const float VREF = 3.3f;
const int   ADC_MAX = 4095;
float adcToTempC(int raw) {
  float volts = (raw * VREF) / ADC_MAX;
  return volts * 100.0f; // LM35-style example
}
float adcToHumidity(int raw) {
  float volts = (raw * VREF) / ADC_MAX;
  const float vMin = 0.8f, vMax = 3.0f;
  float pct = (volts - vMin) * 100.0f / (vMax - vMin);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// --- Timing ---
const uint32_t SAMPLE_INTERVAL_MS      = 2000;
const uint32_t RELAY_POLL_INTERVAL_MS  = 2000;
const uint32_t WIFI_RETRY_MS           = 3000;
const uint32_t PUBLISH_INTERVAL_MS     = 2000;

// --- Queues ---
QueueHandle_t qReadings;   // sensor readings to publish
QueueHandle_t qRelayCmd;   // latest relay flag

struct Reading {
  uint64_t ts;
  float temp;
  float hum;
};

// --- Helpers ---
String baseUrl() {
  String proto = USE_HTTPS ? "https://" : "http://";
  return proto + String(BACKEND_HOST) + ":" + String(BACKEND_PORT);
}

uint64_t nowMs() {
  time_t sec = time(nullptr);
  if (sec > 100000) return (uint64_t)sec * 1000ULL;
  return (uint64_t)millis();
}

// --- WiFi Task ---
void TaskWiFi(void* pv) {
  WiFi.mode(WIFI_STA);
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      Serial.print("WiFi connecting");
      uint32_t start = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        Serial.print(".");
        vTaskDelay(pdMS_TO_TICKS(400));
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi IP: "); Serial.println(WiFi.localIP());
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      } else {
        Serial.println("WiFi connect failed");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_MS));
  }
}

// --- Sensor Task ---
void TaskSensors(void* pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    int rawT = analogRead(TEMP_ADC_PIN);
    int rawH = analogRead(HUMI_ADC_PIN);
    Reading r;
    r.ts = nowMs();
    r.temp = adcToTempC(rawT);
    r.hum  = adcToHumidity(rawH);
    xQueueSend(qReadings, &r, 0);
    Serial.printf("Raw T=%d Raw H=%d -> T=%.2fC H=%.2f%%\n", rawT, rawH, r.temp, r.hum);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
  }
}

// --- Relay Poll Task ---
bool fetchRelayFlag(bool &relayOut) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = baseUrl() + "/api/devices/" + DEVICE_ID + "/control";
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
  if (doc["control"].isNull()) return false;
  relayOut = doc["control"]["relay"] | false;
  return true;
}

void TaskRelayPoll(void* pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    bool flag = false;
    if (fetchRelayFlag(flag)) {
      digitalWrite(RELAY_PELTIER_PIN, flag ? HIGH : LOW);
      digitalWrite(RELAY_HUMIDIFIER_PIN, flag ? HIGH : LOW);
      Serial.printf("Relay flag=%d\n", flag);
      xQueueOverwrite(qRelayCmd, &flag);
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(RELAY_POLL_INTERVAL_MS));
  }
}

// --- Publisher Task ---
bool postReading(const Reading& r) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = baseUrl() + "/api/readings/" + DEVICE_ID;
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-secret", DEVICE_SECRET);
  StaticJsonDocument<256> doc;
  doc["ts"] = r.ts;
  doc["temperature"] = r.temp;
  doc["humidity"] = r.hum;
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  if (code < 200 || code >= 300) {
    Serial.printf("POST failed code=%d\n", code);
    http.end();
    return false;
  }
  http.end();
  return true;
}

void TaskPublisher(void* pv) {
  TickType_t last = xTaskGetTickCount();
  Reading r;
  for (;;) {
    if (xQueueReceive(qReadings, &r, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (postReading(r)) {
        Serial.println("Reading sent");
      } else {
        Serial.println("Send failed (will retry on next reading)");
      }
    }
    vTaskDelayUntil(&last, pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PELTIER_PIN, OUTPUT);
  pinMode(RELAY_HUMIDIFIER_PIN, OUTPUT);
  digitalWrite(RELAY_PELTIER_PIN, LOW);
  digitalWrite(RELAY_HUMIDIFIER_PIN, LOW);
  analogReadResolution(12);

  qReadings = xQueueCreate(10, sizeof(Reading));
  qRelayCmd = xQueueCreate(1, sizeof(bool));

  xTaskCreatePinnedToCore(TaskWiFi,      "WiFi",     4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensors,   "Sensors",  2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskRelayPoll, "RelayPoll",4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskPublisher, "Publish",  4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(portMAX_DELAY); // idle
}