// SHELVY_ESP1_SENSOR.ino
// ESP32 #1 = SENSOR NODE (isolated, clean).
//   - Powered by POWER BANK (isolated ground -> clean analog reading)
//   - Reads DFR0588 analog SHT30
//   - Publishes temperature + humidity to the backend (device esp32-01)
//   - Has NO relay. Its ONLY job is a clean reading + publish.
//
// The relay node (ESP2) fetches this data wirelessly -> no wire between the two
// ESP32s -> the noisy 12V/relay ground can NEVER reach this clean sensor.
//
// WIRING:
//   Sensor: VCC->3V3(LDO)  T->GPIO34  RH->GPIO35  GND->GND
//   Power : POWER BANK -> ESP32 USB-C  (isolated, do NOT tie to 12V ground)

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ---- SENSOR ----
const int PIN_TEMP_ADC = 34;
const int PIN_HUMI_ADC = 35;
const float V_OUT_MIN = 0.30f, V_OUT_MAX = 2.70f;
const float TEMP_RANGE_MIN = -40.0f, TEMP_RANGE_MAX = 125.0f;
const float HUMI_RANGE_MIN = 0.0f,  HUMI_RANGE_MAX = 100.0f;
const float TEMP_CAL_OFFSET = 0.8f;    // cal 2026-09-25: read 22.87 vs real 26.5 (+3.63)
const float HUMI_CAL_OFFSET = -2.1f;   // cal 2026-09-25: read 70.1 vs real 67 (-3.1)
const float V_VALID_MIN = 0.15f, V_VALID_MAX = 2.95f;
const int ADC_SAMPLES = 41, ADC_TRIM = 8;

float voltsToTempC(float v){ return TEMP_RANGE_MIN + (v-V_OUT_MIN)*(TEMP_RANGE_MAX-TEMP_RANGE_MIN)/(V_OUT_MAX-V_OUT_MIN) + TEMP_CAL_OFFSET; }
float voltsToHumidity(float v){ return HUMI_RANGE_MIN + (v-V_OUT_MIN)*(HUMI_RANGE_MAX-HUMI_RANGE_MIN)/(V_OUT_MAX-V_OUT_MIN) + HUMI_CAL_OFFSET; }

// ---- NETWORK ----
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred WIFI_NETWORKS[] = {
  { "Bawal  Connect!", "@Cute@@KamE" },
  { "pd-shelvy",       "12345678" },
  { "shelvyapp",       "PUT_SHELVYAPP_PASSWORD_HERE" },
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS)/sizeof(WIFI_NETWORKS[0]);
WiFiMulti wifiMulti;
const char* BACKEND_URL   = "https://shelvy-backend.vercel.app";
const char* DEVICE_ID     = "esp32-01";   // publishes straight to the app's device -> sensor shows even with relay ESP32 OFF
const char* DEVICE_SECRET = "dev-device-secret-please-change";
const uint32_t SAMPLE_MS  = 1000;   // read every 1s
const uint32_t PUBLISH_MS = 1500;   // publish every 1.5s (tighter link)

float readVolts(int pin){
  uint16_t buf[64]; int n = ADC_SAMPLES; if(n>64)n=64;
  for(int i=0;i<n;i++){ buf[i]=(uint16_t)analogReadMilliVolts(pin); delayMicroseconds(200); }
  for(int i=1;i<n;i++){ uint16_t k=buf[i]; int j=i-1; while(j>=0&&buf[j]>k){buf[j+1]=buf[j];j--;} buf[j+1]=k; }
  int lo=ADC_TRIM, hi=n-ADC_TRIM; if(hi<=lo){lo=0;hi=n;}
  uint32_t s=0; int c=0; for(int i=lo;i<hi;i++){s+=buf[i];c++;}
  return (s/(float)c)/1000.0f;
}
uint64_t nowMs(){ time_t s=time(nullptr); if(s>1700000000) return (uint64_t)s*1000ULL; return 0; }
bool isHttps(){ return strncmp(BACKEND_URL,"https://",8)==0; }

bool publish(float t, float h){
  if(WiFi.status()!=WL_CONNECTED) return false;
  HTTPClient http; String url = String(BACKEND_URL) + "/api/readings/" + DEVICE_ID;
  WiFiClientSecure sc; bool ok;
  if(isHttps()){ sc.setInsecure(); ok=http.begin(sc,url); } else ok=http.begin(url);
  if(!ok) return false;
  http.setTimeout(10000);
  http.addHeader("Content-Type","application/json");
  http.addHeader("x-device-secret",DEVICE_SECRET);
  StaticJsonDocument<256> doc;
  doc["ts"]=nowMs(); doc["temperature"]=t; doc["humidity"]=h; doc["source"]="esp1-sensor";
  String body; serializeJson(doc,body);
  int code=http.POST(body); http.end();
  return (code>=200&&code<300);
}

void setup(){
  Serial.begin(115200); delay(300);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_TEMP_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_HUMI_ADC, ADC_11db);
  WiFi.mode(WIFI_STA);
  wifi_country_t country = { "PH", 1, 13, 0, WIFI_COUNTRY_POLICY_MANUAL };
  esp_wifi_set_country(&country);
  for(int i=0;i<WIFI_NETWORK_COUNT;i++) wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
  Serial.println("[ESP1] SENSOR NODE - reads SHT30, publishes to backend");
}

uint32_t lastPub=0;
void loop(){
  static bool was=false;
  if(wifiMulti.run(8000)==WL_CONNECTED){
    if(!was){ Serial.print("[WIFI] "); Serial.println(WiFi.SSID()); configTime(0,0,"pool.ntp.org","time.nist.gov"); was=true; }
  } else { was=false; Serial.println("[WIFI] searching..."); }

  float vT=readVolts(PIN_TEMP_ADC), vH=readVolts(PIN_HUMI_ADC);
  bool ok = (vT>=V_VALID_MIN&&vT<=V_VALID_MAX&&vH>=V_VALID_MIN&&vH<=V_VALID_MAX);
  float t=voltsToTempC(vT), h=voltsToHumidity(vH);
  if(h<0)h=0; if(h>100)h=100;

  if(ok){
    Serial.printf("[SENSOR] T=%.2f C  RH=%.2f %%\n", t, h);
    if(millis()-lastPub >= PUBLISH_MS){
      lastPub=millis();
      Serial.println(publish(t,h) ? "[HTTP] published" : "[HTTP] publish failed");
    }
  } else {
    Serial.printf("[SENSOR] out of range vT=%.3f vH=%.3f - check VCC=3V3, T=34, RH=35\n", vT, vH);
  }
  delay(SAMPLE_MS);
}
