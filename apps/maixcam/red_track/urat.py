"""
MaixCAM / MaixCAM-Pro：读取 IMU，屏幕显示，触摸计算欧拉角

操作：
  - 屏幕实时显示加速度 / 角速度 / 温度
  - 点「计算欧拉角」按钮：用当前姿态解算结果冻结显示 pitch/roll/yaw
  - 点「校准」按钮：静止校准陀螺仪 10 秒
  - 点「Exit」退出

接线（USB 转 TTL，可选）：
  板子 A16 (TX) -> 模块 RX
  板子 A17 (RX) -> 模块 TX
  板子 GND      -> 模块 GND
  电平 3.3V，不要接 5V

说明：
  - IMU 默认识别板载 QMI8658（无磁力计，yaw 会有漂移）
  - 欧拉角使用 maix.ahrs.MahonyAHRS 互补滤波
  - 首次运行会校准陀螺仪：请保持板子水平静止约 10 秒
"""

from maix import pinmap, uart, sys, err, time, app, image, display, touchscreen, ahrs
from maix.ext_dev import imu
import math


# 是否同时把数据发到串口（电脑串口助手）
ENABLE_UART = False

# Mahony PI 参数：P 越大响应越快但易超调；I 越大稳态误差消得快但可能漂
AHRS_KP = 2.0
AHRS_KI = 0.01


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
        # maixcam / maixcam-pro
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
            size=24,
        )
        image.set_default_font("sourcehansans")
        print("font loaded: sourcehansans")
    except Exception as e:
        print("load font failed, use default:", e)

    print(f"display: {disp.width()}x{disp.height()}")
    return disp


def is_in_button(x, y, btn_pos):
    return (
        x > btn_pos[0]
        and x < btn_pos[0] + btn_pos[2]
        and y > btn_pos[1]
        and y < btn_pos[1] + btn_pos[3]
    )


def show_msg(disp, msg):
    img = image.Image(disp.width(), disp.height())
    img.draw_rect(
        0, 0, disp.width(), disp.height(),
        color=image.Color.from_rgb(0, 0, 0),
        thickness=-1,
    )
    # 简单多行居中
    lines = msg.split("\n")
    line_h = 36
    total_h = line_h * len(lines)
    y = (disp.height() - total_h) // 2
    for line in lines:
        size = image.string_size(line, scale=1.4)
        x = (disp.width() - size.width()) // 2
        img.draw_string(x, y, line, image.COLOR_WHITE, scale=1.4)
        y += line_h
    disp.show(img)


def make_buttons(disp):
    """底部三个按钮：Exit / 计算欧拉角 / 校准"""
    btn_h = 52
    gap = 8
    y = disp.height() - btn_h - 8
    # 三等分
    total_w = disp.width() - gap * 4
    btn_w = total_w // 3

    exit_btn = [gap, y, btn_w, btn_h]
    calc_btn = [gap * 2 + btn_w, y, btn_w, btn_h]
    calib_btn = [gap * 3 + btn_w * 2, y, btn_w, btn_h]
    return exit_btn, calc_btn, calib_btn


def draw_button(img, btn, label, active=False):
    color = image.Color.from_rgb(80, 180, 255) if active else image.COLOR_WHITE
    fill = image.Color.from_rgb(30, 70, 120) if active else image.Color.from_rgb(28, 32, 48)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=fill, thickness=-1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=color, thickness=2)
    size = image.string_size(label, scale=1.2)
    tx = btn[0] + (btn[2] - size.width()) // 2
    ty = btn[1] + (btn[3] - size.height()) // 2
    img.draw_string(tx, ty, label, color=color, scale=1.2)


def euler_from_acc(ax, ay, az):
    """仅用加速度计估算 pitch/roll（单位：度）。无磁力计时 yaw 无法单帧得到。"""
    # pitch: 绕 X；roll: 绕 Y
    pitch = math.degrees(math.atan2(ay, math.sqrt(ax * ax + az * az)))
    roll = math.degrees(math.atan2(-ax, az if abs(az) > 1e-6 else 1e-6))
    return pitch, roll


def draw_screen(
    disp,
    ax, ay, az, gx, gy, gz, temp,
    live_pitch, live_roll, live_yaw,
    snap_pitch, snap_roll, snap_yaw, has_snap,
    exit_btn, calc_btn, calib_btn,
    calc_pressed=False,
    fps=0.0,
):
    img = image.Image(disp.width(), disp.height())

    # 背景
    img.draw_rect(
        0, 0, disp.width(), disp.height(),
        color=image.Color.from_rgb(10, 14, 28),
        thickness=-1,
    )

    # 标题
    img.draw_rect(
        0, 0, disp.width(), 44,
        color=image.Color.from_rgb(24, 48, 96),
        thickness=-1,
    )
    img.draw_string(12, 10, "IMU + Euler", image.COLOR_WHITE, scale=1.5)
    if fps > 0:
        img.draw_string(
            disp.width() - 110, 14, f"{fps:4.1f}FPS",
            image.Color.from_rgb(160, 200, 255), scale=1.2,
        )

    white = image.COLOR_WHITE
    cyan = image.Color.from_rgb(80, 220, 255)
    orange = image.Color.from_rgb(255, 170, 60)
    green = image.Color.from_rgb(90, 230, 140)
    yellow = image.Color.from_rgb(255, 220, 80)
    gray = image.Color.from_rgb(150, 160, 180)

    y = 56
    scale = 1.25
    line_h = 28

    # 原始 IMU
    img.draw_string(12, y, "ACC(g)", color=cyan, scale=scale)
    img.draw_string(
        120, y,
        f"{ax:6.2f} {ay:6.2f} {az:6.2f}",
        color=white, scale=scale,
    )
    y += line_h
    img.draw_string(12, y, "GYRO", color=orange, scale=scale)
    img.draw_string(
        120, y,
        f"{gx:6.1f} {gy:6.1f} {gz:6.1f}",
        color=white, scale=scale,
    )
    y += line_h
    img.draw_string(12, y, f"TEMP {temp:5.1f}C", color=green, scale=scale)

    # 实时欧拉角（Mahony 持续解算）
    y += line_h + 8
    img.draw_string(12, y, "Live Euler (Mahony)", color=cyan, scale=scale)
    y += line_h
    img.draw_string(
        20, y,
        f"P:{live_pitch:7.2f}  R:{live_roll:7.2f}  Y:{live_yaw:7.2f}",
        color=white, scale=scale,
    )

    # 点击冻结的结果
    y += line_h + 10
    img.draw_rect(
        10, y - 6, disp.width() - 20, line_h * 3 + 20,
        color=image.Color.from_rgb(20, 36, 60),
        thickness=-1,
    )
    img.draw_rect(
        10, y - 6, disp.width() - 20, line_h * 3 + 20,
        color=yellow if has_snap else gray,
        thickness=2,
    )
    img.draw_string(20, y, "Snap Euler (touch Calc)", color=yellow, scale=scale)
    y += line_h
    if has_snap:
        img.draw_string(20, y, f"Pitch: {snap_pitch:8.2f} deg", color=white, scale=scale)
        y += line_h
        img.draw_string(20, y, f"Roll : {snap_roll:8.2f} deg", color=white, scale=scale)
        y += line_h
        img.draw_string(20, y, f"Yaw  : {snap_yaw:8.2f} deg", color=white, scale=scale)
    else:
        img.draw_string(20, y, "Tap [Calc] to freeze", color=gray, scale=scale)
        y += line_h
        img.draw_string(20, y, "current Euler angles", color=gray, scale=scale)

    # 按钮
    draw_button(img, exit_btn, "Exit")
    draw_button(img, calc_btn, "Calc", active=calc_pressed)
    draw_button(img, calib_btn, "Calib")

    disp.show(img)


def main():
    serial_dev = setup_uart() if ENABLE_UART else None
    sensor = setup_imu()
    disp = setup_display()
    ts = touchscreen.TouchScreen()
    ahrs_filter = ahrs.MahonyAHRS(AHRS_KP, AHRS_KI)

    exit_btn, calc_btn, calib_btn = make_buttons(disp)

    if serial_dev is not None:
        serial_dev.write_str("IMU UART ready\r\n")

    print("开始读取 IMU，触摸 [Calc] 可冻结当前欧拉角...")

    # 点击边沿检测
    pressed_last = False
    has_snap = False
    snap_pitch = snap_roll = snap_yaw = 0.0
    calc_flash_until = 0

    last_time = time.ticks_s()

    while not app.need_exit():
        # 触摸
        tx, ty, pressed = ts.read()

        # IMU：姿态解算需要弧度制角速度
        data = sensor.read_all(calib_gryo=True, radian=True)
        ax, ay, az = data.acc.x, data.acc.y, data.acc.z
        # 显示用 dps
        gx_dps = data.gyro.x * ahrs.RAD2DEG if hasattr(ahrs, "RAD2DEG") else data.gyro.x * (180.0 / math.pi)
        gy_dps = data.gyro.y * ahrs.RAD2DEG if hasattr(ahrs, "RAD2DEG") else data.gyro.y * (180.0 / math.pi)
        gz_dps = data.gyro.z * ahrs.RAD2DEG if hasattr(ahrs, "RAD2DEG") else data.gyro.z * (180.0 / math.pi)
        temp = data.temp

        now = time.ticks_s()
        dt = now - last_time
        if dt <= 0:
            dt = 0.001
        last_time = now

        # 持续 Mahony 解算（背景一直更新，点一下再冻结）
        angle = ahrs_filter.get_angle(data.acc, data.gyro, data.mag, dt, radian=False)
        live_pitch, live_roll, live_yaw = angle.x, angle.y, angle.z

        # 触摸点击（按下边沿）
        if pressed and not pressed_last:
            if is_in_button(tx, ty, exit_btn):
                app.set_exit_flag(True)
            elif is_in_button(tx, ty, calc_btn):
                # 用当前实时解算结果冻结显示
                snap_pitch, snap_roll, snap_yaw = live_pitch, live_roll, live_yaw
                # 同时给一份纯加速度估算，方便对照（可选打印）
                acc_p, acc_r = euler_from_acc(ax, ay, az)
                has_snap = True
                calc_flash_until = now + 0.25
                print(
                    f"[Calc] Mahony  Pitch:{snap_pitch:7.2f} Roll:{snap_roll:7.2f} Yaw:{snap_yaw:7.2f} | "
                    f"AccOnly Pitch:{acc_p:7.2f} Roll:{acc_r:7.2f}"
                )
                if serial_dev is not None:
                    serial_dev.write_str(
                        f"EULER:{snap_pitch:.2f},{snap_roll:.2f},{snap_yaw:.2f}\r\n"
                    )
            elif is_in_button(tx, ty, calib_btn):
                for i in range(3, 0, -1):
                    show_msg(disp, f"Keep still\nStart in {i}s")
                    time.sleep(1)
                show_msg(disp, "Calibrating...\nDon't move 10s")
                sensor.calib_gyro(10000)
                ahrs_filter.reset()
                last_time = time.ticks_s()
                has_snap = False
                print("校准完成，AHRS 已复位")

        pressed_last = pressed

        fps = time.fps()
        calc_pressed = now < calc_flash_until

        draw_screen(
            disp,
            ax, ay, az, gx_dps, gy_dps, gz_dps, temp,
            live_pitch, live_roll, live_yaw,
            snap_pitch, snap_roll, snap_yaw, has_snap,
            exit_btn, calc_btn, calib_btn,
            calc_pressed=calc_pressed,
            fps=fps,
        )

        # 控制台
        print(
            f"ACC {ax:6.2f},{ay:6.2f},{az:6.2f} | "
            f"EUL {live_pitch:7.2f},{live_roll:7.2f},{live_yaw:7.2f} | "
            f"{temp:.1f}C | {fps:.1f}FPS"
        )

        if serial_dev is not None:
            serial_dev.write_str(
                f"ACC:{ax:.2f},{ay:.2f},{az:.2f} "
                f"GYRO:{gx_dps:.2f},{gy_dps:.2f},{gz_dps:.2f} "
                f"EUL:{live_pitch:.2f},{live_roll:.2f},{live_yaw:.2f} "
                f"TEMP:{temp:.1f}\r\n"
            )

        # 循环尽量快一点，AHRS 更稳；略 sleep 降低 CPU
        time.sleep_ms(10)


if __name__ == "__main__":
    main()
