#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>

const char webpage[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <title>GATE CONFIG</title>
  </head>

  <body>
    <h1>GATE CONFIG</h1>

    server IP
    <br>
    <input id="inp_ip" type="tel">
    <br>

    WiFi SSID
    <br>
    <input id="inp_ssid" type="text">
    <br>
        
    WiFi password
    <br>
    <input id="inp_pass" type="text">
    <br>

    <br>
    <button id="btn_set" onclick="set()">Set configuration</button>
    <br>
    <br>
    <button id="btn_open" onclick="openGate()">Open gate</button>      
    <button id="btn_close" onclick="closeGate()">Close gate</button>

    <script>
        function set(){
            let config = {
                ip: document.getElementById('inp_ip').value,
                ssid: document.getElementById('inp_ssid').value,
                pass: document.getElementById('inp_pass').value
            }

            fetch('/config/set', {
                method: 'POST',
                headers: {'Content-Type': 'application/json' },
                body: JSON.stringify(config)
            });
        }

        function openGate(){
            fetch('/config/open');
        }
        function closeGate(){
            fetch('/config/close');
        }
    </script>

  </body>
  </html>
)rawliteral";

Preferences prefs;

// --- Wi-Fi ---
String WIFI_SSID;
String WIFI_PASS;

// --- Сервер ---
String SERVER_URL; //"http://192.168.31.200:8080/upload";  // 192.168.31.200 | 10.93.134.114
String API_KEY = "D@nilHVZVLXPZVA3";

// --- Точка доступа ---
String AP_SSID = "gate-config";
String AP_PASS = "D@nilHVZVLXPZVA3";

// --- Web сервер ---
WebServer server(80);

// --- Пин общения с контроллером серво ---
#define COM_PIN 14
#define RESET_BTN 12

// --- Пины камеры (AI-Thinker ESP32-CAM) ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const unsigned long SNAPSHOT_INTERVAL_MS = 1000;
unsigned long lastSnapshot = 0;
bool is_forced = false;
uint32_t time_to_close = 0;
bool operating_mode = false;
static unsigned long last_wifi_check = 0;

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_VGA;     // 640x480 — хватит для номера
    config.jpeg_quality = 12;                // 10-15, меньше = лучше качество/больше файл
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return false;
    }
    return true;
}

void prefs_reset(){
    prefs.begin("gate", false);    
    prefs.putString("server_ip", "");
    prefs.putString("ssid", "");
    prefs.putString("pass", "");
    prefs.end();
    ESP.restart();
}

bool sendSnapshot() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return false;
    }

    JsonDocument gateState;
    gateState["state"] = digitalRead(COM_PIN) ? "open" : "closed";
    gateState["mode"] = is_forced ? "manual" : "auto";
    //gateState["api_key"] = API_KEY;
    String gateStateJSON;
    serializeJson(gateState, gateStateJSON);

    HTTPClient http;
    http.begin(SERVER_URL);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Gate-State", gateStateJSON);

    int code = http.POST(fb->buf, fb->len);

    Serial.printf("POST %u bytes -> HTTP %d", fb->len, code);
    if (code > 0) {
        String response = http.getString();
        Serial.printf("resp: %s\n", response.c_str());

        StaticJsonDocument<256> doc;

        if (!deserializeJson(doc, response)) {
            String command = doc["command"] | "wait";
            int timeout = doc["timeout"] | 5000;

            Serial.println(command);

            if (command == "reset"){
                prefs_reset();
            } else if (command == "open"){
                digitalWrite(COM_PIN, 1);
                if (timeout == 0){
                    is_forced = true;
                } else {
                    time_to_close = millis() + timeout;
                    is_forced = false;
                }
            } else if (command == "close") {
                digitalWrite(COM_PIN, 0);
                if (timeout == 0){
                    is_forced = true;
                } else {
                    is_forced = false;
                }
            } else if (command == "wait"){
                is_forced = false;
            }
        }
    } else {
        Serial.printf("  err: %s\n", http.errorToString(code).c_str());
    }

    http.end();
    esp_camera_fb_return(fb);
    return code == 200;
}

void setup() {
    pinMode(COM_PIN, OUTPUT);
    pinMode(RESET_BTN, INPUT_PULLUP);
    Serial.begin(115200);
    delay(500);

    if (digitalRead(RESET_BTN) == LOW){
        prefs_reset();
    }

    prefs.begin("gate", true);
    String p_server_ip = prefs.getString("server_ip", "");
    WIFI_SSID = prefs.getString("ssid", "");
    WIFI_PASS = prefs.getString("pass", "");
    prefs.end();

    if (p_server_ip.isEmpty() || WIFI_SSID.isEmpty()){
        operating_mode = false;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());

        Serial.println("\nconfig mode");
        Serial.println(WiFi.softAPIP());

        server.on("/", [](){
            server.send(200, "text/html", webpage);
        });

        server.on("/config/open", [](){
            digitalWrite(COM_PIN, 1);
            server.send(200);
        });

        server.on("/config/close", [](){
            digitalWrite(COM_PIN, 0);
            server.send(200);
        });


        server.on("/config/set", HTTP_POST, [](){
            if (!server.hasArg("plain")){
                server.send(400, "text/plain", "body not received");
                return;
            }

            JsonDocument json_config_resolver;
            DeserializationError error = deserializeJson(json_config_resolver, server.arg("plain"));

            if (!error){
                prefs.begin("gate", false);    
                prefs.putString("server_ip", json_config_resolver["ip"].as<String>());
                prefs.putString("ssid", json_config_resolver["ssid"].as<String>());
                prefs.putString("pass", json_config_resolver["pass"].as<String>());
                prefs.end();
                server.send(200, "application/json", "{\"status\":\"ok\"}");
                delay(200);
                ESP.restart();
            } else {
                server.send(400, "application/json", "{\"status\":\"error\",\"msg\":\"bad json\"}");
            }
        });

        server.begin();
    } else {
        operating_mode = true;
        Serial.println("\nESP32-CAM -> server");

        SERVER_URL = "http://" + p_server_ip + ":8080/upload";

        if (!initCamera()) {
            Serial.println("Halt: camera failed");
            while (true) delay(1000);
        }

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
        Serial.print("Connecting to WiFi");
        int setup_timeout = 20;
        while (WiFi.status() != WL_CONNECTED && setup_timeout > 0) {
            delay(500);
            Serial.print(".");
            setup_timeout--;
        }
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("\nWiFi failed → config mode");
            prefs_reset();
        }
        Serial.printf("\nConnected. IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Target server: %s\n", SERVER_URL);
    }
}

void loop() {
    if (operating_mode){
        if (millis() - last_wifi_check > 10000) {
            last_wifi_check = millis();
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi lost, reconnecting...");
                WiFi.reconnect();
            }
        }

        if (millis() - lastSnapshot >= SNAPSHOT_INTERVAL_MS) {
            lastSnapshot = millis();
            sendSnapshot();
        }

        if ((long)(millis() - time_to_close) >= 0 && !is_forced){
            digitalWrite(COM_PIN, 0);
        }
    } else {
        server.handleClient();
    }
}