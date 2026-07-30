# OpenMV H7 → ESP32 UART JPEG stream (QVGA, stable)
# Wire: P4(TX)->ESP GPIO16(RX), P5(RX)<-ESP GPIO17(TX optional), GND-GND
# Protocol: 0xAA 0x55 | len_le32 | jpeg_bytes

import sensor
import time
from pyb import UART

UART_ID = 3
BAUD = 460800
JPEG_QUALITY = 35
FRAME_MS = 30
MAX_JPEG = 24 * 1024
LOG_EVERY = 10

MAGIC = b"\xaa\x55"


def jpeg_bytes(img):
    try:
        img.compress(quality=JPEG_QUALITY)
        try:
            return img.bytearray()
        except AttributeError:
            return bytes(img)
    except Exception:
        pass
    try:
        c = img.compressed(quality=JPEG_QUALITY)
        try:
            return c.bytearray()
        except AttributeError:
            return bytes(c)
    except Exception as e:
        print("jpeg fail:", e)
        return None


def send_frame(uart, raw):
    n = len(raw)
    if n < 1 or n > MAX_JPEG:
        return False
    if n < 2 or raw[0] != 0xFF or raw[1] != 0xD8:
        print("not jpeg soi")
        return False
    uart.write(MAGIC)
    uart.write(n.to_bytes(4, "little"))
    mv = memoryview(raw)
    off = 0
    while off < n:
        wrote = uart.write(mv[off : off + 1024])
        if not wrote:
            break
        off += wrote
    return off == n


def main():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time=2000)

    uart = UART(UART_ID, BAUD)
    try:
        uart.init(BAUD, bits=8, parity=None, stop=1, timeout_char=10)
    except TypeError:
        pass

    try:
        while uart.any():
            uart.read(64)
    except Exception:
        pass

    clock = time.clock()
    n = 0
    print(
        "openmv QVGA q=%d baud=%d frame_ms=%d max_jpg=%d"
        % (JPEG_QUALITY, BAUD, FRAME_MS, MAX_JPEG)
    )

    while True:
        clock.tick()
        img = sensor.snapshot()
        raw = jpeg_bytes(img)
        if raw is None:
            time.sleep_ms(FRAME_MS)
            continue
        ok = send_frame(uart, raw)
        n += 1
        if n % LOG_EVERY == 0:
            print("fps=%.1f jpg=%d ok=%s" % (clock.fps(), len(raw), ok))
        time.sleep_ms(FRAME_MS)


if __name__ == "__main__":
    main()
