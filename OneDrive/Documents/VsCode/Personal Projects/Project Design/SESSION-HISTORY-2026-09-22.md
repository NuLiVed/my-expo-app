# Shelvy — Session History 2026-09-22 (prototype-validation prep)

**Deadline context:** prototype validation is 2026-09-23 (tomorrow). Lead panel = Dr. Alonica R. Villanueva (doctorate). This session = get the Pi, app, and sensing ready + honest test-case framing.

## ✅ HARDWARE / SENSOR — SOLVED
- **Switched to SHT30 (DFRobot DFR0588 analog SHT30) + isolated power = accurate + no spikes.**
  - SHT30 vs external reference: **±0.12°C, ±0.6% RH** at steady state — within datasheet spec (±0.3°C, ±3%).
  - Root fix = powering **ESP32 + SHT30 from a separate power bank (5V/2.1A)**, isolated from Peltier noise. The old PSU-sag spikes (65°C) are GONE.
  - Verified stable under machine load (10-min cooling run: smooth curve, no spikes).
- **Cooling works:** chamber pulled 29°C → **24.9°C (below 25°C target)** in ~13 min; humidity 86% → ~74% heading to 70%. Pull-down ~8 min slow-start then accelerates.
- **Calibration: NOT needed** (within spec). Also calibrating to the reference would make TC1 circular — report raw agreement instead.
- **Power rules:** ESP32+SHT30 → power bank (2.1A). Pi 5 → its own 5V/5A (NOT 2.1A). Peltiers → separate 12V high-current. ESP32 is 2.4GHz-only.
- Insulation upgrades still ongoing (user's hardware task).

## ✅ RASPBERRY PI — v4 MODEL + PLUG-AND-PLAY DONE
- **v4 model exported to ONNX:** `version4.pt` → `version4.onnx` (640, opset12, [1,38,8400] = 2-class seg). On Pi at `/home/nuli/shelvy/version4.onnx`.
- **Plug-and-play installed:** `shelvy-detect.service` (systemd, auto-start on boot, waits for internet, restart=always) + WiFi auto-connect. Files: `raspi-mold-detection/{detect_pi.py, version4.onnx, shelvy-detect.service, INSTALL-ON-PI.sh, PLUG-AND-PLAY-SETUP.txt}`.
- **WiFi auto-connect:** Pi (nmcli) = Bawal Connect! 5g (pri 100) + pd-shelvy (12345678, pri 50). ESP32 (WiFiMulti in v6 firmware) = Bawal Connect!, pd-shelvy, shelvyapp(placeholder pw). At school: all 3 join **pd-shelvy (2.4GHz)** → data + video work.
- **detect_pi.py changes (all deployed):**
  - v4 @ 640, NO --crop (crop is v5-only).
  - IP self-heal (re-resolves LAN IP each report → live-feed URL survives late WiFi join / IP change).
  - **Continuous autofocus** (imx708 Camera Module 3) — fixes blur.
  - **Stream downscaled to 640px + JPEG q65 @ 15fps** for smooth preview over WiFi; DETECTION still uses full-res frame (unaffected).
  - Pixel-intensity = **mold region when mold detected (low), bread surface when fresh (high)**; area-weighted.
  - `--mold-max-intensity` corroboration filter (default 130) — **disabled (255)** because NoIR mold isn't dark enough (reads ~136, not <130).
  - `--green-mold-pct` green detector (OFF) — NoIR washes out green.
  - **`--dark-mold-pct` DARK-SPOT detector** — catches black mold the model misses; when it fires, **intensity drops to the dark-spots' mean brightness** (so mold → low intensity, as intended).

## ⭐ KEY DETECTION INSIGHT: camera at ~15cm
- **Mold detection works reliably with the camera ~15cm from the bread** (the 14cm food stand). At that distance the mold fills the frame at training-scale → model + dark-spot detector both catch it.
- **LOCK 15cm as a fixed setup parameter** for all TC2/TC3 captures (repeatable results).
- **FINAL working Pi settings:** `--model version4.onnx --size 640 --conf 0.20 --mold-max-intensity 255 --dark-mold-pct 2.0 --dark-mold-thresh 90`

## ✅ MOBILE APP — newest APK
- **`Shelvy-app-2026-09-22_1945.apk`** (install THIS, over the old one). Built from short-path `C:\s` (JS-only, native cached, BUILD SUCCESSFUL 3m08s).
- Changes (display-layer ONLY — HTTP fetch / data-capture pipeline in api.js UNTOUCHED):
  - **TC5 step 8:** `DISABLE_SENSOR_SANITY_CHECKS=false` → rejects out-of-range readings (>80°C, >100%).
  - **TC5 step 6:** camera stale >60s → shows **"CAMERA OFFLINE"** (not old frame). `CAMERA_STALE_MS=60000` in ReadingsScreen.
  - Offline temp/humidity → **0** (STALE_READING_THRESHOLD_MS=5min).
  - Notifications fire in bar when app open/background (local notifications; NO FCM).
- Pixel intensity is DISPLAY-only; the MODEL is the detector. Confirmed does not affect model confidence.

## PIXEL INTENSITY — honest facts (for defense)
- Scale is **0–255 (8-bit grayscale)** — max=255 set by the image format, NOT the camera. Fresh bread ≈ **150–160** under this NoIR+lighting; mold reads **lower** (model-mold ~136; dark-spot mold ~60–80).
- **No universal per-object pixel-intensity standard exists** (hardware/lighting dependent). TC2 standards used = ISO 5725 (accuracy/precision) + ISO 24029 (NN robustness) — correct (no pixel-value standard claimed).
- Defense line: *"Pixel intensity is a supporting indicator on the 0–255 grayscale; the freshness verdict is from the segmentation model. We established our baseline empirically (fresh ~150) and measure the relative decrease."*

## TEST CASE STATUS
- **TC1** (temp/humidity accuracy): 🟢 sensor accurate + cooling works; reading interval = 2s (SAMPLE_INTERVAL_MS, compliant with 1–2s spec); publish interval 5s (separate). Insulation ongoing.
- **TC2** (pixel intensity freshness): 🟢 system final (fresh high / mold low); needs multi-day capture at 15cm. Retake any shot with confidence 0% (whole-frame fallback artifact).
- **TC3** (mold detection): 🟢 dark mold reliable at 15cm; green/white/small-spot = dataset limitation → future work.
- **TC4** (alert system): 🟢 REWORDED step 7 to "open + background" (removed "fully closed" — no FCM). Broaden to test humidity(>70%)/temp(>27°C)/mold alerts. Background: trigger alert within ~1–2 min of minimizing.
- **TC5** (error handling): 🟢 all 5 faults handled by `_1945` APK (phone offline, Pi offline, camera off→CAMERA OFFLINE, sensor off, out-of-range rejected).

## LIMITATIONS → FUTURE WORK (defense-ready)
- **Green/white/small-spot mold** under NoIR = dataset + domain-gap limitation. Fix = **retrain AFTER defense with a NEW NoIR-native dataset captured at 15cm, consistent lighting, more mold variety + specimens** (matches the TERM Risk Plan "add training samples and retrain"). Do NOT modify the NoIR camera (invalidates the model). Do NOT retrain before the defense.
- **Closed-app push notifications** = future work (FCM). App delivers alerts while active (open/background).
- YOLOv8 on Pi 5 = CPU-only (~0.7s/inference) → low FPS by design; fine for stationary bread.

## NEXT / OPEN
- Install `_1945`, run TC2/TC3 (15cm) + TC4 (open/bg) + TC5 (disconnect trials) tonight/this week.
- Decide TC1 interval reporting (reading=2s already compliant, or set publish=2s too).
- Clean up TERM plan typos (TC2–TC5) — offered, pending.
- After defense: retrain with NoIR 15cm dataset; optionally add FCM push + gallery-save for captures.
