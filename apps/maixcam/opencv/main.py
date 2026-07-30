"""
MaixCAM：OpenCV 钢珠位置检测 → UART 发给摆杆主控

算法：
  - 固定凹槽 ROI
  - 灰度阈值（亮/暗/自动）+ 形态学 + 轮廓质心
  - 失败回退 1D 轴向投影
  - 像素 → 相对 O 的 mm（0.1mm 上报）

帧格式 type=0x02（13 字节，小端）：
  AA 55 | type=0x02 | flags | pos_01mm i16 | cx i16 | cy i16
        | conf u8 | mode u8 | checksum
  flags bit0=found；pos_01mm 单位 0.1mm；checksum=sum(body)&0xFF

接线 3.3V：A16 TX → MCU RX，GND 共地（Pro 同）
"""

from maix import camera, display, image, app, time, touchscreen, sys, err
import struct

print("[ball] boot...")

try:
    import cv2
    import numpy as np
    print("[ball] cv2 ok", cv2.__version__ if hasattr(cv2, "__version__") else "")
except Exception as e:
    print("[ball] import cv2 FAIL:", e)
    raise

# 可选串口（失败不退出）
try:
    from maix import pinmap, uart
    HAS_UART_MOD = True
except Exception as e:
    print("[ball] uart module missing:", e)
    HAS_UART_MOD = False

ENABLE_UART = True
CAM_W, CAM_H = 320, 240
TX_MIN_MS = 20

# 凹槽 ROI：只罩白色摆杆，别把上方模块框进来（按实装微调 ROI_Y/H）
ROI_X, ROI_Y, ROI_W, ROI_H = 25, 108, 270, 28
BAR_LEN_MM = 250.0
O_OFFSET_PX = ROI_W // 2

# 0 bright(高光) / 1 dark(白底黑球，默认) / 2 auto
DETECT_MODE = 1
TH_BRIGHT = 200          # 高光球
TH_DARK = 110            # 白杆上偏暗的钢珠
TH_BAR_WHITE = 130       # 判定“在白色摆杆上”的灰度下限
MIN_AREA = 25
MAX_AREA = 500           # 球约 1cm，拒大块模块
MIN_CIRCULARITY = 0.45
MAX_ASPECT = 1.8         # |w/h| 接近圆
BALL_DIAM_PX = 14
POS_ALPHA = 0.35
# 质心越靠近 ROI 竖直中线加分（压在凹槽上）
Y_CENTER_WEIGHT = 25.0

MODE_NAMES = ("BRI", "DRK", "AUT")


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
        print("[ball] device_id:", device_id)
        print("[ball] uart list:", uart.list_devices())

        if device_id == "maixcam2":
            pins = {"A21": "UART4_TX", "A22": "UART4_RX"}
            device = "/dev/ttyS4"
        else:
            # maixcam / maixcam-pro
            pins = {"A16": "UART0_TX", "A17": "UART0_RX"}
            device = "/dev/ttyS0"

        for pin, func in pins.items():
            ret = pinmap.set_pin_function(pin, func)
            if ret != err.Err.ERR_NONE:
                print(f"[ball] pinmap warn {pin}->{func}: {ret}")
            else:
                print(f"[ball] pinmap {pin}->{func}")

        ser = uart.UART(device, 115200)
        ser.write_str("BALL ready\r\n")
        print("[ball] UART open", device)
        return ser
    except Exception as e:
        print("[ball] UART fail (continue without):", e)
        return None


def clamp_roi(x, y, w, h, iw, ih):
    x = max(0, min(int(x), max(0, iw - 1)))
    y = max(0, min(int(y), max(0, ih - 1)))
    w = max(8, min(int(w), iw - x))
    h = max(8, min(int(h), ih - y))
    return x, y, w, h


def px_to_mm(cx_roi, roi_w, o_px, bar_len_mm):
    if roi_w <= 1:
        return 0.0
    return (float(cx_roi) - float(o_px)) * (bar_len_mm / float(roi_w))


def _best_blob(mask, gray=None):
    """在 mask 里找最像钢珠的小圆斑；gray 用于加分（更暗更好）。"""
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)
    cnts = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contours = cnts[0] if len(cnts) == 2 else cnts[1]
    best = None
    h = mask.shape[0]
    y_mid = h * 0.5
    target_area = 3.1416 * (BALL_DIAM_PX * 0.5) ** 2

    for c in contours:
        area = cv2.contourArea(c)
        if area < MIN_AREA or area > MAX_AREA:
            continue
        x, y, bw, bh = cv2.boundingRect(c)
        if bw < 1 or bh < 1:
            continue
        aspect = max(bw, bh) / float(min(bw, bh))
        if aspect > MAX_ASPECT:
            continue
        peri = cv2.arcLength(c, True)
        if peri < 1e-3:
            continue
        circ = 4.0 * 3.14159265 * area / (peri * peri)
        if circ < MIN_CIRCULARITY:
            continue
        m = cv2.moments(c)
        if m["m00"] < 1e-3:
            continue
        cx = m["m10"] / m["m00"]
        cy = m["m01"] / m["m00"]

        # 圆度 + 尺寸接近期望球 + 靠近凹槽中线
        size_score = 30.0 * max(0.0, 1.0 - abs(area - target_area) / max(target_area, 1.0))
        y_score = Y_CENTER_WEIGHT * max(0.0, 1.0 - abs(cy - y_mid) / max(y_mid, 1.0))
        dark_score = 0.0
        if gray is not None:
            # 取邻域均值，越暗（相对白杆）越好
            x0 = max(0, int(cx) - 2)
            x1 = min(gray.shape[1], int(cx) + 3)
            y0 = max(0, int(cy) - 2)
            y1 = min(gray.shape[0], int(cy) + 3)
            patch = gray[y0:y1, x0:x1]
            if patch.size > 0:
                dark_score = max(0.0, (180.0 - float(patch.mean())) / 180.0 * 20.0)
        score = circ * 50.0 + size_score + y_score + dark_score
        if best is None or score > best[0]:
            best = (score, cx, cy, area, (x, y, bw, bh))
    return best


def _proj_dark_on_bar(gray, bar_mask):
    """白杆上沿轴向找最暗谷（钢珠）。"""
    h, w = gray.shape[:2]
    if w < 4 or h < 2:
        return None
    g = gray.astype(np.float32)
    # 非杆区域抬高，避免干扰
    g = np.where(bar_mask > 0, g, 255.0)
    proj = g.mean(axis=0)
    win = max(3, int(BALL_DIAM_PX) | 1)
    half = win // 2
    kernel = np.ones(win, dtype=np.float32) / float(win)
    smooth = np.convolve(proj, kernel, mode="same")
    lo, hi = half, w - half
    if hi <= lo:
        return None
    # 只在杆像素足够的列上找
    col_ok = (bar_mask > 0).sum(axis=0) >= max(2, h // 4)
    seg = smooth[lo:hi].copy()
    for i in range(lo, hi):
        if not col_ok[i]:
            seg[i - lo] = 1e6
    if float(np.min(seg)) >= 1e5:
        return None
    idx = int(np.argmin(seg)) + lo
    val = float(smooth[idx])
    base = float(np.median(smooth[col_ok])) if col_ok.any() else float(np.median(smooth[lo:hi]))
    contrast = base - val  # 球应比杆暗
    if contrast < 8.0:
        return None
    # y：该列杆像素的中心
    col = bar_mask[:, idx]
    ys = np.where(col > 0)[0]
    cy = float(ys.mean()) if len(ys) else h * 0.5
    score = min(100.0, contrast * 2.5)
    return (score, float(idx), cy)


def detect_ball(bgr, roi, mode):
    """
    针对白 PPR 凹槽 + 深色钢珠：
      1) 先提白色摆杆区域
      2) 在杆上找暗色小圆 / 轴向暗谷
    """
    ih, iw = bgr.shape[0], bgr.shape[1]
    rx, ry, rw, rh = clamp_roi(roi[0], roi[1], roi[2], roi[3], iw, ih)
    crop = bgr[ry:ry + rh, rx:rx + rw]
    if crop is None or crop.size == 0:
        return _empty(mode)

    if len(crop.shape) == 3:
        gray = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY)
    else:
        gray = crop
    gray = cv2.GaussianBlur(gray, (3, 3), 0)

    # 白色摆杆掩膜：排除上方深色模块
    _, bar_mask = cv2.threshold(gray, TH_BAR_WHITE, 255, cv2.THRESH_BINARY)
    bar_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 3))
    bar_mask = cv2.morphologyEx(bar_mask, cv2.MORPH_CLOSE, bar_kernel, iterations=1)
    bar_mask = cv2.morphologyEx(bar_mask, cv2.MORPH_OPEN, bar_kernel, iterations=1)

    candidates = []
    modes = (0, 1) if mode == 2 else (mode,)
    for mu in modes:
        if mu == 0:
            # 高光：很亮的小点，且应落在杆附近
            _, msk = cv2.threshold(gray, TH_BRIGHT, 255, cv2.THRESH_BINARY)
            msk = cv2.bitwise_and(msk, bar_mask)
        else:
            # 白底暗球：暗于 TH_DARK，且必须在白杆区域内（或紧邻）
            # 稍膨胀 bar，避免球把杆“挖空”后被裁掉
            bar_dil = cv2.dilate(bar_mask, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7)))
            _, dark = cv2.threshold(gray, TH_DARK, 255, cv2.THRESH_BINARY_INV)
            msk = cv2.bitwise_and(dark, bar_dil)
        blob = _best_blob(msk, gray)
        if blob is not None:
            candidates.append((blob, mu, "blob"))

    if candidates:
        best, mode_used, via = max(candidates, key=lambda t: t[0][0])
        score, cx_r, cy_r, _area, bbox_r = best
        bx, by, bw, bh = bbox_r
        return {
            "found": True,
            "cx": int(round(cx_r + rx)),
            "cy": int(round(cy_r + ry)),
            "pos_mm": px_to_mm(cx_r, rw, O_OFFSET_PX, BAR_LEN_MM),
            "conf": int(max(0, min(100, score))),
            "bbox": (bx + rx, by + ry, bw, bh),
            "mode_used": mode_used,
            "via": via,
        }

    # 回退：白杆 1D 暗谷
    pk = _proj_dark_on_bar(gray, bar_mask)
    if pk is None:
        return _empty(mode)

    score, cx_r, cy_r = pk
    r = max(4, BALL_DIAM_PX // 2)
    cx = int(round(cx_r + rx))
    cy = int(round(cy_r + ry))
    return {
        "found": True,
        "cx": cx,
        "cy": cy,
        "pos_mm": px_to_mm(cx_r, rw, O_OFFSET_PX, BAR_LEN_MM),
        "conf": int(max(0, min(100, score))),
        "bbox": (cx - r, cy - r, r * 2, r * 2),
        "mode_used": 1,
        "via": "proj",
    }


def _empty(mode):
    return {
        "found": False,
        "cx": 0,
        "cy": 0,
        "pos_mm": 0.0,
        "conf": 0,
        "bbox": None,
        "mode_used": mode,
        "via": "none",
    }


def pack_ball_frame(found, pos_mm, cx, cy, conf, mode):
    flags = 0x01 if found else 0x00

    def i16(v):
        v = int(round(v))
        return 32767 if v > 32767 else (-32768 if v < -32768 else v)

    body = struct.pack(
        "<BBhhhBB",
        0x02,
        flags,
        i16(pos_mm * 10.0),
        i16(cx),
        i16(cy),
        max(0, min(255, int(conf))),
        max(0, min(255, int(mode))),
    )
    return bytes([0xAA, 0x55]) + body + bytes([sum(body) & 0xFF])


def is_in_button(x, y, btn):
    return btn[0] < x < btn[0] + btn[2] and btn[1] < y < btn[1] + btn[3]


def draw_button(img, btn, label):
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], image.Color.from_rgb(28, 32, 48), -1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], image.COLOR_WHITE, 2)
    size = image.string_size(label, scale=1.0)
    tx = btn[0] + (btn[2] - size.width()) // 2
    ty = btn[1] + (btn[3] - size.height()) // 2
    img.draw_string(tx, ty, label, image.COLOR_WHITE, scale=1.0)


def draw_overlay(img, roi, det, pos_filt, mode, fps, uart_ok):
    rx, ry, rw, rh = roi
    img.draw_rect(rx, ry, rw, rh, image.Color.from_rgb(80, 180, 255), 2)
    ox = rx + O_OFFSET_PX
    img.draw_line(ox, ry, ox, ry + rh, image.Color.from_rgb(255, 220, 80), 1)

    if det["found"]:
        if det["bbox"] is not None:
            bx, by, bw, bh = det["bbox"]
            img.draw_rect(bx, by, bw, bh, image.COLOR_GREEN, 2)
        img.draw_circle(det["cx"], det["cy"], 4, image.COLOR_GREEN, -1)
        status = "LOCK"
    else:
        status = "----"

    uflag = "U1" if uart_ok else "U0"
    line = f"{status} {pos_filt:+6.1f}mm c={det['conf']:3d} {MODE_NAMES[mode]} {det['via']} {uflag}"
    img.draw_string(4, 4, line, image.COLOR_WHITE, scale=1.05)
    img.draw_string(
        max(4, img.width() - 78), 4,
        f"{fps:4.1f}FPS",
        image.Color.from_rgb(160, 200, 255),
        scale=1.0,
    )

    w, h = img.width(), img.height()
    btn_h, gap = 36, 4
    yb = h - btn_h - 2
    tw = (w - gap * 4) // 3
    exit_btn = [gap, yb, tw, btn_h]
    mode_btn = [gap * 2 + tw, yb, tw, btn_h]
    roi_btn = [gap * 3 + tw * 2, yb, tw, btn_h]
    draw_button(img, exit_btn, "Exit")
    draw_button(img, mode_btn, f"Md:{MODE_NAMES[mode]}")
    draw_button(img, roi_btn, "ROI y")
    return exit_btn, mode_btn, roi_btn


def main():
    disp = setup_display()
    show_boot(disp, "Ball OpenCV starting...")

    serial_dev = setup_uart_safe()
    show_boot(disp, "Opening camera...")

    # 官方推荐：BGR 直接给 OpenCV，少一次转换
    cam = camera.Camera(CAM_W, CAM_H, image.Format.FMT_BGR888)
    print("[ball] camera", cam.width(), "x", cam.height(), cam.format())

    ts = touchscreen.TouchScreen()
    show_boot(disp, "Running...")

    roi = [ROI_X, ROI_Y, ROI_W, ROI_H]
    mode = DETECT_MODE
    pos_filt = 0.0
    has_filt = False
    last_tx_ms = 0
    pressed_last = False
    roi_nudge = 0
    frame_i = 0
    err_cnt = 0

    print("[ball] loop start")

    while not app.need_exit():
        try:
            img = cam.read()
            # BGR 相机：ensure_bgr=False；copy=True 更稳
            bgr = image.image2cv(img, ensure_bgr=False, copy=True)

            roi[0], roi[1], roi[2], roi[3] = clamp_roi(
                roi[0], roi[1], roi[2], roi[3], bgr.shape[1], bgr.shape[0]
            )
            det = detect_ball(bgr, roi, mode)

            if det["found"]:
                if not has_filt:
                    pos_filt = det["pos_mm"]
                    has_filt = True
                else:
                    pos_filt = POS_ALPHA * det["pos_mm"] + (1.0 - POS_ALPHA) * pos_filt
                pos_tx = pos_filt
            else:
                has_filt = False
                pos_tx = 0.0
                pos_filt = 0.0

            fps = time.fps()
            exit_btn, mode_btn, roi_btn = draw_overlay(
                img, roi, det, pos_filt if has_filt else 0.0, mode, fps, serial_dev is not None
            )
            disp.show(img)

            tx, ty, pressed = ts.read()
            if pressed and not pressed_last:
                if is_in_button(tx, ty, exit_btn):
                    app.set_exit_flag(True)
                elif is_in_button(tx, ty, mode_btn):
                    mode = (mode + 1) % 3
                    print("[ball] mode", MODE_NAMES[mode])
                elif is_in_button(tx, ty, roi_btn):
                    roi_nudge = (roi_nudge + 1) % 3
                    if roi_nudge == 0:
                        roi[1] = ROI_Y
                    elif roi_nudge == 1:
                        roi[1] = max(0, ROI_Y - 25)
                    else:
                        roi[1] = min(CAM_H - roi[3], ROI_Y + 25)
                    print("[ball] ROI y", roi[1])
            pressed_last = pressed

            now_ms = time.ticks_ms()
            if serial_dev is not None and (now_ms - last_tx_ms) >= TX_MIN_MS:
                found = det["found"]
                frame = pack_ball_frame(
                    found,
                    pos_tx if found else 0.0,
                    det["cx"] if found else 0,
                    det["cy"] if found else 0,
                    det["conf"] if found else 0,
                    mode,
                )
                serial_dev.write(frame)
                last_tx_ms = now_ms

            frame_i += 1
            if frame_i % 30 == 0:
                print(
                    f"[ball] f={frame_i} found={det['found']} "
                    f"pos={pos_filt:+.1f} conf={det['conf']} via={det['via']} fps={fps:.1f}"
                )
        except Exception as e:
            err_cnt += 1
            print("[ball] loop err:", e)
            show_boot(disp, f"ERR: {e}")
            time.sleep_ms(500)
            if err_cnt > 20:
                break

    if serial_dev is not None:
        try:
            serial_dev.write(pack_ball_frame(False, 0.0, 0, 0, 0, mode))
        except Exception:
            pass
    print("[ball] exit")


if __name__ == "__main__":
    main()
