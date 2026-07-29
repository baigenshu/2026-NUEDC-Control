"""Minimal AVI+MJPEG writer for MaixCAM (VLC-friendly)."""

import os
import struct


class AviMjpegWriter:
    def __init__(self):
        self._f = None
        self._path = ""
        self._w = 0
        self._h = 0
        self._fps = 15
        self._frames = 0
        self._movi_size = 4  # "movi"
        self._pos_riff = 0
        self._pos_frames = 0
        self._pos_strh_len = 0
        self._pos_movi_list = 0

    @property
    def path(self):
        return self._path

    @property
    def frames(self):
        return self._frames

    def is_open(self):
        return self._f is not None

    def _u16(self, v):
        self._f.write(struct.pack("<H", v & 0xFFFF))

    def _u32(self, v):
        self._f.write(struct.pack("<I", v & 0xFFFFFFFF))

    def _fcc(self, s):
        self._f.write(s.encode("ascii")[:4].ljust(4, b"\0"))

    def _patch_u32(self, pos, v):
        cur = self._f.tell()
        self._f.seek(pos)
        self._u32(v)
        self._f.seek(cur)

    def begin(self, path, width, height, fps=15):
        if self._f:
            self.end()
        d = os.path.dirname(path)
        if d and not os.path.isdir(d):
            os.makedirs(d)
        self._path = path
        self._w = int(width)
        self._h = int(height)
        self._fps = max(1, int(fps))
        self._frames = 0
        self._movi_size = 4
        self._f = open(path, "wb")

        # RIFF
        self._fcc("RIFF")
        self._pos_riff = self._f.tell()
        self._u32(0)
        self._fcc("AVI ")

        # LIST hdrl size=192
        self._fcc("LIST")
        self._u32(192)
        self._fcc("hdrl")

        # avih 56
        self._fcc("avih")
        self._u32(56)
        self._u32(1000000 // self._fps)
        self._u32(0)
        self._u32(0)
        self._u32(0x10)
        self._pos_frames = self._f.tell()
        self._u32(0)
        self._u32(0)
        self._u32(1)
        self._u32(0)
        self._u32(self._w)
        self._u32(self._h)
        self._u32(0)
        self._u32(0)
        self._u32(0)
        self._u32(0)

        # LIST strl size=116
        self._fcc("LIST")
        self._u32(116)
        self._fcc("strl")

        # strh 56
        self._fcc("strh")
        self._u32(56)
        self._fcc("vids")
        self._fcc("MJPG")
        self._u32(0)
        self._u16(0)
        self._u16(0)
        self._u32(0)
        self._u32(1)
        self._u32(self._fps)
        self._u32(0)
        self._pos_strh_len = self._f.tell()
        self._u32(0)
        self._u32(0)
        self._u32(0xFFFFFFFF)
        self._u32(0)
        self._u16(0)
        self._u16(0)
        self._u16(self._w)
        self._u16(self._h)

        # strf 40
        self._fcc("strf")
        self._u32(40)
        self._u32(40)
        self._u32(self._w)
        self._u32(self._h)
        self._u16(1)
        self._u16(24)
        self._fcc("MJPG")
        self._u32(self._w * self._h * 3)
        self._u32(0)
        self._u32(0)
        self._u32(0)
        self._u32(0)

        # LIST movi
        self._fcc("LIST")
        self._pos_movi_list = self._f.tell()
        self._u32(0)
        self._fcc("movi")
        return True

    def write_frame(self, jpeg_bytes):
        if not self._f or not jpeg_bytes:
            return False
        raw = jpeg_bytes if isinstance(jpeg_bytes, (bytes, bytearray)) else bytes(jpeg_bytes)
        n = len(raw)
        pad = n & 1
        chunk = n + pad
        self._fcc("00dc")
        self._u32(chunk)
        self._f.write(raw)
        if pad:
            self._f.write(b"\0")
        self._movi_size += 8 + chunk
        self._frames += 1
        if (self._frames % 15) == 0:
            self._f.flush()
        return True

    def end(self):
        if not self._f:
            return True
        size = self._f.tell()
        self._patch_u32(self._pos_riff, size - 8)
        self._patch_u32(self._pos_frames, self._frames)
        self._patch_u32(self._pos_strh_len, self._frames)
        self._patch_u32(self._pos_movi_list, self._movi_size)
        self._f.flush()
        self._f.close()
        self._f = None
        return True


def next_rec_path(rec_dir):
    if not os.path.isdir(rec_dir):
        os.makedirs(rec_dir)
    idx = 1
    while idx < 10000:
        path = os.path.join(rec_dir, "VID_{:04d}.avi".format(idx))
        if not os.path.exists(path):
            return path
        idx += 1
    return os.path.join(rec_dir, "VID_9999.avi")


def list_recordings(rec_dir):
    if not os.path.isdir(rec_dir):
        return []
    items = []
    for name in sorted(os.listdir(rec_dir)):
        low = name.lower()
        if not (low.endswith(".avi") or low.endswith(".mjpg")):
            continue
        path = os.path.join(rec_dir, name)
        try:
            st = os.stat(path)
            items.append(
                {
                    "name": name,
                    "size": st.st_size,
                    "mtime": int(st.st_mtime),
                }
            )
        except OSError:
            pass
    items.sort(key=lambda x: x["mtime"], reverse=True)
    return items


def iter_avi_jpeg_frames(path):
    """Yield JPEG bytes from a simple MJPEG-AVI (00dc chunks) or raw MJPEG file."""
    with open(path, "rb") as f:
        data = f.read()
    # Prefer AVI 00dc parsing
    i = 0
    found = False
    while True:
        j = data.find(b"00dc", i)
        if j < 0:
            break
        found = True
        if j + 8 > len(data):
            break
        size = struct.unpack_from("<I", data, j + 4)[0]
        start = j + 8
        end = start + size
        if end > len(data):
            break
        chunk = data[start:end]
        # strip pad
        if chunk and chunk[-1:] == b"\0" and len(chunk) >= 2 and chunk[-2] == 0xD9:
            pass
        # find jpeg inside
        soi = chunk.find(b"\xff\xd8")
        eoi = chunk.rfind(b"\xff\xd9")
        if soi >= 0 and eoi > soi:
            yield chunk[soi : eoi + 2]
        i = end
    if found:
        return
    # fallback: scan SOI/EOI in whole file
    i = 0
    while True:
        soi = data.find(b"\xff\xd8", i)
        if soi < 0:
            break
        eoi = data.find(b"\xff\xd9", soi + 2)
        if eoi < 0:
            break
        yield data[soi : eoi + 2]
        i = eoi + 2
