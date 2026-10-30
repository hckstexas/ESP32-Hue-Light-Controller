# ESP32 Hue Light Controller

PlatformIO firmware for an ESP32 board with a 135×240 ST7789 display. It
controls Hue room groups over local Wi-Fi through the Hue Bridge's local v1 API.

## Hardware

This pinout is for the ideaspark ESP32 1.14-inch display board:

| Display/control | GPIO |
| --- | --- |
| ST7789 MOSI | 23 |
| ST7789 SCLK | 18 |
| ST7789 CS | 15 |
| ST7789 DC | 2 |
| ST7789 reset | 4 |
| ST7789 backlight | 32 |
| BOOT action button | 0 (active-low, internal pull-up) |

BOOT is the only app control. EN resets the ESP32. GPIO 0 is a boot-strapping
pin; holding BOOT while powering or resetting can enter download mode. The
rear KEY21 is not used as a room control and may affect the LCD on some board
revisions; its exact wiring is not verified for every revision.

## Configure and flash

1. Install PlatformIO Core or the PlatformIO IDE extension.
2. Copy `include/config.example.h` to `include/config.h`.
3. Set your local Wi-Fi SSID/password, Hue Bridge address, and Hue API username
   in `config.h`.
4. Replace the example room names and IDs in `ROOMS` with group IDs from your
   own Hue Bridge. Use numeric group IDs, not lists of light IDs.
5. Build and upload the `esp32` PlatformIO environment; use 115200 baud for the
   serial monitor.

`include/config.h` is ignored by Git. Never commit or share it. Do not publish
serial logs without removing local IP addresses and any private network details.
The example configuration contains placeholders only.

The dependencies are declared in `platformio.ini`. PlatformIO installs
TFT_eSPI and ArduinoJson; Wi-Fi and HTTPClient come from the ESP32 Arduino
framework.

## BOOT controls

- **Tap:** toggle the selected room. The action waits for the 350 ms double-tap
  window so it can distinguish a room change.
- **Double-tap:** advance to the next configured room and load its current Hue
  state. Start the second press within 350 ms of releasing the first.
- **Hold for at least 500 ms:** dim while held. The direction alternates
  between long presses; the final brightness is sent on release.
- **After inactivity:** the display backlight fades off after 30 seconds. The
  ESP32, Wi-Fi, and LCD controller remain awake. The first press only wakes the
  display.

The device loads the current room state from Hue at startup and after room
changes. Group brightness is Hue's last group brightness command, not an
average of individually adjusted lights. If Hue does not provide brightness,
the display shows `--%` and dimming is unavailable.

The display renders complete frames in a 64,800-byte RGB565 sprite to avoid
clear-and-redraw flicker. Check available heap on devices with other large
allocations.

## Tests

Run the native button logic tests with a C++ compiler:

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/button_input_test.cpp -o /tmp/hue-buttons
/tmp/hue-buttons
g++ -std=c++11 -Wall -Wextra -Werror tests/boot_gesture_test.cpp -o /tmp/hue-gestures
/tmp/hue-gestures
```

See [FAQ](FAQ.md) for setup and troubleshooting notes.
