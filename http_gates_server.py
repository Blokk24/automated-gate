from utils.recognition import recognition
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from datetime import datetime, timedelta
import os
import numpy as np
import cv2
import json
import easyocr
from ultralytics import YOLO

config = {
  "timeout": 10000,
  "port": 8080,
  "whitelist":[]
}

OPEN_COMMAND = "open"
CLOSE_COMMAND = "close"
WAIT_COMMAND = "wait"

WORK_DIR = os.getcwd()

yolo = None
ocr = None

latest_jpeg = None

gate_state = None
gate_mode = None
last_update = None

is_forsed = False
forced_responce = None

def load_config(path = os.path.join(WORK_DIR, "files", "config.json")):
    try:
        with open(path, "r", encoding="utf-8") as f:
            user_cfg = json.load(f)
    except FileNotFoundError:
        user_cfg = {}
    except json.JSONDecodeError as e:
        print(f"[!] Broken config: {e}, using defaults")
        user_cfg = {}

    cfg = {**config, **user_cfg}
    return cfg

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        global forced_responce
        global is_forsed
        if self.path == "/upload":
            # Content-Length обязателен — по нему читаем ровно столько байт
            length = int(self.headers.get("Content-Length", 0))
            if length == 0:
                self.send_response(400)
                self.end_headers()
                return

            data = self.rfile.read(length)

            # Проверим, что это JPEG (начинается с FF D8, кончается FF D9)
            if not (data[:2] == b"\xff\xd8" and data[-2:] == b"\xff\xd9"):
                print(f"[!] Got {len(data)} bytes, but it's not a JPEG")
                self.send_response(400)
                self.end_headers()
                return

            global gate_state
            global gate_mode
            global last_update
            state_hdr = self.headers.get("X-Gate-State")
            if state_hdr:
                try:
                    st = json.loads(state_hdr)
                    gate_state = st.get("state", "unknown")
                    gate_mode = st.get("mode", "unknown")
                    last_update = datetime.now()
                except json.JSONDecodeError:
                    pass
                except AttributeError:
                    pass

            global latest_jpeg
            latest_jpeg = data

            # Также сохраним "последний кадр" — пригодится для отладки
            path = os.path.join(WORK_DIR, "files", "latest.jpg")
            with open(path, "wb") as f:
                f.write(data)

            if is_forsed and forced_responce is not None:
                body = json.dumps(forced_responce).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            else:
                arr = np.frombuffer(data, dtype=np.uint8)
                img = cv2.imdecode(arr, cv2.IMREAD_COLOR)

                gates_command = WAIT_COMMAND
                plate = recognition(img, 
                                    yolo=yolo,
                                    ocr=ocr,
                                    conf=0.5)
                
                if (plate != "") and (plate in config.get("whitelist")):
                    gates_command = OPEN_COMMAND
        
                response = {
                    "command": gates_command, 
                    "timeout": config.get("timeout"),
                }
                body = json.dumps(response).encode()

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        elif self.path == "/command":
            user_command = self.headers.get("X-Gate-Command", "")

            if user_command == "auto":
                forced_responce = None
                is_forsed = False
            else:
                forced_responce = {
                    "command": user_command, 
                    "timeout": 0,
                }
                is_forsed = True

            self.send_response(200)
            self.end_headers()

        else:
            self.send_response(404)
            self.end_headers()
            return

    def do_GET(self):
        if self.path == "/camera":
            img = latest_jpeg
            if img is None:
                self.send_response(404)
                self.end_headers()
                self.wfile.write(b"No data")
                return
            self.send_response(200)
            self.send_header("Content-Type", "image/jpeg")
            self.send_header("Content-Length", str(len(img)))

            if last_update is not None:
                update_delta = datetime.now() - last_update
                update_delta_sec = int(update_delta.total_seconds())
            else:
                update_delta_sec = -2

            gate_state_response = {
                "state": gate_state,
                "mode": gate_mode,
                "last_update": update_delta_sec
            }
            body = json.dumps(gate_state_response)

            self.send_header("X-Gate-State", body)
            self.end_headers()
            self.wfile.write(img)
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, fmt, *args):
        # Отключаем дефолтный лог
        pass

if __name__ == "__main__":
    config = load_config()

    yolo = YOLO(os.path.join(WORK_DIR, "utils", "best.pt"))
    ocr = easyocr.Reader(['en'] , gpu=False) # gpu=True, если есть видеокарта NVIDIA

    HOST, PORT = "0.0.0.0", config.get("port")
    print(f"Listening on http://{HOST}:{PORT}/upload")
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()