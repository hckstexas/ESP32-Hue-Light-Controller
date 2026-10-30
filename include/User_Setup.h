// TFT_eSPI setup for the ideaspark ESP32 1.14-inch ST7789 board.
#define USER_SETUP_INFO "ideaspark ESP32 ST7789 135x240"

#define ST7789_DRIVER
#define TFT_WIDTH 135
#define TFT_HEIGHT 240

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4

#define TFT_BL 32
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4

#define SPI_FREQUENCY 27000000

