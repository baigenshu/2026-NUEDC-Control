"""
钢珠 YOLOv5 检测（MaixHub 任务 302745）
模型与 main.py 同目录，或 /root/models/
"""

from maix import camera, display, image, nn, app, time
import os


CONF_TH = 0.45
IOU_TH = 0.45

# 显示名：避免默认字体不支持中文时出现 ????
# 键为 class_id；也可按模型原始标签覆盖
DISPLAY_NAMES = {
    0: "ball",
}
# 若已加载中文字体，可改成中文显示
DISPLAY_NAMES_ZH = {
    0: "钢珠",
}


def find_model():
    here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "."
    candidates = [
        os.path.join(here, "model_302745.mud"),
        "model_302745.mud",
        "/root/models/model_302745.mud",
        "/root/detect_ball/model_302745.mud",
        "/root/models/maixhub/302745/model_302745.mud",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    raise FileNotFoundError("model_302745.mud not found")


def setup_font():
    """加载中文字体，成功返回 True。"""
    font_path = "/maixapp/share/font/SourceHanSansCN-Regular.otf"
    try:
        image.load_font("sourcehansans", font_path, size=18)
        image.set_default_font("sourcehansans")
        print("font ok:", font_path)
        return True
    except Exception as e:
        print("font load fail, use ascii labels:", e)
        return False


def class_name(detector, class_id, use_zh):
    if use_zh and class_id in DISPLAY_NAMES_ZH:
        return DISPLAY_NAMES_ZH[class_id]
    if class_id in DISPLAY_NAMES:
        return DISPLAY_NAMES[class_id]
    if class_id < len(detector.labels):
        raw = detector.labels[class_id]
        # 默认字体下中文会变成 ???，强制回退
        try:
            if all(ord(c) < 128 for c in raw):
                return raw
        except Exception:
            pass
        return f"cls{class_id}"
    return str(class_id)


def main():
    use_zh = setup_font()
    model_path = find_model()
    print("load:", model_path)
    detector = nn.YOLOv5(model=model_path, dual_buff=True)
    print("input:", detector.input_width(), detector.input_height(), detector.input_format())
    print("labels:", detector.labels)

    cam = camera.Camera(
        detector.input_width(),
        detector.input_height(),
        detector.input_format(),
    )
    disp = display.Display()

    while not app.need_exit():
        img = cam.read()
        objs = detector.detect(img, conf_th=CONF_TH, iou_th=IOU_TH)
        for obj in objs:
            img.draw_rect(obj.x, obj.y, obj.w, obj.h, color=image.COLOR_GREEN, thickness=2)
            name = class_name(detector, obj.class_id, use_zh)
            img.draw_string(
                obj.x, max(0, obj.y - 18),
                f"{name}:{obj.score:.2f}",
                color=image.COLOR_GREEN,
            )
        img.draw_string(4, 4, f"n={len(objs)} {time.fps():.1f}fps", color=image.COLOR_WHITE)
        disp.show(img)


if __name__ == "__main__":
    main()
