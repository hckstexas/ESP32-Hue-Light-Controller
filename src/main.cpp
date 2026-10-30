#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "ButtonInput.h"

// Required local file; do not silently substitute example credentials.
#include "../include/config.h"

namespace {

// BOOT is the only user control. Rear KEY21 is not a room button.
constexpr uint8_t kActionButtonPin = 0;
constexpr uint8_t kBacklightPin = 32;
constexpr uint8_t kBacklightChannel = 7;

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kButtonArmMs = 250;
constexpr uint32_t kLongPressMs = 500;
constexpr uint32_t kDoubleTapMs = 350;
constexpr uint32_t kDimStepMs = 90;
constexpr uint32_t kScreenTimeoutMs = 30000;
constexpr uint32_t kSleepCloseoutMs = 400;
constexpr uint8_t kDimStep = 5;
constexpr uint8_t kMinimumBrightness = 1;
constexpr uint8_t kMaximumBrightness = 254;
constexpr uint8_t kBacklightFullDuty = 255;
constexpr uint32_t kWifiRetryMs = 5000;
constexpr uint32_t kStateRefreshMs = 10000;

TFT_eSPI display;
TFT_eSprite frame(&display);
WiFiClient wifiClient;
bool displayReady = false;
bool screenDirty = true;

ButtonState actionButton{kActionButtonPin};
BootGestureState bootGesture;

size_t activeRoom = 0;
bool lightsOn = false;
bool stateKnown = false;
uint8_t brightness = 0;
bool brightnessKnown = false;
bool dimmingActive = false;
bool dimDownOnNextHold = true;
bool screenAwake = true;
int8_t dimDirection = -1;
uint32_t lastDimStepAt = 0;
uint32_t lastWifiRetryAt = 0;
uint32_t lastUserActivityAt = 0;
uint32_t lastStateRefreshAt = 0;
bool wifiWasConnected = false;
String statusText = "Starting";

String groupUrl(const char* suffix) {
  String url = "http://";
  url += HUE_BRIDGE_IP;
  url += "/api/";
  url += HUE_API_KEY;
  url += "/groups/";
  url += ROOMS[activeRoom].groupId;
  url += suffix;
  return url;
}

void setStatus(const String& message) {
  if (statusText != message) {
    screenDirty = true;
  }
  statusText = message;
}

void drawScreen() {
  if (!screenAwake || !displayReady || !screenDirty) {
    return;
  }

  constexpr int16_t kRoomNameY = 26;
  constexpr int16_t kRoomTrackY = 86;
  constexpr int16_t kStateY = 109;
  constexpr int16_t kLevelY = 167;
  constexpr int16_t kDividerY = 202;
  constexpr int16_t kReadyY = 216;
  const int16_t centerX = display.width() / 2;
  const bool brightTheme = stateKnown && lightsOn;
  const uint16_t background =
      brightTheme ? display.color565(237, 242, 215) : TFT_BLACK;
  const uint16_t primaryText =
      brightTheme ? display.color565(36, 53, 44) : TFT_WHITE;
  const uint16_t mutedText =
      brightTheme ? display.color565(104, 117, 101) : TFT_LIGHTGREY;
  const uint16_t divider =
      brightTheme ? display.color565(171, 182, 154) : TFT_DARKGREY;
  const uint16_t activeRoomMark =
      brightTheme ? display.color565(83, 108, 68)
                  : display.color565(216, 242, 107);
  const uint16_t inactiveRoomMark =
      brightTheme ? divider : display.color565(60, 72, 65);

  // Compose off-screen so the LCD never shows an intermediate black/empty frame.
  frame.fillSprite(background);
  if (brightTheme) {
    // The board has no separate bezel light; use a subtle LCD edge glow.
    frame.drawRoundRect(1, 1, display.width() - 2, display.height() - 2, 6,
                          display.color565(196, 216, 150));
    frame.drawRoundRect(3, 3, display.width() - 6, display.height() - 6, 4,
                          display.color565(228, 237, 207));
  } else {
    frame.drawRoundRect(1, 1, display.width() - 2, display.height() - 2, 6,
                          display.color565(25, 31, 28));
  }
  frame.setTextDatum(TL_DATUM);
  frame.setTextSize(1);
  frame.setTextColor(primaryText, background);
  String roomName = ROOMS[activeRoom].name;
  if (frame.textWidth(roomName, 4) <= display.width() - 12) {
    frame.drawCentreString(roomName, centerX, kRoomNameY + 12, 4);
  } else {
    // Long room names retain the large font on two lines when possible.
    const int space = roomName.lastIndexOf(' ');
    const int split = space > 0 ? space : roomName.length() / 2;
    String first = roomName.substring(0, split);
    String second = roomName.substring(split + (space > 0 ? 1 : 0));
    const int font = frame.textWidth(first, 4) <= display.width() - 12 &&
                             frame.textWidth(second, 4) <= display.width() - 12
                         ? 4 : 2;
    frame.drawCentreString(first, centerX, kRoomNameY, font);
    frame.drawCentreString(second, centerX, kRoomNameY + 28, font);
  }

  const int16_t trackStartX =
      centerX - static_cast<int16_t>((ROOM_COUNT - 1) * 16 / 2) - 5;
  for (size_t roomIndex = 0; roomIndex < ROOM_COUNT; ++roomIndex) {
    const int16_t markX =
        trackStartX + static_cast<int16_t>(roomIndex * 16);
    if (roomIndex == activeRoom) {
      frame.fillRoundRect(markX, kRoomTrackY, 10, 4, 2, activeRoomMark);
    } else {
      frame.fillCircle(markX + 5, kRoomTrackY + 2, 2, inactiveRoomMark);
    }
  }

  if (!stateKnown) {
    frame.setTextColor(TFT_ORANGE, background);
    frame.drawCentreString("NO STATE", centerX, kStateY + 15, 2);
  } else {
    frame.setTextColor(lightsOn ? display.color565(75, 104, 42)
                               : TFT_LIGHTGREY, background);
    frame.setTextSize(2);
    frame.drawCentreString(lightsOn ? "ON" : "OFF", centerX, kStateY, 4);
    frame.setTextSize(1);
    frame.setTextColor(primaryText, background);
    const String level = brightnessKnown
        ? String((brightness * 100U + 127U) / 254U) + "%" : "--%";
    frame.drawCentreString(level, centerX, kLevelY, 4);
  }

  frame.drawFastHLine(12, kDividerY, display.width() - 24, divider);

  frame.setTextColor(mutedText, background);
  const bool statusFitsLarge = frame.textWidth(statusText, 2) <= display.width() - 12;
  frame.drawCentreString(statusText, centerX,
                           statusFitsLarge ? kReadyY : kReadyY + 4,
                           statusFitsLarge ? 2 : 1);
  frame.pushSprite(0, 0);
  screenDirty = false;
}

void setBacklight(uint8_t duty) {
  ledcWrite(kBacklightChannel, duty);
}

void wakeDisplay() {
  screenAwake = true;
  screenDirty = true;
  drawScreen();
  setBacklight(kBacklightFullDuty);
}

void noteUserActivity() {
  lastUserActivityAt = millis();
  if (!screenAwake) {
    wakeDisplay();
  }
}

bool cancelCloseOutForButtonPress() {
  const bool actionPressed =
      actionButton.armed && digitalRead(kActionButtonPin) == LOW;
  if (!actionPressed) {
    return false;
  }

  if (actionPressed) {
    // A press during the close-out is only meant to keep/wake the display.
    actionButton.wakeOnlyUntilRelease = true;
    actionButton.rawHigh = false;
    actionButton.rawChangedAt = millis();
    bootGesture = BootGestureState{};
  }
  lastUserActivityAt = millis();
  screenAwake = true;
  screenDirty = true;
  setBacklight(kBacklightFullDuty);
  drawScreen();
  return true;
}

void closeOutDisplay() {
  constexpr uint8_t kFrameCount = 32;
  constexpr uint32_t kFrameIntervalMs = kSleepCloseoutMs / kFrameCount;
  const uint32_t animationStartedAt = millis();

  for (uint8_t frame = 0; frame < kFrameCount; ++frame) {
    if (cancelCloseOutForButtonPress()) {
      return;
    }

    const uint8_t duty = static_cast<uint8_t>(
        kBacklightFullDuty -
        ((static_cast<uint16_t>(frame + 1) * kBacklightFullDuty) /
         kFrameCount));
    setBacklight(duty);

    const uint32_t frameDeadline =
        animationStartedAt + (frame + 1) * kFrameIntervalMs;
    while (static_cast<int32_t>(millis() - frameDeadline) < 0) {
      if (cancelCloseOutForButtonPress()) {
        return;
      }
      delay(2);
    }
  }

  setBacklight(0);
  // Keep LCD RAM/controller awake. Backlight-only timeout avoids sleep/wake
  // sequencing faults and restores the screen immediately on BOOT.
  screenAwake = false;
}

void sleepScreenAfterInactivity() {
  if (!screenAwake || !actionButton.stableHigh || bootGesture.tapPending ||
      bootGesture.holding) {
    return;
  }

  if (millis() - lastUserActivityAt >= kScreenTimeoutMs) {
    closeOutDisplay();
  }
}

bool requestHue(const String& url, const char* method, const String& body,
                String& responseBody) {
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("Wi-Fi disconnected");
    return false;
  }

  HTTPClient http;
  http.setTimeout(2500);
  if (!http.begin(wifiClient, url)) {
    setStatus("HTTP setup failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  int code = -1;
  if (strcmp(method, "GET") == 0) {
    code = http.GET();
  } else {
    code = http.PUT(body);
  }

  responseBody = http.getString();
  http.end();

  if (code < 200 || code >= 300) {
    setStatus(code < 0 ? "Bridge unreachable" : "Hue HTTP " + String(code));
    return false;
  }

  // Hue's legacy local API can report application errors in a 200 response.
  {
    JsonDocument result;
    if (deserializeJson(result, responseBody) == DeserializationError::Ok &&
        result.is<JsonArray>() && result[0]["error"].is<JsonObject>()) {
      const int errorType = result[0]["error"]["type"] | -1;
      // Do not log raw responses or error addresses: they can contain the API key.
      Serial.printf("[Hue] API error type: %d\n", errorType);
      if (errorType == 1) {
        setStatus("Hue key unauthorized");
      } else if (errorType == 3) {
        setStatus("Hue group not found");
      } else {
        setStatus("Hue error " + String(errorType));
      }
      return false;
    }
  }
  return true;
}

bool refreshRoomState() {
  lastStateRefreshAt = millis();
  String response;
  if (!requestHue(groupUrl(""), "GET", "", response)) {
    stateKnown = false;
    brightnessKnown = false;
    screenDirty = true;
    drawScreen();
    return false;
  }

  JsonDocument group;
  const DeserializationError parseError = deserializeJson(group, response);
  if (parseError != DeserializationError::Ok ||
      !group["state"].is<JsonObject>() ||
      !group["action"].is<JsonObject>()) {
    Serial.printf("[Hue] Group JSON parse: %s; state object: %d; action object: %d\n",
                  parseError.c_str(), group["state"].is<JsonObject>() ? 1 : 0,
                  group["action"].is<JsonObject>() ? 1 : 0);
    stateKnown = false;
    brightnessKnown = false;
    screenDirty = true;
    setStatus("Invalid group response");
    drawScreen();
    return false;
  }

  const bool reportedOn = group["state"]["any_on"] | false;
  const bool hasBrightness = group["action"]["bri"].is<int>();
  const int reportedBrightness = group["action"]["bri"] | 0;
  const uint8_t nextBrightness = static_cast<uint8_t>(
      constrain(reportedBrightness, kMinimumBrightness, kMaximumBrightness));
  screenDirty = screenDirty || !stateKnown || lightsOn != reportedOn ||
                brightnessKnown != hasBrightness ||
                (hasBrightness && brightness != nextBrightness);
  lightsOn = reportedOn;
  brightnessKnown = hasBrightness;
  brightness = hasBrightness ? nextBrightness : 0;
  stateKnown = true;
  setStatus("Ready");
  return true;
}

bool sendGroupCommand(const String& body, const String& successMessage) {
  String response;
  if (!requestHue(groupUrl("/action"), "PUT", body, response)) {
    drawScreen();
    return false;
  }
  setStatus(successMessage);
  drawScreen();
  return true;
}

void toggleLights() {
  if (!refreshRoomState()) {
    return;
  }

  const bool requestedState = !lightsOn;
  JsonDocument command;
  command["on"] = requestedState;
  String body;
  serializeJson(command, body);

  if (sendGroupCommand(body, requestedState ? "Turning on" : "Turning off")) {
    lightsOn = requestedState;
    stateKnown = true;
    screenDirty = true;
    setStatus("Ready");
    drawScreen();
  }
}

void startDimming() {
  if (!brightnessKnown) {
    setStatus("No dimmable lights");
    return;
  }
  dimDirection = dimDownOnNextHold ? -1 : 1;
  dimDownOnNextHold = !dimDownOnNextHold;

  // If the selected direction is already at its limit, reverse so a hold
  // always changes brightness instead of appearing unresponsive.
  if ((dimDirection < 0 && brightness <= kMinimumBrightness) ||
      (dimDirection > 0 && brightness >= kMaximumBrightness)) {
    dimDirection *= -1;
  }

  dimmingActive = true;
  lightsOn = true;
  stateKnown = true;
  screenDirty = true;
  setStatus(dimDirection < 0 ? "Dimming down..." : "Dimming up...");
  lastDimStepAt = millis();
  drawScreen();
}

void updateDimming() {
  if (!dimmingActive || actionButton.stableHigh ||
      digitalRead(kActionButtonPin) == HIGH) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastDimStepAt < kDimStepMs) {
    return;
  }
  lastDimStepAt = now;

  const int next = static_cast<int>(brightness) + dimDirection * kDimStep;
  const uint8_t nextBrightness = static_cast<uint8_t>(
      constrain(next, kMinimumBrightness, kMaximumBrightness));
  if (brightness == nextBrightness) {
    return;
  }
  brightness = nextBrightness;
  lightsOn = true;
  stateKnown = true;
  screenDirty = true;
  drawScreen();
}

void commitBrightness() {
  JsonDocument command;
  command["on"] = true;
  command["bri"] = brightness;
  String body;
  serializeJson(command, body);
  if (sendGroupCommand(body, "Ready")) {
    refreshRoomState();
  }
  drawScreen();
}

void cycleRoom() {
  activeRoom = (activeRoom + 1) % ROOM_COUNT;
  stateKnown = false;
  brightnessKnown = false;
  screenDirty = true;
  setStatus("Loading room...");
  drawScreen();
  refreshRoomState();
}

void pollBootButton() {
  ButtonState& button = actionButton;
  const uint32_t now = millis();
  const bool rawHigh = digitalRead(button.pin) == HIGH;
  const ButtonEvent event =
      updateButtonInput(button, rawHigh, now, kDebounceMs, kButtonArmMs);
  if (event == ButtonEvent::Armed) {
    Serial.printf("[Button] GPIO %u armed after release.\n", button.pin);
  }
  if (event == ButtonEvent::Pressed && !screenAwake) {
    button.wakeOnlyUntilRelease = true;
  }
  if (event == ButtonEvent::Pressed || event == ButtonEvent::Released) {
    noteUserActivity();
  }
  if (event == ButtonEvent::Pressed) {
    Serial.println("[Button] BOOT pressed.");
  }

  if (button.wakeOnlyUntilRelease) {
    bootGesture = BootGestureState{};
    if (event == ButtonEvent::Released ||
        (button.stableHigh && rawHigh &&
         now - button.rawChangedAt >= kDebounceMs)) {
      button.wakeOnlyUntilRelease = false;
      Serial.println("[Button] Wake-only press released.");
    }
    return;
  }
  if (!button.armed) {
    return;
  }
  const BootGesture gesture = updateBootGesture(
      bootGesture, event, button, rawHigh, now, kDoubleTapMs, kLongPressMs);
  switch (gesture) {
    case BootGesture::SingleTap:
      Serial.println("[Button] Single tap: toggle.");
      toggleLights();
      break;
    case BootGesture::DoubleTap:
      Serial.println("[Button] Double tap: next room.");
      cycleRoom();
      break;
    case BootGesture::HoldStart:
      Serial.println("[Button] Hold: dim.");
      if (refreshRoomState() && digitalRead(kActionButtonPin) == LOW) {
        startDimming();
      }
      break;
    case BootGesture::HoldEnd:
      if (dimmingActive) {
        dimmingActive = false;
        commitBrightness();
      }
      break;
    default:
      break;
  }
}

void logWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] Associated with access point; waiting for an IP address.");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[WiFi] Connected. IP: ");
      Serial.print(WiFi.localIP());
      Serial.printf(" | signal: %d dBm\n", WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] Disconnected. ESP32 reason code: %u\n",
                    static_cast<unsigned int>(info.wifi_sta_disconnected.reason));
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("[WiFi] Lost IP address.");
      break;
    default:
      break;
  }
}

void keepWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastWifiRetryAt < kWifiRetryMs) {
    return;
  }
  lastWifiRetryAt = now;
  Serial.printf("[WiFi] Retrying connection; status code: %d\n",
                static_cast<int>(WiFi.status()));
  setStatus("Reconnecting Wi-Fi");
  WiFi.reconnect();
  drawScreen();
}

void connectWifi() {
  WiFi.onEvent(logWifiEvent);
  Serial.println("[WiFi] Settings source: include/config.h");
  Serial.println("[WiFi] Starting connection (credentials are not logged).");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setStatus("Connecting Wi-Fi");
  drawScreen();

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < 15000) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    setStatus("Wi-Fi connected");
  } else {
    Serial.printf("[WiFi] No IP connection after 15 seconds; status code: %d\n",
                  static_cast<int>(WiFi.status()));
    setStatus("Wi-Fi unavailable");
  }
  drawScreen();
}

}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);
  display.init();
  display.setRotation(0);  // 135x240 portrait
  ledcSetup(kBacklightChannel, 20000, 8);
  ledcAttachPin(kBacklightPin, kBacklightChannel);
  setBacklight(0);
  display.fillScreen(TFT_BLACK);
  frame.setColorDepth(16);
  displayReady = frame.createSprite(135, 240) != nullptr;
  if (!displayReady) {
    Serial.println("[Display] ERROR: cannot allocate 64,800-byte display buffer.");
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.drawString("Display RAM error", 3, 100, 2);
    setBacklight(kBacklightFullDuty);
    return;  // loop also stops; never control lights after failed initialization.
  }
  Serial.printf("[Display] Buffer allocated; free heap: %u bytes.\n",
                ESP.getFreeHeap());
  drawScreen();
  setBacklight(kBacklightFullDuty);

  pinMode(kActionButtonPin, INPUT_PULLUP);
  Serial.println("[Button] BOOT only: tap=toggle, double-tap=room, hold=dim.");

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    refreshRoomState();
  }
  Serial.printf("[Memory] Free heap after Wi-Fi/Hue startup: %u bytes.\n",
                ESP.getFreeHeap());
  wifiWasConnected = WiFi.status() == WL_CONNECTED;
  lastUserActivityAt = millis();
  actionButton.rawChangedAt = millis();
}

void loop() {
  if (!displayReady) {
    delay(100);
    return;
  }
  pollBootButton();

  // A held button is ongoing activity, so a long dimming press cannot cause
  // the display to sleep while the user is still operating the remote.
  if (actionButton.armed && !actionButton.stableHigh) {
    noteUserActivity();
  }

  updateDimming();
  keepWifiConnected();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !dimmingActive && actionButton.stableHigh &&
      digitalRead(kActionButtonPin) == HIGH && !bootGesture.tapPending &&
      (!wifiWasConnected || (screenAwake &&
       millis() - lastStateRefreshAt >= kStateRefreshMs))) {
    refreshRoomState();
  }
  wifiWasConnected = connected;
  drawScreen();
  sleepScreenAfterInactivity();
  delay(4);
}
