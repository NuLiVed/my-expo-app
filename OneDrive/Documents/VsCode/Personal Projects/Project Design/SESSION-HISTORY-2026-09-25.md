# Shelvy Session — 2026-09-25 (validation eve, overnight hardware marathon)

## HEADLINE
Two-ESP32 wireless architecture is LIVE and the relay node finally WORKS (on the spare board). Sensor publishing accurate data; relay fetching it and controlling the Peltier.

## ARCHITECTURE (final)
- **ESP1 = SENSOR node** (`SHELVY_ESP1_SENSOR`): power bank (isolated/clean), reads DFR0588 SHT30 (T→GPIO34, RH→GPIO35, VCC→3V3), publishes temp+humidity to backend device `esp32-01`.
- **ESP2 = RELAY node** (`SHELVY_ESP2_RELAY_25`): buck 5V→VIN (common ground with 12V), FETCHES temp from backend, runs control, drives relay (IN1→32, IN2→33), publishes peltier state to `esp32-01`.
- No wire between the two ESP32s → the noisy 12V/relay ground can never corrupt the clean analog sensor. Comms via backend HTTP (`shelvy-backend.vercel.app`), ~1.5–3s.

## LATEST CODE (use these)
- **Relay:** `esp32-firmware/SHELVY_ESP2_RELAY_25/` — **target 25.0°C** (cools until 25, clicks OFF 30s, repeats). Chamber ~26.5°C so it ACTIVELY cools/switches (the 27.5 version just sat off).
  - `RELAY_BOARD_ACTIVE_LOW = false` (board energizes on HIGH → **jumper on HIGH**, S1/S2→High)
  - Load on COM + NC (cooling ON by default). Self-test removed (no boot click).
- **Sensor:** `esp32-firmware/SHELVY_ESP1_SENSOR/` — **calibration fixed 2026-09-25**: `TEMP_CAL_OFFSET = 0.8` (was −2.8), `HUMI_CAL_OFFSET = −2.1` (was 1.0). Reads ~26.5°C/~67% to match reference.
- Old `SHELVY_ESP2_RELAY` (27.5°C) kept but NOT used.

## HARDWARE LESSONS (this session)
- **Relay board bug:** this red board **energizes on HIGH** (not LOW). Code fixed to `ACTIVE_LOW=false`, jumper must be on **HIGH** (Com–High) for S1/S2. Cooling ON = de-energized = NC closed = power flows on boot (no click).
- **Power the ESP32 with 5V→VIN, NOT 3.3V→3V3.** Buck at 3.3V into 3V3 pin = brownout, no red LED, unstable. Red power LED lives on the 5V rail. 5V→VIN mimics USB.
- **SMOKE ROOT CAUSE:** high-current output (COM→NC→Peltier+2 fans ≈ 7A) was on **thin 22 AWG + a bad white/overheated solder joint** → burned. Fix: **16–18 AWG** on the whole COM→NC→Peltier→fan path, re-solder clean shiny joints, ideally run fans on their own 12V wire. DC+/DC− (coil only, ~40mA) = 22 AWG is fine.
- Relay `SRD-12VDC-SL-C` = 10A/30VDC per contact (fine for one Peltier ~6A). Coil ~400Ω; tested good (survived).
- **Original relay ESP32 = flash chip damaged** (persistent `partition 0 invalid magic number 0x2200` boot loop even after erase + verified write). Switched to **SPARE ESP32** → flashed clean first try.
- **Flash recipe that works:** bare board (off expansion), Upload Speed **115200**, "Erase All Flash Before Sketch Upload" **Enabled**, **CLOSE Serial Monitor** (else port busy), hold **BOOT** if it hangs. Look for `Hash of data verified` + `Hard resetting via RTS pin`.

## STATUS
- Sensor: ✅ publishing accurate temp/humidity to `esp32-01`.
- Relay (spare): ✅ flashed clean, WiFi connected, `[FETCH] T=26.65` working, control loop running (`REST(off)` because 26.65 < old 27.5 setpoint → new 25°C code fixes this to actively cool).
- App: newest APK `Shelvy-app-2026-09-23_1952.apk` — reads `esp32-01`, no app change needed (relay publishes peltier state there).

## NEXT
- Upload `SHELVY_ESP2_RELAY_25` to spare + calibrated `SHELVY_ESP1_SENSOR` to sensor board.
- Wire spare to expansion: buck 5V→VIN (red LED), IN1→32, IN2→33, DC+/DC− thin, Peltier on COM+NC with **thick 16–18 AWG**, jumper on HIGH.
- Confirm live: relay shows `[CTRL] COOLING` then `reached 25.0C → OFF 30s (CLICK)`; app fires peltier notification.
- TC1 (temp/humidity) + TC4 (alerts/peltier notif) demo-ready once both boards live.
