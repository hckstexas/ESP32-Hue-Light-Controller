# FAQ and troubleshooting

## What do I need to configure before building?

Copy `include/config.example.h` to `include/config.h`, then enter your own
Wi-Fi name/password, Hue Bridge IP address, Hue API username, and room group
names/IDs. The private file is ignored by Git. Keep it local and never paste it
into a public issue or repository.

## Which Wi-Fi band should I use?

The ESP32-WROOM-32 uses 2.4 GHz Wi-Fi, not 5 GHz. Use a 2.4 GHz network that
the board can reach. Confirm the SSID/password in `config.h`, and confirm that
the Bridge and ESP32 are on networks that can communicate with each other.
Serial output reports connection state and the board's local IP, but does not
print the Wi-Fi password.

## I see “Invalid Group Response” or Hue API error type 3. What should I check?

The configured ID must refer to a Hue group that the bridge can return from
the groups endpoint. Open
`http://<bridge-ip>/api/<hue-username>/groups` on your local network and copy
the numeric group ID. Do not use a comma-separated list of light IDs.
Type 3 generally indicates that the requested resource/group was not found.
Type 1 indicates an unauthorized API username/key. Do not share the URL or
response publicly because it contains the local Hue credential.

## PlatformIO builds, but the editor shows unknown compiler arguments.

Some editor/clangd problem panels do not understand all flags supplied by the
ESP32 toolchain. Use a successful PlatformIO build as the compile check. If the
build itself fails, use the first actual compiler error in the PlatformIO
terminal rather than relying only on editor diagnostics.

## The screen dims or turns off after 30 seconds. Did the ESP32 sleep?

No. The firmware fades the backlight off after 30 seconds without button
activity. The ESP32, Wi-Fi, and LCD controller remain awake. Press BOOT once to
wake the display; that wake press does not also toggle, dim, or change rooms.

## What do BOOT, EN, and the rear KEY21 button do?

- BOOT: tap to toggle, double-tap to change rooms, or hold to dim.
- EN: reset the ESP32.
- Rear KEY21: not an app control. Its exact electrical wiring varies by board
  revision and has not been verified here; do not rely on it for room cycling.

If a press blanks the display while the backlight remains on, stop using that
button as a control and use BOOT. The screen state can be restored by resetting
the board.

## Why does the brightness not match every bulb in a room?

The firmware displays Hue's group `action.bri` value, which is the last
brightness command for the group. It is not an average of lights that were
adjusted separately.

## The display flickers or the board runs out of memory. What can I do?

The firmware draws each screen into a 64,800-byte RGB565 buffer before sending
it to the display. That avoids intermediate clear-and-redraw frames but uses
heap. Check the free-heap messages in the serial monitor and avoid adding
other large buffers. The firmware stops its controls if it cannot allocate
the display buffer.

## Can I power it from a 3.7 V battery?

This project is currently intended to run from USB-C. Battery operation and
safe simultaneous USB/battery power have not been validated for every board
revision. Do not connect a cell directly to the 3.3 V rail or combine power
sources without a suitable, verified power-path design.

## What should I remove before sharing logs?

Remove Wi-Fi SSIDs, local IP addresses, Hue Bridge addresses, Hue API
usernames/keys, and any full Hue API URLs or raw responses. The firmware avoids
printing the key and raw Hue error body, but its connection log can include the
device's local IP address.
