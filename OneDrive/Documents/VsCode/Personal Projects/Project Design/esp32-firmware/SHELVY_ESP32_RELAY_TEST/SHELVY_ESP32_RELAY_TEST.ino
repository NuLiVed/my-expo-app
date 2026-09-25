// SHELVY_ESP32_RELAY_TEST.ino
// ============================================================
// RELAY CLICK / SWITCH TEST — no WiFi, no sensor, no temp logic.
// Just cycles the two relays on a timer so you can SEE + HEAR them switch
// (use low-speed fans on COM+NO as the load to watch them turn on/off).
//
// USES THE SAME WIRING CONFIG AS FINAL3, so if it clicks correctly here,
// it will click correctly in the real firmware.
//
// WIRING (same as FINAL3):
//   IN1 <- GPIO 33   IN2 <- GPIO 26
//   VCC <- ESP32 5V   GND <- ESP32 GND     (clean side)
//   JD-VCC <- 12V(+)  coil GND <- 12V COM  (dirty side, yellow cap OFF)
//   Fan/Peltier -> relay COM(middle) + NO(right)
//
// WHAT YOU SHOULD OBSERVE each step:
//   - the relay's channel LED changes
//   - an audible CLICK
//   - the fan on that relay turns ON (NO closes) / OFF (NO opens)
// ============================================================

// ---- SAME CONFIG AS FINAL3 (keep these identical) ----
const bool RELAY_BOARD_ACTIVE_LOW = true;
const bool RELAY_NC[2] = { false, false };   // fans/Peltiers on COM + NO
const int  PIN_PELTIER_A = 32;   // Relay 1 (IN)  (output-capable; move relay wire here from 34)
const int  PIN_PELTIER_B = 33;   // Relay 2 (IN)  (output-capable; move relay wire here from 35)
const int  RELAY_PIN[2]  = { PIN_PELTIER_A, PIN_PELTIER_B };

// ---- HOW LONG each step is held ----
// 5000 = 5 seconds per step (quick testing).
// Change to 30000 if you want 30-second holds like the real alternation.
const uint32_t STEP_MS = 5000;

int levelForLoad(int idx, bool loadOn) {
  bool energize = RELAY_NC[idx] ? !loadOn : loadOn;
  if (RELAY_BOARD_ACTIVE_LOW) return energize ? LOW : HIGH;
  return energize ? HIGH : LOW;
}

void setLoad(int idx, bool on) {
  digitalWrite(RELAY_PIN[idx], levelForLoad(idx, on));
}

void step(const char* label, bool r1, bool r2) {
  setLoad(0, r1);
  setLoad(1, r2);
  Serial.printf("\n[TEST] %s  ->  Relay1=%s  Relay2=%s\n",
                label, r1 ? "ON" : "OFF", r2 ? "ON" : "OFF");
  Serial.println("       (listen for CLICK, watch channel LED + fan)");
  // countdown so you can match the click to the label
  for (int s = STEP_MS / 1000; s > 0; s--) {
    Serial.printf("       holding... %d\n", s);
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  // init both relays OFF first, then set pinMode (avoids a boot glitch click)
  for (int i = 0; i < 2; i++) {
    digitalWrite(RELAY_PIN[i], levelForLoad(i, false));
    pinMode(RELAY_PIN[i], OUTPUT);
    digitalWrite(RELAY_PIN[i], levelForLoad(i, false));
  }
  Serial.println("\n==============================================");
  Serial.println(" SHELVY RELAY TEST - both relays start OFF");
  Serial.println(" Fans on COM+NO should be OFF right now.");
  Serial.println("==============================================");
  delay(2000);
}

void loop() {
  // Full cycle so you can verify each relay independently, then together.
  step("STEP 1  Relay 1 ON only ", true,  false);  // fan 1 spins, fan 2 off
  step("STEP 2  both OFF        ", false, false);  // both fans off
  step("STEP 3  Relay 2 ON only ", false, true);   // fan 2 spins, fan 1 off
  step("STEP 4  both OFF        ", false, false);  // both fans off
  step("STEP 5  BOTH ON         ", true,  true);   // both fans spin
  step("STEP 6  both OFF        ", false, false);  // both fans off
  Serial.println("\n[TEST] cycle complete - repeating...\n");
}
