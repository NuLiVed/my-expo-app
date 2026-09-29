# Shelvy — Project History & Context (upload this to claude.ai for help)

> This file is a full context handoff. If you (Weslie) start a new Claude chat,
> upload this file and say what you need — the assistant will have the picture.

## What Shelvy is
A school **Project Design** capstone (Team, TIP-QC): a **bread storage chamber**
that monitors temperature/humidity, cools with Peltier modules, and detects
**mold** with a camera + AI. Built March 2026 → Sept 2026. **Technical defense
PASSED** (panel said "needs minor improvement to pass").

## Architecture (two-ESP32 + Pi + cloud + app)
```
ESP32 SENSOR (power bank, isolated)  --reads SHT30-->  publishes temp/humidity ┐
ESP32 RELAY  (buck 5V, opto relay)   --fetches temp--> drives Peltier          ├─> Vercel backend --> Firebase --> Mobile app (APK)
Raspberry Pi (NoIR cam + YOLOv8-seg ONNX) --> publishes mold verdict           ┘
```
- All devices use device id **`esp32-01`** and header `x-device-secret: dev-device-secret-please-change`.
- The two ESP32s do NOT share a wire — the relay's noisy 12V ground can't corrupt the analog sensor. They coordinate through the backend.

## Cloud / storage
| Service | URL / id | Notes |
|---|---|---|
| Backend | https://shelvy-backend.vercel.app | Vercel, account **weslie23** |
| Database | Firebase **env-monitor-v2** (Realtime DB) | service key = base64 env `FIREBASE_SERVICE_ACCOUNT` |
| Code | github.com/**NuLiVed/my-expo-app** (branch main) | repo root is C:\Users\wesli; only add specific Shelvy paths |

Backend endpoints: `POST/GET /api/readings/esp32-01[/latest|/history|/alerts]`, `GET /api/mold-alerts/esp32-01/latest`, `POST /api/mold-alerts`, `/api/auth/*`.
Redeploy backend: `vercel deploy --prod --yes` from `Project Design\shelvy-mobile-app-backend\env-monitor-backend`.

## Local folders (base: `...\Personal Projects\Project Design\`)
- `esp32-firmware\` — all ESP32 code
- `shelvy-mobile-app-frontend\` — React Native (Expo framework) app
- `shelvy-mobile-app-backend\env-monitor-backend\` — Node/Express backend
- `raspi-mold-detection\` — Pi code (detect_pi.py, model, service)
- `Weslie_Shelvy_Mold_Training.ipynb` — Colab training notebook
- `Shelvy-app-*.apk` — built APKs (newest: `Shelvy-app-2026-09-23_1952.apk`)
- `SESSION-HISTORY-*.md`, `goodbye_instructions.txt` — notes/handoff

## ESP32 firmware (in esp32-firmware\)
| Node | HOME (house WiFi) | SCHOOL (phone hotspot) |
|---|---|---|
| Sensor | `SHELVY_ESP1_SENSOR` | `SHELVY_ESP1_SENSOR_5Ghotspot` |
| Relay | `SHELVY_ESP2_RELAY_25` | `SHELVY_ESP2_RELAY_5Ghotspot` |
- Only WiFi differs. HOME SSID `Bawal  Connect!` (2 spaces) / `@Cute@@KamE`. SCHOOL SSID `Bawal Connect! 5g` / `12345678`.
- Relay: target **25.0°C**, off 30s; IN1→GPIO32, IN2→GPIO33; `RELAY_BOARD_ACTIVE_LOW=true` (blue opto); load on COM+NC (cooling ON default).
- Sensor: SHT30 analog, T→34, RH→35, VCC→3V3; cal `TEMP_CAL_OFFSET=0.8`, `HUMI_CAL_OFFSET=-2.1`.
- Pins: 34/35/36/39 = INPUT-ONLY (sensor only, can't drive relay); 32/33/25/26/27/14 = output OK; avoid 0/2/12/15 (strapping), 6-11 (flash).

## Relay hardware
- **Main = 6-ch 12V OPTO (blue, 2 grounds):** input header GND/IN1-6/VCC → ESP32 (VCC=5V, GND=ESP32 GND); power header JD-VCC→12V+, GND→12V−, **remove jumper**. Load on COM/NC with **16-18 AWG** (Peltier ~6-7A). Active-LOW. Opto isolates ESP32 from 12V (fixes the ground/smoke issues).
- **Backup = 4-ch 12V high/low (red, male pins):** transistor board, 5VDC coils, needs **common ground** (ESP32 GND + 12V−). High/Low jumper must match `RELAY_BOARD_ACTIVE_LOW`.
- Power ESP32: **buck 5.0V → VIN**, never 12V, never 3.3V-into-3V3. Red LED = powered.

## Raspberry Pi (mold camera)
- User `nuli`, `/home/nuli/shelvy`, **SSH OFF** (use monitor+keyboard or enable via `ssh` file on SD boot partition).
- Run: `python3 detect_pi.py --model version4.onnx --size 640 --conf 0.15 --mold-max-intensity 255 --dark-mold-pct 2.0 --dark-mold-thresh 90 --api https://shelvy-backend.vercel.app --secret dev-device-secret-please-change`
- `--conf 0.15` = lowered so it catches more mold (bread is dominant). Dark-spot detector catches black mold. Camera ~15cm = reliable.
- Live stream: `http://<PI-IP>:8080/stream.mjpg`. systemd `shelvy-detect` auto-starts on boot.
- WiFi via `nmcli` on the Pi. Same-SSID house-vs-hotspot caused it to grab the wrong network — give them different names or turn one off.

## Model / training
- Current Pi model `version4.onnx` (2-class seg, 640, opset12). Retraining from a new dataset (mold/bread/mixed) with green + dark mold focus.
- Notebook `Weslie_Shelvy_Mold_Training.ipynb`: yolov8s-seg, imgsz 640 (960 for tiny green mold), 150 epochs, confusion matrix (TC3), ONNX export. Roboflow: annotate each instance, augment 2-3×, keep variety not repeats.

## Mobile app
- Reads `esp32-01`. Alerts: humidity>70%, temp>27°C, mold, ESP32-offline (context/TelemetryContext.js). Data pipeline untouched.
- **NEW (2026-09-30): pull-to-refresh** on Dashboard + Readings (swipe down to reload; label "Reloading…").
- Build from short path **C:\s** (OneDrive long path breaks the native build). See goodbye_instructions.txt §8 for the full recipe.

## Test cases (standards)
TC1 temp/humidity (ISO 5725) · TC2 pixel intensity (ISO 5725 + 24029) · TC3 mold (ISO 24029) · TC4 alerts (ISO/IEC 25010) · TC5 error handling (ISO/IEC 25010). All passing; TC3 improving with retrain.

## Hard-won gotchas (don't relearn these)
- Flash ESP32 **bare** (off expansion), **115200**, **Erase All Flash**, **close Serial Monitor**, hold **BOOT**. Success = "Hash of data verified".
- `partition 0 invalid magic number 0x2200` boot loop = corrupt/damaged flash → erase+reflash; if persists, the flash chip is dead → use a spare.
- Relay smoke = thin wire (22 AWG) + bad solder joint on the ~7A Peltier path → 16-18 AWG + clean joints. DC+/DC− and coil = tiny current (thin wire OK).
- A 30A PSU doesn't push 30A through a coil wire — current is set by the load; danger is only a short.
- Opto relay needs NO common ground (light bridge); the red transistor board DOES.

## Current open items
- Reflash both ESP32s (new WiFi builds) + wire the new opto relay.
- Retrain model with new dataset → deploy as `version5.onnx` (`--conf 0.15`).
- Rebuild APK if you want pull-to-refresh in the installed app.
- (Optional) lower app temp alert to ~26°C to match the 25°C setpoint.

## Status
Defense PASSED. All code on GitHub. Hardware being finalized with 2 new relays + 2 new ESP32s.
