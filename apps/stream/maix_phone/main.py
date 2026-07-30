"""
maix_phone — 纯 MJPEG 推流（无网页 UI / 无 YOLO / 无 Flask）

拉流地址（给自有 App 用）：
  http://<MaixIP>:8000/stream
"""

import re
import socket
import threading
import time as pytime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

from maix import app, camera, image, time

HTTP_PORT = 8000
CAM_W, CAM_H = 320, 240
JPEG_QUALITY = 45
FRAME_INTERVAL_MS = 50

_lock = threading.Lock()
_jpeg = None
_stop = False


def log(msg):
    print(msg, flush=True)


def get_ip():
    try:
        import subprocess
        out = subprocess.check_output(["ifconfig", "wlan0"], stderr=subprocess.DEVNULL)
        text = out.decode(errors="ignore")
        m = re.search(r"inet addr:(\d+\.\d+\.\d+\.\d+)", text)
        if not m:
            m = re.search(r"inet (\d+\.\d+\.\d+\.\d+)", text)
        if m:
            ip = m.group(1)
            if ip not in ("0.0.0.0", "127.0.0.1"):
                return ip
    except Exception as e:
        log("ifconfig skip: " + str(e))
    try:
        from maix import network
        ip = network.wifi.Wifi().get_ip()
        if ip and ip not in ("0.0.0.0", "127.0.0.1", ""):
            return ip
    except Exception as e:
        log("wifi get_ip skip: " + str(e))
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("127."):
            return ip
    except Exception as e:
        log("socket ip skip: " + str(e))
    return "0.0.0.0"


def img_to_jpeg_bytes(img, quality):
    try:
        jpg = img.to_jpeg(quality=quality)
    except TypeError:
        try:
            jpg = img.to_jpeg()
        except Exception:
            jpg = img.to_format(image.Format.FMT_JPEG)
    if hasattr(jpg, "to_bytes"):
        try:
            return jpg.to_bytes()
        except TypeError:
            return jpg.to_bytes(True)
    return bytes(jpg)


def camera_loop(cam):
    global _jpeg
    log("cam loop start")
    fps_t = pytime.time()
    fps_n = 0
    while not _stop and not app.need_exit():
        t0 = pytime.time()
        try:
            img = cam.read()
            raw = img_to_jpeg_bytes(img, JPEG_QUALITY)
            with _lock:
                _jpeg = raw
            fps_n += 1
        except Exception as e:
            log("cam err: " + str(e))
            time.sleep_ms(50)
            continue
        now = pytime.time()
        if now - fps_t >= 1.0:
            log("stream fps: {:.1f}".format(fps_n / (now - fps_t)))
            fps_n = 0
            fps_t = now
        rest = FRAME_INTERVAL_MS - (pytime.time() - t0) * 1000.0
        if rest > 1:
            time.sleep_ms(int(rest))
    log("cam loop end")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return

    def do_GET(self):
        path = self.path.split("?")[0]
        # 只提供 MJPEG；/ 也指向同一流，方便 App 接入
        if path in ("/", "/stream"):
            self.send_response(200)
            self.send_header(
                "Content-Type", "multipart/x-mixed-replace; boundary=frame"
            )
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            try:
                while not _stop and not app.need_exit():
                    with _lock:
                        frame = _jpeg
                    if not frame:
                        pytime.sleep(0.05)
                        continue
                    try:
                        self.wfile.write(
                            b"--frame\r\n"
                            b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n"
                        )
                        self.wfile.flush()
                    except (BrokenPipeError, ConnectionResetError, OSError):
                        break
                    pytime.sleep(0.03)
            except Exception as e:
                log("client end: " + str(e))
            return
        self.send_error(404)


def main():
    global _stop
    log("=== stream only ===")
    ip = get_ip()
    log("ip=" + str(ip))

    cam = camera.Camera(CAM_W, CAM_H)
    log("camera %dx%d" % (CAM_W, CAM_H))
    img0 = cam.read()
    raw0 = img_to_jpeg_bytes(img0, JPEG_QUALITY)
    with _lock:
        _jpeg = raw0
    log("warmup %d bytes" % len(raw0))

    threading.Thread(target=camera_loop, args=(cam,), name="cam", daemon=True).start()

    url = "http://{}:{}/stream".format(ip, HTTP_PORT)
    log("MJPEG: " + url)

    server = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    server.timeout = 0.5
    try:
        while not app.need_exit():
            server.handle_request()
    except KeyboardInterrupt:
        pass
    finally:
        _stop = True
        try:
            server.server_close()
        except Exception:
            pass
        log("exit")


if __name__ == "__main__":
    main()
