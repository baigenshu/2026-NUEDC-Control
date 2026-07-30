"""
钢珠：YOLOv5 + ByteTrack/外推 + 白轨精修 → 一维 s → UART

检测稳定性（新数据集代码）：
  低 conf + ByteTrack + 短时保持 + 匀速外推
精修与输出（方案）：
  ROI maix find_blobs 质心 → 白轨投影 s + EMA → HUD + type=0x02

模型默认：/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud（可改 MODEL_MUD）
UART（115200，小端）：
  AA 55 | 0x02 | flags | s i16 | conf u8 | cx i16 | cy i16 | csum
接线：A16 TX -> MCU RX，GND 共地
"""

from maix import (
    nn, display, camera, app, image, tracker, time,
    pinmap, uart, sys, err, touchscreen,
)
import os
import struct
import math


# ---------- 模型 ----------
# 优先绝对路径；也可只写文件名，由 find_model 搜索
MODEL_MUD = "/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud"

CONF_TH = 0.2
IOU_TH = 0.45

# ByteTrack
MAX_LOST = 30
TRACK_THRESH = 0.25
HIGH_THRESH = 0.4
MATCH_THRESH = 0.7
MAX_HISTORY = 15

# 检测连续丢 N 帧内匀速外推
HOLD_PREDICT_FRAMES = 8

# ---------- 精修 / 白轨 ----------
ROI_MARGIN = 0.35
ROI_MIN_SIDE = 24
BALL_L_MIN, BALL_L_MAX = 0, 70
BALL_A_MIN, BALL_A_MAX = -40, 40
BALL_B_MIN, BALL_B_MAX = -40, 40
MIN_BALL_PIXELS = 25
MIN_CIRCULARITY = 0.45
TRACK_L_MIN, TRACK_L_MAX = 70, 100
TRACK_A_MIN, TRACK_A_MAX = -20, 20
TRACK_B_MIN, TRACK_B_MAX = -20, 20
MIN_TRACK_PIXELS = 400
TRACK_BAND_HALF = 28
TRACK_REFIT_FRAMES = 45
EMA_ALPHA = 0.35
REFINE_ON_TRACK = True  # track 模式也做轻量精修

# ---------- UART ----------
ENABLE_UART = True
TX_MIN_MS = 33


def find_model():
    mud = MODEL_MUD
    if os.path.isabs(mud) and os.path.exists(mud):
        return mud
    try:
        here = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        here = "."
    base = os.path.basename(mud) if mud else "model_308837.mud"
    candidates = [
        mud,
        os.path.join(here, base),
        os.path.join(here, "model_308837.mud"),
        os.path.join("/root/models", base),
        "/root/models/model_308837.mud",
        os.path.join(here, "steel_ball.mud"),
        "/root/models/steel_ball.mud",
        "/root/detect_ball/model_308837.mud",
    ]
    for p in candidates:
        if p and os.path.exists(p):
            return p
    raise FileNotFoundError(
        "mud not found: {} (put model_308837.mud + cvimodel under /root/models/)".format(mud)
    )


def setup_uart():
    device_id = sys.device_id()
    print("device_id:", device_id)
    if device_id == "maixcam2":
        pin_function = {"A21": "UART4_TX", "A22": "UART4_RX"}
        device = "/dev/ttyS4"
    else:
        pin_function = {"A16": "UART0_TX", "A17": "UART0_RX"}
        device = "/dev/ttyS0"
    for pin, func in pin_function.items():
        ret = pinmap.set_pin_function(pin, func)
        err.check_raise(ret, "Failed set pin {} -> {}".format(pin, func))
        print("pinmap: {} -> {}".format(pin, func))
    serial_dev = uart.UART(device, 115200)
    print("UART:", device, "115200")
    return serial_dev


def setup_font():
    try:
        image.load_font(
            "sourcehansans",
            "/maixapp/share/font/SourceHanSansCN-Regular.otf",
            size=18,
        )
        image.set_default_font("sourcehansans")
        return True
    except Exception as e:
        print("font fail:", e)
        return False


def i16(v):
    v = int(round(v))
    if v > 32767:
        return 32767
    if v < -32768:
        return -32768
    return v


def pack_ball_frame(found, s, conf, cx, cy):
    flags = 0x01 if found else 0x00
    c = int(conf)
    if c < 0:
        c = 0
    if c > 255:
        c = 255
    body = struct.pack("<BBhBhh", 0x02, flags, i16(s), c, i16(cx), i16(cy))
    return bytes([0xAA, 0x55]) + body + bytes([sum(body) & 0xFF])


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def expand_roi(x, y, w, h, img_w, img_h, margin=ROI_MARGIN):
    cx = x + w * 0.5
    cy = y + h * 0.5
    nw = max(ROI_MIN_SIDE, int(w * (1.0 + margin)))
    nh = max(ROI_MIN_SIDE, int(h * (1.0 + margin)))
    x0 = clamp(int(cx - nw * 0.5), 0, img_w - 1)
    y0 = clamp(int(cy - nh * 0.5), 0, img_h - 1)
    x1 = clamp(x0 + nw, 1, img_w)
    y1 = clamp(y0 + nh, 1, img_h)
    return x0, y0, x1 - x0, y1 - y0


def _blob_circularity(b):
    bw, bh = b.w(), b.h()
    m = max(bw, bh, 1)
    return min(bw, bh) / float(m)


def _find_blobs_roi(img, thresholds, x, y, w, h, pixels_th, area_th):
    kwargs = dict(
        pixels_threshold=pixels_th,
        area_threshold=area_th,
        merge=True,
    )
    try:
        blobs = img.find_blobs(thresholds, roi=(x, y, w, h), **kwargs)
        if blobs is not None:
            return list(blobs)
    except TypeError:
        pass
    except Exception:
        pass
    try:
        blobs = img.find_blobs(
            thresholds,
            x_start=x, y_start=y, x_end=x + w, y_end=y + h,
            **kwargs
        )
        if blobs is not None:
            return list(blobs)
    except TypeError:
        pass
    except Exception:
        pass
    try:
        blobs = img.find_blobs(thresholds, **kwargs)
    except Exception:
        return []
    if not blobs:
        return []
    x1, y1 = x + w, y + h
    return [b for b in blobs if x <= b.cx() < x1 and y <= b.cy() < y1]


def pick_best_ball_blob(blobs):
    if not blobs:
        return None
    scored = []
    for b in blobs:
        if b.pixels() < MIN_BALL_PIXELS and b.area() < MIN_BALL_PIXELS:
            continue
        circ = _blob_circularity(b)
        if circ < MIN_CIRCULARITY:
            continue
        scored.append((b.area() * circ, b))
    if not scored:
        return None
    scored.sort(key=lambda t: t[0], reverse=True)
    return scored[0][1]


def refine_ball_in_roi(img, rx, ry, rw, rh):
    thr = [(BALL_L_MIN, BALL_L_MAX, BALL_A_MIN, BALL_A_MAX, BALL_B_MIN, BALL_B_MAX)]
    blobs = _find_blobs_roi(img, thr, rx, ry, rw, rh, MIN_BALL_PIXELS, MIN_BALL_PIXELS)
    best = pick_best_ball_blob(blobs)
    if best is None:
        return None
    return float(best.cx()), float(best.cy())


def estimate_track_axis(img):
    thr = [(TRACK_L_MIN, TRACK_L_MAX, TRACK_A_MIN, TRACK_A_MAX, TRACK_B_MIN, TRACK_B_MAX)]
    try:
        blobs = img.find_blobs(
            thr,
            pixels_threshold=MIN_TRACK_PIXELS,
            area_threshold=MIN_TRACK_PIXELS,
            merge=True,
        )
    except Exception as e:
        print("track find_blobs fail:", e)
        return None
    if not blobs:
        return None

    def track_score(b):
        bw, bh = max(b.w(), 1), max(b.h(), 1)
        aspect = bw / float(bh)
        if aspect < 1.0:
            aspect = 1.0 / aspect
        return b.area() * aspect

    best = max(blobs, key=track_score)
    if best.area() < MIN_TRACK_PIXELS:
        return None

    p0x, p0y = float(best.cx()), float(best.cy())
    bw, bh = float(best.w()), float(best.h())
    if bw >= bh:
        ux, uy, length = 1.0, 0.0, bw
    else:
        ux, uy, length = 0.0, 1.0, bh
    try:
        rot = best.rotation()
        ux, uy = math.cos(rot), math.sin(rot)
        n = math.hypot(ux, uy) or 1.0
        ux, uy = ux / n, uy / n
        if ux < 0:
            ux, uy = -ux, -uy
    except Exception:
        pass

    return {
        "p0x": p0x,
        "p0y": p0y,
        "ux": ux,
        "uy": uy,
        "length": length,
        "rect": (best.x(), best.y(), best.w(), best.h()),
    }


def default_track_axis(img_w, img_h):
    return {
        "p0x": img_w * 0.5,
        "p0y": img_h * 0.5,
        "ux": 1.0,
        "uy": 0.0,
        "length": float(img_w),
        "rect": None,
    }


def project_s(cx, cy, axis):
    return (cx - axis["p0x"]) * axis["ux"] + (cy - axis["p0y"]) * axis["uy"]


def point_on_axis(s, axis):
    return axis["p0x"] + s * axis["ux"], axis["p0y"] + s * axis["uy"]


def in_track_band(cx, cy, axis, half=TRACK_BAND_HALF):
    dx = cx - axis["p0x"]
    dy = cy - axis["p0y"]
    return abs(-axis["uy"] * dx + axis["ux"] * dy) <= half


def blend_axis(axis, est, a=0.3):
    axis["p0x"] = (1 - a) * axis["p0x"] + a * est["p0x"]
    axis["p0y"] = (1 - a) * axis["p0y"] + a * est["p0y"]
    ux = (1 - a) * axis["ux"] + a * est["ux"]
    uy = (1 - a) * axis["uy"] + a * est["uy"]
    n = math.hypot(ux, uy) or 1.0
    axis["ux"], axis["uy"] = ux / n, uy / n
    if axis["ux"] < 0:
        axis["ux"], axis["uy"] = -axis["ux"], -axis["uy"]
    axis["length"] = est["length"]
    axis["rect"] = est.get("rect")


def best_det(objs):
    if not objs:
        return None
    return max(objs, key=lambda o: o.score)


def is_in_button(x, y, btn):
    return btn[0] < x < btn[0] + btn[2] and btn[1] < y < btn[1] + btn[3]


def draw_button(img, btn, label):
    fill = image.Color.from_rgb(28, 32, 48)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=fill, thickness=-1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=image.COLOR_WHITE, thickness=2)
    size = image.string_size(label, scale=1.0)
    tx = btn[0] + max(0, (btn[2] - size.width()) // 2)
    ty = btn[1] + max(0, (btn[3] - size.height()) // 2)
    img.draw_string(tx, ty, label, color=image.COLOR_WHITE, scale=1.0)


def mode_color(mode):
    if mode == "detect":
        return image.COLOR_GREEN
    if mode == "track":
        return image.COLOR_YELLOW
    if mode == "predict":
        return image.Color.from_rgb(80, 160, 255)
    return image.COLOR_RED


def draw_hud(img, found, cx, cy, s, score, mode, axis, box, fps, miss):
    w, h = img.width(), img.height()
    if axis is not None:
        L = axis.get("length", w) * 0.6
        x0, y0 = point_on_axis(-L, axis)
        x1, y1 = point_on_axis(L, axis)
        img.draw_line(
            int(x0), int(y0), int(x1), int(y1),
            image.Color.from_rgb(80, 180, 255), 2,
        )

    color = mode_color(mode)
    if box is not None:
        x, y, bw, bh, sc = box
        img.draw_rect(int(x), int(y), int(bw), int(bh), color, thickness=2)
        img.draw_string(
            int(x), max(0, int(y) - 16),
            "{} {:.2f}".format(mode, sc),
            color,
        )

    if found:
        img.draw_circle(int(cx), int(cy), 5, image.Color.from_rgb(0, 255, 80), -1)
        img.draw_circle(int(cx), int(cy), 12, image.Color.from_rgb(0, 255, 80), 1)
        if axis is not None:
            px, py = point_on_axis(s, axis)
            img.draw_line(
                int(cx), int(cy), int(px), int(py),
                image.Color.from_rgb(255, 120, 80), 1,
            )
            img.draw_circle(int(px), int(py), 4, image.Color.from_rgb(255, 200, 0), -1)

    img.draw_string(
        2, 2,
        "{} s={:.1f} miss={}".format("LOCK" if found else "----", s if found else 0.0, miss),
        image.COLOR_WHITE, scale=1.1,
    )
    img.draw_string(
        2, 22,
        "fps={:.1f} conf>={}".format(fps, CONF_TH),
        image.Color.from_rgb(160, 200, 255), scale=1.0,
    )
    if found:
        img.draw_string(
            2, 42,
            "xy=({},{})".format(int(cx), int(cy)),
            image.Color.from_rgb(180, 220, 180), scale=1.0,
        )

    btn_h = 40
    gap = 6
    yb = h - btn_h - 4
    tw = (w - gap * 3) // 2
    exit_btn = [gap, yb, tw, btn_h]
    calib_btn = [gap * 2 + tw, yb, tw, btn_h]
    draw_button(img, exit_btn, "Exit")
    draw_button(img, calib_btn, "Calib")
    return exit_btn, calib_btn


def main():
    setup_font()
    model_path = find_model()
    print("load:", model_path)

    detector = nn.YOLO11(model=model_path, dual_buff=True)
    print(
        "input:",
        detector.input_width(),
        detector.input_height(),
        detector.input_format(),
    )
    try:
        print("labels:", detector.labels)
    except Exception:
        pass

    cam = camera.Camera(
        detector.input_width(),
        detector.input_height(),
        detector.input_format(),
    )
    disp = display.Display()
    ts = touchscreen.TouchScreen()
    bt = tracker.ByteTracker(MAX_LOST, TRACK_THRESH, HIGH_THRESH, MATCH_THRESH, MAX_HISTORY)
    serial_dev = setup_uart() if ENABLE_UART else None

    img_w, img_h = cam.width(), cam.height()
    axis = default_track_axis(img_w, img_h)

    last_cx, last_cy = None, None
    vx, vy = 0.0, 0.0
    miss = 0
    s_ema = None
    frame_i = 0
    last_tx_ms = 0
    pressed_last = False

    boot = cam.read()
    est = estimate_track_axis(boot)
    if est is not None:
        axis = est
        print(
            "track ok: p0=({:.1f},{:.1f}) u=({:.2f},{:.2f})".format(
                axis["p0x"], axis["p0y"], axis["ux"], axis["uy"]
            )
        )
    else:
        print("track fit fail, use mid-line")

    if serial_dev is not None:
        serial_dev.write_str("BALL ready\r\n")
    print("YOLOv5 + ByteTrack + rail s -> UART")

    while not app.need_exit():
        img = cam.read()
        frame_i += 1
        fps = time.fps()

        if TRACK_REFIT_FRAMES > 0 and frame_i % TRACK_REFIT_FRAMES == 0:
            est = estimate_track_axis(img)
            if est is not None:
                blend_axis(axis, est, 0.3)

        objs = detector.detect(img, conf_th=CONF_TH, iou_th=IOU_TH)
        t_objs = [
            tracker.Object(o.x, o.y, o.w, o.h, o.class_id, o.score) for o in objs
        ]
        tracks = bt.update(t_objs)

        det = best_det(objs)
        show_box = None
        mode = "none"
        score = 0.0

        if det is not None:
            cx = det.x + det.w // 2
            cy = det.y + det.h // 2
            if last_cx is not None:
                vx = 0.6 * vx + 0.4 * (cx - last_cx)
                vy = 0.6 * vy + 0.4 * (cy - last_cy)
            last_cx, last_cy = float(cx), float(cy)
            miss = 0
            score = float(det.score)
            show_box = (det.x, det.y, det.w, det.h, det.score)
            mode = "detect"
        else:
            miss += 1
            alive = [t for t in tracks if not t.lost]
            if alive:
                obj = alive[0].history[-1]
                show_box = (obj.x, obj.y, obj.w, obj.h, alive[0].score)
                last_cx = float(obj.x + obj.w // 2)
                last_cy = float(obj.y + obj.h // 2)
                score = float(alive[0].score)
                mode = "track"
            elif last_cx is not None and miss <= HOLD_PREDICT_FRAMES:
                last_cx = last_cx + vx
                last_cy = last_cy + vy
                bw = bh = 24
                show_box = (
                    int(last_cx - bw // 2),
                    int(last_cy - bh // 2),
                    bw,
                    bh,
                    0.0,
                )
                score = 0.0
                mode = "predict"
            else:
                mode = "lost"
                show_box = None

        found = False
        cx_out, cy_out = 0.0, 0.0
        s_out = 0.0

        if show_box is not None and last_cx is not None:
            bx, by, bw, bh, _sc = show_box
            cx_out, cy_out = float(last_cx), float(last_cy)

            do_refine = mode == "detect" or (REFINE_ON_TRACK and mode == "track")
            if do_refine:
                rx, ry, rw, rh = expand_roi(bx, by, bw, bh, img_w, img_h)
                refined = refine_ball_in_roi(img, rx, ry, rw, rh)
                if refined is not None:
                    cx_out, cy_out = refined
                    last_cx, last_cy = cx_out, cy_out

            # predict：沿轨约束外推点，减小垂直漂移
            if mode == "predict" and axis is not None:
                s_tmp = project_s(cx_out, cy_out, axis)
                cx_out, cy_out = point_on_axis(s_tmp, axis)
                last_cx, last_cy = cx_out, cy_out

            s_raw = project_s(cx_out, cy_out, axis)
            if s_ema is None:
                s_ema = s_raw
            else:
                s_ema = EMA_ALPHA * s_raw + (1.0 - EMA_ALPHA) * s_ema
            s_out = s_ema
            found = mode in ("detect", "track", "predict")

        exit_btn, calib_btn = draw_hud(
            img, found, cx_out, cy_out, s_out, score, mode, axis, show_box, fps, miss,
        )
        disp.show(img)

        tx, ty, pressed = ts.read()
        if pressed and not pressed_last:
            if is_in_button(tx, ty, exit_btn):
                app.set_exit_flag(True)
            elif is_in_button(tx, ty, calib_btn):
                est = estimate_track_axis(img)
                if est is not None:
                    axis = est
                    s_ema = None
                    print("calib track ok p0=({:.1f},{:.1f})".format(axis["p0x"], axis["p0y"]))
                else:
                    print("calib track failed")
        pressed_last = pressed

        now_ms = time.ticks_ms()
        if serial_dev is not None and (now_ms - last_tx_ms) >= TX_MIN_MS:
            conf_u8 = int(clamp(score * 100.0, 0, 255)) if found else 0
            serial_dev.write(
                pack_ball_frame(
                    found,
                    s_out if found else 0,
                    conf_u8,
                    cx_out if found else 0,
                    cy_out if found else 0,
                )
            )
            last_tx_ms = now_ms

    if serial_dev is not None:
        serial_dev.write(pack_ball_frame(False, 0, 0, 0, 0))


if __name__ == "__main__":
    main()

# 换模型：mud+cvimodel → /root/models/ 或改 MODEL_MUD
# mud model_type=yolo11 用 nn.YOLO11；yolov5 用 nn.YOLOv5；yolov8 用 nn.YOLOv8
