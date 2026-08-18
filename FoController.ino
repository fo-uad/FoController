#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BleGamepad.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define CURRENT_VERSION "1.0"
#define GITHUB_USER     "Fo-uad"
#define GITHUB_REPO     "FoController"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BUTTON_PIN1     27  
#define BUTTON_PIN2     12  
#define BUTTON_PIN3     14  
#define BUTTON_PIN4     13  
#define FN_BUTTON_PIN   18
#define VRX_JOYSTICK    34
#define VRY_JOYSTICK    35
#define JOYSTICK_SW     32  

#define RGB_RED_PIN     26
#define RGB_BLUE_PIN    25
#define RGB_GREEN_PIN   33

BleGamepad bleGamepad("FoController", "FoController", 100);

const char* routerSSID = "esp32";   
const char* routerPass = "esp32test";        

WebServer server(80);
String localIP = "Starting AP...";

bool lastConnectedState = false;
bool displayInitialized = false;
bool inTestMode = false;
bool isUpdating = false;

unsigned long b1PressTime = 0;
bool b1LongPressTriggered = false;

unsigned long fnPressStartTime = 0;
bool fnIsHeldLong = false;
bool modeComboTriggered = false;
bool prevFnState = HIGH;

float hue = 0.0; 

bool isDrawingMode = false;
unsigned long diagonalHoldStart = 0;
int drawCursorX = 64;
int drawCursorY = 32;

int drawSpeed = 1;
int drawEggStage = 0;
unsigned long drawEggTimer = 0;
bool drawB1WasPressed = false;
bool drawJoyWasUp = false;

enum InputCode { IN_UP, IN_DOWN, IN_LEFT, IN_RIGHT, IN_B1, IN_B2, IN_B3, IN_NONE };
InputCode konamiSequence[] = {IN_UP, IN_UP, IN_DOWN, IN_DOWN, IN_LEFT, IN_RIGHT, IN_B1, IN_B2};
InputCode arcadeSequence[] = {IN_DOWN, IN_DOWN, IN_B1};
InputCode screensaverMenuSequence[] = {IN_UP, IN_LEFT, IN_LEFT};
InputCode fireworksSequence[] = {IN_RIGHT, IN_RIGHT, IN_LEFT, IN_LEFT};
InputCode creditsSequence[] = {IN_B3, IN_B3, IN_B3};
InputCode inputBuffer[8] = {IN_NONE, IN_NONE, IN_NONE, IN_NONE, IN_NONE, IN_NONE, IN_NONE, IN_NONE};
bool joyWasUp = false, joyWasDown = false, joyWasLeft = false, joyWasRight = false;
bool b1WasPressed = false, b2WasPressed = false, b3WasPressed = false;
bool swWasPressed = false;

bool isArcadeMode = false;
bool isPaused = false;
enum ArcadeGame { MENU, SNAKE, TETRIS, PONG, BREAKOUT, FLAPPY };
ArcadeGame currentGame = MENU;
int menuSelection = 0;

enum ScreensaverType { SS_DISCO, SS_MATRIX, SS_SPIN, SS_GLITCH, SS_COUNT };
bool isScreensaverMode = false;
bool isScreensaverMenu = false;
ScreensaverType currentScreensaver = SS_DISCO;
int ssMenuSelection = 0;
bool fn4ComboTriggered = false;
bool b4WasPressedMenu = false;

#define MATRIX_COLS 16
int matrixY[MATRIX_COLS];
int matrixSpeed[MATRIX_COLS];

float spinAngle = 0.0;

int udGestureCount = 0;
bool udGestureLastWasUp = false;
unsigned long udGestureLastTime = 0;
bool isGlitchBurst = false;
unsigned long glitchBurstStart = 0;

bool isFireworksMode = false;
unsigned long fireworksStartTime = 0;
struct Particle { float x, y, vx, vy; bool alive; };
Particle fireworkParticles[20];

bool isCreditsMode = false;
unsigned long creditsStartTime = 0;
int creditsScrollX = 128;

int brPaddleX = 50;
float brBallX = 64, brBallY = 58;
int brBallDirX = 1, brBallDirY = -1;
bool brBricks[4][8];
int brScore = 0;
bool brBallLaunched = false;
unsigned long lastBreakoutTick = 0;

int flyBirdY = 32;
float flyBirdVel = 0;
int flyPipeX[2] = {128, 160};
int flyPipeGapY[2] = {20, 20};
int flyScore = 0;
bool flyStarted = false;
unsigned long lastFlyTick = 0;

int sX[64], sY[64], sLen = 4, sDir = 1; 
int foodX, foodY;
unsigned long lastGameTick = 0;
bool gameOver = false;

uint16_t board[20]; 
int tetX = 3, tetY = 0, tetRot = 0, tetPiece = 0;
unsigned long lastDropTick = 0;
int tetScore = 0;

int paddleY = 24;
int aiPaddleY = 24;
int ballX = 64, ballY = 32;
int ballDirX = 1, ballDirY = 1;
int pongScore = 0;
unsigned long lastPongTick = 0;

const uint16_t shapes[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},
  {0x0660, 0x0660, 0x0660, 0x0660},
  {0x0E40, 0x4C40, 0x4E00, 0x4640},
  {0x0E20, 0x44C0, 0x8E00, 0x6440},
  {0x0E80, 0xC440, 0x2E00, 0x4460},
  {0x06C0, 0x8C40, 0x06C0, 0x8C40},
  {0x0C60, 0x4C80, 0x0C60, 0x4C80}
};

struct ControllerPreset {
  String name;
  int b1, b2, b3, b4, b5, sw;
  int chord12;
  int longPressB1;
  String color;
  int swapXY;
  int invertX;
  int invertY;
};

#define MAX_PRESETS 2  
ControllerPreset presets[MAX_PRESETS];
int numPresets = 2; 
int activePresetIndex = 0;

int mapAxisBLE(int rawValue, bool invert);
void initFireworks();
void initMatrixScreensaver();
void initBreakout();
void initFlappy();

void setLEDColor(String hexColor) {
  if (hexColor.length() == 7 && hexColor.startsWith("#")) {
    long rgb = strtol(&hexColor[1], NULL, 16);
    int r = (rgb >> 16) & 0xFF;
    int g = (rgb >> 8) & 0xFF;
    int b = rgb & 0xFF;
    analogWrite(RGB_RED_PIN, r);
    analogWrite(RGB_GREEN_PIN, g);
    analogWrite(RGB_BLUE_PIN, b);
  }
}

void setRGB(int r, int g, int b) {
  analogWrite(RGB_RED_PIN, r);
  analogWrite(RGB_GREEN_PIN, g);
  analogWrite(RGB_BLUE_PIN, b);
}

void hsvToRgb(float h, float s, float v, int &r, int &g, int &b) {
  int i = int(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);
  switch(i % 6) {
    case 0: r = v * 255; g = t * 255; b = p * 255; break;
    case 1: r = q * 255; g = v * 255; b = p * 255; break;
    case 2: r = p * 255; g = v * 255; b = t * 255; break;
    case 3: r = p * 255; g = q * 255; b = v * 255; break;
    case 4: r = t * 255; g = p * 255; b = v * 255; break;
    case 5: r = v * 255; g = p * 255; b = q * 255; break;
  }
}

void saveConfig() {
  JsonDocument doc;
  doc["actPreset"] = activePresetIndex;
  doc["numPresets"] = numPresets;
  JsonArray presetsArray = doc["presets"].to<JsonArray>();
  for (int i = 0; i < numPresets; i++) {
    JsonObject p = presetsArray.add<JsonObject>();
    p["n"] = presets[i].name; p["1"] = presets[i].b1; p["2"] = presets[i].b2;
    p["3"] = presets[i].b3; p["4"] = presets[i].b4; p["5"] = presets[i].b5;
    p["sw"] = presets[i].sw; p["c12"] = presets[i].chord12; p["lp1"] = presets[i].longPressB1;
    p["col"] = presets[i].color; p["sxy"] = presets[i].swapXY; p["inx"] = presets[i].invertX;
    p["iny"] = presets[i].invertY;
  }
  File file = LittleFS.open("/config.json", FILE_WRITE);
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

void loadConfigFromMemory() {
  if (!LittleFS.begin(true)) return;
  File file = LittleFS.open("/config.json", FILE_READ);
  if (!file || file.size() == 0) {
    if (file) file.close();
    activePresetIndex = 0; numPresets = 2;
    presets[0] = {"PC", 1, 2, 3, 4, 5, 6, 7, 8, "#0000FF", 1, 1, 0};
    presets[1] = {"Phone", 1, 2, 3, 4, 5, 6, 7, 8, "#00FF00", 1, 1, 0};
    saveConfig();
    return;
  }
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return;
  
  activePresetIndex = doc["actPreset"] | 0;
  numPresets = 2; 
  JsonArray presetsArray = doc["presets"];
  int i = 0;

  for (JsonVariant v : presetsArray) {
    if (i >= MAX_PRESETS) break;
    JsonObject p = v.as<JsonObject>(); 
    
    presets[i].name = p["n"] | ((i == 0) ? "PC" : "Phone");
    presets[i].b1 = p["1"] | 1; presets[i].b2 = p["2"] | 2; presets[i].b3 = p["3"] | 3;
    presets[i].b4 = p["4"] | 4; presets[i].b5 = p["5"] | 5; presets[i].sw = p["sw"] | 6;
    presets[i].chord12 = p["c12"] | 7; presets[i].longPressB1 = p["lp1"] | 8;
    presets[i].color = p["col"] | (i == 0 ? "#0000FF" : "#00FF00");
    presets[i].swapXY = p["sxy"] | 1; presets[i].invertX = p["inx"] | 1; presets[i].invertY = p["iny"] | 0;
    i++;
  }
  
  if (i < 2) {
    presets[0] = {"PC", 1, 2, 3, 4, 5, 6, 7, 8, "#0000FF", 1, 1, 0};
    presets[1] = {"Phone", 1, 2, 3, 4, 5, 6, 7, 8, "#00FF00", 1, 1, 0};
  }
}

void updateMainScreen(bool connected) {
  if (!displayInitialized || inTestMode || isUpdating || isArcadeMode || isDrawingMode ||
      isScreensaverMode || isScreensaverMenu || isGlitchBurst || isFireworksMode || isCreditsMode) return;
  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, WHITE);
  display.setTextSize(1); display.setTextColor(BLACK); display.setCursor(4, 2);
  display.print(F("FoController v")); display.print(CURRENT_VERSION);
  display.setTextColor(WHITE); display.setCursor(0, 16); display.print(F("WiFi: ")); display.print(localIP);
  display.setCursor(0, 27); display.print(F("BT:   "));
  if (connected) display.print(F("CONNECTED")); else display.print(F("PAIRING..."));
  display.drawRoundRect(0, 39, 128, 25, 4, WHITE);
  if (activePresetIndex == 0) {
    display.setCursor(6, 47); display.setTextSize(1); display.print(F("MODE:"));
    display.setTextSize(2); display.setCursor(65, 44); display.print(F("PC"));
  } else {
    display.setCursor(6, 47); display.setTextSize(1); display.print(F("MODE:"));
    display.setTextSize(2); display.setCursor(50, 44); display.print(F("PHONE"));
  }
  display.display();
}

void renderTestModeScreen(int mappedX, int mappedY) {
  if (!displayInitialized) return;
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(0, 0); display.print(F("--- TEST DIAGNOSTICS ---"));
  display.setCursor(0, 12); display.printf("Preset Mode: [%s]", presets[activePresetIndex].name.c_str());
  display.setCursor(0, 24); display.printf("B1:%d B2:%d B3:%d B4:%d", !digitalRead(BUTTON_PIN1), !digitalRead(BUTTON_PIN2), !digitalRead(BUTTON_PIN3), !digitalRead(BUTTON_PIN4));
  display.setCursor(0, 35); display.printf("Fn:%d SW:%d", !digitalRead(FN_BUTTON_PIN), !digitalRead(JOYSTICK_SW));
  int printX = map(mappedX, 0, 32767, -100, 100); int printY = map(mappedY, 0, 32767, -100, 100);
  display.setCursor(0, 48); display.printf("X:%4d Y:%4d", printX, printY);
  int boxX = 96, boxY = 32, boxSize = 30;
  display.drawRect(boxX, boxY, boxSize, boxSize, WHITE);
  int px = map(mappedX, 0, 32767, 0, boxSize - 1), py = map(mappedY, 0, 32767, 0, boxSize - 1);
  display.fillRect(boxX + px - 1, boxY + py - 1, 3, 3, WHITE);
  display.display();
}

void performOTAUpdate() {
  isUpdating = true;
  display.clearDisplay(); display.setTextSize(1); display.setCursor(0, 0);
  display.println(F("--- OTA UPDATE ---")); display.println(F("Connecting To Network...")); display.display();
  WiFi.mode(WIFI_STA); WiFi.begin(routerSSID, routerPass);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) delay(500);
  
  if (WiFi.status() != WL_CONNECTED) {
    display.println(F("Hotspot Connect Fail!")); display.display(); delay(2000);
    ESP.restart(); return;
  }
  
  display.println(F("Checking GitHub...")); display.display();

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http; 
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  
  String versionURL = "https://github.com/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest/download/version.txt";
  
  http.begin(secureClient, versionURL); 
  int httpCode = http.GET(); 
  String latestVersion = "1.0"; 
  if (httpCode == 200) { latestVersion = http.getString(); latestVersion.trim(); } 
  http.end();
  
  display.clearDisplay(); display.setCursor(0,0); display.print(F("Remote Ver: ")); display.println(latestVersion); display.display(); delay(1500);
  if (latestVersion <= CURRENT_VERSION) {
    display.println(F("Already up to date!")); display.display(); delay(2000); ESP.restart(); return;
  }
  
  display.clearDisplay(); display.setCursor(0, 0); display.println(F("Update Available!")); display.setCursor(0, 16); display.print(F("Ver: ")); display.println(latestVersion);
  display.setCursor(0, 32); display.println(F("Do you want update?")); display.setCursor(0, 48); display.println(F("Yes:Fn+2  No:Fn+1")); display.display();
  
  bool waitingForChoice = true, proceedWithUpdate = false;
  while (waitingForChoice) {
    server.handleClient();
    if (digitalRead(FN_BUTTON_PIN) == LOW && digitalRead(BUTTON_PIN2) == LOW) { proceedWithUpdate = true; waitingForChoice = false; delay(300); } 
    else if (digitalRead(FN_BUTTON_PIN) == LOW && digitalRead(BUTTON_PIN1) == LOW) { proceedWithUpdate = false; waitingForChoice = false; delay(300); }
    delay(50);
  }
  
  if (!proceedWithUpdate) { ESP.restart(); return; }
  
  display.clearDisplay(); display.setCursor(0, 0); display.println(F("Downloading Update...")); display.display();
  
  String binURL = "https://github.com/" + String(GITHUB_USER) + "/" + String(GITHUB_REPO) + "/releases/latest/download/firmware.bin";
  http.begin(secureClient, binURL); 
  int code = http.GET();


  Update.onProgress([](size_t progress, size_t total) {
    static unsigned long lastProgressUpdate = 0;
    if (millis() - lastProgressUpdate > 250 || progress == total) {
      lastProgressUpdate = millis();
      display.clearDisplay(); 
      display.setTextSize(1); 
      display.setCursor(0, 0);
      display.println(F("Downloading Update...")); 
      
      int progressPct = (progress * 100) / total;
      display.setCursor(0, 16);
      display.printf("Progress: %d%%", progressPct);

      display.drawRect(14, 32, 100, 12, WHITE);
      display.fillRect(16, 34, (96 * progress) / total, 8, WHITE);
      
      display.display();
    }
  });
  
  if (code != 200 || !Update.begin(http.getSize())) { ESP.restart(); return; }
  WiFiClient *client = http.getStreamPtr();
  if (Update.writeStream(*client) == http.getSize()) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Update Success!")); 
    display.display();
    if (Update.end()) { delay(3000); ESP.restart(); }
  } else {
    Update.abort(); ESP.restart();
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>FoController Pro</title><style>body{background:#090d16;color:#f8fafc;font-family:sans-serif;padding:20px;display:flex;justify-content:center;}.container{width:100%;max-width:480px;}.card{background:#131c2e;border-radius:16px;padding:20px;}label{display:block;margin-top:10px;font-size:12px;color:#94a3b8;}input,select{width:100%;padding:10px;margin-top:4px;border-radius:8px;background:#0b111d;color:#fff;border:1px solid #1e293b;}.btn{width:100%;background:#3b82f6;color:#fff;padding:12px;border:none;border-radius:8px;margin-top:15px;cursor:pointer;}.assign-row{display:flex;gap:8px;} .assign-btn{background:#1e293b;color:#fff;border:none;padding:10px;border-radius:8px;cursor:pointer;}</style><script>let actId=null;function r(id){actId=id;document.getElementById(id).value='Press key...';window.addEventListener('keydown',e=>{e.preventDefault();if(actId){document.getElementById(actId).value=e.keyCode;actId=null;}},{once:true});}</script></head><body><div class='container'><div class='card'><h3>FoController Pro</h3><form action='/savePreset' method='GET'><label>Mode:</label><select name='activePreset' onchange='this.form.submit()'>";
  for(int i=0;i<numPresets;i++) html += "<option value='"+String(i)+"'"+(i==activePresetIndex?" selected":"")+">"+presets[i].name+"</option>";
  html += "</select></form><form action='/savePreset' method='GET'><input type='hidden' name='activePreset' value='"+String(activePresetIndex)+"'><label>Name:</label><input type='text' name='pName' value='"+presets[activePresetIndex].name+"'><label>LED Color:</label><input type='color' name='pColor' value='"+presets[activePresetIndex].color+"' style='height:40px;padding:0;'>";
  html += "<label>B1 Map:</label><div class='assign-row'><input type='text' id='b1' name='b1' value='"+String(presets[activePresetIndex].b1)+"'><button type='button' class='assign-btn' onclick=\"r('b1')\">Record</button></div>";
  html += "<label>B2 Map:</label><div class='assign-row'><input type='text' id='b2' name='b2' value='"+String(presets[activePresetIndex].b2)+"'><button type='button' class='assign-btn' onclick=\"r('b2')\">Record</button></div>";
  html += "<label>B3 Map:</label><div class='assign-row'><input type='text' id='b3' name='b3' value='"+String(presets[activePresetIndex].b3)+"'><button type='button' class='assign-btn' onclick=\"r('b3')\">Record</button></div>";
  html += "<label>B4 Map:</label><div class='assign-row'><input type='text' id='b4' name='b4' value='"+String(presets[activePresetIndex].b4)+"'><button type='button' class='assign-btn' onclick=\"r('b4')\">Record</button></div>";
  html += "<label>Fn Map:</label><div class='assign-row'><input type='text' id='b5' name='b5' value='"+String(presets[activePresetIndex].b5)+"'><button type='button' class='assign-btn' onclick=\"r('b5')\">Record</button></div>";
  html += "<label>L3 Map:</label><div class='assign-row'><input type='text' id='sw' name='sw' value='"+String(presets[activePresetIndex].sw)+"'><button type='button' class='assign-btn' onclick=\"r('sw')\">Record</button></div>";
  html += "<label>Chord(B1+B2):</label><div class='assign-row'><input type='text' id='c12' name='c12' value='"+String(presets[activePresetIndex].chord12)+"'><button type='button' class='assign-btn' onclick=\"r('c12')\">Record</button></div>";
  html += "<label>B1 Long Press:</label><div class='assign-row'><input type='text' id='lp1' name='lp1' value='"+String(presets[activePresetIndex].longPressB1)+"'><button type='button' class='assign-btn' onclick=\"r('lp1')\">Record</button></div>";
  html += "<div style='margin-top:15px;'><label><input type='checkbox' name='sxy' value='1' "+String(presets[activePresetIndex].swapXY?"checked":"")+"> Swap X/Y</label><label><input type='checkbox' name='inx' value='1' "+String(presets[activePresetIndex].invertX?"checked":"")+"> Invert X</label><label><input type='checkbox' name='iny' value='1' "+String(presets[activePresetIndex].invertY?"checked":"")+"> Invert Y</label></div>";
  html += "<button type='submit' class='btn'>Save</button></form></div></div></body></html>";
  server.send(200, "text/html", html);
}

void handleSavePreset() {
  if (server.hasArg("activePreset")) activePresetIndex = server.arg("activePreset").toInt();
  int i = activePresetIndex;
  if (server.hasArg("pName")) presets[i].name = server.arg("pName"); 
  if (server.hasArg("b1")) presets[i].b1 = server.arg("b1").toInt(); 
  if (server.hasArg("b2")) presets[i].b2 = server.arg("b2").toInt(); 
  if (server.hasArg("b3")) presets[i].b3 = server.arg("b3").toInt(); 
  if (server.hasArg("b4")) presets[i].b4 = server.arg("b4").toInt(); 
  if (server.hasArg("b5")) presets[i].b5 = server.arg("b5").toInt(); 
  if (server.hasArg("sw")) presets[i].sw = server.arg("sw").toInt(); 
  if (server.hasArg("c12")) presets[i].chord12 = server.arg("c12").toInt(); 
  if (server.hasArg("lp1")) presets[i].longPressB1 = server.arg("lp1").toInt(); 
  if (server.hasArg("pColor")) presets[i].color = server.arg("pColor"); 
  presets[i].swapXY = server.hasArg("sxy") ? 1 : 0;
  presets[i].invertX = server.hasArg("inx") ? 1 : 0;
  presets[i].invertY = server.hasArg("iny") ? 1 : 0;
  setLEDColor(presets[activePresetIndex].color);
  saveConfig(); 
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "Saved");
}

void runDrawingApp() {
  int rawX = analogRead(VRX_JOYSTICK);
  int rawY = analogRead(VRY_JOYSTICK);
  ControllerPreset activeP = presets[activePresetIndex];
  int processX = activeP.swapXY ? rawY : rawX;
  int processY = activeP.swapXY ? rawX : rawY;
  int mappedX = mapAxisBLE(processX, activeP.invertX);
  int mappedY = mapAxisBLE(processY, activeP.invertY);

  bool jUp = mappedY < 5000;
  bool jDown = mappedY > 27000;
  bool jLeft = mappedX < 5000;
  bool jRight = mappedX > 27000;

  bool b1 = digitalRead(BUTTON_PIN1) == LOW;
  bool b3 = digitalRead(BUTTON_PIN3) == LOW;
  bool b4 = digitalRead(BUTTON_PIN4) == LOW;

  if (b3 && b4) {
    isDrawingMode = false;
    drawSpeed = 1;
    drawEggStage = 0;
    setLEDColor(presets[activePresetIndex].color);
    updateMainScreen(bleGamepad.isConnected());
    for(int i=0; i<8; i++) inputBuffer[i] = IN_NONE;
    return;
  }

  if (drawEggStage > 0 && millis() - drawEggTimer > 1500) drawEggStage = 0;

  if (b1 && !drawB1WasPressed) {
    drawEggStage = 1;
    drawEggTimer = millis();
  }
  if (jUp && !drawJoyWasUp) {
    if (drawEggStage == 1) {
      drawEggStage = 2;
      drawEggTimer = millis();
    } else if (drawEggStage == 2) {
      drawSpeed = min(drawSpeed + 1, 10);
      drawEggStage = 0;
    }
  }
  drawB1WasPressed = b1;
  drawJoyWasUp = jUp;

  if (jUp && drawCursorY > 0) drawCursorY = max(0, drawCursorY - drawSpeed);
  if (jDown && drawCursorY < 63) drawCursorY = min(63, drawCursorY + drawSpeed);
  if (jLeft && drawCursorX > 0) drawCursorX = max(0, drawCursorX - drawSpeed);
  if (jRight && drawCursorX < 127) drawCursorX = min(127, drawCursorX + drawSpeed);

  display.drawPixel(drawCursorX, drawCursorY, WHITE);

  display.fillRect(0, 0, 42, 8, BLACK);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(F("Speed:"));
  display.print(drawSpeed);

  display.display();
  delay(25);
}

void playArcadeIntro() {
  for (int r = 0; r <= 80; r += 6) {
    display.clearDisplay();
    display.drawCircle(64, 32, r, WHITE);
    display.drawCircle(64, 32, (r > 20) ? r - 20 : 0, WHITE);
    display.fillRect(14, 22, 100, 20, BLACK);
    display.drawRect(14, 22, 100, 20, WHITE);
    display.setTextSize(2); display.setTextColor(WHITE);
    display.setCursor(20, 25); display.print("ARCADE!");
    display.display();
    delay(15);
  }
  delay(400);
}

void initTetris() {
  memset(board, 0, sizeof(board));
  tetX = 3; tetY = 0; tetScore = 0; gameOver = false;
  tetPiece = random(7); tetRot = 0;
}

bool tetrisCollision(int tX, int tY, int tR, int tP) {
  for (int i=0; i<16; i++) {
    if ((shapes[tP][tR] >> (15 - i)) & 1) {
      int px = tX + (i % 4), py = tY + (i / 4);
      if (px < 0 || px >= 10 || py >= 20 || (py >= 0 && (board[py] & (1 << px)))) return true;
    }
  }
  return false;
}

void lockTetrisPiece() {
  for (int i=0; i<16; i++) {
    if ((shapes[tetPiece][tetRot] >> (15 - i)) & 1) {
      int px = tetX + (i % 4), py = tetY + (i / 4);
      if (py >= 0) board[py] |= (1 << px);
    }
  }
  for (int y=19; y>=0; y--) {
    if (board[y] == 0x3FF) { 
      tetScore += 10;
      for (int k=y; k>0; k--) board[k] = board[k-1];
      board[0] = 0; y++; 
    }
  }
  tetPiece = random(7); tetRot = 0; tetX = 3; tetY = 0;
  if (tetrisCollision(tetX, tetY, tetRot, tetPiece)) gameOver = true;
}

void initBreakout() {
  brPaddleX = 56; brBallLaunched = false; brScore = 0; gameOver = false;
  brBallDirX = 1; brBallDirY = -1;
  for (int r = 0; r < 4; r++) for (int c = 0; c < 8; c++) brBricks[r][c] = true;
}

void initFlappy() {
  flyBirdY = 32; flyBirdVel = 0; flyScore = 0; gameOver = false; flyStarted = false;
  flyPipeX[0] = 128; flyPipeX[1] = 160;
  flyPipeGapY[0] = random(6, 38); flyPipeGapY[1] = random(6, 38);
}

void runArcade() {
  int rawX = analogRead(VRX_JOYSTICK);
  int rawY = analogRead(VRY_JOYSTICK);
  
  int processX = rawY; 
  int processY = rawX;
  int mappedX = mapAxisBLE(processX, true);  
  int mappedY = mapAxisBLE(processY, false); 

  bool jUp = mappedY < 5000;
  bool jDown = mappedY > 27000;
  bool jLeft = mappedX < 5000;
  bool jRight = mappedX > 27000;

  bool b1 = digitalRead(BUTTON_PIN1) == LOW;
  bool b3 = digitalRead(BUTTON_PIN3) == LOW;
  bool b4 = digitalRead(BUTTON_PIN4) == LOW;
  bool sw = digitalRead(JOYSTICK_SW) == LOW;

  if (b3 && b4) { 
    isArcadeMode = false;
    isPaused = false;
    setLEDColor(presets[activePresetIndex].color);
    updateMainScreen(bleGamepad.isConnected());
    for(int i=0; i<8; i++) inputBuffer[i] = IN_NONE;
    return;
  }

  if (currentGame != MENU) {
    if (sw && !swWasPressed) {
      isPaused = !isPaused;
    }
  }
  swWasPressed = sw;

  display.clearDisplay();

  if (isPaused) {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(24, 20);
    display.print("PAUSED");
    display.setTextSize(1);
    display.setCursor(14, 45);
    display.print("Press R3 to resume");
    display.display();
    joyWasUp = jUp; joyWasDown = jDown; joyWasLeft = jLeft; joyWasRight = jRight; b1WasPressed = b1;
    delay(15);
    return;
  }

  if (currentGame == MENU) {
    display.setTextSize(1); display.setCursor(40, 0); display.print("ARCADE");
    const char* gameNames[5] = {"SNAKE", "TETRIS", "PONG", "BREAKOUT", "FLAPPY BIRD"};
    for (int i = 0; i < 5; i++) {
      display.setCursor(10, 12 + i * 10);
      display.print(menuSelection == i ? "> " : "  ");
      display.print(gameNames[i]);
    }

    if (jDown && !joyWasDown) menuSelection = (menuSelection + 1) % 5;
    if (jUp && !joyWasUp) menuSelection = (menuSelection - 1 + 5) % 5;
    
    if (b1 && !b1WasPressed) {
      if (menuSelection == 0) currentGame = SNAKE;
      else if (menuSelection == 1) currentGame = TETRIS;
      else if (menuSelection == 2) currentGame = PONG;
      else if (menuSelection == 3) currentGame = BREAKOUT;
      else if (menuSelection == 4) currentGame = FLAPPY;
      
      gameOver = false;
      if (currentGame == SNAKE) {
        sLen = 3; sX[0]=16; sY[0]=8; sX[1]=15; sY[1]=8; sX[2]=14; sY[2]=8; sDir=1;
        foodX = random(32); foodY = random(16);
      } else if (currentGame == TETRIS) {
        initTetris();
      } else if (currentGame == PONG) {
        paddleY = 24; aiPaddleY = 24; ballX = 64; ballY = 32; ballDirX = 1; ballDirY = 1; pongScore = 0;
      } else if (currentGame == BREAKOUT) {
        initBreakout();
      } else if (currentGame == FLAPPY) {
        initFlappy();
      }
    }
  } 
  else if (currentGame == SNAKE) {
    if (gameOver) {
      display.setCursor(35, 20); display.print("GAME OVER");
      display.setCursor(35, 35); display.print("Score: "); display.print(sLen * 10);
      if (b1) currentGame = MENU;
    } else {
      if (jUp && sDir != 2) sDir = 0; else if (jRight && sDir != 3) sDir = 1;
      else if (jDown && sDir != 0) sDir = 2; else if (jLeft && sDir != 1) sDir = 3;
      
      if (millis() - lastGameTick > 150) {
        lastGameTick = millis();
        for (int i=sLen-1; i>0; i--) { sX[i] = sX[i-1]; sY[i] = sY[i-1]; }
        if (sDir == 0) sY[0]--; else if (sDir == 1) sX[0]++; else if (sDir == 2) sY[0]++; else if (sDir == 3) sX[0]--;
        if (sX[0] < 0 || sX[0] > 31 || sY[0] < 0 || sY[0] > 15) gameOver = true;
        for (int i=1; i<sLen; i++) if (sX[0] == sX[i] && sY[0] == sY[i]) gameOver = true;
        if (sX[0] == foodX && sY[0] == foodY) {
          if (sLen < 64) sLen++; 
          foodX = random(32); foodY = random(16);
        }
      }
      display.fillRect(foodX*4, foodY*4, 4, 4, WHITE);
      for (int i=0; i<sLen; i++) display.fillRect(sX[i]*4, sY[i]*4, 4, 4, WHITE);

      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.print(sLen * 10);
    }
  }
  else if (currentGame == TETRIS) {
    if (gameOver) {
      display.setCursor(35, 20); display.print("GAME OVER");
      display.setCursor(35, 35); display.print("Score: "); display.print(tetScore);
      if (b1) currentGame = MENU;
    } else {
      int tDelay = 400;
      if (jDown) tDelay = 50; 
      if (jLeft && !joyWasLeft && !tetrisCollision(tetX-1, tetY, tetRot, tetPiece)) tetX--;
      if (jRight && !joyWasRight && !tetrisCollision(tetX+1, tetY, tetRot, tetPiece)) tetX++;
      if (b1 && !b1WasPressed) {
        int nextRot = (tetRot + 1) % 4;
        if (!tetrisCollision(tetX, tetY, nextRot, tetPiece)) tetRot = nextRot;
      }
      if (millis() - lastDropTick > tDelay) {
        lastDropTick = millis();
        if (!tetrisCollision(tetX, tetY+1, tetRot, tetPiece)) tetY++; else lockTetrisPiece();
      }
      
      int offX = 49, offY = 2; 
      display.drawRect(offX-1, offY-1, 32, 62, WHITE);
      for (int y=0; y<20; y++) {
        for (int x=0; x<10; x++) {
          if (board[y] & (1 << x)) display.fillRect(offX + x*3, offY + y*3, 2, 2, WHITE);
        }
      }
      for (int i=0; i<16; i++) {
        if ((shapes[tetPiece][tetRot] >> (15 - i)) & 1) {
          display.fillRect(offX + (tetX + (i%4))*3, offY + (tetY + (i/4))*3, 2, 2, WHITE);
        }
      }

      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.print(tetScore);
    }
  }
  else if (currentGame == PONG) {
    if (gameOver) {
      display.setCursor(35, 20); display.print("GAME OVER");
      display.setCursor(35, 35); display.print("Score: "); display.print(pongScore);
      if (b1) currentGame = MENU;
    } else {
      if (jUp) paddleY -= 2;
      if (jDown) paddleY += 2;
      if (paddleY < 0) paddleY = 0;
      if (paddleY > 48) paddleY = 48;

      if (aiPaddleY + 8 < ballY) aiPaddleY += 1;
      else if (aiPaddleY + 8 > ballY) aiPaddleY -= 1;
      if (aiPaddleY < 0) aiPaddleY = 0;
      if (aiPaddleY > 48) aiPaddleY = 48;

      if (millis() - lastPongTick > 40) {
        lastPongTick = millis();
        ballX += ballDirX;
        ballY += ballDirY;

        if (ballY <= 0) { ballY = 0; ballDirY = -ballDirY; }
        if (ballY >= 63) { ballY = 63; ballDirY = -ballDirY; }

        if (ballX <= 6 && ballX >= 4) {
          if (ballY >= paddleY && ballY <= paddleY + 16) {
            ballDirX = -ballDirX;
            pongScore++;
          }
        }

        if (ballX >= 121 && ballX <= 123) {
          if (ballY >= aiPaddleY && ballY <= aiPaddleY + 16) {
            ballDirX = -ballDirX;
          }
        }

        if (ballX < 0 || ballX > 127) {
          gameOver = true;
        }
      }

      display.fillRect(4, paddleY, 3, 16, WHITE);
      display.fillRect(121, aiPaddleY, 3, 16, WHITE);
      display.fillRect(ballX, ballY, 2, 2, WHITE);

      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.print(pongScore);
    }
  }
  else if (currentGame == BREAKOUT) {
    if (gameOver) {
      display.setCursor(35, 20); display.print("GAME OVER");
      display.setCursor(35, 35); display.print("Score: "); display.print(brScore);
      if (b1 && !b1WasPressed) currentGame = MENU;
    } else {
      if (jLeft) brPaddleX -= 3;
      if (jRight) brPaddleX += 3;
      brPaddleX = constrain(brPaddleX, 0, 112);

      if (!brBallLaunched) {
        brBallX = brPaddleX + 8;
        brBallY = 58;
        if (b1 && !b1WasPressed) {
          brBallLaunched = true;
          brBallDirX = random(0, 2) == 0 ? -1 : 1;
          brBallDirY = -1;
        }
      } else if (millis() - lastBreakoutTick > 20) {
        lastBreakoutTick = millis();
        brBallX += brBallDirX; brBallY += brBallDirY;

        if (brBallX <= 0 || brBallX >= 127) brBallDirX = -brBallDirX;

        if (brBallY >= 8 && brBallY < 24) {
          int col = (int)brBallX / 16;
          int row = ((int)brBallY - 8) / 4;
          if (col >= 0 && col < 8 && row >= 0 && row < 4 && brBricks[row][col]) {
            brBricks[row][col] = false;
            brBallDirY = -brBallDirY;
            brScore += 5;
          }
        }

        if (brBallY >= 59 && brBallY <= 61 && brBallDirY > 0) {
          if (brBallX >= brPaddleX - 2 && brBallX <= brPaddleX + 18) {
            brBallDirY = -brBallDirY;
            brBallY = 58;
          }
        }

        if (brBallY > 63) gameOver = true;

        bool anyLeft = false;
        for (int r = 0; r < 4; r++) for (int c = 0; c < 8; c++) if (brBricks[r][c]) anyLeft = true;
        if (!anyLeft) gameOver = true;
      }

      for (int r = 0; r < 4; r++)
        for (int c = 0; c < 8; c++)
          if (brBricks[r][c]) display.fillRect(c * 16, 8 + r * 4, 15, 3, WHITE);

      display.fillRect(brPaddleX, 60, 16, 3, WHITE);
      display.fillRect((int)brBallX, (int)brBallY, 2, 2, WHITE);

      display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 0); display.print(brScore);
      if (!brBallLaunched) { display.setCursor(24, 0); display.print("B1:Launch"); }
    }
  }
  else if (currentGame == FLAPPY) {
    if (gameOver) {
      display.setCursor(35, 20); display.print("GAME OVER");
      display.setCursor(35, 35); display.print("Score: "); display.print(flyScore);
      if (b1 && !b1WasPressed) currentGame = MENU;
    } else {
      if (b1 && !b1WasPressed) {
        flyStarted = true;
        flyBirdVel = -3.2;
      }

      if (flyStarted && millis() - lastFlyTick > 30) {
        lastFlyTick = millis();
        flyBirdVel += 0.35; if (flyBirdVel > 4) flyBirdVel = 4;
        flyBirdY += (int)flyBirdVel;

        for (int p = 0; p < 2; p++) {
          flyPipeX[p] -= 2;
          if (flyPipeX[p] < -10) {
            flyPipeX[p] = 128;
            flyPipeGapY[p] = random(6, 38);
            flyScore++;
          }
          if (flyPipeX[p] < 16 && flyPipeX[p] + 8 > 12) {
            if (flyBirdY < flyPipeGapY[p] || flyBirdY > flyPipeGapY[p] + 18) gameOver = true;
          }
        }
        if (flyBirdY < 0 || flyBirdY > 63) gameOver = true;
      }

      for (int p = 0; p < 2; p++) {
        display.fillRect(flyPipeX[p], 0, 8, flyPipeGapY[p], WHITE);
        display.fillRect(flyPipeX[p], flyPipeGapY[p] + 18, 8, 64 - (flyPipeGapY[p] + 18), WHITE);
      }
      display.fillRect(14, flyBirdY, 4, 4, WHITE);

      display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 0); display.print(flyScore);
      if (!flyStarted) { display.setCursor(20, 30); display.print("Press B1 to fly"); }
    }
  }
  
  joyWasUp = jUp; joyWasDown = jDown; joyWasLeft = jLeft; joyWasRight = jRight; b1WasPressed = b1;
  display.display();
  delay(15);
}

void playBootAnimation() {
  if (!displayInitialized) return;

  for (int i = 0; i <= 100; i += 5) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(28, 25);
    display.print(F("FoController"));
    
    display.drawRect(28, 40, 72, 6, WHITE);
    display.fillRect(28, 40, (72 * i) / 100, 6, WHITE);
    
    display.display();
    delay(25);
  }
  delay(200);

  float dropY = 25.0;
  float dropVel = 0.0;
  for (int frame = 0; frame < 15; frame++) {
    display.clearDisplay();
    display.setCursor(28, 25); display.print(F("F"));
    display.setCursor(40, 25); display.print(F("C"));

    dropVel += 0.8; 
    dropY += dropVel;

    if (dropY < 64) {
      display.setCursor(34, (int)dropY); display.print(F("o"));
      display.setCursor(46, (int)dropY); display.print(F("o"));
      display.setCursor(52, (int)dropY); display.print(F("n"));
      display.setCursor(58, (int)dropY); display.print(F("t"));
      display.setCursor(64, (int)dropY); display.print(F("r"));
      display.setCursor(70, (int)dropY); display.print(F("o"));
      display.setCursor(76, (int)dropY); display.print(F("l"));
      display.setCursor(82, (int)dropY); display.print(F("l"));
      display.setCursor(88, (int)dropY); display.print(F("e"));
      display.setCursor(94, (int)dropY); display.print(F("r"));
    }
    display.display();
    delay(30);
  }

  float fX = 28.0, cX = 40.0;
  for (int frame = 0; frame <= 10; frame++) {
    display.clearDisplay();
    fX += (52.0 - 28.0) / 10.0;
    cX += (64.0 - 40.0) / 10.0;
    display.setCursor((int)fX, 25); display.print(F("F"));
    display.setCursor((int)cX, 25); display.print(F("C"));
    display.display();
    delay(30);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(52, 21);
  display.print(F("FC"));
  display.display();
  delay(700);

  for (int r = 0; r <= 80; r += 6) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(52, 21);
    display.print(F("FC"));
    
    display.fillCircle(64, 32, r, BLACK);
    display.drawCircle(64, 32, r, WHITE);
    display.display();
    delay(15);
  }
  display.clearDisplay();
  display.display();
}

void setup() {
  Serial.begin(115200);
  pinMode(RGB_RED_PIN, OUTPUT); pinMode(RGB_GREEN_PIN, OUTPUT); pinMode(RGB_BLUE_PIN, OUTPUT);
  loadConfigFromMemory(); setLEDColor(presets[activePresetIndex].color);
  Wire.begin(21, 22);

  if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    displayInitialized = true; 
    display.clearDisplay(); 
  }

  const uint8_t inputs[] = {BUTTON_PIN1, BUTTON_PIN2, BUTTON_PIN3, BUTTON_PIN4, FN_BUTTON_PIN, JOYSTICK_SW};
  for(int i = 0; i < 6; i++) pinMode(inputs[i], INPUT_PULLUP);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("FoController", "FoController V1");
  localIP = WiFi.softAPIP().toString();
  
  server.on("/", handleRoot);
  server.on("/savePreset", handleSavePreset);
  server.begin();

  bleGamepad.begin();
  
  playBootAnimation();
  updateMainScreen(false);
}

void pushInput(InputCode code) {
  for (int i = 0; i < 7; i++) inputBuffer[i] = inputBuffer[i+1];
  inputBuffer[7] = code;
  
  bool matchKonami = true;
  for (int i = 0; i < 8; i++) {
    if (inputBuffer[i] != konamiSequence[i]) { matchKonami = false; break; }
  }
  if (matchKonami) {
    currentScreensaver = SS_DISCO;
    isScreensaverMode = true;
  }

  bool matchArcade = true;
  for (int i = 0; i < 3; i++) {
    if (inputBuffer[5 + i] != arcadeSequence[i]) { matchArcade = false; break; }
  }
  if (matchArcade) {
    isArcadeMode = true;
    isPaused = false;
    currentGame = MENU;
    menuSelection = 0;
    playArcadeIntro();
  }

  bool matchScreensaverMenu = true;
  for (int i = 0; i < 3; i++) {
    if (inputBuffer[5 + i] != screensaverMenuSequence[i]) { matchScreensaverMenu = false; break; }
  }
  if (matchScreensaverMenu) {
    isScreensaverMenu = true;
    ssMenuSelection = 0;
  }

  bool matchFireworks = true;
  for (int i = 0; i < 4; i++) {
    if (inputBuffer[4 + i] != fireworksSequence[i]) { matchFireworks = false; break; }
  }
  if (matchFireworks) {
    isFireworksMode = true;
    fireworksStartTime = millis();
    initFireworks();
  }

  bool matchCredits = true;
  for (int i = 0; i < 3; i++) {
    if (inputBuffer[5 + i] != creditsSequence[i]) { matchCredits = false; break; }
  }
  if (matchCredits) {
    isCreditsMode = true;
    creditsStartTime = millis();
    creditsScrollX = 128;
  }
}

int mapAxisBLE(int rawValue, bool invert) {
  int center = 1850, deadzone = 250, outCenter = 16383, outMin = 0, outMax = 32767;
  if (abs(rawValue - center) < deadzone) return outCenter;
  int mapped;
  if (rawValue < center) mapped = constrain(map(rawValue, 0, center - deadzone, outMin, outCenter), outMin, outCenter);
  else mapped = constrain(map(rawValue, center + deadzone, 4095, outCenter, outMax), outCenter, outMax);
  return invert ? (outMax - mapped) : mapped;
}

const char* screensaverNames[SS_COUNT] = {"DISCO", "MATRIX", "SPIN", "GLITCH"};

void initMatrixScreensaver() {
  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixY[i] = random(-60, 0);
    matrixSpeed[i] = random(1, 3);
  }
}

void runDiscoScreensaver() {
  int r, g, b;
  hsvToRgb(hue, 1.0, 1.0, r, g, b);
  setRGB(r, g, b);
  hue += 0.05; if (hue >= 1.0) hue = 0.0;

  display.clearDisplay();
  int cx = 64, cy = 32;
  static int r1 = 0;
  display.drawCircle(cx, cy, r1, WHITE);
  display.drawCircle(cx, cy, (r1+15)%64, WHITE);
  display.drawCircle(cx, cy, (r1+30)%64, WHITE);
  r1 += 2; if (r1 > 64) r1 = 0;

  display.fillRect(15, 24, 98, 16, BLACK);
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(20, 25); display.print("PARTY!");
  display.display();
  delay(30);
}

void runMatrixScreensaver() {
  setRGB(0, 0, 0);
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  for (int i = 0; i < MATRIX_COLS; i++) {
    int x = i * 8;
    for (int j = 0; j < 6; j++) {
      int y = matrixY[i] - j * 9;
      if (y >= 0 && y < 64) {
        char c = 33 + random(90);
        display.setCursor(x, y);
        display.print(c);
      }
    }
    matrixY[i] += matrixSpeed[i];
    if (matrixY[i] > 64 + 54) {
      matrixY[i] = random(-60, -8);
      matrixSpeed[i] = random(1, 3);
    }
  }
  display.display();
  delay(45);
}

void runSpinScreensaver() {
  setRGB(0, 0, 0);
  display.clearDisplay();
  int cx = 64, cy = 32, radius = 26;
  spinAngle += 0.25; if (spinAngle > TWO_PI) spinAngle -= TWO_PI;

  int lx = cx + (int)(cos(spinAngle) * radius);
  int ly = cy + (int)(sin(spinAngle) * radius);
  display.drawLine(cx, cy, lx, ly, WHITE);
  display.drawCircle(cx, cy, radius, WHITE);

  int wobble = (int)(sin(spinAngle * 2) * 4);
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(52 + wobble, 21); display.print(F("FC"));

  display.setTextSize(1);
  display.setCursor(24, 54); display.print(F("FoController"));

  display.display();
  delay(20);
}

void runGlitchScreensaver() {
  static unsigned long lastFlicker = 0;
  if (millis() - lastFlicker > 100) {
    lastFlicker = millis();
    setRGB(random(0, 256), random(0, 256), random(0, 256));
  }
  display.clearDisplay();
  for (int i = 0; i < 30; i++) {
    int y = random(64);
    int h = random(1, 4);
    int w = random(20, 128);
    int x = random(-10, 108);
    display.fillRect(constrain(x, 0, 127), y, w, h, WHITE);
  }
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(random(0, 20), 24);
  display.print(F("FoController"));
  display.display();
  delay(35);
}

void runScreensaver() {
  bool b3 = digitalRead(BUTTON_PIN3) == LOW;
  bool b4 = digitalRead(BUTTON_PIN4) == LOW;
  if (b3 && b4) {
    isScreensaverMode = false;
    setLEDColor(presets[activePresetIndex].color);
    updateMainScreen(bleGamepad.isConnected());
    for (int i = 0; i < 8; i++) inputBuffer[i] = IN_NONE;
    return;
  }

  switch (currentScreensaver) {
    case SS_DISCO:  runDiscoScreensaver();  break;
    case SS_MATRIX: runMatrixScreensaver(); break;
    case SS_SPIN:   runSpinScreensaver();   break;
    case SS_GLITCH: runGlitchScreensaver(); break;
    default: break;
  }
}

void runScreensaverMenu() {
  bool b1 = digitalRead(BUTTON_PIN1) == LOW;
  bool b3 = digitalRead(BUTTON_PIN3) == LOW;
  bool b4 = digitalRead(BUTTON_PIN4) == LOW;

  if (b3 && b4) {
    isScreensaverMenu = false;
    for (int i = 0; i < 8; i++) inputBuffer[i] = IN_NONE;
    updateMainScreen(bleGamepad.isConnected());
    return;
  }

  if (b1 && !b1WasPressed) ssMenuSelection = (ssMenuSelection + 1) % SS_COUNT;

  if (b4 && !b4WasPressedMenu && !b3) {
    currentScreensaver = (ScreensaverType)ssMenuSelection;
    if (currentScreensaver == SS_MATRIX) initMatrixScreensaver();
    isScreensaverMenu = false;
    isScreensaverMode = true;
    for (int i = 0; i < 8; i++) inputBuffer[i] = IN_NONE;
    b1WasPressed = b1; b4WasPressedMenu = b4;
    return;
  }

  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(16, 0); display.print(F("SCREENSAVERS"));
  for (int i = 0; i < SS_COUNT; i++) {
    display.setCursor(20, 14 + i * 12);
    display.print(ssMenuSelection == i ? "> " : "  ");
    display.print(screensaverNames[i]);
  }
  display.setCursor(0, 56); display.print(F("B1:Next B4:Start"));
  display.display();

  b1WasPressed = b1; b4WasPressedMenu = b4;
  delay(80);
}

void runGlitchBurst() {
  if (millis() - glitchBurstStart > 2000) {
    isGlitchBurst = false;
    setLEDColor(presets[activePresetIndex].color);
    updateMainScreen(bleGamepad.isConnected());
    return;
  }
  display.clearDisplay();
  for (int i = 0; i < 25; i++) {
    int y = random(64);
    int h = random(1, 4);
    int w = random(20, 128);
    int x = random(-10, 108);
    display.fillRect(constrain(x, 0, 127), y, w, h, WHITE);
  }
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(random(0, 20), 24);
  display.print(F("FoController"));
  display.display();
  delay(20);
}

void initFireworks() {
  for (int i = 0; i < 20; i++) {
    float angle = random(0, 360) * PI / 180.0;
    float speed = random(10, 30) / 10.0;
    fireworkParticles[i] = {64.0, 32.0, (float)cos(angle) * speed, (float)sin(angle) * speed, true};
  }
}

void runFireworks() {
  if (millis() - fireworksStartTime > 2500) {
    isFireworksMode = false;
    updateMainScreen(bleGamepad.isConnected());
    for (int i = 0; i < 8; i++) inputBuffer[i] = IN_NONE;
    return;
  }
  display.clearDisplay();
  for (int i = 0; i < 20; i++) {
    if (fireworkParticles[i].alive) {
      fireworkParticles[i].x += fireworkParticles[i].vx;
      fireworkParticles[i].y += fireworkParticles[i].vy;
      fireworkParticles[i].vy += 0.05;
      if (fireworkParticles[i].x < 0 || fireworkParticles[i].x > 127 || fireworkParticles[i].y > 63) {
        fireworkParticles[i].alive = false;
      } else {
        display.drawPixel((int)fireworkParticles[i].x, (int)fireworkParticles[i].y, WHITE);
      }
    }
  }
  display.display();
  delay(20);
}

void runCredits() {
  if (millis() - creditsStartTime > 4000) {
    isCreditsMode = false;
    setLEDColor(presets[activePresetIndex].color);
    updateMainScreen(bleGamepad.isConnected());
    for (int i = 0; i < 8; i++) inputBuffer[i] = IN_NONE;
    return;
  }
  int r, g, b;
  hsvToRgb(hue, 1.0, 1.0, r, g, b);
  setRGB(r, g, b);
  hue += 0.03; if (hue >= 1.0) hue = 0.0;

  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(creditsScrollX, 28);
  display.print(F("Made with love by Fouad :)"));
  creditsScrollX -= 2;
  if (creditsScrollX < -170) creditsScrollX = 128;
  display.display();
  delay(30);
}

void loop() {
  server.handleClient();
  if (isUpdating) return;
  
  if (isArcadeMode) {
    runArcade();
    return;
  }

  if (isDrawingMode) {
    runDrawingApp();
    return;
  }

  if (isScreensaverMode) {
    runScreensaver();
    return;
  }

  if (isScreensaverMenu) {
    runScreensaverMenu();
    return;
  }

  if (isGlitchBurst) {
    runGlitchBurst();
    return;
  }

  if (isFireworksMode) {
    runFireworks();
    return;
  }

  if (isCreditsMode) {
    runCredits();
    return;
  }

  ControllerPreset activeP = presets[activePresetIndex];
  bool fnState = (digitalRead(FN_BUTTON_PIN) == LOW);
  bool b1Raw = (digitalRead(BUTTON_PIN1) == LOW);
  bool b2State = (digitalRead(BUTTON_PIN2) == LOW);
  bool b3State = (digitalRead(BUTTON_PIN3) == LOW);
  bool b4State = (digitalRead(BUTTON_PIN4) == LOW);
  bool swState = (digitalRead(JOYSTICK_SW) == LOW);

  int rawX = analogRead(VRX_JOYSTICK);
  int rawY = analogRead(VRY_JOYSTICK);
  int processX = activeP.swapXY ? rawY : rawX;
  int processY = activeP.swapXY ? rawX : rawY;
  int mappedX = mapAxisBLE(processX, activeP.invertX);
  int mappedY = mapAxisBLE(processY, activeP.invertY);

  bool jUp = mappedY < 5000; 
  bool jDown = mappedY > 27000;
  bool jLeft = mappedX < 5000; 
  bool jRight = mappedX > 27000;
  bool isDiagonal = (jUp && jLeft) || (jUp && jRight) || (jDown && jLeft) || (jDown && jRight);

  if (isDiagonal && !inTestMode && !isScreensaverMode && !isScreensaverMenu && !isArcadeMode && !isDrawingMode && !fnState) {
    if (diagonalHoldStart == 0) {
      diagonalHoldStart = millis();
    } else if (millis() - diagonalHoldStart >= 3000) {
      isDrawingMode = true;
      diagonalHoldStart = 0;
      display.clearDisplay();
      display.display();
      drawCursorX = 64;
      drawCursorY = 32;
      drawSpeed = 1;
      drawEggStage = 0;
      drawB1WasPressed = false;
      drawJoyWasUp = false;
      return;
    }
  } else {
    diagonalHoldStart = 0;
  }

  if (!fnState && !inTestMode) {
    if (jUp && !joyWasUp) pushInput(IN_UP); else if (jDown && !joyWasDown) pushInput(IN_DOWN);
    else if (jLeft && !joyWasLeft) pushInput(IN_LEFT); else if (jRight && !joyWasRight) pushInput(IN_RIGHT);
    if (b1Raw && !b1WasPressed) pushInput(IN_B1);
    if (b2State && !b2WasPressed) pushInput(IN_B2);
    if (b3State && !b3WasPressed) pushInput(IN_B3);

    if (jUp && !joyWasUp) {
      if (!udGestureLastWasUp && millis() - udGestureLastTime < 700) udGestureCount++; else udGestureCount = 1;
      udGestureLastWasUp = true; udGestureLastTime = millis();
    }
    if (jDown && !joyWasDown) {
      if (udGestureLastWasUp && millis() - udGestureLastTime < 700) udGestureCount++; else udGestureCount = 1;
      udGestureLastWasUp = false; udGestureLastTime = millis();
    }
    if (udGestureCount >= 10) {
      isGlitchBurst = true;
      glitchBurstStart = millis();
      udGestureCount = 0;
    }

    joyWasUp = jUp; joyWasDown = jDown; joyWasLeft = jLeft; joyWasRight = jRight;
    b1WasPressed = b1Raw; b2WasPressed = b2State; b3WasPressed = b3State;
  }

  bool currentConnected = bleGamepad.isConnected();
  if (currentConnected != lastConnectedState) {
    updateMainScreen(currentConnected);
    lastConnectedState = currentConnected;
  }

  static unsigned long fnPressTimer = 0;
  if (fnState && prevFnState == false) fnPressTimer = millis();
  if (!fnState && prevFnState == true) {
    unsigned long pressDuration = millis() - fnPressTimer;
    if (pressDuration > 40 && pressDuration < 400 && !b1Raw) {
      inTestMode = !inTestMode;
      if (!inTestMode) { setLEDColor(presets[activePresetIndex].color); updateMainScreen(currentConnected); } 
      else setLEDColor("#00FF00");
    }
  }
  prevFnState = fnState;

  static unsigned long otaHoldStart = 0;
  if (b1Raw && b2State && b3State && b4State && !fnState) {
    if (otaHoldStart == 0) otaHoldStart = millis();
    else if (millis() - otaHoldStart > 2000) {
      performOTAUpdate();
      otaHoldStart = 0;
    }
    return;
  } else {
    otaHoldStart = 0;
  }

  if (inTestMode) {
    renderTestModeScreen(mappedX, mappedY);
    if (fnState && b1Raw) {
      if (!modeComboTriggered) {
        modeComboTriggered = true; activePresetIndex = (activePresetIndex + 1) % numPresets;
        saveConfig(); setLEDColor("#00FF00");
      }
    } else modeComboTriggered = false;
    delay(100); return;
  }

  if (fnState && b1Raw) {
    if (!modeComboTriggered) {
      modeComboTriggered = true; activePresetIndex = (activePresetIndex + 1) % numPresets;
      saveConfig(); setLEDColor(presets[activePresetIndex].color); updateMainScreen(bleGamepad.isConnected());
    }
    bleGamepad.release(activeP.b1); bleGamepad.release(activeP.longPressB1); bleGamepad.release(activeP.b5);
    delay(15); return;
  } else modeComboTriggered = false;

  if (fnState && b4State) {
    if (!fn4ComboTriggered) {
      fn4ComboTriggered = true;
      if (isScreensaverMode && currentScreensaver == SS_MATRIX) {
        isScreensaverMode = false;
        setLEDColor(presets[activePresetIndex].color);
        updateMainScreen(bleGamepad.isConnected());
      } else {
        currentScreensaver = SS_MATRIX;
        initMatrixScreensaver();
        isScreensaverMode = true;
      }
    }
    bleGamepad.release(activeP.b4);
    delay(15); return;
  } else fn4ComboTriggered = false;

  if (fnState) {
    if (fnPressStartTime == 0) { fnPressStartTime = millis(); fnIsHeldLong = false; } 
    else if (!fnIsHeldLong && (millis() - fnPressStartTime > 400)) fnIsHeldLong = true;
  } else {
    fnPressStartTime = 0; fnIsHeldLong = false;
  }

  bool b1State = b1Raw && !fnState;
  bool chordActive = (b1State && b2State);
  
  if (b1State && !chordActive) {
    if (b1PressTime == 0) { b1PressTime = millis(); b1LongPressTriggered = false; } 
    else if (!b1LongPressTriggered && (millis() - b1PressTime > 600)) b1LongPressTriggered = true;
  } else {
    b1PressTime = 0; b1LongPressTriggered = false;
  }

  bleGamepad.setLeftThumb(mappedX, mappedY);

  if (chordActive) {
    bleGamepad.press(activeP.chord12); bleGamepad.release(activeP.b1); bleGamepad.release(activeP.b2);
  } else {
    bleGamepad.release(activeP.chord12);
    if (b1State) {
      if (b1LongPressTriggered) { bleGamepad.press(activeP.longPressB1); bleGamepad.release(activeP.b1); } 
      else { bleGamepad.press(activeP.b1); bleGamepad.release(activeP.longPressB1); }
    } else {
      bleGamepad.release(activeP.b1); bleGamepad.release(activeP.longPressB1);
    }
    if (b2State) bleGamepad.press(activeP.b2); else bleGamepad.release(activeP.b2);
  }

  if (b3State) bleGamepad.press(activeP.b3); else bleGamepad.release(activeP.b3);
  if (b4State) bleGamepad.press(activeP.b4); else bleGamepad.release(activeP.b4);
  if (fnState && fnIsHeldLong) bleGamepad.press(activeP.b5); else bleGamepad.release(activeP.b5);
  if (swState) bleGamepad.press(activeP.sw); else bleGamepad.release(activeP.sw);

  delay(15);
}