"""
Flask web UI for Phone Web mode:
  - upper half live MJPEG
  - start/stop record to Maix local storage
  - list + playback of AVI/MJPEG files
"""

import os
import threading
import time as pytime
from flask import Flask, Response, jsonify, request, send_file

from avi_mjpeg import AviMjpegWriter, iter_avi_jpeg_frames, list_recordings, next_rec_path

REC_DIR = "/root/recordings"
HTTP_PORT = 8000

# shared state
_lock = threading.Lock()
_latest_jpeg = None
_cam = None
_cfg = None
_stop = False
_recording = False
_avi = None
_rec_name = ""
_rec_error = ""
_frame_i = 0
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
.video-pane{flex:0 0 48vh;min-height:180px;padding:8px 10px;display:flex;align-items:center;justify-content:center;
            background:#000;border-bottom:1px solid #1e2a38}
.video-pane img{max-width:100%;max-height:100%;object-fit:contain;border-radius:6px}
.panel{flex:1 1 auto;overflow:auto;padding:10px 12px 16px;max-width:720px;width:100%;margin:0 auto}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}
button{flex:1 1 120px;padding:12px 10px;border:0;border-radius:8px;font-size:.95rem;font-weight:600;color:#fff}
#btnStart{background:#1a7a45}
#btnStop{background:#a33a3a}
#btnRefresh{background:#2a3a4a;flex:0 0 auto;padding:12px 14px}
button:disabled{opacity:.45}
.status{font-size:.8rem;color:#9ab;margin-bottom:10px;min-height:1.2em}
h2{font-size:.85rem;color:#9ab;margin:8px 0 6px;font-weight:600}
.list{list-style:none}
.list li{display:flex;align-items:center;gap:8px;padding:10px;margin-bottom:6px;
         background:#121821;border:1px solid #243044;border-radius:8px}
.list .meta{flex:1;min-width:0}
.list .name{font-size:.85rem;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.list .sub{font-size:.7rem;color:#6b7c8f;margin-top:2px}
.list a,.list button.play{flex:0 0 auto;padding:8px 12px;border-radius:6px;background:#2a5a8a;
         color:#fff;text-decoration:none;border:0;font-size:.8rem}
.empty{color:#667;font-size:.8rem;padding:8px 0}
</style>
</head>
<body>
<header>
  <h1>MaixCAM Live + Record</h1>
  <p>上半实时预览 · 下半录制/回放 · 无登录 · data-mode=web</p>
</header>
<div class="video-pane">
  <img id="live" src="/stream" alt="live" decoding="async"/>
</div>
<div class="panel">
  <div class="row">
    <button id="btnStart" type="button">开始录制</button>
    <button id="btnStop" type="button" disabled>结束录制</button>
    <button id="btnRefresh" type="button">刷新列表</button>
  </div>
  <div class="status" id="status">就绪</div>
  <h2>录像列表（本机）</h2>
  <ul class="list" id="list"></ul>
  <p class="empty" id="empty">暂无录像</p>
</div>
<script>
(function(){
  var img=document.getElementById('live');
  var st=document.getElementById('status');
  var list=document.getElementById('list');
  var empty=document.getElementById('empty');
  var btnStart=document.getElementById('btnStart');
  var btnStop=document.getElementById('btnStop');
  var n=0;
  function bump(){ n++; img.src='/stream?t='+Date.now()+'-'+n; }
  img.onerror=function(){ setTimeout(bump,800); };

  function fmtSize(b){
    if(b<1024) return b+' B';
    if(b<1048576) return (b/1024).toFixed(1)+' KB';
    return (b/1048576).toFixed(2)+' MB';
  }
  function setRecUi(rec, name){
    btnStart.disabled=!!rec;
    btnStop.disabled=!rec;
    st.textContent=rec?('录制中: '+(name||'')):'空闲';
  }
  function loadStatus(){
    fetch('/api/rec/status').then(function(r){return r.json();}).then(function(j){
      setRecUi(j.recording, j.name);
      if(j.error) st.textContent='错误: '+j.error;
    }).catch(function(){});
  }
  function loadList(){
    fetch('/api/rec/list').then(function(r){return r.json();}).then(function(j){
      list.innerHTML='';
      var arr=j.items||[];
      empty.style.display=arr.length?'none':'block';
      arr.forEach(function(it){
        var li=document.createElement('li');
        var meta=document.createElement('div'); meta.className='meta';
        var nm=document.createElement('div'); nm.className='name'; nm.textContent=it.name;
        var sub=document.createElement('div'); sub.className='sub';
        sub.textContent=fmtSize(it.size);
        meta.appendChild(nm); meta.appendChild(sub);
        var a=document.createElement('a');
        a.className='play'; a.textContent='播放';
        a.href='/play?name='+encodeURIComponent(it.name);
        li.appendChild(meta); li.appendChild(a);
        list.appendChild(li);
      });
    }).catch(function(e){ st.textContent='列表失败'; });
  }
  btnStart.onclick=function(){
    fetch('/api/rec/start',{method:'POST'}).then(function(r){return r.json();}).then(function(j){
      if(j.ok){ setRecUi(true,j.name); loadList(); }
      else st.textContent='开始失败: '+(j.error||'');
    });
  };
  btnStop.onclick=function(){
    fetch('/api/rec/stop',{method:'POST'}).then(function(r){return r.json();}).then(function(j){
      setRecUi(false,'');
      if(j.error) st.textContent='结束: '+j.error;
      loadList();
    });
  };
  document.getElementById('btnRefresh').onclick=loadList;
  loadStatus(); loadList();
  setInterval(loadStatus, 2000);
})();
</script>
</body>
</html>
"""

PLAY_HTML = """<!DOCTYPE html>
<html lang="zh-CN" data-mode="web">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Playback</title>
<style>
body{margin:0;background:#0b0f14;color:#e8eef5;font-family:system-ui,sans-serif;display:flex;flex-direction:column;height:100vh}
header{padding:10px 12px;background:#121821;display:flex;align-items:center;gap:10px}
a{color:#7ab;text-decoration:none}
.wrap{flex:1;display:flex;align-items:center;justify-content:center;padding:8px;background:#000}
img{max-width:100%;max-height:100%;object-fit:contain}
</style>
</head>
<body>
<header>
  <a href="/">← 返回</a>
  <span id="title">playback</span>
</header>
<div class="wrap">
  <img id="v" src="" alt="play" decoding="async"/>
</div>
<script>
(function(){
  var q=new URLSearchParams(location.search);
  var name=q.get('name')||'';
  document.getElementById('title').textContent=name;
  document.getElementById('v').src='/api/rec/play?name='+encodeURIComponent(name)+'&t='+Date.now();
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
    global _latest_jpeg, _recording, _avi, _rec_name, _rec_error, _frame_i, _fps_show
    from maix import app

    quality = int(_cfg.get("jpeg_quality", 32))
    interval_ms = int(_cfg.get("frame_interval_ms", 45))
    w = int(_cfg.get("cam_w", 160))
    h = int(_cfg.get("cam_h", 120))
    fps_hint = max(1, int(round(1000.0 / max(1, interval_ms))))

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
            _frame_i += 1
            if _recording and _avi is not None:
                try:
                    _avi.write_frame(raw)
                except Exception as e:
                    _rec_error = str(e)
                    print("rec write err:", e)

        fps_n += 1
        now = pytime.time()
        if now - fps_t >= 1.0:
            _fps_show = fps_n / (now - fps_t)
            fps_n = 0
            fps_t = now
            print("web push fps: {:.1f} rec={}".format(_fps_show, _recording))

        elapsed = (pytime.time() - t0) * 1000.0
        rest = interval_ms - elapsed
        if rest > 1:
            pytime.sleep(rest / 1000.0)


def create_app():
    app_flask = Flask(__name__)

    @app_flask.route("/")
    def index():
        return INDEX_HTML

    @app_flask.route("/play")
    def play_page():
        return PLAY_HTML

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
                pytime.sleep(0.03)

        return Response(
            gen(),
            mimetype="multipart/x-mixed-replace; boundary=frame",
            headers={"Cache-Control": "no-store, no-cache, must-revalidate", "Pragma": "no-cache"},
        )

    @app_flask.route("/api/rec/status")
    def rec_status():
        with _lock:
            return jsonify(
                {
                    "recording": _recording,
                    "name": os.path.basename(_rec_name) if _rec_name else "",
                    "frames": _avi.frames if _avi else 0,
                    "error": _rec_error,
                    "fps": round(_fps_show, 1),
                }
            )

    @app_flask.route("/api/rec/start", methods=["POST", "GET"])
    def rec_start():
        global _recording, _avi, _rec_name, _rec_error
        with _lock:
            if _recording:
                return jsonify({"ok": True, "name": os.path.basename(_rec_name), "msg": "already"})
            try:
                path = next_rec_path(REC_DIR)
                w = int(_cfg.get("cam_w", 160))
                h = int(_cfg.get("cam_h", 120))
                interval_ms = int(_cfg.get("frame_interval_ms", 45))
                fps_hint = max(1, int(round(1000.0 / max(1, interval_ms))))
                avi = AviMjpegWriter()
                avi.begin(path, w, h, fps_hint)
                _avi = avi
                _rec_name = path
                _recording = True
                _rec_error = ""
                print("rec start", path)
                return jsonify({"ok": True, "name": os.path.basename(path)})
            except Exception as e:
                _rec_error = str(e)
                print("rec start fail", e)
                return jsonify({"ok": False, "error": str(e)})

    @app_flask.route("/api/rec/stop", methods=["POST", "GET"])
    def rec_stop():
        global _recording, _avi, _rec_name, _rec_error
        with _lock:
            if not _recording:
                return jsonify({"ok": True, "msg": "idle"})
            try:
                name = os.path.basename(_rec_name) if _rec_name else ""
                frames = _avi.frames if _avi else 0
                if _avi:
                    _avi.end()
                _avi = None
                _recording = False
                print("rec stop", name, "frames", frames)
                return jsonify({"ok": True, "name": name, "frames": frames})
            except Exception as e:
                _rec_error = str(e)
                _recording = False
                _avi = None
                return jsonify({"ok": False, "error": str(e)})

    @app_flask.route("/api/rec/list")
    def rec_list():
        return jsonify({"items": list_recordings(REC_DIR)})

    @app_flask.route("/api/rec/play")
    def rec_play():
        name = request.args.get("name", "")
        # prevent path traversal
        name = os.path.basename(name)
        path = os.path.join(REC_DIR, name)
        if not name or not os.path.isfile(path):
            return "not found", 404

        boundary = b"frame"

        def gen():
            try:
                for jpeg in iter_avi_jpeg_frames(path):
                    if _stop:
                        break
                    yield (
                        b"--" + boundary + b"\r\n"
                        b"Content-Type: image/jpeg\r\n\r\n" + jpeg + b"\r\n"
                    )
                    pytime.sleep(0.06)
            except Exception as e:
                print("play err", e)

        return Response(
            gen(),
            mimetype="multipart/x-mixed-replace; boundary=frame",
            headers={"Cache-Control": "no-store"},
        )

    @app_flask.route("/rec/<path:name>")
    def rec_download(name):
        name = os.path.basename(name)
        path = os.path.join(REC_DIR, name)
        if not os.path.isfile(path):
            return "not found", 404
        return send_file(path, as_attachment=True, download_name=name)

    return app_flask


def run_web_server(cam, cfg, ip, port=HTTP_PORT):
    """
    Block until app.need_exit(). Starts camera thread + Flask.
    """
    global _cam, _cfg, _stop, _latest_jpeg, _recording, _avi, _rec_name, _rec_error
    from maix import app as mapp

    _cam = cam
    _cfg = cfg
    _stop = False
    _latest_jpeg = None
    _recording = False
    _avi = None
    _rec_name = ""
    _rec_error = ""

    if not os.path.isdir(REC_DIR):
        try:
            os.makedirs(REC_DIR)
        except Exception as e:
            print("mkdir recordings fail", e)

    th = threading.Thread(target=_camera_loop, name="cam-web", daemon=True)
    th.start()

    flask_app = create_app()
    print("Flask web on http://{}:{}  (record dir {})".format(ip, port, REC_DIR))
    print("Open phone browser: http://{}:{}".format(ip, port))

    # Werkzeug blocking server; poll need_exit in a side thread to shutdown is hard,
    # so use short timeout loop via werkzeug serving with use_reloader=False.
    # Flask app.run blocks — break via need_exit by running in thread and joining with poll.
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
    with _lock:
        if _recording and _avi:
            try:
                _avi.end()
            except Exception:
                pass
            _recording = False
            _avi = None
    print("web server loop end")
