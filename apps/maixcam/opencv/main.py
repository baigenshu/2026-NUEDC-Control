"""
MaixCAM：OpenCV 钢珠位置 → UART → balance 摆杆主控（闭环停球）
         + 本机屏 HUD + 可选手机 MJPEG 双显

对接：apps/balance/docs/vision_proto.md 、 ball_proto.h

帧 13B 小端：
  0x02 球位：AA 55 | 02 | flags | pos_mm i16 | cx i16 | cy i16 | conf | mode | csum
  0x12 定点：AA 55 | 12 | 00 | target_mm i16 | pad×6 | csum
  pos/target = 整 mm；csum = sum(body)&0xFF

WiFi：已连复用 / 未连扫码可 Skip；无网禁 MJPEG；Menu→WiFi 可重连
MJPEG：http://<IP>:8000/stream
接线：Maix A16 TX → balance PA31；Cam2 用 A21/UART4
"""

from maix import camera, display, image, app, time, touchscreen, sys, err
import re
import socket
import struct
import threading
import time as pytime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

print("[ball] boot...")

try:
    import cv2
    import numpy as np
    print("[ball] cv2 ok", getattr(cv2, "__version__", ""))
except Exception as e:
    print("[ball] import cv2 FAIL:", e)
    raise

try:
    from maix import pinmap, uart
    HAS_UART_MOD = True
except Exception as e:
    print("[ball] uart module missing:", e)
    HAS_UART_MOD = False

# ---------- 协议 / 相机 ----------
ENABLE_UART = True
PROTO_MAGIC0, PROTO_MAGIC1 = 0xAA, 0x55
PROTO_TYPE_BALL, PROTO_TYPE_SETPOINT = 0x02, 0x12
PROTO_FLAG_FOUND = 0x01
CONF_MIN = 30                 # 与 MCU BALL_CONF_MIN 一致
# 球位发送：与检测同频（每主循环 1 包）。>0 时为最小间隔 ms（旧行为节流）
TX_MIN_MS = 0
BAUD = 115200
CAM_W, CAM_H = 320, 240

# ---------- WiFi / MJPEG ----------
WIFI_CONNECT_TIMEOUT = 60
ENABLE_MJPEG = True
HTTP_PORT = 8000
JPEG_QUALITY = 45
MJPEG_EVERY_N = 2
DISP_EVERY_N = 3          # LCD 再降频；检测/UART 仍每帧

_mjpeg_lock = threading.Lock()
_mjpeg_jpeg = None
_mjpeg_stop = False
_mjpeg_server = None
_mjpeg_clients = 0
_mjpeg_pending = None
_mjpeg_pending_lock = threading.Lock()
_mjpeg_wake = threading.Event()
_stream_ready = False
_wifi_ip = ""

# ---------- UI 色板 ----------
UI_BG = (36, 36, 48)
UI_BG_ACTIVE = (40, 90, 70)
UI_BG_DANGER = (70, 36, 36)
UI_BG_ACCENT = (36, 56, 88)
UI_BORDER = (200, 200, 210)
UI_BORDER_ACTIVE = (80, 220, 140)
UI_BORDER_DANGER = (220, 100, 100)
UI_TEXT_DIM = (160, 160, 170)
UI_ROI = (80, 180, 255)
UI_O = (255, 220, 80)
UI_SP = (255, 64, 180)
UI_SP_DIM = (160, 80, 140)
UI_FPS = (120, 200, 255)
UI_OK = (80, 220, 140)
_COL = {}
_LAYOUT_CACHE = {}
_TEXT_WH_CACHE = {}
_PROJ_KERNEL = None
_PROJ_WIN = 0

# 凹槽 ROI（已标定，勿改）
ROI_X, ROI_Y, ROI_W, ROI_H = 36, 128, 256, 15
ROI = [ROI_X, ROI_Y, ROI_W, ROI_H]
BAR_LEN_MM = 234.0
O_OFFSET_PX = ROI_W // 2
SP_MIN_MM = int(-BAR_LEN_MM * 0.5)
SP_MAX_MM = int(BAR_LEN_MM * 0.5)
# 板载补光（MaixCAM-Pro = B3 照明 LED）
ENABLE_FILL_LIGHT = True

ENABLE_PROJ_FALLBACK = True
PROJ_MIN_CONTRAST = 12.0     # 略高于旧值 10，少吃极浅影
PROJ_SCORE_SCALE = 2.0
PROJ_VALLEY_W_MAX = 36       # 仅挡明显拉长的影；真球谷宽通常更窄

DETECT_MODE = 1              # 0 亮 / 1 暗 / 2 双试
TH_BRIGHT = 200
TH_DARK = 115                # 恢复，保证小球能进 mask
TH_BAR_WHITE = 120
TH_REL_DARK = 12             # 略严于旧 10
MIN_AREA = 12
MAX_AREA = 500
MIN_CIRCULARITY = 0.28
MAX_ASPECT = 2.4
BALL_DIAM_PX = 12
# 阴影：硬拒只挡极端；一般靠评分降权（偶尔误检可接受）
BALL_GRAY_HARD = 145         # 比这还亮直接丢（几乎不是球）
BALL_CONTRAST_HARD = 8.0     # 对比极弱直接丢
BALL_GRAY_SOFT = 110         # 偏亮 → 降分
BALL_CONTRAST_SOFT = 16.0    # 对比偏弱 → 降分
POS_ALPHA = 0.35
Y_CENTER_WEIGHT = 20.0
POS_HYST_MM = 0.55
TRACK_VEL_ALPHA = 0.35
TRACK_MAX_JUMP_MM = 28.0
TRACK_HOLD_FRAMES = 1
# 跟踪锁定后只搜预测邻域（px）；丢球/跳变后回退全 ROI
SEARCH_HALF_W = 56
SEARCH_SCORE_OK = 55.0       # 局部命中且分数够高则不再全框重扫
SKIP_BLUR = True             # 细 ROI + 阈值，省 blur
EARLY_BLOB_SCORE = 72.0      # 轮廓分够高提前结束

MM_PER_PX = BAR_LEN_MM / float(ROI_W) if ROI_W > 1 else 1.0
PX_PER_MM = float(ROI_W) / BAR_LEN_MM if BAR_LEN_MM > 1e-6 else 1.0
_TARGET_AREA = 3.1416 * (BALL_DIAM_PX * 0.5) ** 2
_EMPTY_DET = {
    "found": False, "cx": 0, "cy": 0, "pos_mm": 0.0,
    "conf": 0, "bbox": None, "mode_used": 1, "via": "none",
}

_K_CLOSE = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
_K_OPEN = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2, 2))
_K_BAR_DIL = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 5))
_K_BAR = cv2.getStructuringElement(
    cv2.MORPH_RECT, (5, max(2, min(3, ROI_H // 4)))
)


# ===================== 小工具 =====================

def _rgb(rgb):
    c = _COL.get(rgb)
    if c is None:
        c = image.Color.from_rgb(rgb[0], rgb[1], rgb[2])
        _COL[rgb] = c
    return c


def _text_wh(text, scale=1.0):
    key = (text, scale)
    hit = _TEXT_WH_CACHE.get(key)
    if hit is not None:
        return hit
    tw = th = 0
    try:
        sz = image.string_size(text, scale=scale)
        if isinstance(sz, (tuple, list)) and len(sz) >= 2:
            tw, th = int(sz[0]), int(sz[1])
    except Exception:
        pass
    if tw <= 0 or th <= 0:
        s = float(scale) or 1.0
        tw = max(1, int(round(len(text) * 9.0 * s)))
        th = max(1, int(round(16.0 * s)))
    _TEXT_WH_CACHE[key] = (tw, th)
    return tw, th


def _ip_ok(ip):
    return bool(ip) and ip not in ("", "0.0.0.0", "127.0.0.1")


def _proj_kernel():
    global _PROJ_KERNEL, _PROJ_WIN
    win = max(3, int(BALL_DIAM_PX) | 1)
    if _PROJ_KERNEL is None or _PROJ_WIN != win:
        _PROJ_WIN = win
        _PROJ_KERNEL = np.ones(win, dtype=np.float32) / float(win)
    return _PROJ_KERNEL, win // 2


def _i16(v):
    v = int(round(v))
    return 32767 if v > 32767 else (-32768 if v < -32768 else v)


def _frame_with_csum(body):
    return bytes((PROTO_MAGIC0, PROTO_MAGIC1)) + body + bytes((sum(body) & 0xFF,))


def _wifi_unescape(s):
    out, i, n = [], 0, len(s)
    while i < n:
        if s[i] == "\\" and i + 1 < n:
            out.append(s[i + 1])
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


# ===================== WiFi =====================

def get_ip():
    try:
        import subprocess
        out = subprocess.check_output(["ifconfig", "wlan0"], stderr=subprocess.DEVNULL)
        text = out.decode(errors="ignore")
        m = re.search(r"inet addr:(\d+\.\d+\.\d+\.\d+)", text) or re.search(
            r"inet (\d+\.\d+\.\d+\.\d+)", text
        )
        if m and _ip_ok(m.group(1)):
            return m.group(1)
    except Exception:
        pass
    try:
        from maix import network
        ip = network.wifi.Wifi().get_ip() or ""
        if _ip_ok(ip):
            return ip
    except Exception:
        pass
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        if ip and not ip.startswith("127."):
            return ip
    except Exception:
        pass
    return "0.0.0.0"


def wifi_current_ip():
    """已连接且 IP 有效 → ip，否则 None。"""
    try:
        from maix import network
        ip = network.wifi.Wifi().get_ip() or ""
        if not _ip_ok(ip):
            ip = get_ip()
        if _ip_ok(ip):
            return ip
    except Exception as e:
        print("[ball] wifi_current_ip:", e)
    ip = get_ip()
    return ip if _ip_ok(ip) else None


def parse_wifi_qr(payload):
    """WIFI:T:..;S:..;P:..;; / ssid|pass / 两行 → (ssid, password) 或 None。"""
    if not payload:
        return None
    text = payload.strip()
    if not text:
        return None

    if text.upper().startswith("WIFI:"):
        body = text[5:]
        if body.endswith(";;"):
            body = body[:-1]
        fields = {}
        for part in re.split(r"(?<!\\);", body):
            part = part.strip()
            if not part or ":" not in part:
                continue
            k, v = part.split(":", 1)
            fields[k.strip().upper()] = _wifi_unescape(v)
        ssid = fields.get("S", "").strip()
        if not ssid:
            return None
        t = fields.get("T", "WPA").strip().upper()
        password = "" if t in ("NOPASS", "NONE", "") else fields.get("P", "")
        return ssid, password

    if "|" in text:
        ssid, password = text.split("|", 1)
        ssid = ssid.strip()
        return (ssid, password.strip()) if ssid else None

    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    if len(lines) >= 2:
        return lines[0], lines[1]
    if len(lines) == 1:
        return lines[0], ""
    return None


def wifi_connect(ssid, password):
    try:
        from maix import network, err as maix_err
    except Exception as e:
        print("[ball] wifi module missing:", e)
        return None
    try:
        w = network.wifi.Wifi()
        try:
            w.disconnect()
        except Exception:
            pass
        print("[ball] wifi connect ssid=%r ..." % ssid)
        ret = w.connect(ssid, password or "", wait=True, timeout=int(WIFI_CONNECT_TIMEOUT))
        try:
            ok = ret in (0, None) or ret == maix_err.Err.ERR_NONE
        except Exception:
            ok = ret in (0, None) or str(ret) in ("0", "ERR_NONE", "None")
        if not ok:
            try:
                ok = bool(w.is_connected())
            except Exception:
                ok = False
        try:
            ip = w.get_ip() or ""
        except Exception:
            ip = ""
        if not _ip_ok(ip):
            ip = get_ip()
        if _ip_ok(ip):
            print("[ball] wifi ok ip=%s" % ip)
            return ip
        print("[ball] wifi connect fail ret=%r ip=%r" % (ret, ip))
        return None
    except Exception as e:
        print("[ball] wifi connect err:", e)
        return None


def phase_scan_wifi(disp, cam, ts, allow_skip=True, title="Scan WiFi QR"):
    """成功→ip；Skip→\"\"；退出→None。"""
    print("[ball] phase: scan WiFi QR (skip=%s)" % allow_skip)
    status, status_col = "Aim at WiFi QR", image.COLOR_WHITE
    last_fail_ms = 0
    pressed_last = False
    dw, dh = disp.width(), disp.height()

    while not app.need_exit():
        try:
            img = cam.read()
            iw, ih = img.width(), img.height()
            skip_btn = [iw - 78, ih - 36, 72, 30] if allow_skip else None

            qrcodes = []
            try:
                qrcodes = img.find_qrcodes()
            except Exception as e:
                status, status_col = "QR err", image.COLOR_RED
                print("[ball] find_qrcodes:", e)

            for qr in qrcodes:
                try:
                    c = qr.corners()
                    for i in range(4):
                        img.draw_line(
                            c[i][0], c[i][1], c[(i + 1) % 4][0], c[(i + 1) % 4][1],
                            _rgb(UI_OK), 2,
                        )
                except Exception:
                    pass
                try:
                    payload = qr.payload()
                except Exception:
                    payload = ""
                parsed = parse_wifi_qr(payload)
                if parsed is None:
                    status, status_col = "Not WiFi QR", _rgb(UI_SP)
                    continue
                ssid, password = parsed
                img.draw_string(6, 4, title, image.COLOR_WHITE, scale=1.1)
                img.draw_string(6, 28, "Connecting... " + ssid[:16], _rgb(UI_FPS), scale=1.0)
                if skip_btn:
                    draw_btn(img, skip_btn, "Skip")
                disp.show(img)
                print("[ball] QR ssid=%r" % ssid)
                ip = wifi_connect(ssid, password)
                if ip:
                    img2 = cam.read()
                    img2.draw_string(6, 4, "WiFi OK", _rgb(UI_OK), scale=1.2)
                    img2.draw_string(6, 30, ip, image.COLOR_WHITE, scale=1.1)
                    img2.draw_string(6, 54, "ssid: " + ssid[:22], image.COLOR_WHITE, scale=1.0)
                    disp.show(img2)
                    time.sleep_ms(600)
                    return ip
                status, status_col = "Fail, rescan", image.COLOR_RED
                last_fail_ms = time.ticks_ms()
                break

            m = 28
            img.draw_rect(m, m, max(8, iw - 2 * m), max(8, ih - 2 * m - 40), _rgb(UI_ROI), 1)
            img.draw_string(6, 4, title, image.COLOR_WHITE, scale=1.1)
            img.draw_string(6, 28, status, status_col, scale=1.0)
            img.draw_string(6, 50, "WIFI:T:WPA;S:..;P:..;;", _rgb(UI_TEXT_DIM), scale=0.9)
            if skip_btn:
                draw_btn(img, skip_btn, "Skip")
            disp.show(img)

            tx, ty, pressed = ts.read()
            mx, my = map_touch_to_image(tx, ty, iw, ih, dw, dh)
            if pressed and not pressed_last and skip_btn and is_in_button(mx, my, skip_btn):
                print("[ball] WiFi skip")
                return ""
            pressed_last = pressed

            if last_fail_ms and (time.ticks_ms() - last_fail_ms) > 2500:
                if status.startswith("Fail"):
                    status, status_col = "Aim at WiFi QR", image.COLOR_WHITE
                last_fail_ms = 0
        except Exception as e:
            print("[ball] wifi scan loop:", e)
            time.sleep_ms(100)
    return None


def phase_ensure_wifi(disp, cam, ts, force_scan=False):
    """已连复用；否则扫码可 Skip。返回 ip / \"\" / None(退出)。"""
    global _wifi_ip
    if not force_scan:
        ip = wifi_current_ip()
        if ip:
            print("[ball] wifi reuse ip=%s" % ip)
            _wifi_ip = ip
            img = cam.read()
            img.draw_string(6, 4, "WiFi ready", _rgb(UI_OK), scale=1.2)
            img.draw_string(6, 30, ip, image.COLOR_WHITE, scale=1.1)
            img.draw_string(6, 54, "Using existing network", _rgb(UI_TEXT_DIM), scale=1.0)
            disp.show(img)
            time.sleep_ms(500)
            return ip
        print("[ball] wifi not connected, enter scan")
    else:
        print("[ball] wifi force rescan")

    ip = phase_scan_wifi(
        disp, cam, ts, allow_skip=True,
        title="Reconnect WiFi" if force_scan else "Scan WiFi QR",
    )
    if ip is None:
        return None
    _wifi_ip = ip or ""
    return ip


def apply_stream_for_ip(ip):
    """有 IP 启 MJPEG，否则关闭。"""
    global _wifi_ip
    _wifi_ip = ip or ""
    if ip:
        url = start_mjpeg_server()
        print("[ball] stream on" if _stream_ready else "stream start fail", url or "")
        return _stream_ready
    stop_mjpeg_server()
    print("[ball] no wifi → stream disabled")
    return False


# ===================== MJPEG =====================

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


def _img_copy(img):
    try:
        return img.copy()
    except Exception:
        pass
    try:
        return img.to_format(img.format())
    except Exception:
        return None


def submit_mjpeg(img):
    global _mjpeg_pending, _mjpeg_jpeg
    if not ENABLE_MJPEG or not _stream_ready or _mjpeg_clients <= 0:
        return
    with _mjpeg_pending_lock:
        if _mjpeg_pending is not None:
            return
        copied = _img_copy(img)
        if copied is None:
            try:
                with _mjpeg_lock:
                    _mjpeg_jpeg = img_to_jpeg_bytes(img, JPEG_QUALITY)
            except Exception as e:
                print("[ball] jpeg err:", e)
            return
        _mjpeg_pending = copied
    _mjpeg_wake.set()


def _jpeg_worker_loop():
    global _mjpeg_jpeg, _mjpeg_pending
    while not _mjpeg_stop:
        _mjpeg_wake.wait(timeout=0.2)
        _mjpeg_wake.clear()
        if _mjpeg_stop:
            break
        with _mjpeg_pending_lock:
            img = _mjpeg_pending
            _mjpeg_pending = None
        if img is None:
            continue
        try:
            raw = img_to_jpeg_bytes(img, JPEG_QUALITY)
            with _mjpeg_lock:
                _mjpeg_jpeg = raw
        except Exception as e:
            print("[ball] jpeg err:", e)


class _MjpegHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        return

    def do_GET(self):
        global _mjpeg_clients
        if self.path.split("?")[0] not in ("/", "/stream"):
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        with _mjpeg_lock:
            _mjpeg_clients += 1
        try:
            while not _mjpeg_stop and not app.need_exit():
                with _mjpeg_lock:
                    frame = _mjpeg_jpeg
                if not frame:
                    pytime.sleep(0.05)
                    continue
                try:
                    self.wfile.write(
                        b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + frame + b"\r\n"
                    )
                    self.wfile.flush()
                except (BrokenPipeError, ConnectionResetError, OSError):
                    break
                pytime.sleep(0.03)
        except Exception as e:
            print("[ball] mjpeg client end:", e)
        finally:
            with _mjpeg_lock:
                _mjpeg_clients = max(0, _mjpeg_clients - 1)


def start_mjpeg_server():
    global _mjpeg_server, _mjpeg_stop, _mjpeg_clients, _mjpeg_pending, _stream_ready
    if not ENABLE_MJPEG:
        _stream_ready = False
        return None
    stop_mjpeg_server()
    _mjpeg_stop = False
    _mjpeg_clients = 0
    with _mjpeg_pending_lock:
        _mjpeg_pending = None
    _mjpeg_wake.clear()
    try:
        server = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), _MjpegHandler)
        server.timeout = 0.5
        _mjpeg_server = server

        def _serve():
            while not _mjpeg_stop and not app.need_exit():
                try:
                    server.handle_request()
                except Exception:
                    break
            try:
                server.server_close()
            except Exception:
                pass

        threading.Thread(target=_jpeg_worker_loop, name="jpeg", daemon=True).start()
        threading.Thread(target=_serve, name="mjpeg", daemon=True).start()
        url = "http://{}:{}/stream".format(_wifi_ip or get_ip(), HTTP_PORT)
        _stream_ready = True
        print("[ball] MJPEG:", url)
        return url
    except Exception as e:
        print("[ball] MJPEG server fail:", e)
        _mjpeg_server = None
        _stream_ready = False
        return None


def stop_mjpeg_server():
    global _mjpeg_stop, _mjpeg_server, _mjpeg_pending, _mjpeg_clients, _stream_ready
    _stream_ready = False
    _mjpeg_stop = True
    _mjpeg_wake.set()
    with _mjpeg_pending_lock:
        _mjpeg_pending = None
    _mjpeg_clients = 0
    srv, _mjpeg_server = _mjpeg_server, None
    if srv is not None:
        try:
            srv.server_close()
        except Exception:
            pass


# ===================== 显示 / UART =====================

def setup_fill_light(on=True):
    """
    MaixCAM-Pro 板载照明 LED = B3（高电平亮）。
    返回 GPIO 对象（退出时关灯），失败返回 None。
    """
    if not on:
        return None
    try:
        from maix import gpio, pinmap
    except Exception as e:
        print("[ball] gpio/led module missing:", e)
        return None
    # Pro 补光优先 B3；失败再试 A14 状态灯
    for pin_name, gpio_name, kind in (
        ("B3", "GPIOB3", "fill"),
        ("A14", "GPIOA14", "status"),
    ):
        try:
            pinmap.set_pin_function(pin_name, gpio_name)
            led = gpio.GPIO(gpio_name, gpio.Mode.OUT)
            led.value(1)
            print("[ball] LED on %s (%s)" % (gpio_name, kind))
            return led
        except Exception as e:
            print("[ball] LED try %s fail:" % gpio_name, e)
    return None


def setup_display():
    disp = display.Display()
    try:
        image.load_font(
            "sourcehansans",
            "/maixapp/share/font/SourceHanSansCN-Regular.otf",
            size=18,
        )
        image.set_default_font("sourcehansans")
    except Exception as e:
        print("[ball] font:", e)
    print("[ball] display", disp.width(), "x", disp.height())
    return disp


def show_boot(disp, msg):
    img = image.Image(disp.width(), disp.height())
    img.draw_rect(0, 0, disp.width(), disp.height(), image.COLOR_BLACK, -1)
    img.draw_string(10, disp.height() // 2 - 10, msg, image.COLOR_WHITE, scale=1.3)
    disp.show(img)


def setup_uart_safe():
    if not ENABLE_UART or not HAS_UART_MOD:
        print("[ball] UART disabled")
        return None
    try:
        device_id = sys.device_id()
        print("[ball] device_id:", device_id, "uart:", uart.list_devices())
        if device_id == "maixcam2":
            pins, device = {"A21": "UART4_TX", "A22": "UART4_RX"}, "/dev/ttyS4"
        else:
            pins, device = {"A16": "UART0_TX", "A17": "UART0_RX"}, "/dev/ttyS0"
        for pin, func in pins.items():
            ret = pinmap.set_pin_function(pin, func)
            print("[ball] pinmap %s->%s %s" % (pin, func, ret))
        ser = uart.UART(device, BAUD)
        ser.write_str("BALL ready\r\n")
        print("[ball] UART open", device, BAUD)
        return ser
    except Exception as e:
        print("[ball] UART fail (continue without):", e)
        return None


def pack_ball_frame(found, pos_mm, cx, cy, conf, mode):
    conf_i = max(0, min(100, int(conf)))
    usable = bool(found) and conf_i >= CONF_MIN
    body = struct.pack(
        "<BBhhhBB",
        PROTO_TYPE_BALL,
        PROTO_FLAG_FOUND if usable else 0x00,
        _i16(int(pos_mm) if usable else 0),
        _i16(cx) if usable else 0,
        _i16(cy) if usable else 0,
        conf_i if usable else 0,
        max(0, min(255, int(mode))),
    )
    return _frame_with_csum(body)


def pack_setpoint_frame(target_mm):
    body = struct.pack("<BBh", PROTO_TYPE_SETPOINT, 0x00, _i16(int(round(target_mm)))) + bytes(6)
    return _frame_with_csum(body)


def uart_write(ser, data):
    if ser is None or not data:
        return False
    try:
        ser.write(data)
        return True
    except Exception as e:
        print("[ball] UART write fail:", e)
        return False


def uart_send_sp(ser, sp_mm):
    return uart_write(ser, pack_setpoint_frame(sp_mm))


# ===================== 检测 / 跟踪 =====================

def clamp_roi(x, y, w, h, iw, ih):
    x = max(0, min(int(x), max(0, iw - 1)))
    y = max(0, min(int(y), max(0, ih - 1)))
    w = max(8, min(int(w), iw - x))
    h = max(8, min(int(h), ih - y))
    return x, y, w, h


def px_to_mm(cx_roi):
    return (float(cx_roi) - float(O_OFFSET_PX)) * MM_PER_PX


def mm_to_roi_x(pos_mm, roi=None):
    rx = roi[0] if roi else ROI_X
    return int(round(rx + O_OFFSET_PX + float(pos_mm) * PX_PER_MM))


def roi_x_to_mm(x, roi=None):
    rx = roi[0] if roi else ROI_X
    mm = (float(x) - rx - O_OFFSET_PX) * MM_PER_PX
    return int(round(max(SP_MIN_MM, min(SP_MAX_MM, mm))))


def _core_darkness(gray, cx, cy):
    """球心灰度 + 相对邻域杆面对比。返回 (core_gray, contrast) 或 (None, None)。"""
    h, w = gray.shape[:2]
    yi, xi = int(round(cy)), int(round(cx))
    if yi < 0 or xi < 0 or yi >= h or xi >= w:
        return None, None
    core = gray[max(0, yi - 2):min(h, yi + 3), max(0, xi - 2):min(w, xi + 3)]
    if core.size < 4:
        return None, None
    core_g = float(core.mean())
    rad = max(8, BALL_DIAM_PX)
    y0, y1 = max(0, yi - 3), min(h, yi + 4)
    x0, x1 = max(0, xi - rad * 2), min(w, xi + rad * 2 + 1)
    neigh = gray[y0:y1, x0:x1].astype(np.float32).copy()
    ly, lx = yi - y0, xi - x0
    neigh[max(0, ly - 3):ly + 4, max(0, lx - 3):lx + 4] = np.nan
    flat = neigh[np.isfinite(neigh)]
    if flat.size < 8:
        ref = float(np.median(gray[y0:y1, x0:x1]))
    else:
        ref = float(np.percentile(flat, 70))
    return core_g, ref - core_g


def _dark_score(gray, cx, cy):
    """
    黑暗/对比评分。硬拒仅极端；偏亮/弱对比降权，便于真球仍可检出、
    与阴影并存时优先真球。
    返回 (ok, score_bonus)；ok=False 才丢弃。
    """
    core_g, contrast = _core_darkness(gray, cx, cy)
    if core_g is None:
        return True, 0.0
    if core_g > BALL_GRAY_HARD or contrast < BALL_CONTRAST_HARD:
        return False, 0.0
    bonus = max(0.0, (130.0 - core_g) * 0.2) + min(18.0, max(0.0, contrast) * 0.35)
    if core_g > BALL_GRAY_SOFT:
        bonus -= (core_g - BALL_GRAY_SOFT) * 0.5
    if contrast < BALL_CONTRAST_SOFT:
        bonus -= (BALL_CONTRAST_SOFT - contrast) * 0.6
    return True, bonus


def _best_blob(mask, gray=None):
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, _K_CLOSE, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, _K_OPEN, iterations=1)
    cnts = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contours = cnts[0] if len(cnts) == 2 else cnts[1]
    best = None
    y_mid = mask.shape[0] * 0.5
    inv_ta = 1.0 / max(_TARGET_AREA, 1.0)

    for c in contours:
        area = cv2.contourArea(c)
        if area < MIN_AREA or area > MAX_AREA:
            continue
        x, y, bw, bh = cv2.boundingRect(c)
        if bw < 1 or bh < 1:
            continue
        aspect = (bw if bw >= bh else bh) / float(bh if bw >= bh else bw)
        if aspect > MAX_ASPECT:
            continue
        peri = cv2.arcLength(c, True)
        if peri < 1e-3:
            continue
        circ = 12.5663706 * area / (peri * peri)
        if circ < MIN_CIRCULARITY:
            continue
        m = cv2.moments(c)
        m00 = m["m00"]
        if m00 < 1e-3:
            continue
        cx, cy = m["m10"] / m00, m["m01"] / m00
        dark_bonus = 0.0
        if gray is not None:
            ok, dark_bonus = _dark_score(gray, cx, cy)
            if not ok:
                continue
        size_score = 30.0 * max(0.0, 1.0 - abs(area - _TARGET_AREA) * inv_ta)
        y_score = Y_CENTER_WEIGHT * max(0.0, 1.0 - abs(cy - y_mid) / max(y_mid, 1.0))
        score = circ * 50.0 + size_score + y_score + dark_bonus
        if best is None or score > best[0]:
            best = (score, cx, cy, area, (x, y, bw, bh))
            if score >= EARLY_BLOB_SCORE:
                break
    return best


def _proj_valley_width(smooth, idx, lo, hi, floor, contrast):
    """半深谷宽度。"""
    if contrast < 1.0:
        return 999
    thr = floor + 0.45 * contrast
    left = idx
    while left > lo and smooth[left] <= thr:
        left -= 1
    right = idx
    while right < hi - 1 and smooth[right] <= thr:
        right += 1
    return right - left


def _proj_dark_on_bar(gray, bar_mask):
    h, w = gray.shape[:2]
    if w < 4 or h < 2:
        return None
    col_sum = (bar_mask > 0).sum(axis=0)
    min_col = max(2, h // 4)
    col_ok = col_sum >= min_col
    if not col_ok.any():
        return None
    proj = gray.mean(axis=0)
    proj = np.where(col_ok, proj, 255.0)
    kernel, half = _proj_kernel()
    smooth = np.convolve(proj, kernel, mode="same")
    lo, hi = half, w - half
    if hi <= lo + 2:
        lo, hi = 0, w
    if hi <= lo:
        return None
    work = smooth[lo:hi].copy()
    work[~col_ok[lo:hi]] = 1e6
    amin = float(work.min())
    if amin >= 1e5:
        return None
    idx = int(work.argmin()) + lo
    base = float(np.median(smooth[col_ok]))
    contrast = base - float(smooth[idx])
    if contrast < PROJ_MIN_CONTRAST:
        return None
    vw = _proj_valley_width(smooth, idx, lo, hi, float(smooth[idx]), contrast)
    if vw > PROJ_VALLEY_W_MAX:
        return None
    ys = np.flatnonzero(bar_mask[:, idx])
    cy = float(ys.mean()) if ys.size else h * 0.5
    ok, bonus = _dark_score(gray, float(idx), cy)
    if not ok:
        return None
    score = min(95.0, 25.0 + contrast * PROJ_SCORE_SCALE + max(0.0, bonus) * 0.3)
    return (score, float(idx), cy)


def _hit_result(score, cx_r, cy_r, rx, ry, x_off, bbox, mode_used, via):
    """cx_r 相对 gray 子窗；x_off 为子窗相对全 ROI 的列偏移。"""
    cx_full = cx_r + x_off
    return {
        "found": True,
        "cx": int(round(cx_full + rx)),
        "cy": int(round(cy_r + ry)),
        "pos_mm": px_to_mm(cx_full),
        "conf": int(max(0, min(100, score))),
        "bbox": bbox,
        "mode_used": mode_used,
        "via": via,
    }


def _empty(mode=DETECT_MODE):
    d = _EMPTY_DET.copy()
    d["mode_used"] = mode
    return d


def _detect_in_gray(gray, rx, ry, x_off, mode, allow_median=True):
    """在已裁剪灰度窗内检球。坐标相对 gray，返回时加上 x_off→全 ROI。"""
    if gray is None or gray.size == 0:
        return None
    if not SKIP_BLUR:
        gray = cv2.blur(gray, (3, 3))

    _, bar_mask = cv2.threshold(gray, TH_BAR_WHITE, 255, cv2.THRESH_BINARY)
    bar_mask = cv2.morphologyEx(bar_mask, cv2.MORPH_CLOSE, _K_BAR, iterations=1)

    modes = (0, 1) if mode == 2 else (mode,)
    best_all = None
    bar_dil = None
    for mu in modes:
        if mu == 0:
            _, msk = cv2.threshold(gray, TH_BRIGHT, 255, cv2.THRESH_BINARY)
            msk = cv2.bitwise_and(msk, bar_mask)
            blob = _best_blob(msk, gray)
        else:
            if bar_dil is None:
                bar_dil = cv2.dilate(bar_mask, _K_BAR_DIL)
            _, dark = cv2.threshold(gray, TH_DARK, 255, cv2.THRESH_BINARY_INV)
            msk = cv2.bitwise_and(dark, bar_dil)
            blob = _best_blob(msk, gray)
            if blob is None and allow_median:
                med = cv2.medianBlur(gray, 5)
                _, rel = cv2.threshold(
                    cv2.subtract(med, gray), TH_REL_DARK, 255, cv2.THRESH_BINARY
                )
                blob = _best_blob(
                    cv2.bitwise_and(cv2.bitwise_or(dark, rel), bar_dil), gray
                )
        if blob is not None and (best_all is None or blob[0] > best_all[0][0]):
            best_all = (blob, mu)
            if blob[0] >= EARLY_BLOB_SCORE:
                break

    if best_all is not None:
        blob, mode_used = best_all
        score, cx_r, cy_r, _a, (bx, by, bw, bh) = blob
        return _hit_result(
            score, cx_r, cy_r, rx, ry, x_off,
            (bx + rx + x_off, by + ry, bw, bh), mode_used, "blob",
        )

    if not ENABLE_PROJ_FALLBACK:
        return None
    pk = _proj_dark_on_bar(gray, bar_mask)
    if pk is None:
        return None
    score, cx_r, cy_r = pk
    r = max(4, BALL_DIAM_PX // 2)
    cx = int(round(cx_r + rx + x_off))
    cy = int(round(cy_r + ry))
    return _hit_result(
        score, cx_r, cy_r, rx, ry, x_off,
        (cx - r, cy - r, r * 2, r * 2), 1, "proj",
    )


def detect_ball(bgr, roi, mode, track=None):
    """
    白杆 + 暗球。有跟踪时先局部窗；不够自信再全 ROI。
    """
    ih, iw = bgr.shape[0], bgr.shape[1]
    rx, ry, rw, rh = clamp_roi(roi[0], roi[1], roi[2], roi[3], iw, ih)
    full = bgr[ry:ry + rh, rx:rx + rw]
    if full is None or full.size == 0:
        return _empty(mode)

    if len(full.shape) == 3:
        gray_full = cv2.cvtColor(full, cv2.COLOR_BGR2GRAY)
    else:
        gray_full = full

    # ---- 局部搜索 ----
    if track and track.get("has"):
        cx_pred = O_OFFSET_PX + float(track["pos"]) * PX_PER_MM
        x0 = int(cx_pred - SEARCH_HALF_W)
        x1 = int(cx_pred + SEARCH_HALF_W)
        if x0 < 0:
            x0 = 0
        if x1 > rw:
            x1 = rw
        if x1 - x0 >= 16:
            local = gray_full[:, x0:x1]
            hit = _detect_in_gray(local, rx, ry, x0, mode, allow_median=False)
            if hit is not None and hit["conf"] >= SEARCH_SCORE_OK:
                return hit

    hit = _detect_in_gray(gray_full, rx, ry, 0, mode, allow_median=True)
    return hit if hit is not None else _empty(mode)


def quantize_mm(pos_f, last_i, has_last):
    if not has_last:
        return int(round(pos_f))
    if abs(pos_f - float(last_i)) < POS_HYST_MM:
        return int(last_i)
    return int(round(pos_f))


def _track_hold(det, track, now_ms, predicted):
    track["pos"] = predicted
    track["last_ms"] = now_ms
    det["found"] = True
    det["conf"] = CONF_MIN
    det["pos_mm"] = predicted
    det["cx"] = mm_to_roi_x(predicted)
    det["cy"] = ROI_Y + ROI_H // 2
    return True


def update_track(det, now_ms, track):
    """一维匀速预测，抑制 ROI 跳变；短时丢球 hold。"""
    usable = bool(det["found"]) and int(det["conf"]) >= CONF_MIN
    if not usable:
        track["lost"] += 1
        if track["has"] and track["lost"] <= TRACK_HOLD_FRAMES:
            dt = max(0.03, min(0.20, (now_ms - track["last_ms"]) / 1000.0))
            return _track_hold(det, track, now_ms, track["pos"] + track["vel"] * dt)
        track["has"] = False
        track["vel"] = 0.0
        return False

    raw_pos = float(det["pos_mm"])
    if not track["has"]:
        track.update(pos=raw_pos, vel=0.0, last_ms=now_ms, lost=0, has=True)
        return True

    dt = max(0.03, min(0.20, (now_ms - track["last_ms"]) / 1000.0))
    predicted = track["pos"] + track["vel"] * dt
    innovation = raw_pos - predicted
    if abs(innovation) > TRACK_MAX_JUMP_MM + abs(track["vel"]) * dt:
        track["lost"] += 1
        if track["lost"] <= TRACK_HOLD_FRAMES:
            return _track_hold(det, track, now_ms, predicted)
        track["has"] = False
        track["vel"] = 0.0
        return False

    updated = predicted + POS_ALPHA * innovation
    track["vel"] += TRACK_VEL_ALPHA * ((updated - track["pos"]) / dt - track["vel"])
    track["pos"] = updated
    track["last_ms"] = now_ms
    track["lost"] = 0
    det["pos_mm"] = updated
    return True


# ===================== UI =====================

def is_in_button(x, y, btn):
    if not btn:
        return False
    return btn[0] <= x <= btn[0] + btn[2] and btn[1] <= y <= btn[1] + btn[3]


def map_touch_to_image(tx, ty, img_w, img_h, disp_w, disp_h):
    if disp_w > 0 and disp_h > 0 and (disp_w != img_w or disp_h != img_h):
        return int(tx * img_w / disp_w), int(ty * img_h / disp_h)
    return int(tx), int(ty)


def layout_controls(img_w, img_h, menu_open=False):
    key = (img_w, img_h, bool(menu_open))
    cached = _LAYOUT_CACHE.get(key)
    if cached is not None:
        return cached
    by, bh, gap = img_h - 36, 30, 6
    menu_btn = [6, by, 64, bh]
    set_btn = [menu_btn[0] + menu_btn[2] + gap, by, 72, bh]
    reset_btn = [set_btn[0] + set_btn[2] + gap, by, 72, bh]
    wifi_btn = exit_btn = None
    if menu_open:
        exit_btn = [6, by - (bh + gap), 72, bh]
        wifi_btn = [6, by - 2 * (bh + gap), 72, bh]
    out = {
        "menu": menu_btn, "set": set_btn, "reset": reset_btn,
        "wifi": wifi_btn, "exit": exit_btn, "bar_y": by,
    }
    _LAYOUT_CACHE[key] = out
    return out


def layout_drag_strip(roi, bar_y):
    rx, ry, rw, rh = roi
    strip_y = min(bar_y - 34, ry + rh + 8)
    if strip_y < ry + rh + 2:
        strip_y = ry + rh + 2
    strip_h = 26
    if strip_y + strip_h > bar_y - 4:
        strip_h = max(18, bar_y - 4 - strip_y)
    return [rx, strip_y, rw, strip_h]


def hit_drag_zone(x, y, roi, drag_strip):
    rx, ry, rw, rh = roi
    if (rx - 4) <= x <= (rx + rw + 4) and (ry - 10) <= y <= (ry + rh + 10):
        return True
    return is_in_button(x, y, drag_strip)


def draw_btn(img, btn, label, active=False, danger=False, accent=False):
    if not btn:
        return
    bx, by, bw, bh = btn
    if danger:
        bg, border = _rgb(UI_BG_DANGER), _rgb(UI_BORDER_DANGER)
    elif active:
        bg, border = _rgb(UI_BG_ACTIVE), _rgb(UI_BORDER_ACTIVE)
    elif accent:
        bg, border = _rgb(UI_BG_ACCENT), _rgb(UI_ROI)
    else:
        bg, border = _rgb(UI_BG), _rgb(UI_BORDER)
    img.draw_rect(bx, by, bw, bh, bg, -1)
    img.draw_rect(bx, by, bw, bh, border, 1)
    tw, th = _text_wh(label, 1.0)
    img.draw_string(
        bx + max(2, (bw - tw) // 2),
        by + max(1, (bh - th) // 2),
        label, image.COLOR_WHITE, scale=1,
    )


def draw_ui(img, roi, det, pos_mm_i, sp_mm, usable,
            btns, drag_strip, set_mode, dragging, menu_open, fps,
            stream_on, wifi_ip, clients=0):
    rx, ry, rw, rh = roi
    img.draw_rect(rx, ry, rw, rh, _rgb(UI_ROI), 1)
    ox = rx + O_OFFSET_PX
    img.draw_line(ox, ry - 6, ox, ry + rh + 6, _rgb(UI_O), 1)

    sx = mm_to_roi_x(sp_mm, roi)
    if set_mode:
        col_sp = _rgb(UI_SP)
        img.draw_line(sx, ry - 10, sx, ry + rh + 10, col_sp, 2)
        img.draw_circle(sx, ry + rh // 2, 5, col_sp, -1)
        dsx, dsy, dsw, dsh = drag_strip
        img.draw_rect(dsx, dsy, dsw, dsh, _rgb(UI_BG), -1)
        img.draw_rect(dsx, dsy, dsw, dsh, col_sp if dragging else _rgb(UI_BORDER), 1)
        img.draw_rect(max(dsx, sx - 4), dsy, 8, dsh, col_sp, -1)
        img.draw_string(dsx + 4, dsy + 4, "Drag to set", _rgb(UI_TEXT_DIM), scale=0.9)
    else:
        img.draw_line(sx, ry, sx, ry + rh, _rgb(UI_SP_DIM), 1)

    if usable and det.get("found"):
        img.draw_circle(int(det["cx"]), int(det["cy"]), 3, _rgb(UI_OK), -1)

    tag = " SET" if set_mode else ""
    ball_s = "{:+d}".format(pos_mm_i) if usable else "----"
    img.draw_string(
        6, 4,
        "Ball {} mm  SP {:+d} mm{}".format(ball_s, sp_mm, tag),
        image.COLOR_WHITE, scale=1.05,
    )

    if not wifi_ip:
        net_txt, net_col = "NoNet", _rgb(UI_TEXT_DIM)
    elif stream_on and clients > 0:
        net_txt, net_col = "Live", _rgb(UI_OK)
    else:
        net_txt, net_col = "WiFi", _rgb(UI_FPS)
    nx = max(6, img.width() - 88)
    img.draw_string(nx, 4, "{:4.1f}F".format(fps), _rgb(UI_FPS), scale=1.05)
    img.draw_string(nx, 22, net_txt, net_col, scale=0.95)

    if menu_open:
        py = btns["wifi"][1] - 4
        img.draw_rect(4, py, 76, btns["bar_y"] - py, _rgb(UI_BG), -1)
        img.draw_rect(4, py, 76, btns["bar_y"] - py, _rgb(UI_BORDER), 1)
        draw_btn(img, btns["wifi"], "WiFi", accent=True)
        draw_btn(img, btns["exit"], "Exit", danger=True)

    draw_btn(img, btns["menu"], "Close" if menu_open else "Menu", active=menu_open)
    draw_btn(img, btns["set"], "Done" if set_mode else "Set", active=set_mode)
    draw_btn(img, btns["reset"], "Reset")


# ===================== main =====================

def main():
    global _wifi_ip

    disp = setup_display()
    show_boot(disp, "Ball control...")
    fill_led = setup_fill_light(ENABLE_FILL_LIGHT)
    serial_dev = setup_uart_safe()
    cam = camera.Camera(CAM_W, CAM_H, image.Format.FMT_BGR888)
    print("[ball] camera", cam.width(), "x", cam.height(), "bar", BAR_LEN_MM, "mm")
    ts = touchscreen.TouchScreen()
    dw, dh = disp.width(), disp.height()

    ip = phase_ensure_wifi(disp, cam, ts, force_scan=False)
    if app.need_exit() or ip is None:
        print("[ball] exit before main")
        if fill_led is not None:
            try:
                fill_led.value(0)
            except Exception:
                pass
        return
    apply_stream_for_ip(ip)

    mode = DETECT_MODE
    pos_mm_i = 0
    has_mm_i = False
    last_tx_ms = 0
    sp_mm = 0
    sp_pending = True
    set_mode = False
    dragging_sp = False
    menu_open = False
    pressed_last = False
    err_cnt = 0
    last_log_ms = 0
    track = {"has": False, "pos": 0.0, "vel": 0.0, "last_ms": 0, "lost": 0}
    frame_n = 0
    drag_strip = [0, 0, 0, 0]

    if uart_send_sp(serial_dev, sp_mm):
        sp_pending = False
        print("[ball] SP sync 0 mm")
    print("[ball] UI ready stream=%d" % int(_stream_ready))

    try:
        while not app.need_exit():
            try:
                img = cam.read()
                iw, ih = img.width(), img.height()
                bgr = image.image2cv(img, ensure_bgr=False, copy=False)

                det = detect_ball(bgr, ROI, mode, track)
                now_ms = time.ticks_ms()
                track_ok = update_track(det, now_ms, track)
                usable = track_ok and det["found"] and det["conf"] >= CONF_MIN

                if usable:
                    pos_mm_i = quantize_mm(det["pos_mm"], pos_mm_i, has_mm_i)
                    has_mm_i = True
                else:
                    has_mm_i = False
                    pos_mm_i = 0

                # UART：每检测帧发 0x02（与屏 FPS 同频）；TX_MIN_MS>0 时才节流
                if serial_dev is not None and (
                    TX_MIN_MS <= 0 or (now_ms - last_tx_ms) >= TX_MIN_MS
                ):
                    if sp_pending and uart_send_sp(serial_dev, sp_mm):
                        sp_pending = False
                    uart_write(
                        serial_dev,
                        pack_ball_frame(
                            usable, pos_mm_i,
                            det["cx"] if usable else 0,
                            det["cy"] if usable else 0,
                            det["conf"] if det["found"] else 0,
                            mode,
                        ),
                    )
                    last_tx_ms = now_ms

                tx, ty, pressed = ts.read()
                need_touch = pressed or pressed_last or dragging_sp
                if need_touch:
                    mx, my = map_touch_to_image(tx, ty, iw, ih, dw, dh)
                    if pressed and not pressed_last:
                        btns = layout_controls(iw, ih, menu_open)
                        if set_mode:
                            drag_strip = layout_drag_strip(ROI, btns["bar_y"])
                        if is_in_button(mx, my, btns["menu"]):
                            menu_open = not menu_open
                            dragging_sp = False
                        elif menu_open and is_in_button(mx, my, btns["exit"]):
                            app.set_exit_flag(True)
                        elif menu_open and is_in_button(mx, my, btns["wifi"]):
                            menu_open = False
                            prev_ip = _wifi_ip
                            new_ip = phase_ensure_wifi(disp, cam, ts, force_scan=True)
                            if app.need_exit() or new_ip is None:
                                break
                            if new_ip:
                                apply_stream_for_ip(new_ip)
                            else:
                                keep = wifi_current_ip() or prev_ip
                                if keep and not _stream_ready:
                                    apply_stream_for_ip(keep)
                                elif not keep:
                                    apply_stream_for_ip("")
                            pressed_last = False
                            continue
                        elif is_in_button(mx, my, btns["set"]):
                            set_mode = not set_mode
                            menu_open = False
                            dragging_sp = False
                        elif is_in_button(mx, my, btns["reset"]):
                            sp_mm, sp_pending = 0, True
                            set_mode = menu_open = dragging_sp = False
                            if uart_send_sp(serial_dev, 0):
                                sp_pending = False
                        elif menu_open:
                            menu_open = False
                        elif set_mode and hit_drag_zone(mx, my, ROI, drag_strip):
                            dragging_sp = True
                            sp_mm = roi_x_to_mm(mx, ROI)
                            sp_pending = not uart_send_sp(serial_dev, sp_mm)
                    elif pressed and set_mode and dragging_sp and not menu_open:
                        new_sp = roi_x_to_mm(mx, ROI)
                        if new_sp != sp_mm:
                            sp_mm = new_sp
                            sp_pending = not uart_send_sp(serial_dev, sp_mm)
                    else:
                        if dragging_sp:
                            sp_pending = True
                        dragging_sp = False
                pressed_last = pressed

                do_disp = (DISP_EVERY_N <= 1) or (frame_n % DISP_EVERY_N == 0)
                do_stream = (
                    _stream_ready and _mjpeg_clients > 0
                    and ((MJPEG_EVERY_N <= 1) or (frame_n % MJPEG_EVERY_N == 0))
                )
                frame_n += 1
                fps = time.fps()
                if do_disp or do_stream:
                    btns = layout_controls(iw, ih, menu_open)
                    if set_mode:
                        drag_strip = layout_drag_strip(ROI, btns["bar_y"])
                    draw_ui(
                        img, ROI, det, pos_mm_i, sp_mm, usable,
                        btns, drag_strip, set_mode, dragging_sp, menu_open,
                        fps, _stream_ready, _wifi_ip, _mjpeg_clients,
                    )
                if do_stream:
                    submit_mjpeg(img)
                if do_disp:
                    disp.show(img)

                if now_ms - last_log_ms >= 1000:
                    last_log_ms = now_ms
                    print(
                        "[ball] pos=%+d sp=%+d fps=%.1f stream=%d cli=%d ip=%s"
                        % (pos_mm_i, sp_mm, fps, int(_stream_ready),
                           _mjpeg_clients, _wifi_ip or "-")
                    )
            except Exception as e:
                err_cnt += 1
                print("[ball] loop err:", e)
                time.sleep_ms(200)
                if err_cnt > 30:
                    break
    finally:
        stop_mjpeg_server()
        if fill_led is not None:
            try:
                fill_led.value(0)
            except Exception:
                pass
        uart_write(serial_dev, pack_ball_frame(False, 0, 0, 0, 0, mode))
        print("[ball] exit")


if __name__ == "__main__":
    main()
