/*
 * Pixel Update Receiver for LilyGo T-Display-S3 (ST7789 170x320)
 * PlatformIO conversion.
 *
 * Receives per-pixel updates (x, y, RGB565) over TCP and applies them.
 * Protocol v4 (little-endian):
 *   Header: 'P' 'X' 'U' 'P' (4 bytes) + version (1 byte, 0x04) + frame_id (uint32 LE) + count (uint16)
 *   Body:   count entries of: x (uint16 LE), y (uint16 LE), color (uint16 LE)
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiServer.h>
#include <esp_heap_caps.h>

#include "WaitingLogo.h"

TFT_eSPI tft = TFT_eSPI();
uint8_t currentRotation = DISPLAY_ROTATION;
uint16_t displayWidth = TFT_WIDTH;
uint16_t displayHeight = TFT_HEIGHT;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* WIFI_PLACEHOLDER_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PLACEHOLDER_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* CONFIG_AP_NAME = "ESP32S3-Monitor-Setup";
const char* CONFIG_PORTAL_IP = "192.168.4.1";

WiFiServer server(8090);
WiFiClient client;

const char* MDNS_HOSTNAME = "esp32s3-monitor";
const char* MDNS_SERVICE = "desktopmonitor";
const char* MDNS_PROTOCOL = "tcp";

const uint8_t MAGIC[4] = {'P', 'X', 'U', 'P'};
const uint8_t PROTO_VERSION = 0x04;
const size_t HEADER_SIZE = 11;
const uint8_t MAGIC_RUN[4] = {'P', 'X', 'U', 'R'};
const uint8_t RUN_VERSION = 0x02;
const size_t RUN_HEADER_SIZE = 11;
const uint8_t MAGIC_ORIENTATION[4] = {'P', 'X', 'O', 'R'};
const uint8_t ORIENTATION_VERSION = 0x01;

bool swapBytesSetting = false;
bool clientActive = false;
unsigned long lastClientActivity = 0;
const unsigned long CLIENT_IDLE_TIMEOUT_MS = 3000;

unsigned long frameCount = 0;
unsigned long lastStats = 0;
unsigned long updatesApplied = 0;
uint32_t lastFrameId = 0;

struct PixelUpdate {
  uint16_t x;
  uint16_t y;
  uint16_t len;
  uint16_t color;
};

PixelUpdate* updateBuffer = nullptr;
uint32_t bufferCapacity = 0;
bool dmaEnabled = false;

bool ensureUpdateBuffer(uint32_t needed) {
  if (needed <= bufferCapacity && updateBuffer != nullptr) {
    return true;
  }

  PixelUpdate* tmp = static_cast<PixelUpdate*>(ps_malloc(needed * sizeof(PixelUpdate)));
  if (!tmp) {
    tmp = static_cast<PixelUpdate*>(malloc(needed * sizeof(PixelUpdate)));
  }
  if (!tmp) {
    Serial.println("Failed to allocate update buffer");
    return false;
  }

  if (updateBuffer) {
    free(updateBuffer);
  }

  updateBuffer = tmp;
  bufferCapacity = needed;
  return true;
}

bool readExactly(WiFiClient& c, uint8_t* dst, size_t len) {
  size_t got = 0;
  while (got < len && c.connected()) {
    int chunk = c.read(dst + got, len - got);
    if (chunk > 0) {
      got += static_cast<size_t>(chunk);
    } else {
      delay(1);
    }
  }
  return got == len;
}

void applyColorConfig() {
  tft.setSwapBytes(swapBytesSetting);
}

bool isLandscapeLayout() {
  return displayWidth > displayHeight;
}

void updateDisplayMetrics() {
  displayWidth = tft.width();
  displayHeight = tft.height();
}

void setDisplayRotationRuntime(uint8_t rotation) {
  currentRotation = rotation % 4;
  tft.setRotation(currentRotation);
  applyColorConfig();
  updateDisplayMetrics();
}

void drawHeaderLogo() {
  int16_t x = isLandscapeLayout() ? 12 : (displayWidth - WAITING_LOGO_WIDTH) / 2;
  int16_t y = isLandscapeLayout() ? 20 : 14;
  bool previousSwap = swapBytesSetting;
  tft.setSwapBytes(true);
  tft.pushImage(x, y, WAITING_LOGO_WIDTH, WAITING_LOGO_HEIGHT, waitingLogo);
  tft.setSwapBytes(previousSwap);
}

void showWaitingScreen() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderLogo();
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 30);
  } else {
    tft.setCursor(10, 154);
  }
  tft.println("STATUS:");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 44);
  } else {
    tft.setCursor(10, 168);
  }
  tft.println("Waiting for client");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 74);
  } else {
    tft.setCursor(10, 196);
  }
  tft.println("IP ADDRESS:");
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 90);
  } else {
    tft.setCursor(10, 212);
  }
  tft.println(WiFi.localIP().toString());
}

void showBootLogo() {
  tft.fillScreen(TFT_BLACK);
  drawHeaderLogo();
}

void showStatusScreen(const char* status, const String& detail = "") {
  showBootLogo();
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 30);
  } else {
    tft.setCursor(16, 206);
  }
  tft.println("STATUS:");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 46);
  } else {
    tft.setCursor(16, 222);
  }
  tft.println(status);
  if (detail.length() > 0) {
    tft.setTextSize(1);
    if (isLandscapeLayout()) {
      tft.setCursor(154, 78);
    } else {
      tft.setCursor(16, 250);
    }
    tft.println(detail);
  }
}

void showConfigPortalScreen() {
  showBootLogo();
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 24);
  } else {
    tft.setCursor(16, 184);
  }
  tft.println("SETUP WIFI:");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 40);
  } else {
    tft.setCursor(16, 200);
  }
  tft.println("Connect to AP:");
  if (isLandscapeLayout()) {
    tft.setCursor(154, 54);
  } else {
    tft.setCursor(16, 214);
  }
  tft.println(CONFIG_AP_NAME);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 76);
  } else {
    tft.setCursor(16, 238);
  }
  tft.println("Open:");
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  if (isLandscapeLayout()) {
    tft.setCursor(154, 90);
  } else {
    tft.setCursor(16, 252);
  }
  tft.println(CONFIG_PORTAL_IP);
}

void dropClient(const char* message = nullptr) {
  if (message != nullptr) {
    Serial.println(message);
  }
  if (client) {
    client.stop();
  }
  clientActive = false;
  showWaitingScreen();
}

bool hasDefaultWifiConfig() {
  return strlen(ssid) > 0 && strlen(password) > 0 &&
         strcmp(ssid, WIFI_PLACEHOLDER_SSID) != 0 &&
         strcmp(password, WIFI_PLACEHOLDER_PASSWORD) != 0;
}

bool shouldForceWifiSetup() {
  if (FORCE_WIFI_SETUP) {
    return true;
  }
  pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP);
  delay(10);
  return digitalRead(SETUP_BUTTON_PIN) == LOW;
}

bool connectWithTimeout(const char* connectSsid, const char* connectPassword, int maxAttempts = 30) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(connectSsid, connectPassword);
  for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < maxAttempts; attempts++) {
    delay(250);
    Serial.print('.');
  }
  return WiFi.status() == WL_CONNECTED;
}

bool connectSavedWifi(int maxAttempts = 30) {
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  for (int attempts = 0; WiFi.status() != WL_CONNECTED && attempts < maxAttempts; attempts++) {
    delay(250);
    Serial.print('.');
  }
  return WiFi.status() == WL_CONNECTED;
}

bool ensureWifiConnected() {
  bool forceSetup = shouldForceWifiSetup();
  if (forceSetup) {
    Serial.println("WiFi setup forced by config/button");
  }

  if (!forceSetup && hasDefaultWifiConfig()) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    showStatusScreen("Connecting", ssid);
    if (connectWithTimeout(ssid, password)) {
      return true;
    }
    Serial.println("\nWiFi fallback credentials failed");
  }

  if (!forceSetup) {
    Serial.println("Trying saved WiFi credentials");
    showStatusScreen("Connecting", "Saved network");
    if (connectSavedWifi()) {
      return true;
    }
    Serial.println("\nSaved WiFi credentials failed");
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(200);
  showConfigPortalScreen();
  Serial.print("Starting captive portal AP: ");
  Serial.println(CONFIG_AP_NAME);
  WiFiManager wm;
  wm.setConfigPortalBlocking(true);
  wm.setMinimumSignalQuality(5);
  wm.setTitle("Desktop Monitor Setup");
  if (forceSetup) {
    Serial.println("Clearing saved WiFi settings before captive portal");
    wm.resetSettings();
    WiFi.disconnect(true, true);
    delay(200);
  }
  bool connected = forceSetup ? wm.startConfigPortal(CONFIG_AP_NAME) : wm.autoConnect(CONFIG_AP_NAME);
  if (!connected) {
    Serial.println("Captive portal exited without WiFi connection");
    return false;
  }
  Serial.println("Captive portal connected successfully");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Pixel Update Receiver (PlatformIO) ===");

  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, HIGH);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  dmaEnabled = false;
  setDisplayRotationRuntime(DISPLAY_ROTATION);
  showStatusScreen("Booting...");

  if (!ensureWifiConnected()) {
    Serial.println("\nWiFi connection failed");
    showStatusScreen("WiFi FAIL", CONFIG_AP_NAME);
    while (true) {
      delay(1000);
    }
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService(MDNS_SERVICE, MDNS_PROTOCOL, 8090);
    Serial.print("mDNS ready: ");
    Serial.print(MDNS_HOSTNAME);
    Serial.println(".local");
  } else {
    Serial.println("mDNS start failed");
  }

  showStatusScreen("WiFi OK", WiFi.localIP().toString());
  delay(600);
  showWaitingScreen();

  server.begin();
  server.setNoDelay(true);
  Serial.println("Server listening on port 8090");
}

bool handleClient() {
  if (clientActive && client && !client.connected()) {
    dropClient("Client disconnected");
    return false;
  }

  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("Client connected");
      client.setNoDelay(true);
      client.setTimeout(50);
      clientActive = true;
      lastClientActivity = millis();
      frameCount = 0;
      updatesApplied = 0;
      showStatusScreen("Client OK", "Streaming active");
    }
  }

  if (!client || !client.connected()) {
    return false;
  }

  unsigned long now = millis();
  if (clientActive && client.available() == 0 && (now - lastClientActivity) > CLIENT_IDLE_TIMEOUT_MS) {
    dropClient("Client idle timeout");
    return false;
  }

  if (client.available() < 4) {
    return true;
  }

  lastClientActivity = now;

  uint8_t magicBuf[4];
  if (!readExactly(client, magicBuf, 4)) {
    dropClient("Client disconnected");
    return false;
  }

  bool isRun = (memcmp(magicBuf, MAGIC_RUN, 4) == 0);
  bool isPixel = (memcmp(magicBuf, MAGIC, 4) == 0);
  bool isOrientation = (memcmp(magicBuf, MAGIC_ORIENTATION, 4) == 0);

  if (!isRun && !isPixel && !isOrientation) {
    dropClient("Bad magic; flushing stream");
    return false;
  }

  if (isOrientation) {
    uint8_t config[2];
    if (!readExactly(client, config, sizeof(config))) {
      dropClient("Failed to read orientation packet; dropping client");
      return false;
    }
    if (config[0] != ORIENTATION_VERSION) {
      Serial.print("Unsupported orientation version: ");
      Serial.println(config[0], HEX);
      dropClient();
      return false;
    }
    setDisplayRotationRuntime(config[1]);
    showStatusScreen("Client OK", "Streaming active");
    return true;
  }

  if (isPixel) {
    uint8_t rest[HEADER_SIZE - 4];
    if (!readExactly(client, rest, sizeof(rest))) {
      dropClient("Failed to read pixel header; dropping client");
      return false;
    }
    if (rest[0] != PROTO_VERSION) {
      Serial.print("Unsupported pixel version: ");
      Serial.println(rest[0], HEX);
      dropClient();
      return false;
    }

    uint32_t frameId = static_cast<uint32_t>(rest[1]) |
                       (static_cast<uint32_t>(rest[2]) << 8) |
                       (static_cast<uint32_t>(rest[3]) << 16) |
                       (static_cast<uint32_t>(rest[4]) << 24);
    uint16_t count = rest[5] | (rest[6] << 8);

    if (count == 0) {
      frameCount++;
      lastFrameId = frameId;
      return true;
    }
    if (count > (displayWidth * displayHeight)) {
      Serial.print("Update count too large: ");
      Serial.println(count);
      dropClient();
      return false;
    }

    if (!ensureUpdateBuffer(count)) {
      dropClient("No buffer for updates; dropping client");
      return false;
    }

    uint8_t entry[6];
    for (uint16_t i = 0; i < count; i++) {
      if (!readExactly(client, entry, sizeof(entry))) {
        dropClient("Stream ended mid-frame; dropping client");
        return false;
      }
      updateBuffer[i].x = entry[0] | (static_cast<uint16_t>(entry[1]) << 8);
      updateBuffer[i].y = entry[2] | (static_cast<uint16_t>(entry[3]) << 8);
      updateBuffer[i].color = entry[4] | (static_cast<uint16_t>(entry[5]) << 8);
    }

    tft.startWrite();
    for (uint16_t i = 0; i < count; i++) {
      uint16_t x = updateBuffer[i].x;
      uint16_t y = updateBuffer[i].y;
      if (x < displayWidth && y < displayHeight) {
        tft.setAddrWindow(x, y, 1, 1);
        tft.writeColor(updateBuffer[i].color, 1);
        updatesApplied++;
      }
    }
    tft.endWrite();

    frameCount++;
    lastFrameId = frameId;
    unsigned long statsNow = millis();
    if (statsNow - lastStats > 2000) {
      Serial.print("Frames: ");
      Serial.print(frameCount);
      Serial.print(" (last frameId ");
      Serial.print(lastFrameId);
      Serial.print(") | Updates applied: ");
      Serial.println(updatesApplied);
      lastStats = statsNow;
    }
    return true;
  }

  uint8_t rest[RUN_HEADER_SIZE - 4];
  if (!readExactly(client, rest, sizeof(rest))) {
    dropClient("Failed to read run header; dropping client");
    return false;
  }
  if (rest[0] != RUN_VERSION) {
    Serial.print("Unsupported run version: ");
    Serial.println(rest[0], HEX);
    dropClient();
    return false;
  }

  uint32_t frameId = static_cast<uint32_t>(rest[1]) |
                     (static_cast<uint32_t>(rest[2]) << 8) |
                     (static_cast<uint32_t>(rest[3]) << 16) |
                     (static_cast<uint32_t>(rest[4]) << 24);
  uint16_t count = rest[5] | (rest[6] << 8);

  if (count == 0) {
    frameCount++;
    lastFrameId = frameId;
    return true;
  }
  if (count > (displayWidth * displayHeight)) {
    Serial.print("Run count too large: ");
    Serial.println(count);
    dropClient();
    return false;
  }

  if (!ensureUpdateBuffer(count)) {
    dropClient("No buffer for run updates; dropping client");
    return false;
  }

  uint8_t entry[6];
  for (uint16_t i = 0; i < count; i++) {
    if (!readExactly(client, entry, sizeof(entry))) {
      dropClient("Stream ended mid-run frame; dropping client");
      return false;
    }
    updateBuffer[i].y = entry[0] | (static_cast<uint16_t>(entry[1]) << 8);
    updateBuffer[i].x = entry[2];
    updateBuffer[i].len = entry[3];
    updateBuffer[i].color = entry[4] | (static_cast<uint16_t>(entry[5]) << 8);
  }

  tft.startWrite();
  for (uint16_t i = 0; i < count; i++) {
    uint16_t x0 = updateBuffer[i].x;
    uint16_t y = updateBuffer[i].y;
    uint16_t runLen = updateBuffer[i].len;
    if (x0 < displayWidth && y < displayHeight && runLen > 0 && (x0 + runLen) <= displayWidth) {
      tft.setAddrWindow(x0, y, runLen, 1);
      if (dmaEnabled) {
        tft.pushBlock(updateBuffer[i].color, runLen);
      } else {
        tft.writeColor(updateBuffer[i].color, runLen);
      }
      updatesApplied += runLen;
    }
  }
  tft.endWrite();

  frameCount++;
  lastFrameId = frameId;
  unsigned long statsNow = millis();
  if (statsNow - lastStats > 2000) {
    Serial.print("Frames: ");
    Serial.print(frameCount);
    Serial.print(" (last frameId ");
    Serial.print(lastFrameId);
    Serial.print(") | Updates applied: ");
    Serial.println(updatesApplied);
    lastStats = statsNow;
  }

  return true;
}

void loop() {
  handleClient();
  delay(1);
}
