#!/usr/bin/env python3
"""PC → balance VISION_UART：发送停球定点 type=0x12（单位整 mm）

  python send_ball_setpoint.py COM5 0
  python send_ball_setpoint.py COM5 50
  python send_ball_setpoint.py COM5 -12
"""
import struct
import sys

try:
    import serial
except ImportError:
    print("pip install pyserial")
    sys.exit(1)


def pack_setpoint(target_mm: float) -> bytes:
    pos = int(round(target_mm))
    pos = max(-32768, min(32767, pos))
    body = struct.pack("<BBh", 0x12, 0x00, pos) + bytes(6)
    assert len(body) == 10
    return bytes([0xAA, 0x55]) + body + bytes([sum(body) & 0xFF])


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    port = sys.argv[1]
    mm = float(sys.argv[2])
    frame = pack_setpoint(mm)
    ser = serial.Serial(port, 115200, timeout=0.2)
    ser.write(frame)
    ser.flush()
    print(f"sent setpoint {int(round(mm)):+d} mm  frame={frame.hex()}")
    ser.close()


if __name__ == "__main__":
    main()
