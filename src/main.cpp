#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include "dashboard.h" 

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);

const String FIRMWARE_VERSION = PROJECT_VERSION; 
const char* ssid     = "toi";
const char* password = "dcba@4321";

WebServer server(80);

const int MIC_PIN = 1;          // GPIO1 (Piezo input)
const int BUTTON_PIN = 9;       // GPIO9 (BOOT button)
const int LED_PIN = 8;          // GPIO8 (Onboard blue LED)

const int SAMPLE_WINDOW = 30;   
const int NUM_BARS = 10;        
const int DISPLAY_WIDTH = 72;   
const int DISPLAY_HEIGHT = 40;  
const int X_OFFSET = 28;        
const int Y_OFFSET = 12;        

int barHeights[NUM_BARS] = {0}; 
volatile int globalLiveHeight = 0; 
bool wifiConnected = false;        

int currentMode = 2;            // Boots straight into MEDIUM sensitivity
int maxMappingCeiling = 550;    
String modeString = "MED";
bool ledState = false;          

unsigned long oledSplashEndMillis = 0;

void handleRoot();
void handleGetData();
void handleSetSens();
void handleToggleLED();
void handleRebootDevice();
void flashLEDFeedback();

void handleRoot() {
  server.send(200, "text/html", HTML_INDEX);
}

void handleGetData() {
  String json = "{\"mode\":\"" + modeString + "\"" + 
                ",\"led\":" + String(ledState ? "true" : "false") + 
                ",\"version\":\"" + FIRMWARE_VERSION + "\"}";
  server.send(200, "application/json", json);
}
void flashLEDFeedback() {
  bool originalState = ledState;
  digitalWrite(LED_PIN, originalState ? HIGH : LOW); 
  delay(40);                                         
  digitalWrite(LED_PIN, originalState ? LOW : HIGH); 
}

void handleSetSens() {
  if (server.hasArg("mode")) {
    int modeNum = server.arg("mode").toInt();
    currentMode = modeNum;
    if (currentMode == 0) { maxMappingCeiling = 3200; modeString = "V_LOW"; } 
    else if (currentMode == 1) { maxMappingCeiling = 1800; modeString = "LOW"; } 
    else if (currentMode == 2) { maxMappingCeiling = 550;  modeString = "MED"; } 
    else if (currentMode == 3) { maxMappingCeiling = 180;  modeString = "HIGH"; }
    
    oledSplashEndMillis = millis() + 2000; 
    flashLEDFeedback();                    
  }
  server.send(200, "text/plain", "OK");
}

void handleToggleLED() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? LOW : HIGH); 
  flashLEDFeedback();                      
  server.send(200, "text/plain", "OK");
}

void handleRebootDevice() {
  server.send(200, "text/plain", "REBOOTING...");
  delay(500);
  ESP.restart(); 
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); 

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(X_OFFSET + 10, Y_OFFSET + 15, "LEAK DETECT");
  u8g2.drawStr(X_OFFSET + 14, Y_OFFSET + 28, FIRMWARE_VERSION.c_str());
  u8g2.sendBuffer();
  delay(2000);

  u8g2.clearBuffer();
  u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 15, "CONNECTING");
  u8g2.drawStr(X_OFFSET + 12, Y_OFFSET + 28, "TO WIFI...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  unsigned long startConnectTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startConnectTime < 5000)) { delay(100); }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    if (MDNS.begin("leakdetector")) { MDNS.addService("http", "tcp", 80); }
    
    server.on("/", handleRoot);
    server.on("/getdata", handleGetData);
    server.on("/setsens", handleSetSens);
    server.on("/toggleled", handleToggleLED);
    server.on("/reboot", handleRebootDevice);

    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='5;url=/'></head><body style='background:#121212;color:#00ff66;font-family:monospace;text-align:center;padding-top:50px;'><h3>OTA Flash Success! Returning home...</h3></body></html>");
      delay(1000); ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        u8g2.clearBuffer(); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 20, "RECEIVING");
        u8g2.drawStr(X_OFFSET + 12, Y_OFFSET + 32, "OTA FLASH"); u8g2.sendBuffer();
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { Serial.println("OTA Complete!"); }
      }
    });

    server.begin();
    u8g2.clearBuffer(); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 12, "SYSTEM OK");
    u8g2.drawStr(X_OFFSET + 2, Y_OFFSET + 24, "IP ADDRESS:");
    String ipStr = WiFi.localIP().toString(); u8g2.drawStr(X_OFFSET, Y_OFFSET + 36, ipStr.c_str()); u8g2.sendBuffer();
  } else {
    wifiConnected = false; WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
    u8g2.clearBuffer(); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 15, "SYSTEM OK");
    u8g2.drawStr(X_OFFSET + 4, Y_OFFSET + 28, "NO WIFI MOD"); u8g2.sendBuffer();
  }
  delay(3000);
  analogReadResolution(12); 
}

void loop() {
  if (wifiConnected) server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      currentMode = (currentMode + 1) % 4; 
      if (currentMode == 0) { maxMappingCeiling = 3200; modeString = "V_LOW"; } 
      else if (currentMode == 1) { maxMappingCeiling = 1800; modeString = "LOW"; } 
      else if (currentMode == 2) { maxMappingCeiling = 550;  modeString = "MED"; } 
      else if (currentMode == 3) { maxMappingCeiling = 180;  modeString = "HIGH"; }
      
      oledSplashEndMillis = millis() + 2000; 
      while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
    }
  }

  unsigned long startMillis = millis(); unsigned int signalMax = 0; unsigned int signalMin = 4095;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    int sample = analogRead(MIC_PIN); if (sample > signalMax) signalMax = sample; if (sample < signalMin) signalMin = sample;
  }
  unsigned int peakToPeak = signalMax - signalMin;
  int targetHeight = map(peakToPeak, 20, maxMappingCeiling, 0, DISPLAY_HEIGHT);
  targetHeight = constrain(targetHeight, 0, DISPLAY_HEIGHT);
  globalLiveHeight = targetHeight;

  for (int i = NUM_BARS - 1; i > 0; i--) { barHeights[i] = barHeights[i - 1]; }
  barHeights[0] = targetHeight; 

  u8g2.clearBuffer();
  if (millis() < oledSplashEndMillis) {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(X_OFFSET + 12, Y_OFFSET + 15, "MODE CHG:");
    u8g2.drawStr(X_OFFSET + 20, Y_OFFSET + 28, modeString.c_str());
  } else {
    int barWidth = (DISPLAY_WIDTH / NUM_BARS) - 2; 
    for (int i = 0; i < NUM_BARS; i++) {
      int xPos = X_OFFSET + (i * (barWidth + 2)); 
      int yPos = Y_OFFSET + (DISPLAY_HEIGHT - barHeights[i]); 
      u8g2.drawBox(xPos, yPos, barWidth, barHeights[i]);
    }
  }
  u8g2.sendBuffer();
}
