"""
MaixCAM：OpenCV 钢珠位置 → UART → balance 摆杆主控（闭环停球）

对接文档：
  apps/balance/docs/vision_proto.md
  apps/balance/src/Hardware/Inc/ball_proto.h

算法：
  - 固定凹槽 ROI + 灰度/轮廓/1D 投影
  - 像素 → 相对 O 的 mm，**整 mm** 量化上报（type=0x02）

帧（定长 13B，小端）：
  球位 0x02: AA 55 | 02 | flags | pos_mm i16 | cx i16 | cy i16 | conf | mode | csum
  定点 0x12: AA 55 | 12 | 00 | target_mm i16 | pad×6 | csum
  pos/target 单位 = **1 mm**（整毫米，抑抖）；csum=sum(body)&0xFF

接线 3.3V 共地：
  MaixCAM A16 TX → balance PA31 (UART0 RX)
  MaixCAM A17 RX → balance PA28 (可选)
  MaixCAM2: A21/A22 → UART4
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

try:
    from maix import pinmap, uart
    HAS_UART_MOD = True
except Exception as e:
    print("[ball] uart module missing:", e)
    HAS_UART_MOD = False

# ---------- 与 ball_proto / vision_proto 对齐 ----------
ENABLE_UART = True
PROTO_MAGIC0 = 0xAA
PROTO_MAGIC1 = 0x55
PROTO_TYPE_BALL = 0x02
PROTO_TYPE_SETPOINT = 0x12
PROTO_FLAG_FOUND = 0x01
# 与 MCU BALL_CONF_MIN 一致：低于此视为丢球
CONF_MIN = 30
TX_MIN_MS = 20            # ≤50 Hz，MCU 超时 100 ms
BAUD = 115200

CAM_W, CAM_H = 320, 240

# 凹槽 ROI：用户已标定＝完整有效行程（勿改四数）
# 中线=O；品红线=停球定点；球=小点
ROI_X, ROI_Y, ROI_W, ROI_H = 33, 128, 260, 14
BAR_LEN_MM = 234.0          # 框内左右总长实测
O_OFFSET_PX = ROI_W // 2
SP_MIN_MM = int(-BAR_LEN_MM * 0.5)
SP_MAX_MM = int(BAR_LEN_MM * 0.5)

ENABLE_PROJ_FALLBACK = True
PROJ_MIN_CONTRAST = 10.0
PROJ_SCORE_SCALE = 2.0

DETECT_MODE = 1
TH_BRIGHT = 200
TH_DARK = 115
TH_BAR_WHITE = 120
MIN_AREA = 12
MAX_AREA = 500
MIN_CIRCULARITY = 0.28
MAX_ASPECT = 2.4
BALL_DIAM_PX = 12
POS_ALPHA = 0.35
Y_CENTER_WEIGHT = 20.0
POS_HYST_MM = 0.55

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

        ser = uart.UART(device, BAUD)
        # 文本握手：MCU 状态机忽略非 AA 55
        ser.write_str("BALL ready\r\n")
        print("[ball] UART open", device, BAUD, "-> balance PA31 RX")
        return ser
    except Exception as e:
        print("[ball] UART fail (continue without):", e)
        return None


def _i16(v):
    v = int(round(v))
    if v > 32767:
        return 32767
    if v < -32768:
        return -32768
    return v


def _frame_with_csum(body: bytes) -> bytes:
    """body 必须 10 字节（type..末字段），返回 13 字节整帧。"""
    assert len(body) == 10
    return bytes([PROTO_MAGIC0, PROTO_MAGIC1]) + body + bytes([sum(body) & 0xFF])


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
    """在 mask 里找最像钢珠的斑（细 ROI 下形态学要轻，避免球被腐蚀没）。"""
    # 细框 H≈16：只用 1 次 close 粘连，open 用 2x2 以免抹掉小球
    k_close = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    k_open = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2, 2))
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, k_close, iterations=1)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, k_open, iterations=1)
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
        # 细 ROI 里球常被裁成扁椭圆
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

        size_score = 30.0 * max(0.0, 1.0 - abs(area - target_area) / max(target_area, 1.0))
        y_score = Y_CENTER_WEIGHT * max(0.0, 1.0 - abs(cy - y_mid) / max(y_mid, 1.0))
        dark_score = 0.0
        if gray is not None:
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
    """白杆上沿轴向找最暗谷（钢珠）；ROI 全宽有效，仅避开卷积半窗。"""
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
    if hi <= lo + 2:
        lo, hi = 0, w
    if hi <= lo:
        return None
    # 只在杆像素足够的列上找
    col_ok = (bar_mask > 0).sum(axis=0) >= max(2, h // 4)
    seg = smooth[lo:hi].copy()
    for i in range(lo, hi):
        if i < len(col_ok) and not col_ok[i]:
            seg[i - lo] = 1e6
    if float(np.min(seg)) >= 1e5:
        return None
    idx = int(np.argmin(seg)) + lo
    val = float(smooth[idx])
    base = float(np.median(smooth[col_ok])) if col_ok.any() else float(np.median(smooth[lo:hi]))
    contrast = base - val  # 球应比杆暗
    if contrast < float(PROJ_MIN_CONTRAST):
        return None
    # y：该列杆像素的中心
    col = bar_mask[:, idx]
    ys = np.where(col > 0)[0]
    cy = float(ys.mean()) if len(ys) else h * 0.5
    score = min(95.0, 25.0 + contrast * float(PROJ_SCORE_SCALE))
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
    # 细 ROI 轻度模糊即可
    gray = cv2.GaussianBlur(gray, (3, 3), 0)

    # 白色摆杆掩膜（核随高度缩小，避免 H=16 时 open 掏空）
    _, bar_mask = cv2.threshold(gray, TH_BAR_WHITE, 255, cv2.THRESH_BINARY)
    bh = max(2, min(3, gray.shape[0] // 4))
    bar_kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, bh))
    bar_mask = cv2.morphologyEx(bar_mask, cv2.MORPH_CLOSE, bar_kernel, iterations=1)
    if gray.shape[0] >= 12:
        bar_mask = cv2.morphologyEx(bar_mask, cv2.MORPH_OPEN, bar_kernel, iterations=1)

    candidates = []
    modes = (0, 1) if mode == 2 else (mode,)
    for mu in modes:
        if mu == 0:
            _, msk = cv2.threshold(gray, TH_BRIGHT, 255, cv2.THRESH_BINARY)
            msk = cv2.bitwise_and(msk, bar_mask)
        else:
            # 暗球：阈值 + 相对杆面偏暗；bar 大膨胀，球挖空处仍保留
            bar_dil = cv2.dilate(
                bar_mask,
                cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 5)),
            )
            _, dark = cv2.threshold(gray, TH_DARK, 255, cv2.THRESH_BINARY_INV)
            # 补充：比局部中值更暗（细框内绝对阈值常不够）
            med = cv2.medianBlur(gray, 5)
            diff = cv2.subtract(med, gray)  # 正值 = 比邻域暗
            _, rel = cv2.threshold(diff, 10, 255, cv2.THRESH_BINARY)
            dark = cv2.bitwise_or(dark, rel)
            msk = cv2.bitwise_and(dark, bar_dil)
        blob = _best_blob(msk, gray)
        if blob is not None:
            candidates.append((blob, mu, "blob"))

    if candidates:
        # ROI 全宽有效，不再做左右/行程死区过滤
        best, mode_used, via = max(candidates, key=lambda t: t[0][0])
        score, cx_r, cy_r, _area, bbox_r = best
        bx, by, bw, bh = bbox_r
        pmm = px_to_mm(cx_r, rw, O_OFFSET_PX, BAR_LEN_MM)
        return {
            "found": True,
            "cx": int(round(cx_r + rx)),
            "cy": int(round(cy_r + ry)),
            "pos_mm": pmm,
            "conf": int(max(0, min(100, score))),
            "bbox": (bx + rx, by + ry, bw, bh),
            "mode_used": mode_used,
            "via": via,
        }

    if not ENABLE_PROJ_FALLBACK:
        return _empty(mode)

    pk = _proj_dark_on_bar(gray, bar_mask)
    if pk is None:
        return _empty(mode)

    score, cx_r, cy_r = pk
    pmm = px_to_mm(cx_r, rw, O_OFFSET_PX, BAR_LEN_MM)
    r = max(4, BALL_DIAM_PX // 2)
    cx = int(round(cx_r + rx))
    cy = int(round(cy_r + ry))
    return {
        "found": True,
        "cx": cx,
        "cy": cy,
        "pos_mm": pmm,
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


def quantize_mm(pos_f, last_i, has_last):
    """浮点 mm → 整 mm；带滞回，减少 ±1mm 来回跳。"""
    if not has_last:
        return int(round(pos_f))
    # 仍在 last±HYST 内则保持
    if abs(pos_f - float(last_i)) < float(POS_HYST_MM):
        return int(last_i)
    return int(round(pos_f))


def pack_ball_frame(found, pos_mm, cx, cy, conf, mode):
    """type=0x02；pos_mm 为整毫米（线格式 i16，单位 1mm）。"""
    conf_i = max(0, min(100, int(conf)))
    usable = bool(found) and conf_i >= CONF_MIN
    flags = PROTO_FLAG_FOUND if usable else 0x00
    pos_i = int(pos_mm) if usable else 0
    body = struct.pack(
        "<BBhhhBB",
        PROTO_TYPE_BALL,
        flags,
        _i16(pos_i),
        _i16(cx) if usable else 0,
        _i16(cy) if usable else 0,
        conf_i if usable else 0,
        max(0, min(255, int(mode))),
    )
    return _frame_with_csum(body)


def pack_setpoint_frame(target_mm):
    """type=0x12 停球定点，单位整 mm。"""
    body = struct.pack("<BBh", PROTO_TYPE_SETPOINT, 0x00, _i16(int(round(target_mm)))) + bytes(6)
    return _frame_with_csum(body)


def uart_write(ser, data: bytes) -> bool:
    if ser is None or not data:
        return False
    try:
        ser.write(data)
        return True
    except Exception as e:
        print("[ball] UART write fail:", e)
        return False


def mm_to_roi_x(pos_mm, roi):
    """O 相对 mm → 画面 x。"""
    rx, _ry, rw, _rh = roi
    if BAR_LEN_MM <= 1e-6 or rw <= 1:
        return rx + O_OFFSET_PX
    cx_r = float(O_OFFSET_PX) + float(pos_mm) * (float(rw) / BAR_LEN_MM)
    return int(round(rx + cx_r))


def roi_x_to_mm(x, roi):
    """触摸 x → 相对 O 的 mm（可超框则钳位到杆长半幅）。"""
    rx, _ry, rw, _rh = roi
    if rw <= 1 or BAR_LEN_MM <= 1e-6:
        return 0
    cx_r = float(x) - float(rx)
    mm = (cx_r - float(O_OFFSET_PX)) * (BAR_LEN_MM / float(rw))
    if mm > SP_MAX_MM:
        return SP_MAX_MM
    if mm < SP_MIN_MM:
        return SP_MIN_MM
    return int(round(mm))


def is_in_button(x, y, btn):
    return btn[0] <= x <= btn[0] + btn[2] and btn[1] <= y <= btn[1] + btn[3]


def map_touch_to_image(tx, ty, img_w, img_h, disp_w, disp_h):
    """触摸在显示坐标 → 相机/叠加图像坐标（与 collect 一致）。"""
    if disp_w > 0 and disp_h > 0 and (disp_w != img_w or disp_h != img_h):
        return int(tx * img_w / disp_w), int(ty * img_h / disp_h)
    return int(tx), int(ty)


def layout_controls(img_w, img_h):
    """Bottom bar: Exit | Set | Done/Reset — all English labels."""
    by = img_h - 36
    bh = 30
    gap = 6
    exit_btn = [6, by, 52, bh]
    set_btn = [exit_btn[0] + exit_btn[2] + gap, by, 72, bh]
    reset_btn = [set_btn[0] + set_btn[2] + gap, by, 72, bh]
    return exit_btn, set_btn, reset_btn


def layout_drag_strip(img_w, img_h, roi, exit_btn):
    """Drag strip shown only in Set mode."""
    rx, ry, rw, rh = roi
    strip_y = min(exit_btn[1] - 34, ry + rh + 8)
    if strip_y < ry + rh + 2:
        strip_y = ry + rh + 2
    strip_h = 26
    if strip_y + strip_h > exit_btn[1] - 4:
        strip_h = max(18, exit_btn[1] - 4 - strip_y)
    return [rx, strip_y, rw, strip_h]


def hit_drag_zone(x, y, roi, drag_strip):
    """Bar or drag strip (Set mode only)."""
    rx, ry, rw, rh = roi
    if (rx - 4) <= x <= (rx + rw + 4) and (ry - 10) <= y <= (ry + rh + 10):
        return True
    return is_in_button(x, y, drag_strip)


def draw_btn(img, btn, label, active=False, danger=False):
    bx, by, bw, bh = btn
    if danger:
        bg = image.Color.from_rgb(70, 36, 36)
    elif active:
        bg = image.Color.from_rgb(40, 90, 70)
    else:
        bg = image.Color.from_rgb(36, 36, 48)
    img.draw_rect(bx, by, bw, bh, bg, -1)
    border = image.Color.from_rgb(80, 220, 140) if active else image.COLOR_WHITE
    img.draw_rect(bx, by, bw, bh, border, 1)
    img.draw_string(bx + 6, by + 6, label, image.COLOR_WHITE, scale=1.0)


def draw_ui(img, roi, det, pos_mm_i, sp_mm, usable,
            exit_btn, set_btn, reset_btn, drag_strip,
            set_mode, dragging, fps):
    """ROI + center + SP (edit only in Set mode) + FPS + bottom buttons (EN)."""
    rx, ry, rw, rh = roi
    img.draw_rect(rx, ry, rw, rh, image.Color.from_rgb(80, 180, 255), 1)

    ox = rx + O_OFFSET_PX
    img.draw_line(ox, ry - 6, ox, ry + rh + 6, image.Color.from_rgb(255, 220, 80), 1)

    if set_mode:
        sx = mm_to_roi_x(sp_mm, roi)
        col_sp = image.Color.from_rgb(255, 64, 180)
        img.draw_line(sx, ry - 10, sx, ry + rh + 10, col_sp, 2)
        img.draw_circle(sx, ry + rh // 2, 5, col_sp, -1)

        dsx, dsy, dsw, dsh = drag_strip
        bar_bg = image.Color.from_rgb(36, 36, 48)
        bar_fg = col_sp if dragging else image.Color.from_rgb(140, 140, 160)
        img.draw_rect(dsx, dsy, dsw, dsh, bar_bg, -1)
        img.draw_rect(dsx, dsy, dsw, dsh, bar_fg, 1)
        img.draw_rect(max(dsx, sx - 4), dsy, 8, dsh, col_sp, -1)
        img.draw_string(
            dsx + 4, dsy + 4,
            "Drag to set",
            image.Color.from_rgb(200, 200, 210),
            scale=0.9,
        )
    else:
        sx = mm_to_roi_x(sp_mm, roi)
        img.draw_line(
            sx, ry, sx, ry + rh,
            image.Color.from_rgb(160, 80, 140), 1,
        )

    if usable and det.get("found"):
        img.draw_circle(int(det["cx"]), int(det["cy"]), 3, image.COLOR_GREEN, -1)

    mode_tag = " SET" if set_mode else ""
    if usable:
        txt = f"Ball {pos_mm_i:+d} mm   SP {sp_mm:+d} mm{mode_tag}"
    else:
        txt = f"Ball ----   SP {sp_mm:+d} mm{mode_tag}"
    img.draw_string(6, 4, txt, image.COLOR_WHITE, scale=1.1)

    # Top-right live FPS
    fps_txt = f"{fps:4.1f} FPS"
    fps_x = max(6, img.width() - 92)
    img.draw_string(
        fps_x, 4, fps_txt,
        image.Color.from_rgb(120, 200, 255), scale=1.1,
    )

    draw_btn(img, exit_btn, "Exit", danger=True)
    draw_btn(img, set_btn, "Done" if set_mode else "Set", active=set_mode)
    draw_btn(img, reset_btn, "Reset")


def main():
    disp = setup_display()
    show_boot(disp, "Ball control...")

    serial_dev = setup_uart_safe()
    cam = camera.Camera(CAM_W, CAM_H, image.Format.FMT_BGR888)
    print("[ball] camera", cam.width(), "x", cam.height(), "bar", BAR_LEN_MM, "mm")
    print("[ball] display", disp.width(), "x", disp.height())

    ts = touchscreen.TouchScreen()

    roi = [ROI_X, ROI_Y, ROI_W, ROI_H]
    mode = DETECT_MODE
    pos_filt = 0.0
    pos_mm_i = 0
    has_filt = False
    has_mm_i = False
    last_tx_ms = 0
    sp_mm = 0
    sp_pending = True          # 上电只同步一次 sp=0
    set_mode = False           # 默认不开启设目标/拖动
    dragging_sp = False
    pressed_last = False
    err_cnt = 0
    last_log_ms = 0

    if serial_dev is not None:
        if uart_write(serial_dev, pack_setpoint_frame(sp_mm)):
            sp_pending = False
            print("[ball] SP sync 0 mm")

    print("[ball] UI EN: Exit | Set/Done | Reset")

    while not app.need_exit():
        try:
            img = cam.read()
            iw, ih = img.width(), img.height()
            bgr = image.image2cv(img, ensure_bgr=False, copy=False)

            roi[0], roi[1], roi[2], roi[3] = ROI_X, ROI_Y, ROI_W, ROI_H
            exit_btn, set_btn, reset_btn = layout_controls(iw, ih)
            drag_strip = layout_drag_strip(iw, ih, roi, exit_btn)
            det = detect_ball(bgr, roi, mode)

            usable = bool(det["found"]) and int(det["conf"]) >= CONF_MIN
            if usable:
                if not has_filt:
                    pos_filt = float(det["pos_mm"])
                    has_filt = True
                else:
                    pos_filt = POS_ALPHA * float(det["pos_mm"]) + (1.0 - POS_ALPHA) * pos_filt
                pos_mm_i = quantize_mm(pos_filt, pos_mm_i, has_mm_i)
                has_mm_i = True
            else:
                has_filt = False
                has_mm_i = False
                pos_filt = 0.0
                pos_mm_i = 0

            tx, ty, pressed = ts.read()
            mx, my = map_touch_to_image(tx, ty, iw, ih, disp.width(), disp.height())

            if pressed and not pressed_last:
                if is_in_button(mx, my, exit_btn):
                    print("[ball] Exit @", mx, my)
                    app.set_exit_flag(True)
                elif is_in_button(mx, my, set_btn):
                    set_mode = not set_mode
                    dragging_sp = False
                    print("[ball] Set mode ->", set_mode)
                elif is_in_button(mx, my, reset_btn):
                    sp_mm = 0
                    sp_pending = True
                    set_mode = False
                    dragging_sp = False
                    if serial_dev is not None:
                        uart_write(serial_dev, pack_setpoint_frame(0))
                        sp_pending = False
                    print("[ball] Reset SP=0")
                elif set_mode and hit_drag_zone(mx, my, roi, drag_strip):
                    dragging_sp = True
                    sp_mm = roi_x_to_mm(mx, roi)
                    sp_pending = True
                    if serial_dev is not None:
                        if uart_write(serial_dev, pack_setpoint_frame(sp_mm)):
                            sp_pending = False
            elif pressed and set_mode and dragging_sp:
                new_sp = roi_x_to_mm(mx, roi)
                if new_sp != sp_mm:
                    sp_mm = new_sp
                    sp_pending = True
                    if serial_dev is not None:
                        if uart_write(serial_dev, pack_setpoint_frame(sp_mm)):
                            sp_pending = False
            else:
                if dragging_sp:
                    sp_pending = True
                dragging_sp = False
            pressed_last = pressed

            fps = time.fps()
            draw_ui(
                img, roi, det, pos_mm_i, sp_mm, usable,
                exit_btn, set_btn, reset_btn, drag_strip,
                set_mode, dragging_sp, fps,
            )
            disp.show(img)

            now_ms = time.ticks_ms()
            if serial_dev is not None and (now_ms - last_tx_ms) >= TX_MIN_MS:
                if sp_pending:
                    if uart_write(serial_dev, pack_setpoint_frame(sp_mm)):
                        sp_pending = False
                frame = pack_ball_frame(
                    usable,
                    pos_mm_i,
                    det["cx"] if usable else 0,
                    det["cy"] if usable else 0,
                    det["conf"] if det["found"] else 0,
                    mode,
                )
                uart_write(serial_dev, frame)
                last_tx_ms = now_ms

            if (now_ms - last_log_ms) >= 1000:
                last_log_ms = now_ms
                print(
                    "[ball] pos=%+d sp=%+d set=%d fps=%.1f"
                    % (pos_mm_i, sp_mm, int(set_mode), fps)
                )
        except Exception as e:
            err_cnt += 1
            print("[ball] loop err:", e)
            time.sleep_ms(200)
            if err_cnt > 30:
                break

    if serial_dev is not None:
        uart_write(serial_dev, pack_ball_frame(False, 0, 0, 0, 0, mode))
    print("[ball] exit")


if __name__ == "__main__":
    main()
