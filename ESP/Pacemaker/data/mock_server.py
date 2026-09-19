#!/usr/bin/env python3
"""
Pacemaker Dashboard - Local Mock Server
----------------------------------------
Runs the exact same index.html / app.js / style.css on your laptop and
fakes the ESP32's HTTP API so you can click through the whole dashboard
without the hardware plugged in.

Usage:
    python3 mock_server.py
    (then open http://localhost:8000 in your browser)

No third-party packages required. If Pillow is installed, the OLED
preview page will render real text; otherwise it falls back to a
simplified animated bar (still shows the OLED tab working).
"""

import json
import os
import random
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    from PIL import Image, ImageDraw, ImageFont
    HAVE_PIL = True
except ImportError:
    HAVE_PIL = False

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PORT = 8000

# --------------------------------------------------------------------------
# Simulated device state (mirrors the globals in Pacemaker.ino)
# --------------------------------------------------------------------------

lock = threading.Lock()

settings = {
    "volume": 60,
    "targetBPM": 75,
    "normalLow": 60,
    "normalHigh": 100,
    "atriumRest": 90,
    "atriumBeat": 70,
    "ventricleRest": 90,
    "ventricleBeat": 70,
    "theme": "system",
    "accent": "violet",
}

state = {
    "rawBPM": 75,
    "effectiveBPM": 75,
    "pacing": False,
    "signalQuality": 97.0,
    "rawSensor": 2048,
    "atriumActive": False,
    "ventricleActive": False,
    "manualBPMEnabled": False,
    "manualBPMValue": 75,
    "reposition": False,
}

fake = {
    "phase": "normal",  # "normal" | "excursion"
    "current": 75.0,
    "target": 75.0,
    "state_until": 0.0,
}

boot_time = time.time()
events = []  # list of {"time": ms_since_boot, "event": str}

SIGNAL_QUALITY_GOOD_THRESHOLD = 95.0


def add_event(text):
    events.append({"time": int((time.time() - boot_time) * 1000), "event": text})
    if len(events) > 20:
        events.pop(0)


add_event("System booted (simulated)")
add_event("WiFi access point started (simulated)")
add_event("Web server started (simulated)")


# --------------------------------------------------------------------------
# Background simulation loop - mirrors updatePulseSimulation() /
# updatePacingLogic() / runBeatScheduler() from the firmware.
# --------------------------------------------------------------------------

def simulate_signal_quality(now):
    # Slow random walk, mostly a good contact, occasionally dipping low
    # so you can see the "reposition" state work too.
    drift = random.uniform(-1.5, 1.5)
    quality = state["signalQuality"] + drift
    if random.random() < 0.01:
        quality -= random.uniform(10, 25)  # occasional simulated bad contact
    state["signalQuality"] = max(60.0, min(99.5, quality))


def update_fake_bpm(now):
    if state["manualBPMEnabled"]:
        state["rawBPM"] = state["manualBPMValue"]
        state["reposition"] = False
        return

    if state["signalQuality"] <= SIGNAL_QUALITY_GOOD_THRESHOLD:
        state["reposition"] = True
        return

    state["reposition"] = False

    if now >= fake["state_until"]:
        if fake["phase"] == "normal":
            if random.random() < 0.08:
                fake["phase"] = "excursion"
                if random.random() < 0.5:
                    fake["target"] = settings["normalHigh"] + random.uniform(10, 40)
                    add_event("Elevated heart rate detected")
                else:
                    fake["target"] = max(
                        30, settings["normalLow"] - random.uniform(5, 30)
                    )
                    add_event("Low heart rate detected")
                fake["state_until"] = now + random.uniform(3, 8)
            else:
                fake["target"] = settings["targetBPM"] + random.uniform(-3, 3)
                fake["state_until"] = now + random.uniform(4, 9)
        else:
            fake["phase"] = "normal"
            fake["target"] = settings["targetBPM"] + random.uniform(-3, 3)
            fake["state_until"] = now + random.uniform(5, 12)
            add_event("Heart rate back to normal")

    fake["current"] += (fake["target"] - fake["current"]) * 0.25
    fake["current"] += random.uniform(-2, 2)
    state["rawBPM"] = int(round(max(30, min(200, fake["current"]))))


def update_pacing_logic():
    bpm = state["rawBPM"]
    abnormal = bpm < settings["normalLow"] or bpm > settings["normalHigh"]

    if abnormal:
        if not state["pacing"]:
            state["pacing"] = True
            add_event("Pacing activated")
        if state["effectiveBPM"] < settings["targetBPM"]:
            state["effectiveBPM"] += 1
        elif state["effectiveBPM"] > settings["targetBPM"]:
            state["effectiveBPM"] -= 1
    else:
        if state["pacing"]:
            add_event("Pacing stopped")
        state["pacing"] = False
        state["effectiveBPM"] = bpm


def status_text():
    if state["reposition"]:
        return "REPOSITION"
    bpm = state["rawBPM"]
    if bpm < settings["normalLow"]:
        return "LOW"
    if bpm > settings["normalHigh"]:
        return "HIGH"
    return "NORMAL"


def simulation_loop():
    last_bpm_tick = 0
    last_beat = 0
    next_ventricle_at = None

    while True:
        now = time.time()

        with lock:
            simulate_signal_quality(now)
            state["rawSensor"] = int(
                2048 + 600 * random.uniform(-1, 1) * (state["signalQuality"] / 100)
            )

            if now - last_bpm_tick >= 0.4:
                last_bpm_tick = now
                update_fake_bpm(now)
                update_pacing_logic()

            # Beat scheduler: pulses atrium then ventricle shortly after,
            # just enough to animate the LED/servo status flags.
            interval = 60.0 / max(state["effectiveBPM"], 1)
            if not state["atriumActive"] and now - last_beat >= interval:
                last_beat = now
                state["atriumActive"] = True
                next_ventricle_at = now + 0.16
                threading.Timer(0.12, _clear_atrium).start()
            if next_ventricle_at and now >= next_ventricle_at:
                state["ventricleActive"] = True
                next_ventricle_at = None
                threading.Timer(0.12, _clear_ventricle).start()

        time.sleep(0.05)


def _clear_atrium():
    with lock:
        state["atriumActive"] = False


def _clear_ventricle():
    with lock:
        state["ventricleActive"] = False


threading.Thread(target=simulation_loop, daemon=True).start()


# --------------------------------------------------------------------------
# OLED preview - best-effort pixel buffer matching the SSD1306 layout
# --------------------------------------------------------------------------

def build_oled_buffer():
    w, h = 128, 64

    if not HAVE_PIL:
        # Simplified fallback: a header rule + a bar whose width tracks BPM.
        buf = bytearray(w * (h // 8))
        for x in range(w):
            buf[x + (12 // 8) * w] = 0xFF  # horizontal rule at y=12-ish
        bar_w = int(w * min(state["rawBPM"], 200) / 200)
        for x in range(bar_w):
            for page in range(4, 6):
                buf[x + page * w] = 0xFF
        return bytes(buf)

    img = Image.new("1", (w, h), 0)
    draw = ImageDraw.Draw(img)
    font = ImageFont.load_default()

    draw.text((0, 0), f"Reading: {state['rawBPM']} bpm", fill=1, font=font)
    draw.line([(0, 12), (w, 12)], fill=1)

    text = status_text()
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    draw.text(((w - tw) / 2, 18), text, fill=1, font=font)
    draw.line([(0, 36), (w, 36)], fill=1)

    if state["reposition"]:
        draw.text((0, 46), "Adjust finger", fill=1, font=font)
        draw.text((0, 56), "and try again", fill=1, font=font)
    elif state["pacing"]:
        draw.text((0, 46), f"PACING -> {int(state['effectiveBPM'])} bpm", fill=1, font=font)
        draw.text((0, 56), "Correcting rhythm", fill=1, font=font)
    else:
        draw.text((0, 46), "Idle, monitoring", fill=1, font=font)

    # Pack into SSD1306's column-major, page(8px)-tall byte layout.
    pixels = img.load()
    buf = bytearray(w * (h // 8))
    for page in range(h // 8):
        for x in range(w):
            byte = 0
            for bit in range(8):
                y = page * 8 + bit
                if pixels[x, y]:
                    byte |= 1 << bit
            buf[x + page * w] = byte
    return bytes(buf)


# --------------------------------------------------------------------------
# HTTP handler
# --------------------------------------------------------------------------

STATIC_FILES = {
    "/": ("index.html", "text/html"),
    "/index.html": ("index.html", "text/html"),
    "/style.css": ("style.css", "text/css"),
    "/app.js": ("app.js", "application/javascript"),
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # keep the console quiet

    def _send_json(self, payload, status=200):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return None
        raw = self.rfile.read(length)
        try:
            return json.loads(raw)
        except json.JSONDecodeError:
            return None

    def do_GET(self):
        if self.path in STATIC_FILES:
            filename, content_type = STATIC_FILES[self.path]
            path = os.path.join(BASE_DIR, filename)
            if not os.path.exists(path):
                self.send_response(404)
                self.end_headers()
                self.wfile.write(b"File not found (place this script next to index.html/app.js/style.css)")
                return
            with open(path, "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path == "/api/data":
            with lock:
                self._send_json({
                    "bpm": state["rawBPM"],
                    "effectiveBPM": int(state["effectiveBPM"]),
                    "status": status_text(),
                    "pacing": state["pacing"],
                    "targetBPM": settings["targetBPM"],
                    "normalLow": settings["normalLow"],
                    "normalHigh": settings["normalHigh"],
                    "signalQuality": state["signalQuality"],
                    "rawSensor": state["rawSensor"],
                    "reposition": state["reposition"],
                    "atriumActive": state["atriumActive"],
                    "ventricleActive": state["ventricleActive"],
                    "volume": settings["volume"],
                    "manualBPM": state["manualBPMEnabled"],
                    "manualBPMValue": state["manualBPMValue"],
                    "uptime": int((time.time() - boot_time) * 1000),
                })
            return

        if self.path == "/api/oled":
            buf = build_oled_buffer()
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
            self.send_header("Content-Length", str(len(buf)))
            self.end_headers()
            self.wfile.write(buf)
            return

        if self.path == "/api/system":
            self._send_json({
                "chipModel": "ESP32 (simulated)",
                "chipRevision": 1,
                "cores": 2,
                "cpuMHz": 240,
                "freeHeap": random.randint(150000, 200000),
                "minFreeHeap": 140000,
                "maxAllocHeap": 110000,
                "flashSize": 4194304,
                "flashSpeed": 40000000,
                "sdkVersion": "mock-server v1.0",
                "uptime": int((time.time() - boot_time) * 1000),
                "wifiMode": "Access Point (simulated)",
                "ssid": "Pacemaker",
                "ip": f"127.0.0.1:{PORT}",
                "connectedClients": 1,
                "mac": "AA:BB:CC:DD:EE:FF",
                "filesystemTotal": 1441792,
                "filesystemUsed": 262144,
            })
            return

        if self.path == "/api/settings":
            with lock:
                self._send_json(dict(settings))
            return

        if self.path == "/api/manual-bpm":
            with lock:
                self._send_json({
                    "enabled": state["manualBPMEnabled"],
                    "bpm": state["manualBPMValue"],
                })
            return

        if self.path == "/api/logs":
            with lock:
                self._send_json({"logs": list(events)})
            return

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"404 Not Found")

    def do_POST(self):
        if self.path == "/api/settings":
            data = self._read_json() or {}
            with lock:
                for key in (
                    "volume", "targetBPM", "normalLow", "normalHigh",
                    "atriumRest", "atriumBeat", "ventricleRest", "ventricleBeat",
                ):
                    if key in data:
                        settings[key] = int(data[key])
                if settings["normalLow"] >= settings["normalHigh"]:
                    settings["normalHigh"] = settings["normalLow"] + 1
                if "theme" in data:
                    settings["theme"] = str(data["theme"])
                if "accent" in data:
                    settings["accent"] = str(data["accent"])
                add_event("Settings updated")
            self._send_json({"success": True})
            return

        if self.path == "/api/manual-bpm":
            data = self._read_json() or {}
            with lock:
                if "enabled" in data:
                    state["manualBPMEnabled"] = bool(data["enabled"])
                if "bpm" in data:
                    state["manualBPMValue"] = max(30, min(200, int(data["bpm"])))
                add_event(
                    "Manual BPM override enabled"
                    if state["manualBPMEnabled"]
                    else "Manual BPM override disabled"
                )
            self._send_json({"success": True})
            return

        if self.path == "/api/restart":
            with lock:
                add_event("Restart requested (simulated - state unchanged)")
            self._send_json({"success": True})
            return

        self.send_response(404)
        self.end_headers()
        self.wfile.write(b"404 Not Found")


if __name__ == "__main__":
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"Pacemaker mock server running -> http://localhost:{PORT}")
    if not HAVE_PIL:
        print("(tip: `pip install pillow` for a pixel-accurate OLED preview)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
