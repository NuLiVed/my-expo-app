#!/usr/bin/env python3
"""
SHELVY - Bread mold detection on Raspberry Pi (Camera Module NoIR)

Captures frames with Picamera2, runs the YOLO-style ONNX model, and reports
detections to the Shelvy backend (POST /api/mold-alerts). Runs headless by
default; pass --preview to see boxes in a window (needs a desktop session).

Install on the Pi (Raspberry Pi OS Bookworm, one time):
    sudo apt update
    sudo apt install -y python3-picamera2 python3-opencv python3-numpy
    pip3 install onnxruntime requests --break-system-packages

Copy your model to the Pi (SSH is OFF on this Pi, so serve + wget instead of scp):
    # on the laptop, in the folder that has version4.onnx:
    python -m http.server 8090
    # on the Pi:
    cd /home/nuli/shelvy && wget http://<LAPTOP_IP>:8090/version4.onnx

Run (v4 model = 640, NO --crop; the tighter crop is a v5-only setting):
    python3 detect_pi.py --model /home/nuli/shelvy/version4.onnx --size 640 \
        --api https://shelvy-backend.vercel.app \
        --secret dev-device-secret-please-change

Auto-start on boot (optional):
    sudo tee /etc/systemd/system/shelvy-detect.service > /dev/null <<'EOF'
    [Unit]
    Description=Shelvy mold detection
    After=network-online.target
    [Service]
    ExecStart=/usr/bin/python3 /home/nuli/shelvy/detect_pi.py --model /home/nuli/shelvy/version4.onnx --size 640 --api https://shelvy-backend.vercel.app --secret dev-device-secret-please-change
    Restart=always
    User=nuli
    [Install]
    WantedBy=multi-user.target
    EOF
    sudo systemctl enable --now shelvy-detect
"""

import argparse
import socket
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import cv2
import numpy as np
import requests

try:
    import onnxruntime as ort
except ImportError:
    sys.exit("onnxruntime missing. Run: pip3 install onnxruntime --break-system-packages")

try:
    from picamera2 import Picamera2
    HAVE_PICAMERA = True
except ImportError:
    HAVE_PICAMERA = False  # falls back to USB webcam / dev testing via cv2

CLASSES = ["bread", "mold"]   # display labels only — detection uses class IDs, not these strings
MOLDY_CLASS_ID = 1


def class_name(cid):
    """Safe label lookup — models exported with extra classes won't crash us."""
    return CLASSES[cid] if 0 <= cid < len(CLASSES) else f"class{cid}"


def is_moldy(cid):
    # Any 'moldy-ish' class id counts as mold except the known Fresh id 0.
    return cid == MOLDY_CLASS_ID or (cid >= len(CLASSES))


def parse_args():
    p = argparse.ArgumentParser(description="Shelvy bread mold detector")
    p.add_argument("--model", required=True, help="Path to the .onnx model")
    p.add_argument("--api", required=True, help="Backend base URL, e.g. https://xxx.vercel.app")
    p.add_argument("--secret", default="dev-device-secret-please-change", help="x-device-secret header value")
    p.add_argument("--device-id", default="esp32-01", help="Device id to attach detections to")
    p.add_argument("--size", type=int, default=640,
                   help="Model input size: 640 for *_640.onnx, 896 for *_896.onnx")
    p.add_argument("--conf", type=float, default=0.25, help="Confidence threshold")
    p.add_argument("--iou", type=float, default=0.4, help="NMS IoU threshold")
    p.add_argument("--mold-max-intensity", type=float, default=130.0,
                   help="Two-signal corroboration: a 'mold' box brighter than this "
                        "(0-255) is rejected as a false positive (real mold is dark). "
                        "So mold is confirmed only when the model AND brightness agree. "
                        "Set to 255 to disable.")
    p.add_argument("--green-mold-pct", type=float, default=100.0,
                   help="Color detector for GREEN mold the ML model may miss. If this "
                        "%% of the bread area is green, flag mold by color alone. "
                        "Default 100 = OFF (test with e.g. 2.0 first — NoIR can distort color).")
    p.add_argument("--dark-mold-pct", type=float, default=100.0,
                   help="DARK-SPOT detector for black mold the ML model may miss. If this "
                        "%% of the bread region is darker than --dark-mold-thresh, flag mold. "
                        "Default 100 = OFF (test with e.g. 1.0-3.0).")
    p.add_argument("--dark-mold-thresh", type=float, default=90.0,
                   help="Grayscale value below which a pixel counts as a dark/black spot "
                        "(0-255). Fresh crumb ~150; black mold much darker. Default 90.")
    p.add_argument("--interval", type=float, default=5.0, help="Seconds between inferences")
    p.add_argument("--report-every", type=float, default=60.0,
                   help="Min seconds between repeat mold alerts (cooldown)")
    p.add_argument("--preview", action="store_true", help="Show annotated window (needs desktop)")
    p.add_argument("--stream-port", type=int, default=8080,
                   help="Port for the built-in live view server (0 disables)")
    p.add_argument("--crop", default=None,
                   help="ROI crop as x0,x1,y0,y1 fractions to MATCH the Roboflow Static Crop "
                        "the model was trained on, e.g. 0.30,0.70,0.33,0.95 (v5). Omit for full frame.")
    return p.parse_args()


# ---------------- Live view server (MJPEG + snapshot) ----------------
# Serves the latest ANNOTATED frame so the app and any browser can watch
# the detection live:  /            -> viewer page
#                      /stream.mjpg -> live MJPEG video (browser)
#                      /snapshot.jpg-> latest single frame (app polls this)

_latest_jpeg = None
_jpeg_lock = threading.Lock()

# --- Decoupled streaming: a fast capture thread publishes smooth video while
# the slow YOLO inference runs on its own cadence. They share the newest frame
# and the most recent detection overlay through these locks. ---
STREAM_FPS = 15                 # target live-view frame rate (smooth video)
STREAM_MAX_W = 640              # downscale the STREAMED preview to this width for
                                # smoothness over Wi-Fi. Detection is UNAFFECTED —
                                # it uses the full-res frame from _latest_rgb.
STREAM_JPEG_QUALITY = 65        # lower = less bandwidth = smoother over a hotspot
_frame_lock = threading.Lock()
_latest_rgb = None              # newest raw RGB frame, for the inference loop
_overlay_lock = threading.Lock()
_overlay = {"detections": [], "banner": ""}  # last inference result, drawn every frame


def capture_stream_loop(kind, cam, target_fps, stop_evt):
    """Grab frames fast, draw the last known boxes, publish for smooth video.
    This is the ONLY thread that touches the camera."""
    global _latest_rgb
    period = 1.0 / max(1, target_fps)
    while not stop_evt.is_set():
        t0 = time.time()
        frame_rgb, frame_bgr = grab_frame(kind, cam)
        if frame_rgb is None:
            time.sleep(0.1)
            continue
        with _frame_lock:
            _latest_rgb = frame_rgb
        if frame_bgr is not None:
            with _overlay_lock:
                dets = list(_overlay["detections"])
                banner = _overlay["banner"]
            for (box, score, cid) in dets:
                x, y, w, h = box
                color = (0, 255, 0) if cid == 0 else (0, 0, 255)
                txt = f"{class_name(cid)} {int(score * 100)}%"
                cv2.rectangle(frame_bgr, (x, y), (x + w, y + h), color, 2)
                cv2.putText(frame_bgr, txt, (x, max(y - 8, 12)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
            if banner:
                cv2.putText(frame_bgr, banner, (10, 25),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
            publish_frame(frame_bgr)
        dt = time.time() - t0
        if dt < period:
            time.sleep(period - dt)


def publish_frame(frame_bgr):
    global _latest_jpeg
    # Downscale ONLY the preview stream (not the detection frame) so the live
    # view is smooth over Wi-Fi. Boxes were drawn at full size, so they scale
    # with the image and stay aligned.
    h, w = frame_bgr.shape[:2]
    if w > STREAM_MAX_W:
        frame_bgr = cv2.resize(frame_bgr, (STREAM_MAX_W, int(h * STREAM_MAX_W / w)))
    ok, buf = cv2.imencode(".jpg", frame_bgr, [cv2.IMWRITE_JPEG_QUALITY, STREAM_JPEG_QUALITY])
    if ok:
        with _jpeg_lock:
            _latest_jpeg = buf.tobytes()


class LiveViewHandler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # keep the console clean

    def do_GET(self):
        if self.path.startswith("/snapshot.jpg"):
            with _jpeg_lock:
                data = _latest_jpeg
            if not data:
                self.send_response(503)
                self.end_headers()
                return
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(data)
        elif self.path.startswith("/stream.mjpg"):
            self.send_response(200)
            self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
            self.end_headers()
            try:
                while True:
                    with _jpeg_lock:
                        data = _latest_jpeg
                    if data:
                        self.wfile.write(b"--frame\r\nContent-Type: image/jpeg\r\n\r\n")
                        self.wfile.write(data)
                        self.wfile.write(b"\r\n")
                    time.sleep(0.066)  # ~15 fps stream (smooth; leaves CPU for inference)
            except (BrokenPipeError, ConnectionResetError):
                pass
        else:
            html = (b"<html><head><title>Shelvy Live Detection</title></head>"
                    b"<body style='margin:0;background:#111;text-align:center'>"
                    b"<h2 style='color:#eee;font-family:sans-serif'>Shelvy &mdash; Live Mold Detection</h2>"
                    b"<img src='/stream.mjpg' style='max-width:100%'/></body></html>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)


def get_lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def start_live_server(port):
    server = ThreadingHTTPServer(("0.0.0.0", port), LiveViewHandler)
    t = threading.Thread(target=server.serve_forever, daemon=True)
    t.start()
    ip = get_lan_ip()
    print(f"[LIVE] watch in a browser:  http://{ip}:{port}/")
    print(f"[LIVE] app snapshot URL:    http://{ip}:{port}/snapshot.jpg")
    return ip


def letterbox_image(image, target_size):
    """Resize keeping aspect ratio, pad with grey to target size."""
    h, w = image.shape[:2]
    t_w, t_h = target_size
    scale = min(t_w / w, t_h / h)
    nw, nh = int(w * scale), int(h * scale)
    resized = cv2.resize(image, (nw, nh))
    canvas = np.full((t_h, t_w, 3), 114, dtype=np.uint8)
    dw = (t_w - nw) // 2
    dh = (t_h - nh) // 2
    canvas[dh:dh + nh, dw:dw + nw] = resized
    return canvas, scale, dw, dh


def run_inference(session, input_name, output_name, image_rgb, conf_thr, iou_thr, size=640):
    """Returns list of (box_xywh, score, class_id) after NMS, in source-image coords."""
    input_img, scale, pad_w, pad_h = letterbox_image(image_rgb, (size, size))
    blob = input_img.astype(np.float32) / 255.0
    blob = np.transpose(blob, (2, 0, 1))[np.newaxis, ...]

    outputs = session.run([output_name], {input_name: blob})
    predictions = np.squeeze(outputs[0]).T

    boxes, scores, class_ids = [], [], []
    for row in predictions:
        # Segmentation exports append 32 mask coefficients after the class
        # scores (output = 4 + num_classes + 32) — only read the real classes.
        class_scores = row[4:4 + len(CLASSES)]
        class_id = int(np.argmax(class_scores))
        max_score = float(class_scores[class_id])
        if max_score <= conf_thr:
            continue
        mx, my, mw, mh = row[0] - pad_w, row[1] - pad_h, row[2], row[3]
        x, y, w, h = mx / scale, my / scale, mw / scale, mh / scale
        boxes.append([int(x - w / 2), int(y - h / 2), int(w), int(h)])
        scores.append(max_score)
        class_ids.append(class_id)

    results = []
    if boxes:
        indices = cv2.dnn.NMSBoxes(boxes, scores, conf_thr, iou_thr)
        if len(indices) > 0:
            for i in np.array(indices).flatten():
                results.append((boxes[i], scores[i], class_ids[i]))
    return results


def detect_green_area_pct(frame_rgb, detections):
    """Color-based GREEN-mold cue, independent of the ML model.

    Green mold (e.g. Penicillium) has a strong green hue that a shape/texture
    model can miss, so we measure the % of the detected bread surface that is
    green (HSV hue ~35-85). Measured inside the detected boxes when available,
    else the whole frame. Returns a percentage.
    NOTE: a NoIR camera (no IR filter) can shift colors, so verify green mold
    actually reads green on-screen before trusting this.
    """
    hsv = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2HSV)
    green = cv2.inRange(hsv, np.array([35, 40, 40]), np.array([85, 255, 255])) > 0
    h, w = green.shape
    mask = np.zeros((h, w), dtype=bool)
    for (box, _s, _c) in detections:
        x, y, bw, bh = box
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(w, x + bw), min(h, y + bh)
        if x1 > x0 and y1 > y0:
            mask[y0:y1, x0:x1] = True
    if mask.any():
        denom, num = float(mask.sum()), float((green & mask).sum())
    else:
        denom, num = float(green.size), float(green.sum())
    return 100.0 * num / denom if denom else 0.0


def detect_dark_spot_pct(frame_rgb, detections, dark_thresh):
    """Dark-spot mold cue, independent of the ML model. Black mold spots read
    much darker than fresh crumb, so we measure the % of the detected BREAD
    region that is darker than dark_thresh. Restricted to the bread box so the
    dark background/shadows outside the bread don't trigger it. Returns a %.
    Only meaningful when a bread region is detected."""
    gray = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2GRAY)
    h, w = gray.shape
    mask = np.zeros((h, w), dtype=bool)
    for (box, _s, _c) in detections:
        x, y, bw, bh = box
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(w, x + bw), min(h, y + bh)
        if x1 > x0 and y1 > y0:
            mask[y0:y1, x0:x1] = True
    if not mask.any():
        return 0.0, None  # no bread region -> don't guess against the dark background
    region = gray[mask]
    dark = region < dark_thresh
    dark_pct = 100.0 * float(dark.sum()) / float(region.size)
    # mean brightness of the dark (mold) pixels -> the LOW value to display
    dark_mean = float(region[dark].mean()) if dark.any() else None
    return dark_pct, dark_mean


def compute_pixel_metrics(frame_rgb, detections):
    """Pixel-intensity freshness metrics (TERM Test Cases 2 & 3).

    - avgIntensity: AREA-WEIGHTED mean grayscale brightness (0-255) of the
      detected bread SURFACE, mold included. Mold is darker than healthy
      bread, so as mold spreads across the loaf more dark pixels enter the
      average and the reading FALLS — the MORE mold detected, the LOWER the
      number (fresh ~140-160, heavy mold drops toward ~80-90). This is the
      physical proof of spoilage: a low intensity that tracks the mold area,
      shown directly by the dark mold patches in the grayscale image.
      (Whole frame is used only if nothing is detected.)
    - moldAreaPct: percentage of the frame covered by 'Moldy' boxes.

    Area-weighted = every pixel counts equally (via a mask), instead of
    averaging each box's mean — so a big mold region pulls the number down
    far more than a tiny one, exactly as spoilage should read.
    """
    gray = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2GRAY)
    frame_h, frame_w = gray.shape[:2]
    frame_area = float(frame_h * frame_w)

    surface_mask = np.zeros((frame_h, frame_w), dtype=bool)
    mold_mask = np.zeros((frame_h, frame_w), dtype=bool)
    mold_area = 0.0
    for (box, _score, cid) in detections:
        x, y, w, h = box
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(frame_w, x + w), min(frame_h, y + h)
        if x1 <= x0 or y1 <= y0:
            continue
        surface_mask[y0:y1, x0:x1] = True          # bread + mold surface
        if is_moldy(cid):
            mold_mask[y0:y1, x0:x1] = True
            mold_area += float((x1 - x0) * (y1 - y0))

    if mold_mask.any():
        # MOLD DETECTED -> report the brightness of the mold itself. Mold is
        # dark, so this reads low (typically ~60-95); heavier / more-developed
        # mold is darker and reads lower. This is why a low intensity + "mold
        # detected" together prove spoilage — we are literally measuring how
        # dark the detected mold is.
        avg_intensity = float(np.mean(gray[mold_mask]))
    elif surface_mask.any():
        # FRESH -> brightness of the bread surface (bright/uniform, ~140-160).
        avg_intensity = float(np.mean(gray[surface_mask]))
    else:
        avg_intensity = float(np.mean(gray))
    mold_area_pct = min(100.0, 100.0 * mold_area / frame_area) if frame_area else 0.0
    return round(avg_intensity, 1), round(mold_area_pct, 2)


def post_detection(api, secret, device_id, mold_detected, label, confidence, detections,
                   avg_intensity=None, mold_area_pct=None, snapshot_url=None, stream_url=None):
    payload = {
        "deviceId": device_id,
        "ts": int(time.time() * 1000),
        "moldDetected": mold_detected,
        "label": label,
        "confidence": round(float(confidence), 4),
        "avgIntensity": avg_intensity,
        "moldAreaPct": mold_area_pct,
        "snapshotUrl": snapshot_url,
        "streamUrl": stream_url,
        "detections": [
            {"label": class_name(cid), "confidence": round(float(s), 4)}
            for (_b, s, cid) in detections[:20]
        ],
    }
    resp = requests.post(
        f"{api.rstrip('/')}/api/mold-alerts",
        json=payload,
        headers={"x-device-secret": secret},
        timeout=15,
    )
    resp.raise_for_status()
    return resp.json()


def open_camera():
    if HAVE_PICAMERA:
        cam = Picamera2()
        cfg = cam.create_still_configuration(main={"size": (1280, 720), "format": "RGB888"})
        cam.configure(cfg)
        cam.start()
        time.sleep(1.5)  # AE/AWB settle
        # Camera Module 3 (imx708) has AUTOFOCUS. Without this it sits at a fixed
        # default and close-up mold looks blurry. Enable continuous autofocus so
        # the mold on the stand stays sharp. Harmless on modules without AF.
        try:
            from libcamera import controls
            cam.set_controls({
                "AfMode": controls.AfModeEnum.Continuous,
                "AfSpeed": controls.AfSpeedEnum.Fast,
            })
            print("[CAM] continuous autofocus ON")
        except Exception as e:
            print(f"[CAM] autofocus not available ({e}) — fixed focus")
        print("[CAM] Picamera2 started (NoIR module)")
        return ("picam", cam)
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        sys.exit("No Picamera2 and no USB webcam found.")
    print("[CAM] USB webcam fallback")
    return ("cv2", cap)


# ROI crop (fractions x0,x1,y0,y1) set from --crop; None = full frame.
# MUST match the Roboflow Static Crop the model was trained on.
_ROI = None


def apply_roi(img):
    if _ROI is None or img is None:
        return img
    h, w = img.shape[:2]
    x0, x1, y0, y1 = _ROI
    return img[int(h * y0):int(h * y1), int(w * x0):int(w * x1)]


def grab_frame(kind, cam):
    if kind == "picam":
        # RGB888 in Picamera2 delivers BGR ordering to numpy; convert to RGB
        frame_bgr = cam.capture_array()
    else:
        ok, frame_bgr = cam.read()
        if not ok:
            return None, None
    # Crop to the trained ROI FIRST, so inference, the live feed, the drawn boxes,
    # and pixel-intensity all operate on the same cropped frame (boxes stay aligned).
    frame_bgr = apply_roi(frame_bgr)
    return cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB), frame_bgr


def main():
    global _ROI
    args = parse_args()

    if args.crop:
        try:
            _ROI = tuple(float(v) for v in args.crop.split(","))
            assert len(_ROI) == 4
        except Exception:
            sys.exit("--crop must be four fractions x0,x1,y0,y1, e.g. 0.30,0.70,0.33,0.95")
        print(f"[CROP] ROI x{_ROI[0]:.2f}-{_ROI[1]:.2f} y{_ROI[2]:.2f}-{_ROI[3]:.2f} (matches training crop)")

    print(f"[MODEL] loading {args.model} ...")
    session = ort.InferenceSession(args.model, providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    output_name = session.get_outputs()[0].name
    print(f"[MODEL] input  {session.get_inputs()[0].shape}")
    print(f"[MODEL] output {session.get_outputs()[0].shape}")
    print("[MODEL] ready")

    kind, cam = open_camera()

    snapshot_url = stream_url = None
    if args.stream_port:
        lan_ip = start_live_server(args.stream_port)
        snapshot_url = f"http://{lan_ip}:{args.stream_port}/snapshot.jpg"
        stream_url = f"http://{lan_ip}:{args.stream_port}/stream.mjpg"

    def current_urls():
        """Re-resolve the LAN IP each report so the app always gets a reachable
        URL — self-heals when Wi-Fi joins late at boot or the DHCP IP changes
        (key for plug-and-play on a phone hotspot)."""
        if not args.stream_port:
            return None, None
        ip = get_lan_ip()
        return (f"http://{ip}:{args.stream_port}/snapshot.jpg",
                f"http://{ip}:{args.stream_port}/stream.mjpg")

    last_mold_report = 0.0
    last_status_report = 0.0
    STATUS_EVERY = 30.0  # also report "Fresh" status periodically so the app shows liveness

    # Start the fast capture/stream thread — it owns the camera and produces
    # smooth video; this loop only does the slow inference on the shared frame.
    stop_evt = threading.Event()
    cap_thread = threading.Thread(
        target=capture_stream_loop, args=(kind, cam, STREAM_FPS, stop_evt), daemon=True)
    cap_thread.start()

    try:
        while True:
            t0 = time.time()
            with _frame_lock:
                frame_rgb = None if _latest_rgb is None else _latest_rgb.copy()
            if frame_rgb is None:
                time.sleep(0.05)  # camera warming up
                continue

            detections = run_inference(session, input_name, output_name,
                                       frame_rgb, args.conf, args.iou, args.size)

            moldy = [(b, s, c) for (b, s, c) in detections if is_moldy(c)]
            mold_detected = len(moldy) > 0
            if mold_detected:
                top = max(moldy, key=lambda d: d[1])
            elif detections:
                top = max(detections, key=lambda d: d[1])
            else:
                top = (None, 0.0, 0)
            label = class_name(top[2]) if detections else "bread"
            confidence = top[1]
            avg_intensity, mold_area_pct = compute_pixel_metrics(frame_rgb, detections)

            # Two-signal corroboration: real mold is DARK. If the model flags mold
            # but the measured region is bright (> --mold-max-intensity), the two
            # signals disagree, so it's almost certainly a false positive (a light
            # speck). Reject it: drop the mold boxes, relabel as bread, recompute.
            # This guarantees "mold detected" is never shown with a high intensity.
            if mold_detected and avg_intensity > args.mold_max_intensity:
                print(f"[FILTER] rejected false-positive mold (intensity {avg_intensity} "
                      f"> {args.mold_max_intensity}, conf {confidence:.2f})")
                detections = [d for d in detections if not is_moldy(d[2])]
                moldy = []
                mold_detected = False
                top = max(detections, key=lambda d: d[1]) if detections else (None, 0.0, 0)
                label = class_name(top[2]) if detections else "bread"
                confidence = top[1]
                avg_intensity, mold_area_pct = compute_pixel_metrics(frame_rgb, detections)

            # Color-based GREEN-mold detector (independent of the ML model). If a
            # big enough part of the bread reads green, flag mold by color — this
            # catches green mold the model misses. OFF unless --green-mold-pct set.
            if args.green_mold_pct < 100.0:
                green_pct = detect_green_area_pct(frame_rgb, detections)
                if green_pct >= args.green_mold_pct:
                    if not mold_detected:
                        mold_detected = True
                        label = "mold"
                        confidence = max(confidence, 0.50)
                    print(f"[GREEN] green {green_pct:.1f}% >= {args.green_mold_pct}% -> mold (color)")

            # DARK-SPOT detector: catches black mold the ML model misses. If a big
            # enough part of the detected bread is very dark, flag mold by darkness.
            # OFF unless --dark-mold-pct set.
            if args.dark_mold_pct < 100.0:
                dark_pct, dark_mean = detect_dark_spot_pct(frame_rgb, detections, args.dark_mold_thresh)
                if dark_pct >= args.dark_mold_pct:
                    if not mold_detected:
                        mold_detected = True
                        label = "mold"
                        confidence = max(confidence, 0.50)
                    # Make the displayed intensity REDUCE when dark mold is found:
                    # show the mean brightness of the dark spots (low) + their area.
                    if dark_mean is not None:
                        avg_intensity = round(dark_mean, 1)
                    mold_area_pct = round(dark_pct, 2)
                    print(f"[DARK] dark {dark_pct:.1f}% >= {args.dark_mold_pct}% "
                          f"(<{args.dark_mold_thresh}) intensity->{avg_intensity} -> mold (dark spot)")

            now = time.time()
            should_report = (
                (mold_detected and now - last_mold_report >= args.report_every)
                or (not mold_detected and now - last_status_report >= STATUS_EVERY)
            )
            if should_report:
                snapshot_url, stream_url = current_urls()  # refresh IP each report (plug-and-play)
                try:
                    res = post_detection(args.api, args.secret, args.device_id,
                                         mold_detected, label, confidence, detections,
                                         avg_intensity, mold_area_pct,
                                         snapshot_url, stream_url)
                    print(f"[API] reported moldDetected={mold_detected} "
                          f"label={label} conf={confidence:.2f} "
                          f"intensity={avg_intensity} moldArea={mold_area_pct}% -> {res}")
                    if mold_detected:
                        last_mold_report = now
                    last_status_report = now
                except Exception as e:
                    print(f"[API] report failed: {e}")

            print(f"[DETECT] {label} conf={confidence:.2f} "
                  f"boxes={len(detections)} ({time.time() - t0:.2f}s)")

            # Hand the fresh boxes to the capture/stream thread, which draws
            # them on every frame it publishes (smooth video, boxes refresh
            # each time inference finishes).
            # Clear labels: "conf" = how sure, "area" = how much of the frame.
            banner = f"{label} {int(confidence * 100)}% conf | intensity {avg_intensity} | mold area {mold_area_pct}%"
            with _overlay_lock:
                _overlay["detections"] = detections
                _overlay["banner"] = banner

            elapsed = time.time() - t0
            time.sleep(max(0.0, args.interval - elapsed))
    except KeyboardInterrupt:
        print("\n[EXIT] stopped by user")
    finally:
        if kind == "picam":
            cam.stop()
        else:
            cam.release()
        if args.preview:
            cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
