#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <DNSServer.h> 
#include <Preferences.h> // Secure Non-volatile internal hardware storage engine
#include "dashboard.h" 

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 6, /* data=*/ 5);
Preferences preferences; // Launches data flash memory register namespace

const String FIRMWARE_VERSION = PROJECT_VERSION; 
WebServer server(80);
DNSServer dnsServer; 

const int MIC_PIN = 1;          
const int BUTTON_PIN = 9;       
const int LED_PIN = 8;          

const int SAMPLE_WINDOW = 30;   
const int NUM_BARS = 10;        
const int DISPLAY_WIDTH = 72;   
const int DISPLAY_HEIGHT = 40;  
const int X_OFFSET = 28;        
const int Y_OFFSET = 12;        

int barHeights[NUM_BARS] = {0}; 
volatile int globalLiveHeight = 0; 
bool wifiConnected = false;        
bool apModeActive = false;

int currentMode = 2;            
int maxMappingCeiling = 550;    
String modeString = "MED";
bool ledState = false;          
bool oledDisplayEnabledState = true;
bool screenFlippedState = false;

unsigned long oledSplashEndMillis = 0;
unsigned long apShutdownTimeoutMillis = 0;
const unsigned long AP_LIFETIME_DURATION = 120000; 

const byte DNS_PORT = 53;
IPAddress ap_local_IP(4,3,2,1);
IPAddress ap_gateway(4,3,2,1);
IPAddress ap_subnet(255,255,255,0);

void handleRoot();
void handleGetData();
void handleSetSens();
void handleSaveWiFi();
void handleToggleDisplayFlip();
void handleToggleScreenPower();
void handleToggleLED();
void handleForceAP();
void handleRebootDevice();
void flashLEDFeedback();
void launchLocalFallbackAP();
void executeWiFiStationConnect();

void handleRoot() { server.send(200, "text/html", HTML_INDEX); }
void handleGetData() {
  String json = "{\"mode\":\"" + modeString + "\",\"led\":" + String(ledState ? "true" : "false") + ",\"version\":\"" + FIRMWARE_VERSION + "\"}";
  server.send(200, "application/json", json);
}
void flashLEDFeedback() {
  bool originalState = ledState;
  digitalWrite(LED_PIN, originalState ? HIGH : LOW); delay(40);
  digitalWrite(LED_PIN, originalState ? LOW : HIGH); 
}

void handleSetSens() {
  if (server.hasArg("mode")) {
    int modeNum = server.arg("mode").toInt(); currentMode = modeNum;
    if (currentMode == 0) { maxMappingCeiling = 3200; modeString = "V_LOW"; } 
    else if (currentMode == 1) { maxMappingCeiling = 1800; modeString = "LOW"; } 
    else if (currentMode == 2) { maxMappingCeiling = 550;  modeString = "MED"; } 
    else if (currentMode == 3) { maxMappingCeiling = 180;  modeString = "HIGH"; }
    oledSplashEndMillis = millis() + 2000; flashLEDFeedback();                    
  }
  server.send(200, "text/plain", "OK");
}

void handleSaveWiFi() {
  if (server.hasArg("ssid")) {
    String req_ssid = server.arg("ssid");
    String req_pass = server.hasArg("pass") ? server.arg("pass") : "";
    
    // Open Preferences namespace called "netcfg", read/write mode (false)
    preferences.begin("netcfg", false);
    preferences.putString("ssid", req_ssid);
    preferences.putString("pass", req_pass);
    preferences.end();
    
    server.send(200, "text/plain", "SAVED");
    delay(500);
    executeWiFiStationConnect(); // Immediately connect to the newly provisioned token maps
  } else {
    server.send(400, "text/plain", "MISSING SSID");
  }
}

void handleToggleDisplayFlip() {
  screenFlippedState = !screenFlippedState;
  u8g2.setDisplayRotation(screenFlippedState ? U8G2_R2 : U8G2_R0);
  oledSplashEndMillis = millis() + 1500; flashLEDFeedback();
  server.send(200, "text/plain", "OK");
}

void handleToggleScreenPower() {
  oledDisplayEnabledState = !oledDisplayEnabledState;
  u8g2.setPowerSave(oledDisplayEnabledState ? 0 : 1); flashLEDFeedback();
  server.send(200, "text/plain", "OK");
}

void handleToggleLED() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? LOW : HIGH); flashLEDFeedback();                      
  server.send(200, "text/plain", "OK");
}

void handleForceAP() { server.send(200, "text/plain", "OK"); delay(500); launchLocalFallbackAP(); }
void handleRebootDevice() { server.send(200, "text/plain", "OK"); delay(500); ESP.restart(); }

void launchLocalFallbackAP() {
  wifiConnected = false; apModeActive = true; WiFi.disconnect(true); WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet); WiFi.softAP("LeakDetector-AP", "12345678"); 
  dnsServer.start(DNS_PORT, "*", ap_local_IP); apShutdownTimeoutMillis = millis() + AP_LIFETIME_DURATION; flashLEDFeedback();
  if (oledDisplayEnabledState) {
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 12, "HOTSPOT ON"); u8g2.drawStr(X_OFFSET, Y_OFFSET + 24, "CAPTIVE ROUTE");
    u8g2.drawStr(X_OFFSET + 10, Y_OFFSET + 36, "IP: 4.3.2.1"); u8g2.sendBuffer();
  }
}

void executeWiFiStationConnect() {
  apModeActive = false; dnsServer.stop(); WiFi.softAPdisconnect(true); WiFi.disconnect(true); WiFi.mode(WIFI_STA);
  
  // Read saved network data dynamically out of flash storage keys
  preferences.begin("netcfg", true); // Open in read-only mode (true)
  String storedSSID = preferences.getString("ssid", "");
  String storedPASS = preferences.getString("pass", "");
  preferences.end();
  
  if (storedSSID == "") {
    launchLocalFallbackAP(); // No tokens exist inside flash -> Boot up Hotspot right away
    return;
  }
  
  if (oledDisplayEnabledState) {
    u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x12_tr); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 15, "CONNECTING"); u8g2.drawStr(X_OFFSET + 12, Y_OFFSET + 28, "TO WIFI..."); u8g2.sendBuffer();
  }
  
  WiFi.begin(storedSSID.c_str(), storedPASS.c_str()); unsigned long startConnectTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startConnectTime < 5000)) { delay(100); }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true; if (MDNS.begin("leakdetector")) { MDNS.addService("http", "tcp", 80); }
    if (oledDisplayEnabledState) {
      u8g2.clearBuffer(); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 12, "SYSTEM OK"); u8g2.drawStr(X_OFFSET + 2, Y_OFFSET + 24, "IP ADDRESS:");
      String ipStr = WiFi.localIP().toString(); u8g2.drawStr(X_OFFSET, Y_OFFSET + 36, ipStr.c_str()); u8g2.sendBuffer();
    }
  } else { launchLocalFallbackAP(); }
}

void setup() {
  Serial.begin(115200); u8g2.begin(); pinMode(BUTTON_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH); 
  u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x12_tr); u8g2.drawStr(X_OFFSET + 10, Y_OFFSET + 15, "LEAK DETECT");
  u8g2.drawStr(X_OFFSET + 14, Y_OFFSET + 28, FIRMWARE_VERSION.c_str()); u8g2.sendBuffer(); delay(2000);
  
  executeWiFiStationConnect();

  server.on("/", handleRoot); server.on("/getdata", handleGetData); server.on("/setsens", handleSetSens);
  server.on("/savewifi", handleSaveWiFi); // Registers the flash memory credential storage listener endpoint path
  server.on("/flipdisplay", handleToggleDisplayFlip); server.on("/togglescreen", handleToggleScreenPower); 
  server.on("/toggleled", handleToggleLED); server.on("/forceap", handleForceAP); server.on("/reboot", handleRebootDevice);
  server.onNotFound([]() { server.send(200, "text/html", HTML_INDEX); });
  
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='5;url=/'></head><body style='background:#121212;color:#00ff66;font-family:monospace;text-align:center;padding-top:50px;'><h3>OTA Flash Success!</h3></body></html>");
    delay(1000); ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (oledDisplayEnabledState) { u8g2.clearBuffer(); u8g2.drawStr(X_OFFSET + 8, Y_OFFSET + 20, "RECEIVING"); u8g2.drawStr(X_OFFSET + 12, Y_OFFSET + 32, "OTA FLASH"); u8g2.sendBuffer(); }
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
    } else if (upload.status == UPLOAD_FILE_END) { if (Update.end(true)) { Serial.println("OTA Complete!"); } }
  });
  server.begin(); delay(2000); analogReadResolution(12); 
}

void loop() {
  if (wifiConnected || apModeActive) server.handleClient();
  if (apModeActive) dnsServer.processNextRequest();
  if (apModeActive) {
    if (WiFi.softAPgetStationNum() > 0) { apShutdownTimeoutMillis = millis() + AP_LIFETIME_DURATION; }
    else if (millis() > apShutdownTimeoutMillis) {
      apModeActive = false; dnsServer.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
      if (oledDisplayEnabledState) { u8g2.clearBuffer(); u8g2.setFont(u8g2_font_6x12_tr); u8g2.drawStr(X_OFFSET + 10, Y_OFFSET + 20, "HOTSPOT TIMEOUT"); u8g2.drawStr(X_OFFSET + 14, Y_OFFSET + 32, "RADIO ASLEEP"); u8g2.sendBuffer(); }
      delay(2000);
    }
  }
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      currentMode = (currentMode + 1) % 4; 
      if (currentMode == 0) { maxMappingCeiling = 3200; modeString = "V_LOW"; } 
      else if (currentMode == 1) { maxMappingCeiling = 1800; modeString = "LOW"; } 
      else if (currentMode == 2) { maxMappingCeiling = 550;  modeString = "MED"; } 
      else if (currentMode == 3) { maxMappingCeiling = 180;  modeString = "HIGH"; }
      oledSplashEndMillis = millis() + 2000; while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
    }
  }
  unsigned long startMillis = millis(); unsigned int signalMax = 0; unsigned int signalMin = 4095;
  while (millis() - startMillis < SAMPLE_WINDOW) {
    int sample = analogRead(MIC_PIN); if (sample > signalMax) signalMax = sample; if (sample < signalMin) signalMin = sample;
  }
  unsigned int peakToPeak = signalMax - signalMin;
  int targetHeight = map(peakToPeak, 20, maxMappingCeiling, 0, DISPLAY_HEIGHT);
  targetHeight = constrain(targetHeight, 0, DISPLAY_HEIGHT); globalLiveHeight = targetHeight;
  for (int i = NUM_BARS - 1; i > 0; i--) { barHeights[i] = barHeights[i - 1]; }
  barHeights = targetHeight; 
  if (!oledDisplayEnabledState) return;

  u8g2.clearBuffer();
  if (millis() < oledSplashEndMillis) {
    u8g2.setFont(u8g2_font_6x12_tr); u8g2.drawStr(X_OFFSET + 6, Y_OFFSET + 12, "SCREEN ORIENT");
    u8g2.drawStr(X_OFFSET + 10, Y_OFFSET + 24, screenFlippedState ? "ROTATION: 180" : "ROTATION: NORMAL");
    u8g2.drawStr(X_OFFSET + 16, Y_OFFSET + 36, modeString.c_str());
  } else {
    int barWidth = (DISPLAY_WIDTH / NUM_BARS) - 2; 
    for (int i = 0; i < NUM_BARS; i++) {
      int xPos = X_OFFSET + (i * (barWidth + 2)); int yPos = Y_OFFSET + (DISPLAY_HEIGHT - barHeights[i]); u8g2.drawBox(xPos, yPos, barWidth, barHeights[i]);
    }
  }
  u8g2.sendBuffer();
}
