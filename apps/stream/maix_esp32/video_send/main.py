"""
video_send — MaixCAM-Pro 钢珠检测直播

固定连接 ESP32 SoftAP：MaixCam-ESP
  - Phone Web：YOLOv8 钢珠检测叠框 → 网页直播；录像仅在手机
  - TFT 入口保留但弃用（ESP 屏方案已停）

模型：steel_ball.mud（+ cvimodel），见 detect_util.find_model()
"""

from maix import camera, display, image, app, time, http, network, err, touchscreen

AP_SSID = "MaixCam-ESP"
AP_PASS = "grx060313"
WIFI_TIMEOUT_S = 60

# Web：ESP 不拉流、Maix 不写盘 → 提高画质/流畅
# TFT：保留代码路径，屏方案已弃用
MODES = [
    {
        "label": "Phone Web",
        "mode": "web",
        "cam_w": 320,
        "cam_h": 240,
        "jpeg_quality": 50,
        "frame_interval_ms": 45,
        "preview_every": 12,
    },
    {
        "label": "TFT (off)",
        "mode": "tft",
        "cam_w": 160,
        "cam_h": 120,
        "jpeg_quality": 35,
        "frame_interval_ms": 50,
        "preview_every": 8,
    },
]

BTN_H = 48
BTN_GAP = 10
MENU_BTN_H = 56

TFT_HTML = """<!DOCTYPE html>
<html lang="zh-CN" data-mode="tft">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>TFT (deprecated)</title>
</head>
<body style="background:#111;color:#ccc;font-family:sans-serif;text-align:center;margin:12px">
<h3>TFT mode (deprecated)</h3>
<p>ESP screen path is disabled. Use Phone Web.</p>
<img id="live" src="/stream" style="max-width:100%;background:#000"/>
</body>
</html>
"""


def is_in_btn(x, y, btn):
    return btn[0] <= x < btn[0] + btn[2] and btn[1] <= y < btn[1] + btn[3]


def draw_btn(img, btn, label, fill_rgb=(28, 32, 48)):
    fill = image.Color.from_rgb(*fill_rgb)
    border = image.COLOR_WHITE
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=fill, thickness=-1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=border, thickness=2)
    try:
        size = image.string_size(label, scale=1.2)
        tx = btn[0] + max(0, (btn[2] - size.width()) // 2)
        ty = btn[1] + max(0, (btn[3] - size.height()) // 2)
    except Exception:
        tx, ty = btn[0] + 8, btn[1] + 12
    img.draw_string(tx, ty, label, color=image.COLOR_WHITE, scale=1.2)


def draw_exit_btn(img, btn):
    draw_btn(img, btn, "Exit", fill_rgb=(120, 40, 40))


def exit_btn_on_image(img_w, img_h):
    bw = max(56, img_w // 3)
    bh = min(BTN_H, max(28, img_h // 4))
    return [img_w - bw - BTN_GAP, img_h - bh - BTN_GAP, bw, bh]


def map_touch(tx, ty, disp_w, disp_h, img_w, img_h):
    if disp_w > 0 and disp_h > 0 and (disp_w != img_w or disp_h != img_h):
        return int(tx * img_w / disp_w), int(ty * img_h / disp_h)
    return int(tx), int(ty)


def encode_jpeg(img, quality):
    try:
        return img.to_jpeg(quality=quality)
    except TypeError:
        try:
            return img.to_format(image.Format.FMT_JPEG, quality=quality)
        except Exception:
            return img.to_jpeg()


def show_exiting(disp):
    try:
        dw, dh = disp.width(), disp.height()
        img = image.Image(dw, dh)
        img.draw_rect(0, 0, dw, dh, color=image.Color.from_rgb(10, 14, 20), thickness=-1)
        img.draw_string(12, dh // 2 - 12, "Exiting...", color=image.COLOR_WHITE, scale=1.4)
        disp.show(img)
        time.sleep_ms(80)
    except Exception:
        pass


def light_cleanup(reason="exit"):
    print("cleanup:", reason)
    try:
        app.set_exit_flag(True)
    except Exception as e:
        print("set_exit_flag skip:", e)


def show_message(disp, title, subtitle=""):
    dw, dh = disp.width(), disp.height()
    img = image.Image(dw, dh)
    img.draw_rect(0, 0, dw, dh, color=image.Color.from_rgb(10, 14, 20), thickness=-1)
    img.draw_string(12, dh // 2 - 24, title, color=image.COLOR_WHITE, scale=1.3)
    if subtitle:
        img.draw_string(12, dh // 2 + 12, subtitle[:40], color=image.Color.from_rgb(100, 200, 255), scale=1.0)
    disp.show(img)


def connect_wifi(ssid, password):
    w = network.wifi.Wifi()
    print("connect to", ssid)
    e = w.connect(ssid, password, wait=True, timeout=WIFI_TIMEOUT_S)
    err.check_raise(e, "connect wifi failed: " + ssid)
    ip = w.get_ip()
    print("connected, ip:", ip)
    return w, ip


def select_mode(disp, ts, ip):
    dw, dh = disp.width(), disp.height()
    n = len(MODES)
    btn_w = dw - BTN_GAP * 2
    start_y = 72
    mode_btns = []
    for i in range(n):
        y = start_y + i * (MENU_BTN_H + BTN_GAP)
        mode_btns.append([BTN_GAP, y, btn_w, MENU_BTN_H])
    exit_y = start_y + n * (MENU_BTN_H + BTN_GAP) + BTN_GAP
    exit_btn = [BTN_GAP, exit_y, btn_w, MENU_BTN_H]

    pressed_last = False
    while not app.need_exit():
        canvas = image.Image(dw, dh)
        canvas.draw_rect(0, 0, dw, dh, color=image.Color.from_rgb(10, 14, 20), thickness=-1)
        canvas.draw_string(BTN_GAP, 10, "Select mode", color=image.COLOR_WHITE, scale=1.3)
        canvas.draw_string(
            BTN_GAP, 36, "WiFi OK  {}".format(ip), color=image.Color.from_rgb(140, 180, 220), scale=0.85
        )
        canvas.draw_string(
            BTN_GAP, 54, "rec on phone only", color=image.Color.from_rgb(100, 140, 160), scale=0.75
        )

        for i, m in enumerate(MODES):
            if m["mode"] == "web":
                color = (30, 90, 140)
                extra = "  (live+phone rec)"
            else:
                color = (60, 60, 60)
                extra = "  (deprecated)"
            draw_btn(canvas, mode_btns[i], m["label"] + extra, fill_rgb=color)

        draw_btn(canvas, exit_btn, "Exit", fill_rgb=(100, 40, 40))
        disp.show(canvas)

        try:
            tx, ty, pressed = ts.read()
        except Exception:
            tx, ty, pressed = 0, 0, False

        if pressed and not pressed_last:
            mx, my = map_touch(tx, ty, dw, dh, dw, dh)
            if is_in_btn(mx, my, exit_btn):
                return None
            for i, btn in enumerate(mode_btns):
                if is_in_btn(mx, my, btn):
                    return MODES[i]
        pressed_last = pressed
        time.sleep_ms(30)

    return None


def run_tft_stream(disp, ts, cfg, ip):
    """Deprecated path — ESP screen disabled; kept for optional re-enable."""
    print("WARNING: TFT mode selected but ESP screen stack is disabled")
    cam = camera.Camera(cfg["cam_w"], cfg["cam_h"])
    stream = http.JpegStreamer()
    try:
        stream.set_html(TFT_HTML)
    except Exception as e:
        print("set_html skip:", e)
    stream.start()

    pub_host = ip if ip else "127.0.0.1"
    port = stream.port()
    print("tft (deprecated) http://{}:{}".format(pub_host, port))

    fps_t = time.ticks_ms()
    fps_n = 0
    frame_i = 0
    pressed_last = False
    dw, dh = disp.width(), disp.height()
    quality = cfg["jpeg_quality"]
    interval = cfg["frame_interval_ms"]
    preview_every = cfg["preview_every"]

    while not app.need_exit():
        t0 = time.ticks_ms()
        try:
            img = cam.read()
            stream.write(encode_jpeg(img, quality))
        except Exception as e:
            if app.need_exit():
                break
            print("frame error:", e)
            time.sleep_ms(50)
            continue

        frame_i += 1
        exit_btn = exit_btn_on_image(img.width(), img.height())
        try:
            tx, ty, pressed = ts.read()
        except Exception:
            tx, ty, pressed = 0, 0, False
        if pressed and not pressed_last:
            mx, my = map_touch(tx, ty, dw, dh, img.width(), img.height())
            if is_in_btn(mx, my, exit_btn):
                app.set_exit_flag(True)
                break
        pressed_last = pressed

        fps_n += 1
        now = time.ticks_ms()
        if now - fps_t >= 1000:
            print("push fps: {:.1f}".format(fps_n * 1000.0 / (now - fps_t)))
            fps_n = 0
            fps_t = now

        if frame_i % preview_every == 0:
            try:
                show = img
                show.draw_string(2, 2, "TFT off use Web", color=image.COLOR_GREEN, scale=0.7)
                draw_exit_btn(show, exit_btn)
                disp.show(show)
            except Exception:
                pass

        rest = interval - (time.ticks_ms() - t0)
        if rest > 0:
            time.sleep_ms(rest)


def run_web_mode(disp, ts, cfg, ip):
    from web_server import run_web_server, HTTP_PORT
    from detect_util import load_detector
    import threading

    show_message(disp, "Loading model...", "steel ball YOLO")
    try:
        detector, use_zh = load_detector()
    except Exception as e:
        show_message(disp, "Model failed", str(e)[:40])
        print("load detector failed:", e)
        time.sleep_ms(2500)
        raise

    # Camera must match detector input (boxes align)
    cam_w = detector.input_width()
    cam_h = detector.input_height()
    cam_fmt = detector.input_format()
    print("camera for detect:", cam_w, cam_h, cam_fmt)
    cam = camera.Camera(cam_w, cam_h, cam_fmt)

    # Keep stream encode settings from cfg; resolution follows model
    stream_cfg = dict(cfg)
    stream_cfg["cam_w"] = cam_w
    stream_cfg["cam_h"] = cam_h

    page_url = "http://{}:{}".format(ip, HTTP_PORT)
    print("mode: web + steel ball detect")
    print("page:", page_url)

    ui_stop = {"v": False}

    def ui_loop():
        dw, dh = disp.width(), disp.height()
        pressed_last = False
        while not app.need_exit() and not ui_stop["v"]:
            try:
                canvas = image.Image(dw, dh)
                canvas.draw_rect(0, 0, dw, dh, color=image.Color.from_rgb(10, 14, 20), thickness=-1)
                canvas.draw_string(8, 12, "DETECT+LIVE", color=image.COLOR_GREEN, scale=1.2)
                canvas.draw_string(8, 40, ip, color=image.COLOR_WHITE, scale=1.0)
                canvas.draw_string(
                    8, 64, ":{}  {}x{}".format(HTTP_PORT, cam_w, cam_h),
                    color=image.Color.from_rgb(120, 200, 255), scale=0.9,
                )
                canvas.draw_string(
                    8, 90, "ball boxes on web", color=image.Color.from_rgb(160, 170, 180), scale=0.8
                )
                exit_btn = [dw - 100, dh - 50, 90, 40]
                draw_exit_btn(canvas, exit_btn)
                disp.show(canvas)
                tx, ty, pressed = ts.read()
                if pressed and not pressed_last:
                    mx, my = map_touch(tx, ty, dw, dh, dw, dh)
                    if is_in_btn(mx, my, exit_btn):
                        app.set_exit_flag(True)
                        break
                pressed_last = pressed
            except Exception as e:
                print("ui err", e)
            time.sleep_ms(100)

    th = threading.Thread(target=ui_loop, name="web-ui", daemon=True)
    th.start()
    try:
        run_web_server(
            cam, stream_cfg, ip, port=HTTP_PORT, detector=detector, use_zh=use_zh
        )
    finally:
        ui_stop["v"] = True


def main():
    disp = display.Display()
    ts = touchscreen.TouchScreen()

    try:
        image.load_font(
            "sourcehansans",
            "/maixapp/share/font/SourceHanSansCN-Regular.otf",
            size=18,
        )
        image.set_default_font("sourcehansans")
    except Exception:
        pass

    show_message(disp, "Connecting...", AP_SSID)
    try:
        _w, ip = connect_wifi(AP_SSID, AP_PASS)
    except Exception as e:
        show_message(disp, "WiFi failed", str(e)[:40])
        print("wifi failed:", e)
        time.sleep_ms(2000)
        show_exiting(disp)
        light_cleanup(reason="wifi failed")
        return

    if app.need_exit():
        show_exiting(disp)
        light_cleanup(reason="user key")
        return

    cfg = select_mode(disp, ts, ip)
    if cfg is None or app.need_exit():
        show_exiting(disp)
        light_cleanup(reason="select exit")
        return

    print("selected:", cfg["mode"], cfg["label"])
    try:
        if cfg["mode"] == "web":
            run_web_mode(disp, ts, cfg, ip)
        else:
            run_tft_stream(disp, ts, cfg, ip)
    except Exception as e:
        print("run error:", e)
    show_exiting(disp)
    light_cleanup(reason="main end")
    print("video_send exited")


if __name__ == "__main__":
    main()
