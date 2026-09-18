# Shelvy — Session History 2026-09-18

## Focus: recover ESP32 (boot loop), harden firmware vs buck noise, YOLOv8 notebook

### ✅ MAJOR WINS TODAY
- **ESP32 boot loop RECOVERED.** Symptom: `invalid header: 0xffffffff` repeating
  (`rst:0x10 RTCWDT_RTC_RESET` and `rst:0x7 TG0WDT_SYS_RESET`) = empty/corrupt flash.
  **Fix (works every time): Tools → "Erase All Flash Before Sketch Upload" → Enabled,
  then re-upload with the SENSOR DISCONNECTED.** Chip is confirmed ALIVE — boots,
  connects WiFi, posts to cloud. **NOT fried.**
- **New firmware v5 created:** `esp32-firmware/SHELVY_ESP32_v5_cloud/SHELVY_ESP32_v5_cloud.ino`
  (in its OWN folder so it won't collide with the other .ino files — Arduino compiles
  ALL .ino in one folder together).
- **v5 confirmed running + posting** fresh data to `shelvy-backend.vercel.app` (today's date,
  advancing timestamps).
- **YOLOv8 Colab notebook created:** `Shelvy_YOLOv8_Seg_Training.ipynb` (mold dataset training).

### FIRMWARE v5 — what changed vs v3_cloud
1. **Ripple-rejecting trimmed mean** in `readVolts()`: 41 samples, sort, drop 8 highest + 8
   lowest (the buck spikes), average the middle ~25. Combats buck-converter switching noise.
2. **Calibration offsets** (tunable): `TEMP_CAL_OFFSET=-2.2`, `HUMI_CAL_OFFSET=-5.0`
   (from USB bench: read 28.7 vs real 26.5; 78.6 vs ~73). RE-VERIFY on stable power.
3. **3 WiFi networks** in WifiMulti (auto-joins strongest): home, pd-shelvy, shelvyapp.
   NOTE: shelvyapp password still PENDING from user (placeholder in code). Same firmware
   works on all ESP32s. Channel-13 fix retained for phone hotspots. All nets must be 2.4 GHz.
- Conversion math UNCHANGED — DFR0588 board regulates its output to a fixed 0.3–2.7V range,
  so the original formula is correct (that's why USB gave ~28.7°C).

### POWER FINDINGS (the core issue)
- Chain: **12V 20A PSU → buck converter (set to 5.0V) → ESP32 5V/VIN pin.**
- Buck output measured: **wanders 4.8–5.0V** (a multimeter only sees this SLOW drift, NOT the
  fast switching ripple that actually corrupts the analog sensor).
- **Laptop USB = clean 5V** (battery-smooth) → accurate readings. **Buck = noisy** → junk.
  Same "5V" on a meter, totally different on the sensor.
- **Recurring boot loop root cause = power brownout** (unstable buck after it "popped", and/or
  a short when sensor VCC+GND were pushed near the 5V terminals). Brownout during flash access
  → flash reads as 0xffffffff → boot loop.

### SENSOR WIRING (settled)
- **VCC → 3V3 pin is BEST** (ESP32's clean regulated output = quietest supply). 5V also works
  and gives the SAME numbers (board regulates), just noisier. Either is fine; 3V3 preferred.
- **GND → a GND pin** (the black jumper hole immediately LEFT of 5V; any GND hole works — all
  common). CRITICAL: VCC and GND must NOT touch → that short caused the brownout.
- **T → P34, RH → P35** (ADC1, WiFi-safe, input-only).
- Add **10µF electrolytic + 0.1µF ceramic across sensor VCC↔GND** = hardware half of noise fix.

### ⏳ STILL OPEN (next session)
- **Sensor reads junk (-50°C / 0%)** even with ESP32 healthy = sensor UNPOWERED or DEAD (steady
  low ~0.14V, not floating noise). **Diagnose with multimeter (DC V):**
  - Sensor VCC↔GND should read ~5V (else VCC wiring bad).
  - Sensor T-output↔GND should read ~1.0–1.3V at room temp (0V while VCC=5V → sensor dead → swap spare).
  - OR just swap in the spare sensor into the same 4 holes and re-check.
- Get buck STEADY at 5.0V (+ caps) so no more brownouts; consider powering ESP32 from USB for a
  stable demo if the buck stays flaky.
- shelvyapp WiFi password (to finish v5 network list).
- Then fine-tune the calibration offsets on stable power.

### YOLOv8 / Roboflow (mold dataset) — answers given
- Notebook: instance-seg, `yolov8s-seg`, epochs=200 + patience=25, **imgsz=768** (small mold spots),
  `copy_paste=0.3` + flips for the class imbalance, prints per-class box+mask mAP, shows prediction
  images, saves best.pt to Drive. User edits only `DATASET_DIR` (Roboflow export in Google Drive).
- Classmate Q&A: (1) bounding boxes do NOT hurt seg — polygons are what train; ensure export =
  Instance Segmentation. (2) NMS is inference-only, not the cause of low mAP. (3) Keep BOTH bread+mold
  classes (needed for mold-coverage %); fix imbalance with augmentation, not by deleting bread.
  (4) mAP is computed by YOLO's own val(), not fabricated — check whether the "50%" was mAP50 (weak)
  or mAP50-95 (actually decent for seg).

### Reminders
- Never plug USB + buck 5V at the same time (backfeed can damage the laptop port).
- `.ino` contains WiFi passwords + DEVICE_SECRET → NEVER pushed to the public repo.
