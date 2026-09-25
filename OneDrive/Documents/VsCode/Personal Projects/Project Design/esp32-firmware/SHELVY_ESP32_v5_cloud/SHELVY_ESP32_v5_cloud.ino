// SHELVY_ESP2_RELAY_25.ino
// ESP32 #2 = RELAY NODE (buck, common ground with 12V).
//   - Powered by BUCK from the 12V PSU (common ground -> relay switches)
//   - FETCHES the temperature from the backend (published by ESP1)
//   - Runs the 25.0C + 30s-off control, drives the relay
//   - Has NO sensor -> no analog pins to corrupt
//
// SETPOINT = 25.0C: since the chamber is ~26.5C, cooling runs ACTIVELY until
// it hits 25.0C, then clicks OFF for 30s, then cools again. This actually
// demonstrates the Peltier switching (unlike 27.5 which sits off at 26.5).
//
// WIRING:
//   Relay:  IN1->GPIO32  IN2->GPIO33   DC+ ->12V(+)  DC- ->12V(-)
//           high/low jumper: S1->High, S2->High   (this board energizes on HIGH)
//           load on COM + NC (cooling ON by default; opens/OFF at 25.0C)
//   Power:  PSU 12V -> Buck(5V) -> ESP32 5V/VIN

#include <WiFi.h>
#include <WiFiMulti.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---- RELAY ----
const bool RELAY_BOARD_ACTIVE_LOW = false;  // this board energizes on HIGH -> cooling ON = de-energized (NC closed)
const bool RELAY_NC[2] = { true, true };   // load on NC -> cooling ON by default
const int PIN_PELTIER_A = 32;
const int PIN_PELTIER_B = 33;
const int RELAY_PIN[2] = { PIN_PELTIER_A, PIN_PELTIER_B };

// ---- CONTROL ----
const float    TEMP_SETPOINT_C = 25.0;    // cool until 25.0C, then OFF 30s
const uint32_t RELAY_OFF_MS    = 30000;   // 30s off when 25.0C reached
const uint32_t FETCH_MS        = 1500;    // fetch temp every 1.5s (tighter link)
const uint32_t DATA_STALE_MS   = 30000;   // if temp older than 30s -> treat as no data

// ---- NETWORK ----
struct WifiCred { const char* ssid; const char* pass; };
const WifiCred WIFI_NETWORKS[] = {
  { "Bawal  Connect!", "@Cute@@KamE" },
  { "pd-shelvy",       "12345678" },
  { "shelvyapp",       "PUT_SHELVYAPP_PASSWORD_HERE" },
};
const int WIFI_NETWORK_COUNT = sizeof(WIFI_NETWORKS)/sizeof(WIFI_NETWORKS[0]);
WiFiMulti wifiMulti;
const char* BACKEND_URL      = "https://shelvy-backend.vercel.app";
const char* SENSOR_DEVICE_ID = "esp32-01";  // ESP1 publishes here (we FETCH temp here)
const char* APP_DEVICE_ID    = "esp32-01";  // we add the peltier state to the same device the app reads
const char* DEVICE_SECRET    = "dev-device-secret-please-change";

bool     g_loadOn[2] = { true, true };
bool     g_resting   = false;
uint32_t g_restStart = 0;

int levelForLoad(int idx, bool on){
  bool en = RELAY_NC[idx] ? !on : on;
  if(RELAY_BOARD_ACTIVE_LOW) return en ? LOW : HIGH;
  return en ? HIGH : LOW;
}
void setBoth(bool on){ for(int i=0;i<2;i++){ g_loadOn[i]=on; digitalWrite(RELAY_PIN[i], levelForLoad(i,on)); } }
void relayInit(){
  for(int i=0;i<2;i++){ digitalWrite(RELAY_PIN[i], levelForLoad(i,true)); pinMode(RELAY_PIN[i],OUTPUT); digitalWrite(RELAY_PIN[i], levelForLoad(i,true)); g_loadOn[i]=true; }
}

bool isHttps(){ return strncmp(BACKEND_URL,"https://",8)==0; }

// fetch latest temperature + humidity from the sensor channel
bool fetchTemp(float &t, float &h){
  if(WiFi.status()!=WL_CONNECTED) return false;
  HTTPClient http; String url = String(BACKEND_URL) + "/api/readings/" + SENSOR_DEVICE_ID + "/latest";
  WiFiClientSecure sc; bool ok;
  if(isHttps()){ sc.setInsecure(); ok=http.begin(sc,url); } else ok=http.begin(url);
  if(!ok) return false;
  http.setTimeout(10000);
  http.addHeader("x-device-secret",DEVICE_SECRET);
  int code=http.GET();
  bool good=false;
  if(code==200){
    String payload=http.getString();
    StaticJsonDocument<512> doc;
    if(deserializeJson(doc,payload)==DeserializationError::Ok){
      if(doc["latest"].containsKey("temperature")){
        t = doc["latest"]["temperature"];
        h = doc["latest"].containsKey("humidity") ? (float)doc["latest"]["humidity"] : NAN;
        good=true;
      }
    }
  } else Serial.printf("[HTTP] GET -> %d\n", code);
  http.end();
  return good;
}

// publish the FULL record (temp + humidity + peltier state) to the APP's device
bool publishState(float t, float h){
  if(WiFi.status()!=WL_CONNECTED) return false;
  HTTPClient http; String url = String(BACKEND_URL) + "/api/readings/" + APP_DEVICE_ID;
  WiFiClientSecure sc; bool ok;
  if(isHttps()){ sc.setInsecure(); ok=http.begin(sc,url); } else ok=http.begin(url);
  if(!ok) return false;
  http.setTimeout(10000);
  http.addHeader("Content-Type","application/json");
  http.addHeader("x-device-secret",DEVICE_SECRET);
  StaticJsonDocument<384> doc;
  doc["temperature"]=t; doc["humidity"]=h;
  doc["peltier1"]=g_loadOn[0]; doc["peltier2"]=g_loadOn[1];
  doc["phase"]= g_resting ? "REST" : (g_loadOn[0] ? "COOLING" : "IDLE");
  String body; serializeJson(doc,body);
  int code=http.POST(body); http.end();
  return (code>=200&&code<300);
}

void setup(){
  Serial.begin(115200); delay(300);
  relayInit();   // cooling ON by default (NC), no boot click
  WiFi.mode(WIFI_STA);
  wifi_country_t country = { "PH", 1, 13, 0, WIFI_COUNTRY_POLICY_MANUAL };
  esp_wifi_set_country(&country);
  for(int i=0;i<WIFI_NETWORK_COUNT;i++) wifiMulti.addAP(WIFI_NETWORKS[i].ssid, WIFI_NETWORKS[i].pass);
  Serial.println("[ESP2] RELAY NODE (25.0C) - fetches temp from backend, drives relay");
}

uint32_t lastFetch=0, lastGood=0, lastPub=0;
float g_temp=NAN, g_hum=NAN;
bool prevP1=true, prevP2=true;

void loop(){
  static bool was=false;
  if(wifiMulti.run(8000)==WL_CONNECTED){
    if(!was){ Serial.print("[WIFI] "); Serial.println(WiFi.SSID()); was=true; }
  } else { was=false; Serial.println("[WIFI] searching..."); }

  // fetch temp + humidity every FETCH_MS
  if(millis()-lastFetch >= FETCH_MS){
    lastFetch=millis();
    float t,h;
    if(fetchTemp(t,h)){ g_temp=t; g_hum=h; lastGood=millis(); Serial.printf("[FETCH] T=%.2f  H=%.1f\n", t, h); }
    else Serial.println("[FETCH] no data");
  }

  bool haveData = (lastGood!=0) && (millis()-lastGood <= DATA_STALE_MS) && !isnan(g_temp);

  if(!haveData){
    // no fresh temp -> safe default: cooling ON (NC de-energized)
    setBoth(true); g_resting=false;
    Serial.println("[CTRL] NO DATA -> cooling ON (safe)");
  } else if(g_resting){
    setBoth(false);
    if(millis()-g_restStart >= RELAY_OFF_MS) g_resting=false;
    Serial.printf("[CTRL] REST(off) T=%.2f\n", g_temp);
  } else {
    if(g_temp <= TEMP_SETPOINT_C){
      setBoth(false); g_resting=true; g_restStart=millis();
      Serial.printf("[CTRL] reached %.1fC -> OFF 30s (CLICK)\n", TEMP_SETPOINT_C);
    } else {
      setBoth(true);
      Serial.printf("[CTRL] COOLING T=%.2f | P1=%d P2=%d\n", g_temp, g_loadOn[0], g_loadOn[1]);
    }
  }

  // publish full record to the APP device: IMMEDIATELY on a peltier change
  // (so the app notifies instantly), otherwise every 3s to keep it fresh
  bool changed = (g_loadOn[0]!=prevP1) || (g_loadOn[1]!=prevP2);
  if(changed || millis()-lastPub >= 3000){
    lastPub=millis();
    prevP1=g_loadOn[0]; prevP2=g_loadOn[1];
    if(!isnan(g_temp)) publishState(g_temp, g_hum);
    if(changed) Serial.println("[PUB] peltier changed -> published immediately");
  }
  delay(1000);
}
