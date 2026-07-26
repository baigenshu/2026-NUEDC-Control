"""
MaixCAM：红色目标检测 + IMU，二进制帧发给云台主控

功能：
  - 摄像头找最大红色色块，计算相对画面中心误差 err_x/err_y
  - 板载 IMU + Mahony 解算 pitch/roll/yaw
  - UART 115200 持续发送 15 字节跟踪帧给云台 MCU
  - 屏幕叠加目标框、中心十字、误差与姿态

接线（3.3V，勿接 5V）：
  MaixCAM A16 (TX) -> MSP PA31 (UART0 RX)
  MaixCAM A17 (RX) -> MSP PA28 (UART0 TX)  可选
  GND 共地

帧格式（小端）：
  AA 55 | type=0x01 | flags(found) | err_x i16 | err_y i16
        | pitch i16×100 | roll i16×100 | yaw i16×100 | checksum
  checksum = sum(bytes[2..13]) & 0xFF
"""

from maix import pinmap, uart, sys, err, time, app, image, display, touchscreen, ahrs, camera
from maix.ext_dev import imu
import struct


ENABLE_UART = True
AHRS_KP = 2.0
AHRS_KI = 0.01

# 摄像头分辨率（越小越快）
CAM_W = 320
CAM_H = 240

# 红色 LAB 阈值（启动后按实景微调）
RED_L_MIN, RED_L_MAX = 20, 80
RED_A_MIN, RED_A_MAX = 40, 80      # a* 偏红
RED_B_MIN, RED_B_MAX = -10, 50
MIN_BLOB_PIXELS = 80

# 发送节流：至少间隔 ms（约 30Hz）
TX_MIN_MS = 33


def setup_uart():
    device_id = sys.device_id()
    print("device_id:", device_id)
    print("uart devices:", uart.list_devices())

    if device_id == "maixcam2":
        pin_function = {
            "A21": "UART4_TX",
            "A22": "UART4_RX",
        }
        device = "/dev/ttyS4"
    else:
        pin_function = {
            "A16": "UART0_TX",
            "A17": "UART0_RX",
        }
        device = "/dev/ttyS0"

    for pin, func in pin_function.items():
        ret = pinmap.set_pin_function(pin, func)
        err.check_raise(ret, f"Failed set pin {pin} -> {func}")
        print(f"pinmap: {pin} -> {func}")

    baudrate = 115200
    serial_dev = uart.UART(device, baudrate)
    print(f"UART opened: {device}, baud={baudrate}")
    return serial_dev


def setup_imu():
    sensor = imu.IMU(
        "default",
        mode=imu.Mode.DUAL,
        acc_scale=imu.AccScale.ACC_SCALE_2G,
        acc_odr=imu.AccOdr.ACC_ODR_1000,
        gyro_scale=imu.GyroScale.GYRO_SCALE_256DPS,
        gyro_odr=imu.GyroOdr.GYRO_ODR_8000,
    )

    force_calibrate = False
    if force_calibrate or not sensor.calib_gyro_exists():
        print("正在进行陀螺仪校准，请保持设备绝对静止 10 秒...")
        sensor.calib_gyro(10000)
        print("校准完成！")
    else:
        sensor.load_calib_gyro()
        print("历史校准数据加载成功！")

    return sensor


def setup_display():
    disp = display.Display()
    try:
        image.load_font(
            "sourcehansans",
            "/maixapp/share/font/SourceHanSansCN-Regular.otf",
            size=20,
        )
        image.set_default_font("sourcehansans")
    except Exception as e:
        print("load font failed, use default:", e)
    print(f"display: {disp.width()}x{disp.height()}")
    return disp


def setup_camera():
    cam = camera.Camera(CAM_W, CAM_H)
    print(f"camera: {cam.width()}x{cam.height()}")
    return cam


def is_in_button(x, y, btn_pos):
    return (
        x > btn_pos[0]
        and x < btn_pos[0] + btn_pos[2]
        and y > btn_pos[1]
        and y < btn_pos[1] + btn_pos[3]
    )


def draw_button(img, btn, label):
    fill = image.Color.from_rgb(28, 32, 48)
    color = image.COLOR_WHITE
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=fill, thickness=-1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=color, thickness=2)
    size = image.string_size(label, scale=1.1)
    tx = btn[0] + (btn[2] - size.width()) // 2
    ty = btn[1] + (btn[3] - size.height()) // 2
    img.draw_string(tx, ty, label, color=color, scale=1.1)


def pack_track_frame(found, err_x, err_y, pitch, roll, yaw):
    """15 字节二进制帧，与 gimbal track_proto 一致。"""
    flags = 0x01 if found else 0x00
    # 限幅到 int16
    def i16(v):
        v = int(round(v))
        if v > 32767:
            v = 32767
        if v < -32768:
            v = -32768
        return v

    p = i16(pitch * 100)
    r = i16(roll * 100)
    y = i16(yaw * 100)
    ex = i16(err_x)
    ey = i16(err_y)

    # type, flags, err_x, err_y, pitch, roll, yaw  → 12 字节
    body = struct.pack("<BBhhhhh", 0x01, flags, ex, ey, p, r, y)
    checksum = sum(body) & 0xFF
    return bytes([0xAA, 0x55]) + body + bytes([checksum])


def detect_red(img):
    """返回 (found, cx, cy, rect) rect=(x,y,w,h) 或 None。
    优先用 find_blobs：LAB 阈值 + 面积过滤，取最大连通域。
    """
    thresholds = [
        (RED_L_MIN, RED_L_MAX, RED_A_MIN, RED_A_MAX, RED_B_MIN, RED_B_MAX)
    ]
    blobs = img.find_blobs(
        thresholds,
        pixels_threshold=MIN_BLOB_PIXELS,
        area_threshold=MIN_BLOB_PIXELS,
        merge=True,
    )
    if not blobs:
        return False, 0, 0, None

    best = max(blobs, key=lambda b: b.area())
    if best.area() < MIN_BLOB_PIXELS:
        return False, 0, 0, None

    cx = best.cx()
    cy = best.cy()
    rect = (best.x(), best.y(), best.w(), best.h())
    return True, cx, cy, rect


def draw_track_overlay(img, found, cx, cy, rect, err_x, err_y,
                       pitch, roll, yaw):
    """在相机画面上叠加 HUD：中心十字、目标框、误差线、姿态、FPS。"""
    w = img.width()
    h = img.height()
    cx0 = w // 2
    cy0 = h // 2

    # 中心十字
    img.draw_line(cx0 - 14, cy0, cx0 + 14, cy0, image.COLOR_WHITE, 1)
    img.draw_line(cx0, cy0 - 14, cx0, cy0 + 14, image.COLOR_WHITE, 1)

    # 目标框 + 圆心 + 误差线
    if found and rect is not None:
        x, y, bw, bh = rect
        img.draw_rect(x, y, bw, bh, image.Color.from_rgb(0, 255, 0), 2)
        img.draw_circle(cx, cy, 4, image.Color.from_rgb(0, 255, 0), -1)
        img.draw_line(cx0, cy0, cx, cy,
                      image.Color.from_rgb(255, 80, 80), 2)

    # 状态行
    status = "LOCK" if found else "----"
    img.draw_string(
        6, 6,
        f"{status} ex={err_x:4d} ey={err_y:4d}",
        image.COLOR_WHITE, scale=1.2,
    )
    # 姿态行
    img.draw_string(
        6, 28,
        f"P{pitch:6.1f} R{roll:6.1f} Y{yaw:6.1f}",
        image.Color.from_rgb(80, 220, 255),
        scale=1.1,
    )
    # FPS
    fps = time.fps()
    img.draw_string(
        w - 90, 6, f"{fps:4.1f}FPS",
        image.Color.from_rgb(160, 200, 255), scale=1.1,
    )

    # 底部按钮
    btn_h = 40
    gap = 6
    yb = h - btn_h - 4
    tw = (w - gap * 3) // 2
    exit_btn = [gap, yb, tw, btn_h]
    calib_btn = [gap * 2 + tw, yb, tw, btn_h]
    draw_button(img, exit_btn, "Exit")
    draw_button(img, calib_btn, "Calib")

    return exit_btn, calib_btn


def show_msg(disp, msg):
    img = image.Image(disp.width(), disp.height())
    img.draw_rect(
        0, 0, disp.width(), disp.height(),
        color=image.Color.from_rgb(0, 0, 0),
        thickness=-1,
    )
    lines = msg.split("\n")
    line_h = 36
    y = (disp.height() - line_h * len(lines)) // 2
    for line in lines:
        size = image.string_size(line, scale=1.4)
        x = (disp.width() - size.width()) // 2
        img.draw_string(x, y, line, image.COLOR_WHITE, scale=1.4)
        y += line_h
    disp.show(img)


def main():
    serial_dev = setup_uart() if ENABLE_UART else None
    sensor = setup_imu()
    disp = setup_display()
    cam = setup_camera()
    ts = touchscreen.TouchScreen()
    ahrs_filter = ahrs.MahonyAHRS(AHRS_KP, AHRS_KI)

    pressed_last = False
    last_time = time.ticks_s()
    last_tx_ms = 0

    cx0 = cam.width() // 2
    cy0 = cam.height() // 2

    if serial_dev is not None:
        # 文本握手一行，随后二进制
        serial_dev.write_str("TRACK ready\r\n")

    print("红目标跟踪启动：检测 + IMU → UART 二进制帧")

    while not app.need_exit():
        tx, ty, pressed = ts.read()

        # ---- IMU ----
        data = sensor.read_all(calib_gryo=True, radian=True)
        now = time.ticks_s()
        dt = now - last_time
        if dt <= 0:
            dt = 0.001
        last_time = now

        angle = ahrs_filter.get_angle(data.acc, data.gyro, data.mag, dt, radian=False)
        live_pitch, live_roll, live_yaw = angle.x, angle.y, angle.z

        # ---- 视觉 ----
        img = cam.read()
        found, cx, cy, rect = detect_red(img)
        err_x = (cx - cx0) if found else 0
        err_y = (cy0 - cy) if found else 0  # 上正下负（相对图像 y 取反）

        # ---- 叠加 HUD ----
        exit_cam, calib_cam = draw_track_overlay(
            img, found, cx, cy, rect, err_x, err_y,
            live_pitch, live_roll, live_yaw,
        )

        disp.show(img)

        # ---- 触摸 ----
        if pressed and not pressed_last:
            if is_in_button(tx, ty, exit_cam):
                app.set_exit_flag(True)
            elif is_in_button(tx, ty, calib_cam):
                for i in range(3, 0, -1):
                    show_msg(disp, f"Keep still\nStart in {i}s")
                    time.sleep(1)
                show_msg(disp, "Calibrating...\nDon't move 10s")
                sensor.calib_gyro(10000)
                ahrs_filter.reset()
                last_time = time.ticks_s()
                print("校准完成，AHRS 已复位")
        pressed_last = pressed

        # ---- UART 二进制帧 ----
        now_ms = time.ticks_ms()
        if serial_dev is not None and (now_ms - last_tx_ms) >= TX_MIN_MS:
            frame = pack_track_frame(
                found, err_x, err_y, live_pitch, live_roll, live_yaw
            )
            serial_dev.write(frame)
            last_tx_ms = now_ms

    if serial_dev is not None:
        # 退出前发 found=0，让云台停
        serial_dev.write(pack_track_frame(False, 0, 0, 0, 0, 0))


if __name__ == "__main__":
    main()