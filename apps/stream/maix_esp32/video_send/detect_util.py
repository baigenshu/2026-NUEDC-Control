"""
Steel ball YOLOv8 helpers (from apps/maixcam/detect_ball).
"""

import os
from maix import image, nn


CONF_TH = 0.45
IOU_TH = 0.45

DISPLAY_NAMES = {0: "ball"}
DISPLAY_NAMES_ZH = {0: "钢珠"}


def setup_font():
    font_path = "/maixapp/share/font/SourceHanSansCN-Regular.otf"
    try:
        image.load_font("sourcehansans", font_path, size=18)
        image.set_default_font("sourcehansans")
        print("font ok:", font_path)
        return True
    except Exception as e:
        print("font load fail, use ascii labels:", e)
        return False


def find_model():
    """Search steel_ball.mud next to this app, detect_ball, or /root/models.

    Layout:
      apps/stream/maix_esp32/video_send/   <- here
      apps/maixcam/detect_ball/            <- repo model source
    """
    try:
        here = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        here = "."
    # video_send -> maix_esp32 -> stream -> apps
    maix_esp32 = os.path.dirname(here)
    stream_dir = os.path.dirname(maix_esp32)
    apps_root = os.path.dirname(stream_dir)
    detect_ball = os.path.join(apps_root, "maixcam", "detect_ball", "steel_ball.mud")
    candidates = [
        os.path.join(here, "steel_ball.mud"),
        os.path.join(here, "models", "steel_ball.mud"),
        detect_ball,
        os.path.normpath(os.path.join(here, "..", "..", "..", "maixcam", "detect_ball", "steel_ball.mud")),
        "steel_ball.mud",
        "/root/models/steel_ball.mud",
        "/root/detect_ball/steel_ball.mud",
        "/root/video_send/steel_ball.mud",
        "/maixapp/apps/video_send/steel_ball.mud",
    ]
    for p in candidates:
        ap = os.path.abspath(p) if not p.startswith("/root") and not p.startswith("/maixapp") else p
        if os.path.exists(ap):
            return ap
        if os.path.exists(p):
            return p
    raise FileNotFoundError(
        "steel_ball.mud not found. Copy from apps/maixcam/detect_ball/ into "
        "apps/stream/maix_esp32/video_send/ or /root/models/ "
        "(keep steel_ball_int8.cvimodel next to the .mud)"
    )


def class_name(detector, class_id, use_zh):
    if use_zh and class_id in DISPLAY_NAMES_ZH:
        return DISPLAY_NAMES_ZH[class_id]
    if class_id in DISPLAY_NAMES:
        return DISPLAY_NAMES[class_id]
    if class_id < len(detector.labels):
        raw = detector.labels[class_id]
        try:
            if all(ord(c) < 128 for c in raw):
                return raw
        except Exception:
            pass
        return "cls{}".format(class_id)
    return str(class_id)


def load_detector():
    use_zh = setup_font()
    model_path = find_model()
    print("load detector:", model_path)
    detector = nn.YOLOv8(model=model_path, dual_buff=True)
    print(
        "input:",
        detector.input_width(),
        detector.input_height(),
        detector.input_format(),
    )
    print("labels:", detector.labels)
    return detector, use_zh


def draw_detections(img, detector, objs, use_zh, fps=0.0):
    """Draw boxes on img in-place; return object count."""
    for obj in objs:
        img.draw_rect(obj.x, obj.y, obj.w, obj.h, color=image.COLOR_GREEN, thickness=2)
        name = class_name(detector, obj.class_id, use_zh)
        img.draw_string(
            obj.x,
            max(0, obj.y - 18),
            "{}:{:.2f}".format(name, obj.score),
            color=image.COLOR_GREEN,
        )
    img.draw_string(
        4,
        4,
        "n={} {:.1f}fps".format(len(objs), fps),
        color=image.COLOR_WHITE,
    )
    return len(objs)
