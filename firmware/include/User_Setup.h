// TFT_eSPI configuration for the ESP32 CYD (Cheap Yellow Display)
// Board: ESP32-2432S028R
// Display: ILI9341 2.8" SPI 320x240
// This file is loaded instead of the library default because -DUSER_SETUP_LOADED
// is set in platformio.ini.

// ─── Driver ──────────────────────────────────────────────────────────────────
#define ILI9341_DRIVER

// ─── Physical display size (portrait orientation in hardware) ─────────────────
// LVGL will handle the landscape rotation in software via tft.setRotation(1)
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── SPI pin mapping ──────────────────────────────────────────────────────────
#define TFT_MOSI  23
#define TFT_MISO  19   // Needed: XPT2046 touch also uses this MISO line
#define TFT_SCLK  18
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4

// ─── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH   // BL is active-high on the CYD

// ─── Colour byte order ────────────────────────────────────────────────────────
// Without TFT_BGR the display shows magenta/green where blue/red should be
#define TFT_RGB_ORDER TFT_BGR

// ─── SPI clock frequencies ───────────────────────────────────────────────────
#define SPI_FREQUENCY       40000000   // 40 MHz — safe for ILI9341 on CYD
#define SPI_READ_FREQUENCY   5000000   // Lower for MISO reads from display
#define SPI_TOUCH_FREQUENCY  2500000   // XPT2046 max is 2.5 MHz

// ─── Fonts (compiled into TFT_eSPI library) ──────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
