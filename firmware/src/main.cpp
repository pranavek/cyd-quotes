#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "quotes.h"

// ─── Hardware ─────────────────────────────────────────────────────────────────
#define TOUCH_CS  27
#define TOUCH_IRQ 25

// ─── Colour palette ───────────────────────────────────────────────────────────
#define C_BG      lv_color_hex(0x0d0d1a)
#define C_CARD    lv_color_hex(0x16213e)
#define C_BORDER  lv_color_hex(0x0f3460)
#define C_TEXT    lv_color_hex(0xffffff)
#define C_ORB1    lv_color_hex(0x0f3460)
#define C_ORB2    lv_color_hex(0x533483)
#define C_ORB3    lv_color_hex(0xe94560)
#define C_WIFI_OK lv_color_hex(0x00c853)
#define C_WIFI_NO lv_color_hex(0x555577)

// ─── LVGL draw buffers (double-buffered, 1/10th screen height) ────────────────
static lv_color_t draw_buf_1[320 * 24];
static lv_color_t draw_buf_2[320 * 24];
static lv_disp_draw_buf_t draw_buf_desc;

// ─── Hardware instances ───────────────────────────────────────────────────────
static TFT_eSPI tft;
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ─── UI widget handles ────────────────────────────────────────────────────────
static lv_obj_t* card_obj    = nullptr;
static lv_obj_t* quote_label = nullptr;
static lv_obj_t* wifi_dot    = nullptr;

// ─── State ────────────────────────────────────────────────────────────────────
static uint8_t  local_quote_idx = 0;
static uint32_t last_fetch_ms   = 0;
static uint32_t last_advance_ms = 0;
static bool     anim_running    = false;   // guard against overlapping fades

static char current_quote[600] = "";
static char pending_quote[600] = "";
static bool pending_valid       = false;

// ─── Forward declarations ─────────────────────────────────────────────────────
static void fetch_quote();
static void advance_quote_anim();

// ─────────────────────────────────────────────────────────────────────────────
// LVGL callbacks
// ─────────────────────────────────────────────────────────────────────────────

static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px_map) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // true = swap bytes: LVGL stores RGB565 little-endian; ILI9341 expects big-endian
    tft.pushColors((uint16_t*)px_map, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);  // MUST call — without this LVGL stalls permanently
}

static void lvgl_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();
        // Map raw ADC (12-bit) to screen pixels. Adjust min/max if touch is off.
        data->point.x = constrain(map(p.x, 200, 3700, 0, 319), 0, 319);
        data->point.y = constrain(map(p.y, 240, 3800, 0, 239), 0, 239);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// esp_timer ISR — called every 5 ms to drive LVGL internal timers
static void lvgl_tick_cb(void*) {
    lv_tick_inc(5);
}

// ─────────────────────────────────────────────────────────────────────────────
// Animation helpers
// ─────────────────────────────────────────────────────────────────────────────

// Opacity setter compatible with lv_anim_exec_xcb_t (void*, int32_t)
static void opa_anim_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}

// Called when fade-in completes — clear the running guard
static void fade_in_done_cb(lv_anim_t*) {
    anim_running = false;
}

// Fade card back in after text was swapped
static void start_fade_in(lv_anim_t*) {
    // Swap quote text while invisible
    if (pending_valid) {
        strncpy(current_quote, pending_quote, sizeof(current_quote) - 1);
        current_quote[sizeof(current_quote) - 1] = '\0';
        pending_valid = false;
    }
    lv_label_set_text(quote_label, current_quote);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card_obj);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 350);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_set_ready_cb(&a, fade_in_done_cb);
    lv_anim_start(&a);

    last_advance_ms = millis();
}

// Fade card out, then swap text, then fade back in
static void advance_quote_anim() {
    if (anim_running || !card_obj) return;
    anim_running = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card_obj);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 350);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_set_ready_cb(&a, start_fade_in);   // swap content while invisible
    lv_anim_start(&a);
}

// ─────────────────────────────────────────────────────────────────────────────
// Background orb animation
// ─────────────────────────────────────────────────────────────────────────────

static void start_orb_anim(lv_obj_t* orb,
                            int32_t x0, int32_t x1,
                            int32_t y0, int32_t y1,
                            uint32_t dur_ms) {
    // Horizontal drift
    lv_anim_t ax;
    lv_anim_init(&ax);
    lv_anim_set_var(&ax, orb);
    lv_anim_set_values(&ax, x0, x1);
    lv_anim_set_time(&ax, dur_ms);
    lv_anim_set_playback_time(&ax, dur_ms);          // bounce back
    lv_anim_set_repeat_count(&ax, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ax, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&ax, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_start(&ax);

    // Vertical drift — slightly different duration for organic movement
    lv_anim_t ay;
    lv_anim_init(&ay);
    lv_anim_set_var(&ay, orb);
    lv_anim_set_values(&ay, y0, y1);
    lv_anim_set_time(&ay, (uint32_t)(dur_ms * 1.35f));
    lv_anim_set_playback_time(&ay, (uint32_t)(dur_ms * 1.35f));
    lv_anim_set_repeat_count(&ay, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&ay, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&ay, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&ay);
}

static lv_obj_t* make_orb(lv_obj_t* parent, lv_color_t color, int16_t size, lv_opa_t opa) {
    lv_obj_t* orb = lv_obj_create(parent);
    lv_obj_set_size(orb, size, size);
    lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(orb, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(orb, opa, LV_PART_MAIN);
    lv_obj_set_style_border_width(orb, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(orb, 0, LV_PART_MAIN);
    lv_obj_clear_flag(orb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(orb, LV_OBJ_FLAG_CLICKABLE);
    return orb;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quote fetch
// ─────────────────────────────────────────────────────────────────────────────

static void fetch_quote() {
    last_fetch_ms = millis();   // reset timer even if fetch fails

    WiFiClientSecure client;
    client.setInsecure();       // skip cert validation — acceptable for a personal gadget

    HTTPClient http;
    if (!http.begin(client, "https://zenquotes.io/api/random")) return;

    int code = http.GET();
    if (code == HTTP_CODE_OK) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        if (!err && doc.is<JsonArray>() && doc[0]["q"].is<const char*>()) {
            const char* q = doc[0]["q"].as<const char*>();
            strncpy(pending_quote, q, sizeof(pending_quote) - 1);
            pending_quote[sizeof(pending_quote) - 1] = '\0';
            pending_valid = true;
            advance_quote_anim();
        }
    }
    http.end();
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi status icon
// ─────────────────────────────────────────────────────────────────────────────

static void update_wifi_icon() {
    if (!wifi_dot) return;
    bool connected = (WiFi.status() == WL_CONNECTED);
    lv_obj_set_style_bg_color(wifi_dot, connected ? C_WIFI_OK : C_WIFI_NO, LV_PART_MAIN);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status screens (shown before main UI is built)
// ─────────────────────────────────────────────────────────────────────────────

static void show_status_screen(const char* msg) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, C_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t* lbl = lv_label_create(scr);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 280);
    lv_obj_set_style_text_color(lbl, C_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(lbl, 6, LV_PART_MAIN);
    lv_label_set_text(lbl, msg);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_refr_now(NULL);   // force immediate render so user sees the message
}

// ─────────────────────────────────────────────────────────────────────────────
// Touch event — tap anywhere to request next quote
// ─────────────────────────────────────────────────────────────────────────────

static void screen_touch_cb(lv_event_t*) {
    if (anim_running || pending_valid) return;

    if (WiFi.status() == WL_CONNECTED) {
        // Trigger an early fetch (resets the 30-minute timer too)
        fetch_quote();
    } else {
        // Offline: cycle local quotes
        local_quote_idx = (local_quote_idx + 1) % QUOTE_COUNT;
        strncpy(pending_quote, QUOTES[local_quote_idx], sizeof(pending_quote) - 1);
        pending_quote[sizeof(pending_quote) - 1] = '\0';
        pending_valid = true;
        advance_quote_anim();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Main UI construction
// ─────────────────────────────────────────────────────────────────────────────

static void ui_create() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, C_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr, screen_touch_cb, LV_EVENT_CLICKED, NULL);

    // ── Background orbs (created first → lowest z-order, behind card) ────────
    // Each orb drifts between two positions at a different speed and direction.
    lv_obj_t* orb;

    orb = make_orb(scr, C_ORB1, 110, 55);
    lv_obj_set_pos(orb, -30, -20);
    start_orb_anim(orb, -30, 70, -20, 50, 10000);

    orb = make_orb(scr, C_ORB2, 85, 48);
    lv_obj_set_pos(orb, 210, 150);
    start_orb_anim(orb, 210, 290, 150, 200, 13500);

    orb = make_orb(scr, C_ORB3, 65, 42);
    lv_obj_set_pos(orb, 90, -15);
    start_orb_anim(orb, 90, 250, -15, 40, 8200);

    orb = make_orb(scr, C_ORB1, 95, 38);
    lv_obj_set_pos(orb, -25, 130);
    start_orb_anim(orb, -25, 55, 130, 195, 11500);

    orb = make_orb(scr, C_ORB2, 75, 52);
    lv_obj_set_pos(orb, 250, 165);
    start_orb_anim(orb, 250, 315, 165, 230, 9300);

    // ── Quote card ─────────────────────────────────────────────────────────────
    card_obj = lv_obj_create(scr);
    lv_obj_set_size(card_obj, 300, 200);
    lv_obj_align(card_obj, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_style_bg_color(card_obj, C_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card_obj, 210, LV_PART_MAIN);  // slight translucency
    lv_obj_set_style_border_color(card_obj, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card_obj, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card_obj, 18, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card_obj, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card_obj, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card_obj, 90, LV_PART_MAIN);
    lv_obj_set_style_shadow_ofs_y(card_obj, 4, LV_PART_MAIN);
    lv_obj_clear_flag(card_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card_obj, LV_OBJ_FLAG_CLICKABLE);

    // ── Quote text label ───────────────────────────────────────────────────────
    quote_label = lv_label_create(card_obj);
    lv_obj_set_width(quote_label, 264);  // card width 300 − 2×18 padding
    lv_label_set_long_mode(quote_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(quote_label, C_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(quote_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_align(quote_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(quote_label, 5, LV_PART_MAIN);
    lv_obj_align(quote_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(quote_label, current_quote[0] ? current_quote : "Loading…");

    // ── WiFi status dot (top-right corner) ────────────────────────────────────
    wifi_dot = lv_obj_create(scr);
    lv_obj_set_size(wifi_dot, 10, 10);
    lv_obj_align(wifi_dot, LV_ALIGN_TOP_RIGHT, -8, 8);
    lv_obj_set_style_radius(wifi_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(wifi_dot, C_WIFI_OK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wifi_dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(wifi_dot, 0, LV_PART_MAIN);
    lv_obj_clear_flag(wifi_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(wifi_dot, LV_OBJ_FLAG_CLICKABLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // 1. Init display
    tft.init();
    tft.setRotation(1);      // landscape: 320 wide × 240 tall
    tft.fillScreen(TFT_BLACK);

    // 2. Init touch (same SPI bus, separate CS)
    touch.begin();
    touch.setRotation(1);

    // 3. Init LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf_desc, draw_buf_1, draw_buf_2, 320 * 24);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf_desc;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    // 4. LVGL tick source — 5 ms hardware timer
    const esp_timer_create_args_t tick_args = { .callback = lvgl_tick_cb, .name = "lvgl_tick" };
    esp_timer_handle_t tick_handle;
    esp_timer_create(&tick_args, &tick_handle);
    esp_timer_start_periodic(tick_handle, 5000);  // 5000 µs = 5 ms

    // 5. Show "connecting" screen immediately
    show_status_screen("Connecting to WiFi\u2026");

    // 6. WiFi — non-blocking so LVGL keeps running during AP portal
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);         // restart after 3 min if not configured
    wm.setConfigPortalBlocking(false);

    bool connected = wm.autoConnect("QuoteViewer-AP");

    if (!connected) {
        // In AP/portal mode — show instructions on screen while portal is active
        show_status_screen(
            "No WiFi saved.\n\n"
            "Connect your phone to:\n"
            "QuoteViewer-AP\n\n"
            "Then open the\ncaptive portal."
        );
    }

    // Loop until connected (wm.process() drives the portal state machine)
    uint32_t ap_start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        wm.process();
        lv_task_handler();
        if (millis() - ap_start > 185000UL) ESP.restart();  // safety timeout
        delay(5);
    }

    // 7. Connected — build the main UI and load the first quote
    strncpy(current_quote, QUOTES[0], sizeof(current_quote) - 1);
    ui_create();
    lv_task_handler();

    // Kick off the first online fetch
    fetch_quote();

    last_advance_ms = millis();
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    lv_task_handler();   // drive LVGL rendering and animations

    update_wifi_icon();

    if (WiFi.status() == WL_CONNECTED) {
        // Online: fetch a new quote every 30 minutes
        if (millis() - last_fetch_ms >= 30UL * 60UL * 1000UL) {
            fetch_quote();
        }
    } else {
        // Offline: cycle through local quotes every 30 seconds
        if (millis() - last_advance_ms >= 30000UL && !anim_running) {
            local_quote_idx = (local_quote_idx + 1) % QUOTE_COUNT;
            strncpy(pending_quote, QUOTES[local_quote_idx], sizeof(pending_quote) - 1);
            pending_quote[sizeof(pending_quote) - 1] = '\0';
            pending_valid = true;
            advance_quote_anim();
        }
    }

    delay(5);  // yield to FreeRTOS watchdog; keeps lv_task_handler call rate ~200/s
}
