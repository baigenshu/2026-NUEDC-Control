"""
Flask web UI (Phone Web mode):
  - Live MJPEG at /stream (Maix performance focused on live)
  - Start/Stop record on the PHONE via MediaRecorder (no Maix disk write)
"""

import threading
import time as pytime
from flask import Flask, Response

HTTP_PORT = 8000

_lock = threading.Lock()
_latest_jpeg = None
_cam = None
_cfg = None
_stop = False
_fps_show = 0.0

INDEX_HTML = """<!DOCTYPE html>
<html lang="zh-CN" data-mode="web">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"/>
<meta http-equiv="Cache-Control" content="no-store"/>
<title>MaixCAM Live</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0b0f14;color:#e8eef5;font-family:system-ui,-apple-system,sans-serif;
     height:100vh;display:flex;flex-direction:column;overflow:hidden}
header{flex:0 0 auto;padding:10px 12px;background:#121821;border-bottom:1px solid #1e2a38}
header h1{font-size:1rem;font-weight:600}
header p{font-size:.72rem;color:#8fa3b8;margin-top:3px}
.video-pane{flex:0 0 50vh;min-height:200px;padding:8px 10px;display:flex;align-items:center;justify-content:center;
            background:#000;border-bottom:1px solid #1e2a38;position:relative}
#live{max-width:100%;max-height:100%;object-fit:contain;border-radius:6px}
#cv{position:absolute;left:-9999px;top:0;width:1px;height:1px}
.panel{flex:1 1 auto;overflow:auto;padding:12px;max-width:720px;width:100%;margin:0 auto}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}
button{flex:1 1 130px;padding:14px 10px;border:0;border-radius:8px;font-size:.95rem;font-weight:600;color:#fff}
#btnStart{background:#1a7a45}
#btnStop{background:#a33a3a}
button:disabled{opacity:.45}
.status{font-size:.82rem;color:#9ab;margin-bottom:8px;min-height:1.3em}
.hint{font-size:.72rem;color:#667;line-height:1.45}
</style>
</head>
<body>
<header>
  <h1>MaixCAM Live</h1>
  <p>上半直播 · 录制仅保存在手机 · Maix 不写盘</p>
</header>
<div class="video-pane">
  <img id="live" src="/stream" alt="live" decoding="async" crossorigin="anonymous"/>
  <canvas id="cv"></canvas>
</div>
<div class="panel">
  <div class="row">
    <button id="btnStart" type="button">开始录制</button>
    <button id="btnStop" type="button" disabled>结束录制</button>
  </div>
  <div class="status" id="status">空闲 — 录像将下载到手机</div>
  <p class="hint">
    建议 Chrome。格式多为 WebM。录的是网页画面，不经过 Maix 存盘。<br/>
    若无法录制，请换 Chrome 或检查浏览器权限。
  </p>
</div>
<script>
(function(){
  var img = document.getElementById('live');
  var cv = document.getElementById('cv');
  var ctx = cv.getContext('2d');
  var st = document.getElementById('status');
  var btnStart = document.getElementById('btnStart');
  var btnStop = document.getElementById('btnStop');
  var n = 0;
  var rec = null;
  var chunks = [];
  var drawTimer = null;
  var t0 = 0;

  function bump(){
    n++;
    img.src = '/stream?t=' + Date.now() + '-' + n;
  }
  img.onerror = function(){ setTimeout(bump, 800); };

  function fitCanvas(){
    var w = img.naturalWidth || 320;
    var h = img.naturalHeight || 240;
    if (cv.width !== w || cv.height !== h) {
      cv.width = w;
      cv.height = h;
    }
  }

  function drawLoop(){
    if (!rec) return;
    try {
      if (img.complete && img.naturalWidth > 0) {
        fitCanvas();
        ctx.drawImage(img, 0, 0, cv.width, cv.height);
      }
    } catch (e) {}
    drawTimer = requestAnimationFrame(drawLoop);
  }

  function setUi(recording){
    btnStart.disabled = !!recording;
    btnStop.disabled = !recording;
  }

  function pickMime(){
    var cands = [
      'video/webm;codecs=vp9',
      'video/webm;codecs=vp8',
      'video/webm',
      'video/mp4'
    ];
    for (var i = 0; i < cands.length; i++) {
      if (window.MediaRecorder && MediaRecorder.isTypeSupported && MediaRecorder.isTypeSupported(cands[i]))
        return cands[i];
    }
    return '';
  }

  btnStart.onclick = function(){
    if (!window.MediaRecorder) {
      st.textContent = '此浏览器不支持 MediaRecorder，请用 Chrome';
      return;
    }
    if (!img.complete || img.naturalWidth === 0) {
      st.textContent = '等待画面…请稍后再点开始';
      bump();
      return;
    }
    fitCanvas();
    ctx.drawImage(img, 0, 0, cv.width, cv.height);
    var stream;
    try {
      stream = cv.captureStream(20);
    } catch (e) {
      st.textContent = 'captureStream 失败: ' + e;
      return;
    }
    chunks = [];
    var mime = pickMime();
    try {
      rec = mime ? new MediaRecorder(stream, { mimeType: mime, videoBitsPerSecond: 1200000 })
                 : new MediaRecorder(stream);
    } catch (e) {
      st.textContent = 'MediaRecorder 失败: ' + e;
      return;
    }
    rec.ondataavailable = function(ev){
      if (ev.data && ev.data.size > 0) chunks.push(ev.data);
    };
    rec.onerror = function(ev){
      st.textContent = '录制错误';
    };
    rec.onstop = function(){
      if (drawTimer) cancelAnimationFrame(drawTimer);
      drawTimer = null;
      var type = (rec && rec.mimeType) ? rec.mimeType : 'video/webm';
      var blob = new Blob(chunks, { type: type });
      chunks = [];
      rec = null;
      setUi(false);
      if (!blob.size) {
        st.textContent = '空闲 — 未录到数据';
        return;
      }
      var ext = type.indexOf('mp4') >= 0 ? 'mp4' : 'webm';
      var a = document.createElement('a');
      var url = URL.createObjectURL(blob);
      a.href = url;
      a.download = 'maix_' + Date.now() + '.' + ext;
      document.body.appendChild(a);
      a.click();
      setTimeout(function(){ URL.revokeObjectURL(url); a.remove(); }, 2000);
      st.textContent = '已保存到手机 (' + ext + ', ' + Math.round(blob.size/1024) + ' KB)';
    };
    rec.start(500);
    t0 = Date.now();
    setUi(true);
    st.textContent = '录制中…';
    drawLoop();
    var tick = setInterval(function(){
      if (!rec) { clearInterval(tick); return; }
      var sec = Math.floor((Date.now() - t0) / 1000);
      st.textContent = '录制中… ' + sec + 's（画面写入手机，Maix 不存盘）';
    }, 500);
  };

  btnStop.onclick = function(){
    if (rec && rec.state !== 'inactive') {
      try { rec.stop(); } catch (e) { st.textContent = '停止失败: ' + e; setUi(false); }
    }
  };
})();
</script>
</body>
</html>
"""


def _jpeg_bytes(img, quality):
    try:
        jpg = img.to_jpeg(quality=quality)
    except TypeError:
        try:
            from maix import image as mimg
            jpg = img.to_format(mimg.Format.FMT_JPEG, quality=quality)
        except Exception:
            jpg = img.to_jpeg()
    if hasattr(jpg, "to_bytes"):
        try:
            return jpg.to_bytes()
        except TypeError:
            return jpg.to_bytes(True)
    return bytes(jpg)


def _camera_loop():
    global _latest_jpeg, _fps_show
    from maix import app

    quality = int(_cfg.get("jpeg_quality", 50))
    interval_ms = int(_cfg.get("frame_interval_ms", 45))
    fps_t = pytime.time()
    fps_n = 0

    while not _stop and not app.need_exit():
        t0 = pytime.time()
        try:
            img = _cam.read()
            raw = _jpeg_bytes(img, quality)
        except Exception as e:
            print("cam loop err:", e)
            pytime.sleep(0.05)
            continue

        with _lock:
            _latest_jpeg = raw

        fps_n += 1
        now = pytime.time()
        if now - fps_t >= 1.0:
            _fps_show = fps_n / (now - fps_t)
            fps_n = 0
            fps_t = now
            print("live fps: {:.1f}".format(_fps_show))

        elapsed = (pytime.time() - t0) * 1000.0
        rest = interval_ms - elapsed
        if rest > 1:
            pytime.sleep(rest / 1000.0)


def create_app():
    app_flask = Flask(__name__)

    @app_flask.route("/")
    def index():
        return INDEX_HTML

    @app_flask.route("/stream")
    def stream():
        boundary = b"frame"

        def gen():
            from maix import app as mapp
            while not _stop and not mapp.need_exit():
                with _lock:
                    frame = _latest_jpeg
                if frame:
                    yield (
                        b"--" + boundary + b"\r\n"
                        b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n"
                    )
                else:
                    pytime.sleep(0.05)
                    continue
                pytime.sleep(0.02)

        return Response(
            gen(),
            mimetype="multipart/x-mixed-replace; boundary=frame",
            headers={
                "Cache-Control": "no-store, no-cache, must-revalidate",
                "Pragma": "no-cache",
                "Access-Control-Allow-Origin": "*",
            },
        )

    return app_flask


def run_web_server(cam, cfg, ip, port=HTTP_PORT):
    global _cam, _cfg, _stop, _latest_jpeg
    from maix import app as mapp

    _cam = cam
    _cfg = cfg
    _stop = False
    _latest_jpeg = None

    th = threading.Thread(target=_camera_loop, name="cam-live", daemon=True)
    th.start()

    flask_app = create_app()
    print("Flask live-only http://{}:{}".format(ip, port))
    print("Record on phone only (MediaRecorder); Maix does not save files")

    server_error = []

    def _serve():
        try:
            flask_app.run(host="0.0.0.0", port=port, threaded=True, use_reloader=False)
        except Exception as e:
            server_error.append(e)
            print("flask err", e)

    srv = threading.Thread(target=_serve, name="flask", daemon=True)
    srv.start()

    while not mapp.need_exit() and not _stop:
        pytime.sleep(0.2)
        if server_error:
            break

    _stop = True
    print("web server loop end")
