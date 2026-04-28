# ESP32 CYD Quote Viewer — Implementation Plan

## Context

New project from scratch: an ESP32 CYD (Cheap Yellow Display) running a
quote viewer with a polished LVGL UI. WiFi credentials are configured via
a captive-portal (phone/laptop browser), and when online a live quote API
is hit every 30 minutes. A subtle animated background runs continuously.
A web flasher lets anyone flash the firmware from Chrome/Edge.

---

## Project Structure

```
esp32-cyd-quote-viewer/
├── firmware/
│   ├── platformio.ini
│   ├── include/
│   │   ├── User_Setup.h      # TFT_eSPI pin config
│   │   ├── lv_conf.h         # LVGL v8 feature flags
│   │   └── quotes.h          # 30 fallback quotes (used offline)
│   └── src/
│       └── main.cpp          # All logic: WiFi, LVGL, UI, fetch, animation
├── web-flasher/
│   ├── index.html            # ESP Web Tools install button
│   └── manifest.json         # Flash offsets for 4 binary parts
└── .github/
    └── workflows/
        └── build.yml         # Build → copy bins → deploy to GH Pages
```

---

## Hardware Pin Map (CYD)

| Signal      | GPIO |
|-------------|------|
| TFT MOSI    | 23   |
| TFT MISO    | 19   |
| TFT SCLK    | 18   |
| TFT CS      | 15   |
| TFT DC      | 2    |
| TFT RST     | 4    |
| TFT BL      | 21   |
| Touch CS    | 27   |
| Touch IRQ   | 25   |

---

## Libraries (`platformio.ini`)

```ini
[env:esp32-cyd]
platform = espressif32 @ ^6.6.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = min_spiffs.csv

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    lvgl/lvgl @ ^8.3.11
    paulstoffregen/XPT2046_Touchscreen @ ^1.4
    tzapu/WiFiManager @ ^2.0.17          ; captive-portal WiFi config
    bblanchon/ArduinoJson @ ^7.0.0       ; parse quote API response

build_flags =
    -DUSER_SETUP_LOADED
    -DLV_CONF_INCLUDE_SIMPLE
    -DCORE_DEBUG_LEVEL=0
    -O2
```

---

## Screens / States

```
Boot → "Connecting…" screen
       ├─ Saved creds + connects → Main quote screen  ─┐
       └─ No creds / fails      → "Connect to          │
              QuoteViewer-AP" AP screen                 │
                    │                                   │
                    └─ User configures → restart ───────┘
```

---

## `firmware/include/User_Setup.h`

```cpp
#define ILI9341_DRIVER
#define TFT_WIDTH 240   #define TFT_HEIGHT 320
#define TFT_MOSI 23   #define TFT_MISO 19   #define TFT_SCLK 18
#define TFT_CS   15   #define TFT_DC    2   #define TFT_RST   4
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH
#define TFT_RGB_ORDER TFT_BGR     // without this colours are wrong (magenta)
#define SPI_FREQUENCY      40000000
#define SPI_READ_FREQUENCY  5000000
#define SPI_TOUCH_FREQUENCY 2500000
```

---

## `firmware/include/lv_conf.h`

Outer guard must be `#if 1` (not `#if 0` as shipped by LVGL template).

```c
#define LV_COLOR_DEPTH 16             // RGB565
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240
#define LV_MEM_SIZE (56U * 1024U)     // 56 KB — extra for anim objects
#define LV_DISP_DEF_REFR_PERIOD 16   // ~60 Hz
#define LV_INDEV_DEF_READ_PERIOD 30
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_USE_ANIMATION 1
#define LV_USE_LOG 0
```

---

## `firmware/src/main.cpp` Architecture

### Colour Palette

```cpp
#define C_BG       lv_color_hex(0x0d0d1a)   // very dark navy
#define C_CARD     lv_color_hex(0x16213e)
#define C_BORDER   lv_color_hex(0x0f3460)
#define C_TEXT     lv_color_hex(0xffffff)
// Orb colours (semi-transparent blobs)
#define C_ORB1     lv_color_hex(0x0f3460)
#define C_ORB2     lv_color_hex(0x533483)
#define C_ORB3     lv_color_hex(0xe94560)
```

### Boot Sequence in `setup()`

1. TFT init + rotation(1) → fill black
2. Touch init
3. `lv_init()` → register draw buffers, display driver, touch driver
4. Start `esp_timer` tick (5 ms period, calls `lv_tick_inc(5)`)
5. Show **"Connecting…"** LVGL screen
6. Call `lv_task_handler()` once to render it
7. Start WiFiManager (`autoConnect("QuoteViewer-AP")`)
   - If AP mode entered: update screen to **"Open Wi-Fi settings → QuoteViewer-AP"**
   - Keep calling `lv_task_handler()` in a loop during AP mode
   - WiFiManager blocks until configured, then ESP restarts
8. If connected: fetch first quote, show **main UI**

### WiFiManager Integration

```cpp
WiFiManager wm;
wm.setConfigPortalTimeout(180);  // 3 min timeout then reboot
bool ok = wm.autoConnect("QuoteViewer-AP");
if (!ok) ESP.restart();
```

While in AP mode, a blocking loop runs `lv_task_handler()` and the
background animation so the screen stays alive. The captive portal is
fully handled by WiFiManager — user connects to the AP from phone,
browser auto-opens config page, enters SSID + password, device restarts.

### Quote API

**Endpoint:** `GET https://zenquotes.io/api/random`  
**Response:** `[{"q": "quote text", "a": "author name", "h": "..."}]`

Only the `q` field is used — author is ignored.

```cpp
void fetch_quote() {
    HTTPClient http;
    http.begin("https://zenquotes.io/api/random");
    if (http.GET() == HTTP_CODE_OK) {
        JsonDocument doc;
        deserializeJson(doc, http.getStream());
        const char* q = doc[0]["q"];
        ui_show_quote(q);   // quote text only
    }
    http.end();
}
```

Fetch is triggered:
- Once at boot (after WiFi connects)
- Every 30 minutes via `millis()` check in `loop()`
- On manual tap (same tap handler as before)

**Offline fallback:** if fetch fails (no WiFi or HTTP error), cycle through
the local `QUOTES[]` array from `quotes.h`.

### LVGL Flush + Touch Callbacks

```cpp
void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px_map) {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, area->x2-area->x1+1, area->y2-area->y1+1);
    tft.pushColors((uint16_t*)px_map, (area->x2-area->x1+1)*(area->y2-area->y1+1), true);
    tft.endWrite();
    lv_disp_flush_ready(drv);   // must call or rendering pipeline stalls
}
```

### Background Animation (Floating Orbs)

Created **behind** the card (z-order: orbs → card → labels).

**5 orbs**, each an `lv_obj_t` circle with:
- Size: 60–120 px, `LV_RADIUS_CIRCLE`
- Background colour from C_ORB1/2/3, opacity 40–60/255
- No border, no scroll

Each orb gets **two `lv_anim_t`** animations — one for x, one for y:

```cpp
void start_orb_anim(lv_obj_t* orb, int32_t x0, int32_t x1,
                                    int32_t y0, int32_t y1,
                                    uint32_t dur_ms) {
    lv_anim_t ax;
    lv_anim_init(&ax);
    lv_anim_set_var(&ax, orb);
    lv_anim_set_values(&ax, x0, x1);
    lv_anim_set_time(&ax, dur_ms);
    lv_anim_set_playback_time(&ax, dur_ms);   // bounce back
    lv_anim_set_repeat_count(&ax, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ax, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&ax, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&ax);
    // Y drift: same pattern, different values & duration
}
```

Durations: 8000–14000 ms (slow, dreamlike). Orbs drift across the full
screen extents independently so they never appear to loop.

### Main UI Layout (z-order bottom to top)

```
Screen (BG: C_BG, clickable)
├── Orb 1..5  (lv_obj circles, semi-transparent, behind card)
├── Card  lv_obj 300×200 centred
│   └── quote_label  Montserrat 20, white, centred, wrapping
└── wifi_icon  small dot top-right (green=connected, grey=offline)
```

No author label. No navigation dots. Just the quote text, large and centred in the card.

**WiFi status dot:** 10 px circle, top-right corner. Green (`#00c853`)
when connected, grey (`#555577`) when offline.

### Quote Advance Flow

```
advance_quote()
  → fade card to 0 opacity (300 ms lv_anim)
  → on ready: update quote_label text + wifi icon
  → fade card back to full opacity (300 ms lv_anim)
  → reset last_fetch_ms / last_advance_ms
```

### `loop()`

```cpp
void loop() {
    lv_task_handler();

    update_wifi_icon();

    // 30-minute fetch timer (online mode)
    if (WiFi.status() == WL_CONNECTED &&
        millis() - last_fetch_ms >= 30UL * 60UL * 1000UL) {
        fetch_quote();
        last_fetch_ms = millis();
    }

    // Local cycle fallback — 30 s when offline
    if (WiFi.status() != WL_CONNECTED &&
        millis() - last_advance_ms >= 30000) {
        show_next_local_quote();
    }

    delay(5);   // yield to FreeRTOS watchdog
}
```

---

## `web-flasher/index.html`

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>CYD Quote Viewer — Web Flasher</title>
  <script type="module"
    src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module">
  </script>
  <style>
    body { font-family: sans-serif; text-align: center; padding: 3rem;
           background: #0d0d1a; color: #fff; }
    h1 { color: #e94560; }
    p  { color: #a0a0c0; }
  </style>
</head>
<body>
  <h1>CYD Quote Viewer</h1>
  <p>Connect your CYD via USB, then click Install.</p>
  <p><small>Requires Chrome or Edge.</small></p>
  <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
</body>
</html>
```

## `web-flasher/manifest.json`

```json
{
  "name": "CYD Quote Viewer",
  "version": "1.0.0",
  "new_install_prompt_erase": true,
  "builds": [{
    "chipFamily": "ESP32",
    "parts": [
      { "path": "firmware/bootloader.bin", "offset": 4096  },
      { "path": "firmware/partitions.bin", "offset": 32768 },
      { "path": "firmware/boot_app0.bin",  "offset": 57344 },
      { "path": "firmware/firmware.bin",   "offset": 65536 }
    ]
  }]
}
```

---

## `.github/workflows/build.yml`

```yaml
name: Build & Deploy
on:
  push:
    branches: [main]
permissions:
  pages: write
  id-token: write
jobs:
  build-and-deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.11' }
      - run: pip install platformio
      - run: cd firmware && pio run
      - run: |
          mkdir -p web-flasher/firmware
          cp firmware/.pio/build/esp32-cyd/bootloader.bin web-flasher/firmware/
          cp firmware/.pio/build/esp32-cyd/partitions.bin  web-flasher/firmware/
          cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin web-flasher/firmware/
          cp firmware/.pio/build/esp32-cyd/firmware.bin    web-flasher/firmware/
      - uses: actions/upload-pages-artifact@v3
        with: { path: web-flasher/ }
      - uses: actions/deploy-pages@v4
```

---

## Known Pitfalls

| Symptom | Fix |
|---------|-----|
| Screen shows static | `-DUSER_SETUP_LOADED` missing |
| Wrong colours | Add `TFT_RGB_ORDER TFT_BGR` in User_Setup.h |
| Display freezes after first frame | Call `lv_disp_flush_ready()` in flush callback |
| `lv_conf.h` not found | `-DLV_CONF_INCLUDE_SIMPLE` missing |
| All LVGL settings ignored | `lv_conf.h` outer guard is `#if 0` — change to `#if 1` |
| Touch inverted on axis | Swap min/max in the Y `map()` call |
| WDT reset after ~5 s | Add `delay(5)` in `loop()` |
| Crash during WiFiManager AP | Need `lv_task_handler()` loop while in AP mode |
| HTTPS fetch fails | Use `WiFiClientSecure` with `setInsecure()` |

---

## Verification Steps

1. **Display only**: `tft.fillScreen(TFT_RED)` before LVGL — confirms pins
2. **LVGL compile**: add `lv_conf.h`, run `pio run`, check no errors
3. **Touch serial-print**: log raw XPT2046 values at screen corners
4. **WiFi**: connect to AP, configure, verify reconnect on restart
5. **Quote fetch**: check Serial for HTTP 200 + parsed JSON
6. **Full UI**: orbs animate, quotes fade, wifi icon toggles
7. **Web flasher (production)**: push to `main` → GitHub Actions builds firmware, deploys `web-flasher/` to GitHub Pages → share the Pages URL, anyone opens it in Chrome/Edge and clicks Install
8. **Web flasher (local test)**: `python -m http.server` in web-flasher/ as a quick sanity check before pushing — Python is not required in production

---

## Files to Create (in order)

1. `firmware/platformio.ini`
2. `firmware/include/User_Setup.h`
3. `firmware/include/lv_conf.h`
4. `firmware/include/quotes.h`
5. `firmware/src/main.cpp`
6. `web-flasher/index.html`
7. `web-flasher/manifest.json`
8. `.github/workflows/build.yml`

> The web flasher is a static HTML page served by **GitHub Pages** — no
> server runtime required. GitHub Actions builds the firmware and publishes
> the `web-flasher/` folder to Pages automatically on every push to `main`.
> Anyone with the Pages URL can flash the CYD directly from Chrome/Edge
> using the WebSerial API (no drivers, no Python, no toolchain).