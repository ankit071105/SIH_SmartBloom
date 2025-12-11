#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ================= WIFI CONFIG =================
const char* WIFI_SSID     = "Ayes Chinmay 1";
const char* WIFI_PASSWORD = "ayeschinmay";

WebServer server(80);

// ================= MOTOR PINS =================
int IN1 = 14;   
int IN2 = 27;   
int IN3 = 26;   
int IN4 = 25;   

// ================= SERVO PINS =================
Servo servo360_1;
Servo servo360_2;
Servo servo360_3;

int servo360_1_Pin = 12;
int servo360_2_Pin = 13;
int servo360_3_Pin = 15;

// ================= SERVO SPEEDS =================
int S1_LEFT_FAST  = 60;
int S1_RIGHT_FAST = 130;

int S_STOP  = 94;

int S2_LEFT_FAST  = 70;   // Pulley UP
int S2_RIGHT_FAST = 120;  // Pulley DOWN

int S3_LEFT_SLOW  = 88;   // Base
int S3_RIGHT_SLOW = 104;

// Servo auto-stop timers
unsigned long s1_stopTime = 0;
unsigned long s2_stopTime = 0;
unsigned long s3_stopTime = 0;

// MOTOR burst timer
unsigned long motorBurstEnd = 0;

// ================= SERVO STOP HELPERS =================
void stop360_1() { servo360_1.write(S_STOP); }
void stop360_2() { servo360_2.write(S_STOP); }
void stop360_3() { servo360_3.write(S_STOP); }

// ================= MOTOR STOP =================
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ================= NON-BLOCKING BURST MOVEMENT =================
void startBurstForward() {
  Serial.println("FORWARD BURST");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  motorBurstEnd = millis() + 80;
}

void startBurstBackward() {
  Serial.println("BACKWARD BURST");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  motorBurstEnd = millis() + 80;
}

void startBurstLeft() {
  Serial.println("LEFT BURST");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  motorBurstEnd = millis() + 80;
}

void startBurstRight() {
  Serial.println("RIGHT BURST");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  motorBurstEnd = millis() + 80;
}

// ================= WEB UI =================
const char MAIN_PAGE[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>
<title>Smart-Bloom Rover</title>
<style>
body { font-family: Arial; text-align:center; margin-top:20px; }
button { width:160px; height:55px; margin:10px; font-size:18px; }
</style>
</head>
<body>

<h1>ESP32 WiFi Rover</h1>

<button onclick="send('forward')">FORWARD</button><br>

<button onclick="send('left')">LEFT</button>
<button onclick="send('stop')">Stop</button>
<button onclick="send('right')">RIGHT</button><br>

<button onclick="send('back')">BACK</button>

<h2> Base (Slow)</h2>
<button onclick="send('s3_left')">RIGHT</button>
<button onclick="send('s3_right')">LEFT</button>

<h2> Pulley (Fast)</h2>
<button onclick="send('s2_right')">DOWN</button>
<button onclick="send('s2_left')">UP</button>

<h2> Pollinator (Full Speed)</h2>
<button onclick="send('s1_left')">BACKWARD</button>
<button onclick="send('s1_right')">FORWARD</button>


<script>
function send(cmd){ fetch("/cmd?c="+cmd); }
</script>

</body>
</html>
)====";

// ================= COMMAND HANDLER =================
void handleCommand() {

  if (!server.hasArg("c")) return;
  String cmd = server.arg("c");

  Serial.println(cmd);

  // -------- MOTOR MAPPINGS (UNCHANGED) --------
  if (cmd == "forward") startBurstLeft();     // UI mapping same
  else if (cmd == "back") startBurstRight();  // mapping same
  else if (cmd == "left") startBurstForward();// mapping same
  else if (cmd == "right") startBurstBackward();// mapping same
  else if (cmd == "stop") stopMotors();

  // -------- SERVO 1 --------
  else if (cmd == "s1_left") {
    servo360_1.write(S1_LEFT_FAST);
    s1_stopTime = millis() + 300;
  }
  else if (cmd == "s1_right") {
    servo360_1.write(S1_RIGHT_FAST);
    s1_stopTime = millis() + 300;
  }

  // -------- SERVO 2 --------
  else if (cmd == "s2_left") {
    servo360_2.write(S2_LEFT_FAST);
    s2_stopTime = millis() + 300;
  }
  else if (cmd == "s2_right") {
    servo360_2.write(S2_RIGHT_FAST);
    s2_stopTime = millis() + 300;
  }

  // -------- SERVO 3 --------
  else if (cmd == "s3_left") {
    servo360_3.write(S3_LEFT_SLOW);
    s3_stopTime = millis() + 300;
  }
  else if (cmd == "s3_right") {
    servo360_3.write(S3_RIGHT_SLOW);
    s3_stopTime = millis() + 300;
  }

  server.send(200, "text/plain", "OK");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servo360_1.attach(servo360_1_Pin, 500, 2400);
  servo360_2.attach(servo360_2_Pin, 500, 2400);
  servo360_3.attach(servo360_3_Pin, 500, 2400);

  stopMotors();
  stop360_1();
  stop360_2();
  stop360_3();

  Serial.println("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", MAIN_PAGE);
  });

  server.on("/cmd", HTTP_GET, handleCommand);

  server.begin();
}

// ================= LOOP =================
void loop() {

  server.handleClient();

  unsigned long now = millis();

  // ---- Auto-stop motor burst ----
  if (motorBurstEnd && now > motorBurstEnd) {
    stopMotors();
    motorBurstEnd = 0;
  }

  // ---- Auto-stop servos ----
  if (s1_stopTime && now > s1_stopTime) { stop360_1(); s1_stopTime = 0; }
  if (s2_stopTime && now > s2_stopTime) { stop360_2(); s2_stopTime = 0; }
  if (s3_stopTime && now > s3_stopTime) { stop360_3(); s3_stopTime = 0; }
}
