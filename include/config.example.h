#pragma once

// Copy to config.h and enter your local network and Hue Bridge details.
// config.h is ignored by Git; never commit or share real credentials.
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr char HUE_BRIDGE_IP[] = "YOUR_BRIDGE_IP";
// Hue's v1 API calls this the "username"; it is the local API key.
constexpr char HUE_API_KEY[] = "YOUR_HUE_API_KEY";

// Use Hue group IDs from GET /api/<username>/groups.
// The names are shown on the screen; group IDs are sent to the bridge.
struct RoomConfig {
  const char* name;
  const char* groupId;
};

constexpr RoomConfig ROOMS[] = {
    {"Room 1", "1"},
    {"Room 2", "2"},
    {"Room 3", "3"},
    {"Room 4", "4"},
    {"Room 5", "5"},
};

constexpr unsigned int ROOM_COUNT = sizeof(ROOMS) / sizeof(ROOMS[0]);

