"""
Steel ball YOLO helpers (aligned with apps/maixcam/detect_ball).

Model default: /root/models/2026H/steel_ball_v11n/yolo11n_ball.mud
mud model_type=yolo11 → nn.YOLO11
"""

import os
from maix import image, nn


# 与 detect_ball 一致：优先绝对路径；也可只写文件名，由 find_model 搜索
MODEL_MUD = "/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud"

CONF_TH = 0.2
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
    """Search yolo11n_ball.mud (same order spirit as detect_ball)."""
    mud = MODEL_MUD
    if os.path.isabs(mud) and os.path.exists(mud):
        return mud
    try:
        here = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        here = "."
    base = os.path.basename(mud) if mud else "yolo11n_ball.mud"
    stream_dir = os.path.dirname(here)
    apps_root = os.path.dirname(stream_dir)
    candidates = [
        mud,
        os.path.join(here, base),
        os.path.join(here, "models", base),
        os.path.join(here, "yolo11n_ball.mud"),
        os.path.join("/root/models", base),
        "/root/models/2026H/steel_ball_v11n/yolo11n_ball.mud",
        "/root/models/yolo11n_ball.mud",
        os.path.join(apps_root, "maixcam", "detect_ball", base),
        os.path.normpath(os.path.join(here, "..", "..", "maixcam", "detect_ball", base)),
        os.path.join(here, "steel_ball.mud"),
        "/root/models/steel_ball.mud",
        "/root/maix_phone/yolo11n_ball.mud",
        "/maixapp/apps/maix_phone/yolo11n_ball.mud",
    ]
    for p in candidates:
        if not p:
            continue
        ap = os.path.abspath(p) if not (
            p.startswith("/root") or p.startswith("/maixapp")
        ) else p
        if os.path.exists(ap):
            return ap
        if os.path.exists(p):
            return p
    raise FileNotFoundError(
        "mud not found: {} (put yolo11n_ball.mud + cvimodel under "
        "/root/models/2026H/steel_ball_v11n/ or next to this app)".format(mud)
    )


def class_name(detector, class_id, use_zh):
    if use_zh and class_id in DISPLAY_NAMES_ZH:
        return DISPLAY_NAMES_ZH[class_id]
    if class_id in DISPLAY_NAMES:
        return DISPLAY_NAMES[class_id]
    try:
        if class_id < len(detector.labels):
            raw = detector.labels[class_id]
            try:
                if all(ord(c) < 128 for c in raw):
                    return raw
            except Exception:
                pass
            return "cls{}".format(class_id)
    except Exception:
        pass
    return str(class_id)


def load_detector():
    use_zh = setup_font()
    model_path = find_model()
    print("load detector:", model_path)
    # mud model_type=yolo11 → nn.YOLO11（与 detect_ball 一致）
    detector = nn.YOLO11(model=model_path, dual_buff=True)
    print(
        "input:",
        detector.input_width(),
        detector.input_height(),
        detector.input_format(),
    )
    try:
        print("labels:", detector.labels)
    except Exception:
        pass
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
