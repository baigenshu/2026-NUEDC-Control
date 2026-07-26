"""
MaixCAM 检测数据集采样（配合 MaixHub 检测模型）

保存：/root/datasets/detect/<train|val>/<pos|neg>/000001.jpg

UI：
  - 折叠：仅 Menu + Snap（少挡视野）
  - 展开：Train/Val、Pos/Neg、Hide、Exit、Snap
"""

from maix import camera, display, image, app, time, touchscreen
import os


CAM_W, CAM_H = 320, 240
SAVE_ROOT = "/root/datasets/detect"
DEFAULT_SPLIT = "train"
DEFAULT_KIND = "pos"
JPEG_QUALITY = 95
BTN_H = 34
GAP = 4


def ensure_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)


def count_images(folder):
    if not os.path.exists(folder):
        return 0
    n = 0
    for name in os.listdir(folder):
        low = name.lower()
        if low.endswith(".jpg") or low.endswith(".jpeg") or low.endswith(".png"):
            n += 1
    return n


def next_index(folder):
    ensure_dir(folder)
    max_idx = 0
    for name in os.listdir(folder):
        base, ext = os.path.splitext(name)
        if ext.lower() not in (".jpg", ".jpeg", ".png"):
            continue
        try:
            idx = int(base)
            if idx > max_idx:
                max_idx = idx
        except ValueError:
            pass
    return max_idx + 1


def is_in_btn(x, y, btn):
    return btn[0] <= x < btn[0] + btn[2] and btn[1] <= y < btn[1] + btn[3]


def draw_btn(img, btn, label, active=False):
    if active:
        fill = image.Color.from_rgb(40, 120, 80)
        border = image.Color.from_rgb(80, 220, 140)
    else:
        fill = image.Color.from_rgb(28, 32, 48)
        border = image.COLOR_WHITE
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=fill, thickness=-1)
    img.draw_rect(btn[0], btn[1], btn[2], btn[3], color=border, thickness=2)
    size = image.string_size(label, scale=1.0)
    tx = btn[0] + max(0, (btn[2] - size.width()) // 2)
    ty = btn[1] + max(0, (btn[3] - size.height()) // 2)
    img.draw_string(tx, ty, label, color=image.COLOR_WHITE, scale=1.0)


def layout(w, h, menu_open):
    half = (w - GAP * 3) // 2
    y_bottom = h - BTN_H - GAP

    # 折叠：底部只留 Menu + Snap
    menu_btn = [GAP, y_bottom, half, BTN_H]
    snap_btn = [GAP * 2 + half, y_bottom, half, BTN_H]

    if not menu_open:
        return {
            "menu": menu_btn,
            "snap": snap_btn,
        }

    # 展开：三行设置 + 底部 Hide/Snap
    y2 = y_bottom - BTN_H - GAP
    y1 = y2 - BTN_H - GAP
    y0 = y1 - BTN_H - GAP
    return {
        "train": [GAP, y0, half, BTN_H],
        "val": [GAP * 2 + half, y0, half, BTN_H],
        "pos": [GAP, y1, half, BTN_H],
        "neg": [GAP * 2 + half, y1, half, BTN_H],
        "hide": [GAP, y2, half, BTN_H],
        "exit": [GAP * 2 + half, y2, half, BTN_H],
        "menu": menu_btn,  # 展开时底部左键改成 Hide 语义，用 hide
        "snap": snap_btn,
    }


def save_dir(split, kind):
    return os.path.join(SAVE_ROOT, split, kind)


def do_capture(cam, split, kind):
    folder = save_dir(split, kind)
    ensure_dir(folder)
    idx = next_index(folder)
    path = os.path.join(folder, f"{idx:06d}.jpg")
    raw = cam.read()
    raw.save(path, quality=JPEG_QUALITY)
    total = count_images(folder)
    return path, idx, total


def main():
    for sp in ("train", "val"):
        for k in ("pos", "neg"):
            ensure_dir(save_dir(sp, k))

    cam = camera.Camera(CAM_W, CAM_H)
    disp = display.Display()
    ts = touchscreen.TouchScreen()

    try:
        image.load_font(
            "sourcehansans",
            "/maixapp/share/font/SourceHanSansCN-Regular.otf",
            size=18,
        )
        image.set_default_font("sourcehansans")
    except Exception as e:
        print("font load skip:", e)

    split = DEFAULT_SPLIT
    kind = DEFAULT_KIND
    menu_open = False
    last_msg = "tap Menu to setup"
    last_path = ""
    pressed_last = False
    flash_ms = 0
    msg_until = 0

    print("save root:", SAVE_ROOT)

    while not app.need_exit():
        img = cam.read()
        w, h = img.width(), img.height()
        btns = layout(w, h, menu_open)
        folder = save_dir(split, kind)
        n_cur = count_images(folder)
        now = time.ticks_ms()

        # 顶部一行状态，尽量不挡
        img.draw_string(
            6, 4,
            f"{split}/{kind} n={n_cur}",
            image.COLOR_WHITE, scale=1.0,
        )
        if now < msg_until:
            img.draw_string(
                6, 22,
                last_msg,
                image.Color.from_rgb(80, 220, 255), scale=0.9,
            )

        if now < flash_ms:
            img.draw_rect(0, 0, w, h, image.Color.from_rgb(255, 255, 255), 3)

        if menu_open:
            draw_btn(img, btns["train"], "Train", active=(split == "train"))
            draw_btn(img, btns["val"], "Val", active=(split == "val"))
            draw_btn(img, btns["pos"], "Pos", active=(kind == "pos"))
            draw_btn(img, btns["neg"], "Neg", active=(kind == "neg"))
            draw_btn(img, btns["hide"], "Hide", active=False)
            draw_btn(img, btns["exit"], "Exit", active=False)
            draw_btn(img, btns["snap"], "Snap", active=False)
        else:
            draw_btn(img, btns["menu"], "Menu", active=False)
            draw_btn(img, btns["snap"], "Snap", active=False)

        disp.show(img)

        tx, ty, pressed = ts.read()
        if pressed and not pressed_last:
            dw, dh = disp.width(), disp.height()
            if dw > 0 and dh > 0 and (dw != w or dh != h):
                mx = int(tx * w / dw)
                my = int(ty * h / dh)
            else:
                mx, my = tx, ty

            if menu_open:
                if is_in_btn(mx, my, btns["train"]):
                    split = "train"
                    last_msg = "split -> train"
                    msg_until = now + 1200
                elif is_in_btn(mx, my, btns["val"]):
                    split = "val"
                    last_msg = "split -> val"
                    msg_until = now + 1200
                elif is_in_btn(mx, my, btns["pos"]):
                    kind = "pos"
                    last_msg = "kind -> pos"
                    msg_until = now + 1200
                elif is_in_btn(mx, my, btns["neg"]):
                    kind = "neg"
                    last_msg = "kind -> neg"
                    msg_until = now + 1200
                elif is_in_btn(mx, my, btns["hide"]):
                    menu_open = False
                    last_msg = "menu closed"
                    msg_until = now + 800
                elif is_in_btn(mx, my, btns["exit"]):
                    app.set_exit_flag(True)
                elif is_in_btn(mx, my, btns["snap"]):
                    try:
                        path, idx, total = do_capture(cam, split, kind)
                        last_path = path
                        last_msg = f"saved #{idx:06d} total={total}"
                        msg_until = now + 1500
                        flash_ms = now + 120
                        print(last_msg, path)
                    except Exception as e:
                        last_msg = f"save fail: {e}"
                        msg_until = now + 2000
                        print(last_msg)
            else:
                if is_in_btn(mx, my, btns["menu"]):
                    menu_open = True
                    last_msg = "menu open"
                    msg_until = now + 800
                elif is_in_btn(mx, my, btns["snap"]):
                    try:
                        path, idx, total = do_capture(cam, split, kind)
                        last_path = path
                        last_msg = f"saved #{idx:06d} total={total}"
                        msg_until = now + 1500
                        flash_ms = now + 120
                        print(last_msg, path)
                    except Exception as e:
                        last_msg = f"save fail: {e}"
                        msg_until = now + 2000
                        print(last_msg)

        pressed_last = pressed
        time.sleep_ms(10)

    print("exit", last_path)


if __name__ == "__main__":
    main()
