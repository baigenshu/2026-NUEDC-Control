"""
maix_phone — MaixCAM 钢珠检测直播（连手机热点）

  - Phone Web：YOLO11 叠框 → Flask MJPEG；录像仅在手机

模型：yolo11n_ball.mud（与 detect_ball 相同路径/加载），见 detect_util
"""

from maix import camera, display, image, app, time, network, err, touchscreen

# 手机个人热点（STA）
PHONE_SSID = "Xiaomi x"
PHONE_PASS = "20060313"
WIFI_TIMEOUT_S = 60

WEB_CFG = {
    "label": "Phone Web",
    "mode": "web",
    "cam_w": 320,
    "cam_h": 240,
    "jpeg_quality": 50,
    "frame_interval_ms": 45,
    "preview_every": 12,
}


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


def map_touch(tx, ty, disp_w, disp_h, img_w, img_h):
    if disp_w > 0 and disp_h > 0 and (disp_w != img_w or disp_h != img_h):
        return int(tx * img_w / disp_w), int(ty * img_h / disp_h)
    return int(tx), int(ty)


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
    print("connect to phone hotspot:", ssid)
    e = w.connect(ssid, password, wait=True, timeout=WIFI_TIMEOUT_S)
    err.check_raise(e, "connect wifi failed: " + ssid)
    ip = w.get_ip()
    print("connected, ip:", ip)
    return w, ip


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

    cam_w = detector.input_width()
    cam_h = detector.input_height()
    cam_fmt = detector.input_format()
    print("camera for detect:", cam_w, cam_h, cam_fmt)
    cam = camera.Camera(cam_w, cam_h, cam_fmt)

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

    show_message(disp, "Connecting...", PHONE_SSID)
    try:
        _w, ip = connect_wifi(PHONE_SSID, PHONE_PASS)
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

    print("start web mode")
    try:
        run_web_mode(disp, ts, WEB_CFG, ip)
    except Exception as e:
        print("run error:", e)
    show_exiting(disp)
    light_cleanup(reason="main end")
    print("maix_phone exited")


if __name__ == "__main__":
    main()
