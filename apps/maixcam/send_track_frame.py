"""
PC 串口 → 云台 UART0：发送跟踪测试帧（与 track_proto.h 一致）

用法:
  pip install pyserial
  python send_track_frame.py COM5

帧 15B:
  AA 55 | 01 | flags | err_x(i16LE) | err_y(i16LE)
  | pitch×100 | roll×100 | yaw×100 | checksum
"""

import argparse
import struct
import time

try:
    import serial
except ImportError:
    raise SystemExit("need: pip install pyserial")


def build_frame(found: int, err_x: int, err_y: int,
                pitch: float = 0.0, roll: float = 0.0, yaw: float = 0.0) -> bytes:
    body = bytearray()
    body.append(0x01)  # type
    body.append(0x01 if found else 0x00)
    body += struct.pack("<hh", int(err_x), int(err_y))
    body += struct.pack("<hhh",
                        int(round(pitch * 100)),
                        int(round(roll * 100)),
                        int(round(yaw * 100)))
    csum = sum(body) & 0xFF
    return bytes([0xAA, 0x55]) + bytes(body) + bytes([csum])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="serial port, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hz", type=float, default=20.0, help="frame rate")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    print(f"open {args.port} @ {args.baud}, {args.hz} Hz")
    print("sequence: found=1, err_x sweep ±40, err_y=0")

    t0 = time.time()
    period = 1.0 / args.hz
    n = 0
    try:
        while True:
            # 正弦扫 err_x，方便看解析是否在变
            phase = (time.time() - t0) * 0.5
            err_x = int(40 * __import__("math").sin(phase))
            err_y = 0
            frame = build_frame(1, err_x, err_y, pitch=0.0, yaw=0.0)
            ser.write(frame)
            n += 1
            if n % 20 == 0:
                print(f"sent #{n} ex={err_x}  {frame.hex(' ')}")
            time.sleep(period)
    except KeyboardInterrupt:
        print("\nstop")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
