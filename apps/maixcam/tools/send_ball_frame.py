"""
PC 串口 → MCU：发送钢珠轨道测试帧（与 detect_ball pack_ball_frame 一致）

  python send_ball_frame.py COM5

帧：
  AA 55 | type=0x02 | flags | s i16LE | conf u8 | cx i16LE | cy i16LE | csum
"""

from __future__ import annotations

import argparse
import math
import struct
import time

try:
    import serial
except ImportError as e:
    raise SystemExit("pip install pyserial") from e


def build_frame(found: int, s: int, conf: int, cx: int, cy: int) -> bytes:
    flags = 0x01 if found else 0x00

    def i16(v: int) -> int:
        v = int(v)
        return max(-32768, min(32767, v))

    c = max(0, min(255, int(conf)))
    body = struct.pack("<BBhBhh", 0x02, flags, i16(s), c, i16(cx), i16(cy))
    csum = sum(body) & 0xFF
    return bytes([0xAA, 0x55]) + body + bytes([csum])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port", help="e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hz", type=float, default=30.0)
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    print("open", args.port, args.baud)
    print("sweep s = 80*sin, found=1")
    n = 0
    t0 = time.time()
    try:
        while True:
            phase = (time.time() - t0) * 1.2
            s = int(80 * math.sin(phase))
            cx = 160 + s
            cy = 120
            frame = build_frame(1, s, 90, cx, cy)
            ser.write(frame)
            n += 1
            if n % 30 == 0:
                print("sent #{} s={} {}".format(n, s, frame.hex(" ")))
            time.sleep(1.0 / max(args.hz, 1.0))
    except KeyboardInterrupt:
        ser.write(build_frame(0, 0, 0, 0, 0))
        print("stop")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
