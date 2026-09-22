#!/bin/bash
# ============================================================
# SHELVY — one-time plug-and-play installer (run ON THE PI)
# After this, every time you POWER ON the Pi it will:
#   1. auto-reconnect to the Wi-Fi it's on now (Bawal Connect 5G)
#   2. wait until it has internet
#   3. auto-start mold detection with version4.onnx
#   4. post its CURRENT IP so the app's live feed just works
# ============================================================
set -e
DIR=/home/nuli/shelvy
cd "$DIR"

echo "== files present? =="
ls -la detect_pi.py version4.onnx shelvy-detect.service

# 1) make the CURRENT Wi-Fi auto-connect on boot (highest priority)
CUR=$(nmcli -t -f NAME connection show --active 2>/dev/null | grep -vi '^lo$' | head -1)
if [ -n "$CUR" ]; then
  sudo nmcli connection modify "$CUR" connection.autoconnect yes connection.autoconnect-priority 100
  echo "[OK] '$CUR' will auto-reconnect on boot"
else
  echo "[WARN] no active Wi-Fi found — connect to Bawal Connect 5G first, then rerun"
fi

# 2) install + enable the auto-start service
sudo cp shelvy-detect.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable shelvy-detect
sudo systemctl restart shelvy-detect
echo "[OK] detection service installed + enabled"

# 3) show it running
sleep 3
systemctl status shelvy-detect --no-pager -l | head -15
echo
echo "== live logs (Ctrl-C to stop watching; the service keeps running) =="
echo "   look for:  [API] reported moldDetected=... -> {...}"
journalctl -u shelvy-detect -f
