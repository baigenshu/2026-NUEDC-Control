# OpenMV H7 → ESP32 UART JPEG stream
# Wire: P4(TX)->ESP GPIO16(RX), P5(RX)<-ESP GPIO17(TX), GND-GND
# Protocol: 0xAA 0x55 | len_le32 | jpeg_bytes

import sensor
import time
from pyb import UART

UART_ID = 3
BAUD = 921600
JPEG_QUALITY = 35
FRAME_MS = 80
MAX_JPEG = 16 * 1024

MAGIC = b"\xaa\x55"


def jpeg_bytes(img):
    # Prefer in-place compress; fall back to compressed() copy.
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
    uart.write(MAGIC)
    uart.write(n.to_bytes(4, "little"))
    # chunk large writes for older firmwares
    mv = memoryview(raw)
    off = 0
    while off < n:
        off += uart.write(mv[off : off + 512])
    return True


def main():
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QQVGA)
    sensor.skip_frames(time=2000)

    uart = UART(UART_ID, BAUD)
    try:
        uart.init(BAUD, bits=8, parity=None, stop=1, timeout_char=10)
    except TypeError:
        pass

    clock = time.clock()
    print("openmv uart stream ready baud=%d" % BAUD)

    while True:
        clock.tick()
        img = sensor.snapshot()
        raw = jpeg_bytes(img)
        if raw is None:
            time.sleep_ms(FRAME_MS)
            continue
        ok = send_frame(uart, raw)
        print("fps=%.1f jpg=%d ok=%s" % (clock.fps(), len(raw), ok))
        time.sleep_ms(FRAME_MS)


if __name__ == "__main__":
    main()
