# Shelvy — Session History 2026-09-17

## Focus: ESP32 live temp/humidity debugging (sensor reading wrong)

### What got DONE and confirmed working ✅
- **ESP32 #1 firmware uploaded successfully** (`SHELVY_ESP32_v3_cloud.ino`, Bawal Connect Wi-Fi, relays on P33/P26).
  - Upload initially failed with `No bootable app partitions` because esptool did a partial "fast reflash." **Fix: Tools → "Erase All Flash Before Sketch Upload" → Enabled**, then re-upload = clean success.
  - Confirmed running: `[WIFI] IP: 192.168.100.96`, `[HTTP] reading sent` every 5 s.
- **ESP32 → cloud pipeline confirmed** by reading the backend directly (curl, not the app):
  `https://shelvy-backend.vercel.app/api/readings/esp32-01/latest` returns fresh device readings.
- **App / cloud / code formula / power ruled OUT** as the cause of the bad temperature — the wrong number is already in the cloud before the app touches it.

### The problem
- Real conditions (reference hygrometer, verified accurate in freezer/fridge): **~27°C / ~72–74%**.
- ESP32/SHT30 reported **junk**: temperature 44–123°C, humidity 41–100%, jumping around.
- Swapping the SHT30 sensor did NOT fix it → **sensor is not the culprit**.
- Power is a clean **3.3V from a buck converter** → power ruled out.

### KEY FINDINGS

**1. Datasheet finding (Sensirion SHT3x-ARP, the chip inside DFR0588):**
- The analog output is **RATIOMETRIC = 10–90% of VDD**, not a fixed 0.3–2.7V.
- Correct formulas:
  - `T(°C)  = -66.875 + 218.75 × (Vout / VDD)`
  - `RH(%)  = -12.5   + 125    × (Vout / VDD)`
- Current code uses a WRONG fixed-voltage formula (`-40 + (v-0.30)*68.75`). Ratiometric fix pending.
- Note ambiguity: DFRobot board wiki rates output as a flat "0.3–2.7V for 3.3–5.5V supply," which *may* mean the board regulates output to a fixed range. Because of this uncertainty, the robust path is a **2-point empirical calibration** (works either way) — but ONLY after the reading is stable.
- Sensor is **factory-calibrated (±0.3°C)** → likely needs little/no calibration once formula + wiring are correct.

**2. WIRING DIAGNOSIS (most likely ROOT CAUSE):**
- Board terminal order: `3V3, EN, SVP, SVN, P34, P35, P32, P33, P25, P26, P14, P12, GND, P13, SD2, SD3, GND, 5V`.
- From the user's photo, the sensor wires appear to be in **P32 and P25**, with **P34 and P35 EMPTY**. Relays correctly in **P33 and P26**.
- Code reads **P34 (temp)** and **P35 (humidity)** — if those are empty, BOTH channels float and read noise → matches the jumping junk on both channels (the times humidity "looked right" were coincidental floating noise).
- **CRITICAL:** GPIO25 (P25) is an **ADC2** pin — ADC2 does NOT work for analogRead while Wi-Fi is on. P34/P35 are ADC1 (Wi-Fi-safe), which is why the code uses them.

### THE FIX (pending user action)
1. **Move sensor Temperature wire → P34**, **Humidity wire → P35** (leave relays on P33/P26).
2. Re-check reading; if still off, apply ratiometric formula with measured `V_SUPPLY` (buck voltage).
3. Only if a small steady offset remains, add 2-point calibration.

### ⭐ BREAKTHROUGH (end of session)
- After moving sensor **T→P34, RH→P35**, the reading became NEARLY CORRECT: **28.7°C vs real 26.5°C** (~2° off), humidity 78.6% vs real 70–76%. **Wiring fix WORKED — sensor/wiring/code are all GOOD.**
- **KEY:** correct reading happens with the **PROTOTYPE POWER OFF and ESP32 on USB only.** When the prototype main power was ON, readings were junk (57–123°C).
- => The culprit is the **prototype power system (likely the BUCK CONVERTER or Peltier current on the shared supply)**, NOT the sensor.
- A **small "pop" sound** was heard and the ESP32 went dead for ~9 min (came back on USB power). **Suspect the buck converter popped.** DO NOT re-power the prototype until the buck is checked (multimeter: output must be exactly 3.3V).
- **Next fixes:** (1) power sensor VCC from ESP32's clean **3V3** pin, not the buck raw output; (2) check/replace buck converter; (3) once stable on prototype power, add a small ~2° calibration offset (now justified since reading is stable+close).

### Still open
- User to verify exact terminals under the T and RH wires, then move to P34/P35.
- ESP32 #2 (defense-day hotspot version): change `WIFI_SSID`/`WIFI_PASS` to the hotspot; handle channel gotcha.
- Mold model improvement via Roboflow (queued).

### Networking notes (clarified this session)
- App on **mobile data alone** shows all cloud numbers (temp/humidity/mold %). **Live camera needs the phone on the SAME network as the Pi/ESP32** (local stream, not cloud).
- Phone as hotspot + data ON + phone Wi-Fi OFF = numbers AND live camera both work on one phone.
- Laptop-hotspot-fed-by-phone-data breaks the app's internet (phone Wi-Fi connecting kills its data route).
- Pi live camera browser URL: `http://192.168.100.93:8080/` (stream `/stream.mjpg`, snapshot `/snapshot.jpg`). Pi Wi-Fi is set in the Pi OS, not in `detect_pi.py`.
