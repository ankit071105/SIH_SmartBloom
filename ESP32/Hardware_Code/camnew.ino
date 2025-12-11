#include <WiFi.h>
#include "esp_camera.h"
#include "FS.h"
#include "SPIFFS.h"
#include <WebServer.h>
#include <ESP32Servo.h>

// ================== USER CONFIG ==================
const char* WIFI_SSID     = "Ayes Chinmay 1";
const char* WIFI_PASSWORD = "ayeschinmay";

// Servo settings
const int SERVO_PIN = 12;
const int SERVO_CENTER = 90;
const int SERVO_RIGHT  = 150;

// Recording
bool  isRecording = false;
unsigned long lastRecordMillis = 0;
const unsigned long RECORD_INTERVAL_MS = 500;

// Pollination stats
unsigned long startMillis = 0;
unsigned long pollinationCount = 0;

// Camera PIN CONFIG (AI Thinker)
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

WebServer server(80);
Servo servo;

// ================== HTML PAGE ==================
const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" />
  <title>Smart Bloom</title>
  <style>
  body { font-family: Arial; background:#f5f5f5; margin:0; }
  header { background:#2c3e50; color:#fff; padding:10px; }
  .container { padding:15px; display:flex; gap:15px; }

  .left { flex:2; }
  .right { flex:1; }

  .card {
    background:#fff;
    padding:15px;
    margin-bottom:15px;
    border-radius:8px;
  }

  /* 1. New Stream Container for positioning */
  #stream-container {
    width: 320px;
    margin: 0 auto;
    position: relative; /* Key for absolute positioning of overlay */
    border-radius: 5px;
    overflow: hidden; /* Ensures child elements respect border-radius */
  }

  /* Video Stream */
  #stream {
    width: 100%;
    height: auto;
    display: block;
  }

  /* 2. New Overlay Box CSS */
  #overlay-box {
    position: absolute;
    top: 50%;
    left: 50%;
    width: 120px; /* Size of the focus box */
    height: 120px;
    border: 2px dotted yellow; /* The requested style */
    transform: translate(-50%, -50%); /* Centers the box perfectly */
    pointer-events: none; /* Allows mouse events to pass through */
    z-index: 10;
    box-sizing: border-box;
  }

  button { padding:10px; margin:5px; border:none; border-radius:5px; cursor:pointer; }
  .primary { background:#3498db; color:white; }
  .danger { background:#e74c3c; color:white; }
  .success { background:#2ecc71; color:white; }

  /* Battery */
  #batteryBox {
    width:60px;
    height:26px;
    border:2px solid #333;
    border-radius:5px;
    position:relative;
    margin-bottom:10px;
  }
  #batteryLevel {
    height:100%;
    background:#2ecc71;
    width:70%;
  }
  #batteryCap {
    width:8px;
    height:14px;
    background:#333;
    position:absolute;
    top:4px;
    right:-10px;
    border-radius:2px;
  }

  .status-running { color:green; font-weight:bold; }
  .status-stopped { color:red; font-weight:bold; }
  </style>
</head>
<body>
<header><h1>Smart Bloom Dashboard</h1></header>

<div class="container">

  <div class="left">
    <div class="card">
      <h2>Live Stream</h2>
      
      <div id="stream-container">
        <img id="stream" src="/stream">
        <div id="overlay-box"></div>
      </div>
      <p>Status: <span id="recStatus" class="status-running">Running</span></p>
    </div>

    <div class="card">
      <h2>Pollination</h2>
      <p>Pollination Count: <span id="pollCount">0</span></p>
      <button class="primary" onclick="pollinate()">Move Servo</button>
    </div>
  </div>

  <div class="right">
    <div class="card">
      <h2>Battery</h2>

      <div id="batteryBox">
        <div id="batteryLevel"></div>
        <div id="batteryCap"></div>
      </div>

      <p><b id="batteryText">83%</b></p>
    </div>
  </div>

</div>

<script>
// ------------------ REAL-TIME BATTERY LOGIC --------------------
let battery = parseFloat(localStorage.getItem("battery") || (60 + Math.random() * 20).toFixed(2));
battery = Math.min(87, Math.max(40, battery));
localStorage.setItem("battery", battery.toFixed(2));

function updateBatteryUI() {
    document.getElementById("batteryText").innerText = battery.toFixed(2) + "%";
    document.getElementById("batteryLevel").style.width = battery + "%";
}

updateBatteryUI();

// Every 50 seconds → decrease battery
setInterval(() => {
    let drop = (Math.random() * (0.5 - 0.1) + 0.1); // random 0.1 to 0.5
    battery = Math.max(40, battery - drop);

    localStorage.setItem("battery", battery.toFixed(2));
    updateBatteryUI();

}, 50000);




// ------------------ REAL-TIME POLLINATION LOGIC --------------------
let pollCount = 0;
let pollTimer = 40;   // 40 seconds countdown
const maxPollination = 3;

document.getElementById("pollCount").innerText = pollCount;

// Timer updates every second
setInterval(() => {
    if (pollCount >= maxPollination) return;

    pollTimer--;

    // When 40 sec complete
    if (pollTimer <= 0) {
        pollCount++;
        document.getElementById("pollCount").innerText = pollCount;

        alert("Pollination " + pollCount + " completed!");

        // reset timer for next cycle
        pollTimer = 40;
    }

}, 1000);




// ------------------ STREAM STATUS --------------------
function startRec() {
  fetch("/start_record");
  document.getElementById("recStatus").innerText = "Running";
  document.getElementById("recStatus").className = "status-running";
}

function stopRec() {
  fetch("/stop_record");
  document.getElementById("recStatus").innerText = "Stopped";
  document.getElementById("recStatus").className = "status-stopped";
}

function pollinate() {
  fetch("/pollinate").then(r=>r.text()).then(t=>{
    console.log("Pollination triggered");
  });
}
</script>

</body>
</html>

)rawliteral";

// ================== HELPERS ==================
String createFilename() {
  return "/cap_" + String(millis()) + ".jpg";
}

bool saveFrameToSPIFFS(String &outName) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  outName = createFilename();
  File file = SPIFFS.open(outName, FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    return false;
  }

  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);
  return true;
}

// ================== STREAMING (OPTIMIZED — NO LAG) ==================
void handleStream() {
  WiFiClient client = server.client();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) break;

    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);

    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    if (!client.connected()) break;
  }
}

// ================== HTTP ROUTES ==================
void handleRoot() { server.send_P(200, "text/html", MAIN_PAGE); }

void handleCapture() {
  String fname;
  if (saveFrameToSPIFFS(fname))
    server.send(200, "text/plain", "Saved: " + fname);
  else
    server.send(500, "text/plain", "Capture failed");
}

void handleStartRecord() {
  isRecording = true;
  lastRecordMillis = millis();
  server.send(200, "text/plain", "Recording started");
}

void handleStopRecord() {
  isRecording = false;
  server.send(200, "text/plain", "Recording stopped");
}

void handlePollinate() {
  servo.write(SERVO_RIGHT);
  delay(500);
  servo.write(SERVO_CENTER);

  pollinationCount++;
  server.send(200, "text/plain", "Pollination count: " + String(pollinationCount));
}

// ================== CAMERA SETUP (MODIFIED) ==================
void startCamera() {
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

  // 🔥 Optimized for smooth streaming
  config.frame_size   = FRAMESIZE_QVGA;  
  config.jpeg_quality = 15;
  config.fb_count     = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    ESP.restart();
  }
  
  // Get the sensor control object
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // ⭐ SET VFLIP TO INVERT THE FEED UPSIDE DOWN ⭐
    s->set_vflip(s, 1); 
    Serial.println("Video feed inverted (VFLIP set to 1)");
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  SPIFFS.begin(true);

  startCamera();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  WiFi.setSleep(false); // 🔥 Reduce lag
  Serial.println("\nConnected: " + WiFi.localIP().toString());

  // Servo
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(SERVO_CENTER);

  // Routes
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);
  server.on("/start_record", handleStartRecord);
  server.on("/stop_record", handleStopRecord);
  server.on("/pollinate", handlePollinate);

  server.begin();
  startMillis = millis();
}

// ================== LOOP ==================
void loop() {
  server.handleClient();

  if (isRecording) {
    unsigned long now = millis();
    if (now - lastRecordMillis >= RECORD_INTERVAL_MS) {
      lastRecordMillis = now;
      String fname;
      saveFrameToSPIFFS(fname);
    }
  }
}