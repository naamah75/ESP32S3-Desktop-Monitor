/*
 * Pixel Update Receiver for LilyGo T-Display-S3 (ST7789 170x320)
 * PlatformIO conversion.
 *
 * Receives per-pixel updates (x, y, RGB565) over TCP and applies them.
 * Protocol v3 (little-endian):
 *   Header: 'P' 'X' 'U' 'P' (4 bytes) + version (1 byte, 0x03) + frame_id (uint32 LE) + count (uint16)
 *   Body:   count entries of: x (uint8), y (uint16 LE), color (uint16 LE)
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include <esp_heap_caps.h>

TFT_eSPI tft = TFT_eSPI();

#if (DISPLAY_ROTATION == 1) || (DISPLAY_ROTATION == 3)
constexpr uint16_t DISPLAY_WIDTH = TFT_HEIGHT;
constexpr uint16_t DISPLAY_HEIGHT = TFT_WIDTH;
#else
constexpr uint16_t DISPLAY_WIDTH = TFT_WIDTH;
constexpr uint16_t DISPLAY_HEIGHT = TFT_HEIGHT;
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WiFiServer server(8090);
WiFiClient client;

const uint8_t MAGIC[4] = {'P', 'X', 'U', 'P'};
const uint8_t PROTO_VERSION = 0x03;
const size_t HEADER_SIZE = 11;
const uint8_t MAGIC_RUN[4] = {'P', 'X', 'U', 'R'};
const uint8_t RUN_VERSION = 0x02;
const size_t RUN_HEADER_SIZE = 11;

bool swapBytesSetting = false;

unsigned long frameCount = 0;
unsigned long lastStats = 0;
unsigned long updatesApplied = 0;
uint32_t lastFrameId = 0;

struct PixelUpdate {
  uint8_t x;
  uint16_t y;
  uint8_t len;
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

void showWaitingScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 20);
  tft.setTextSize(2);
  tft.println("Desktop");
  tft.setCursor(10, 40);
  tft.println("Monitor");
  tft.setCursor(10, 50);
  tft.setTextSize(1);
  tft.println("Status:");
  tft.setCursor(10, 62);
  tft.println("Waiting for client");
  tft.setCursor(10, 82);
  tft.println("IP Address:");
  tft.setCursor(10, 96);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println(WiFi.localIP().toString());
}

void showBootLogo() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRoundRect(16, 24, DISPLAY_WIDTH - 32, 74, 10, TFT_DARKCYAN);
  tft.drawRoundRect(16, 24, DISPLAY_WIDTH - 32, 74, 10, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
  tft.setTextSize(2);
  tft.setCursor(34, 38);
  tft.println("Desktop");
  tft.setCursor(34, 62);
  tft.println("Monitor");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(28, 118);
  tft.println("LilyGo T-Display-S3");
  tft.setCursor(48, 134);
  tft.print(DISPLAY_WIDTH);
  tft.print('x');
  tft.println(DISPLAY_HEIGHT);
}

void showStatusScreen(const char* status, const String& detail = "") {
  showBootLogo();
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(16, 170);
  tft.println("Status");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(16, 186);
  tft.println(status);
  if (detail.length() > 0) {
    tft.setTextSize(1);
    tft.setCursor(16, 214);
    tft.println(detail);
  }
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
  tft.setRotation(DISPLAY_ROTATION);
  applyColorConfig();
  showStatusScreen("Booting...");

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  showStatusScreen("Connecting", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(250);
    Serial.print('.');
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed");
    showStatusScreen("WiFi FAIL", ssid);
    while (true) {
      delay(1000);
    }
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  showStatusScreen("WiFi OK", WiFi.localIP().toString());
  delay(600);
  showWaitingScreen();

  server.begin();
  server.setNoDelay(true);
  Serial.println("Server listening on port 8090");
}

bool handleClient() {
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("Client connected");
      client.setNoDelay(true);
      client.setTimeout(50);
      frameCount = 0;
      updatesApplied = 0;
      showStatusScreen("Client OK", "Streaming active");
    }
  }

  if (!client || !client.connected()) {
    return false;
  }

  if (client.available() < 11) {
    return true;
  }

  uint8_t magicBuf[4];
  if (!readExactly(client, magicBuf, 4)) {
    client.stop();
    return false;
  }

  bool isRun = (memcmp(magicBuf, MAGIC_RUN, 4) == 0);
  bool isPixel = (memcmp(magicBuf, MAGIC, 4) == 0);

  if (!isRun && !isPixel) {
    Serial.println("Bad magic; flushing stream");
    client.stop();
    return false;
  }

  if (isPixel) {
    uint8_t rest[HEADER_SIZE - 4];
    if (!readExactly(client, rest, sizeof(rest))) {
      Serial.println("Failed to read pixel header; dropping client");
      client.stop();
      return false;
    }
    if (rest[0] != PROTO_VERSION) {
      Serial.print("Unsupported pixel version: ");
      Serial.println(rest[0], HEX);
      client.stop();
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
    if (count > (DISPLAY_WIDTH * DISPLAY_HEIGHT)) {
      Serial.print("Update count too large: ");
      Serial.println(count);
      client.stop();
      return false;
    }

    if (!ensureUpdateBuffer(count)) {
      Serial.println("No buffer for updates; dropping client");
      client.stop();
      return false;
    }

    uint8_t entry[5];
    for (uint16_t i = 0; i < count; i++) {
      if (!readExactly(client, entry, sizeof(entry))) {
        Serial.println("Stream ended mid-frame; dropping client");
        client.stop();
        return false;
      }
      updateBuffer[i].x = entry[0];
      updateBuffer[i].y = entry[1] | (static_cast<uint16_t>(entry[2]) << 8);
      updateBuffer[i].color = entry[3] | (static_cast<uint16_t>(entry[4]) << 8);
    }

    tft.startWrite();
    for (uint16_t i = 0; i < count; i++) {
      uint8_t x = updateBuffer[i].x;
      uint8_t y = updateBuffer[i].y;
      if (x < DISPLAY_WIDTH && y < DISPLAY_HEIGHT) {
        tft.setAddrWindow(x, y, 1, 1);
      tft.writeColor(updateBuffer[i].color, 1);
      updatesApplied++;
    }
    }
    tft.endWrite();

    frameCount++;
    lastFrameId = frameId;
    unsigned long now = millis();
    if (now - lastStats > 2000) {
      Serial.print("Frames: ");
      Serial.print(frameCount);
      Serial.print(" (last frameId ");
      Serial.print(lastFrameId);
      Serial.print(") | Updates applied: ");
      Serial.println(updatesApplied);
      lastStats = now;
    }
    return true;
  }

  uint8_t rest[RUN_HEADER_SIZE - 4];
  if (!readExactly(client, rest, sizeof(rest))) {
    Serial.println("Failed to read run header; dropping client");
    client.stop();
    return false;
  }
  if (rest[0] != RUN_VERSION) {
    Serial.print("Unsupported run version: ");
    Serial.println(rest[0], HEX);
    client.stop();
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
  if (count > (DISPLAY_WIDTH * DISPLAY_HEIGHT)) {
    Serial.print("Run count too large: ");
    Serial.println(count);
    client.stop();
    return false;
  }

  if (!ensureUpdateBuffer(count)) {
    Serial.println("No buffer for run updates; dropping client");
    client.stop();
    return false;
  }

  uint8_t entry[6];
  for (uint16_t i = 0; i < count; i++) {
    if (!readExactly(client, entry, sizeof(entry))) {
      Serial.println("Stream ended mid-run frame; dropping client");
      client.stop();
      return false;
    }
    updateBuffer[i].y = entry[0] | (static_cast<uint16_t>(entry[1]) << 8);
    updateBuffer[i].x = entry[2];
    updateBuffer[i].len = entry[3];
    updateBuffer[i].color = entry[4] | (static_cast<uint16_t>(entry[5]) << 8);
  }

  tft.startWrite();
  for (uint16_t i = 0; i < count; i++) {
    uint8_t x0 = updateBuffer[i].x;
    uint8_t y = updateBuffer[i].y;
    uint8_t runLen = updateBuffer[i].len;
    if (x0 < DISPLAY_WIDTH && y < DISPLAY_HEIGHT && runLen > 0 && (x0 + runLen) <= DISPLAY_WIDTH) {
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
  unsigned long now = millis();
  if (now - lastStats > 2000) {
    Serial.print("Frames: ");
    Serial.print(frameCount);
    Serial.print(" (last frameId ");
    Serial.print(lastFrameId);
    Serial.print(") | Updates applied: ");
    Serial.println(updatesApplied);
    lastStats = now;
  }

  return true;
}

void loop() {
  handleClient();
  if (client && !client.connected()) {
    Serial.println("Client disconnected");
    showWaitingScreen();
  }
  delay(1);
}
