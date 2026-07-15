from pathlib import Path

p = Path("keil/template.uvprojx")
raw = p.read_bytes()
bs = bytes([8])

replacements = [
    (b"Inc" + bs + b"oard.h", br"Inc\board.h"),
    (b"Inc" + bs + b"sp_systick.h", br"Inc\bsp_systick.h"),
    (b"Src" + bs + b"sp_systick.c", br"Src\bsp_systick.c"),
]

for old, new in replacements:
    count = raw.count(old)
    print(f"replace {old!r} -> {new!r} count={count}")
    if count == 0:
        raise SystemExit(f"pattern not found: {old!r}")
    raw = raw.replace(old, new)

remaining = raw.count(bs)
print("remaining backspaces:", remaining)
if remaining:
    raise SystemExit("still corrupted")

p.write_bytes(raw)
print("fixed")
for line in raw.decode("utf-8").splitlines():
    if "FilePath" in line and ("Function" in line or "Hardware" in line):
        print(line.strip())
