/**
 * LVGL v8 configuration for ESP32 CYD Quote Viewer
 *
 * IMPORTANT: The outer #if 1 guard MUST stay as 1 (not 0).
 * The LVGL template ships with #if 0 which silently disables the whole file.
 */

#if 1  /* Set this to 1 to enable the config */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/* ILI9341 native format — matches RGB565 */
#define LV_COLOR_DEPTH 16

/* Swap upper and lower bytes of color. Useful if the display has a 8-bit
 * SPI or parallel port, and the color bytes need to be swapped. */
#define LV_COLOR_16_SWAP 0

/* Enable more complex drawing routines to manage screens LV_COLOR_DEPTH < 8 */
#define LV_COLOR_SCREEN_TRANSP 0

/* Adjust colors of LVGL for a color-blind user (0: no, 1: red-green, 2: blue-yellow) */
#define LV_COLOR_MIX_ROUND_OFS 0

/* Images pixels with this color will not be drawn (with chroma keying) */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)  /* pure green */

/*=========================
   MEMORY SETTINGS
 *=========================*/

/* 56 KB internal heap — covers orb animation + label objects + theme */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (56U * 1024U)

/* Set an address for the memory pool. E.g. an external SRAM */
#define LV_MEM_ADR 0

/* Instead of an address, give a memory allocator that will be called to get a
 * memory pool for LVGL. E.g. my_malloc */
#if LV_MEM_CUSTOM == 0
#else   /* LV_MEM_CUSTOM */
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc
#endif  /* LV_MEM_CUSTOM */

/*====================
   HAL SETTINGS
 *====================*/

/* ~60 Hz: how often LVGL checks for dirty areas and schedules redraws */
#define LV_DISP_DEF_REFR_PERIOD 16    /* [ms] */

/* Touch input poll interval */
#define LV_INDEV_DEF_READ_PERIOD 30   /* [ms] */

/* Default display orientation (used when display driver doesn't set it) */
#define LV_DISP_ROT_NONE 0

/* Use a custom tick source that tells the elapsed time in milliseconds.
   It removes the need to manually update the tick with `lv_tick_inc()` */
#define LV_TICK_CUSTOM 0

/*====================
 * FEATURE CONFIGURATION
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/* Enable complex draw engine (shadows, gradients, arcs) */
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
#define LV_SHADOW_CACHE_SIZE 0   /* Shadow cache, 0 = disabled */
#define LV_CIRCLE_CACHE_SIZE 4   /* Circle cache entries */
#endif /* LV_DRAW_COMPLEX */

/* Allow buffering some shadows around text to avoid re-rendering */
#define LV_IMG_CACHE_DEF_SIZE 0

/* Number of stops allowed on gradients */
#define LV_GRADIENT_MAX_STOPS 2

/* Default gradient buffer size */
#define LV_GRAD_CACHE_DEF_SIZE 0

/* Allow dithering the gradients */
#define LV_DITHER_GRADIENT 0

/* Add support for horizontal and vertical merge of objects.
 * Uses extra 2 bytes per 'lv_obj_t' but may result in faster rendering */
#define LV_ATTRIBUTE_FAST_MEM

/*-------------
 * GPU
 *-----------*/
#define LV_USE_GPU_ARM2D     0
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_SWM341_DMA 0
#define LV_USE_GPU_NXP_PXP   0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL       0

/*-------------
 * Logging
 *-----------*/
#define LV_USE_LOG 0   /* Disable for production — reduces code size */
#if LV_USE_LOG
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0
#define LV_LOG_TRACE_MEM        0
#define LV_LOG_TRACE_TIMER      0
#define LV_LOG_TRACE_INDEV      0
#define LV_LOG_TRACE_DISP_REFR  0
#define LV_LOG_TRACE_EVENT      0
#define LV_LOG_TRACE_OBJ_CREATE 0
#define LV_LOG_TRACE_LAYOUT     0
#define LV_LOG_TRACE_ANIM       0
#endif

/*-------------
 * Asserts
 *-----------*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*-------------
 * Debug
 *-----------*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0

/*=================
 * OBJECT TYPES
 ================*/

/* 1: Use the memory monitor component. Requires `LV_USE_LABEL = 1` */
#define LV_USE_MEM_MONITOR_POS LV_ALIGN_BOTTOM_LEFT

/*==================
 *  FONT USAGE
 *==================*/

/* Montserrat fonts — only enable what's needed to keep flash usage down */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* Default font used when no font is explicitly set */
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Enable subpixel rendering (not useful for TFT) */
#define LV_FONT_SUBPX_BGR 0

/* Enable UTF-8 encoding for special characters */
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* Characters that break a line */
#define LV_TXT_BREAK_CHARS " ,.;:-_"

/* Minimum characters in a long word to put on a line before a break */
#define LV_TXT_LINE_BREAK_LONG_LEN 0

/* Minimum number of characters in a long word to put in a line */
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3

/* Minimum number of characters after a long word break */
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3

/* The controlled unicode codepoints are shown as '?' */
#define LV_TXT_COLOR_CMD "#"

/* Support bidirectional texts */
#define LV_USE_BIDI 0

/* Enable Arabic/Persian processing */
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*==================
 *  WIDGET USAGE
 *==================*/

#define LV_USE_ARC         0
#define LV_USE_BAR         0
#define LV_USE_BTN         0
#define LV_USE_BTNMATRIX   0
#define LV_USE_CANVAS      0
#define LV_USE_CHECKBOX    0
#define LV_USE_DROPDOWN    0
#define LV_USE_IMG         0
#define LV_USE_LABEL       1    /* Required for quote text */
#if LV_USE_LABEL
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT  0
#endif
#define LV_USE_LINE        0
#define LV_USE_ROLLER      0
#define LV_USE_SLIDER      0
#define LV_USE_SWITCH      0
#define LV_USE_TEXTAREA    0
#define LV_USE_TABLE       0

/*==================
 * EXTRA COMPONENTS
 *==================*/

/*-------
 * Layouts
 *------*/
#define LV_USE_FLEX  1   /* Used by orb container / dot row */
#define LV_USE_GRID  0

/*-----------
 * 3rd party libs
 *----------*/
#define LV_USE_FS_STDIO     0
#define LV_USE_FS_POSIX     0
#define LV_USE_FS_WIN32     0
#define LV_USE_FS_FATFS     0
#define LV_USE_PNG          0
#define LV_USE_BMP          0
#define LV_USE_SJPG         0
#define LV_USE_GIF          0
#define LV_USE_QRCODE       0
#define LV_USE_FREETYPE     0
#define LV_USE_RLOTTIE      0
#define LV_USE_FFT          0

/*-----------
 * Others
 *----------*/
#define LV_USE_SNAPSHOT     0
#define LV_USE_MONKEY       0
#define LV_USE_GRIDNAV      0
#define LV_USE_FRAGMENT     0
#define LV_USE_IMGFONT      0
#define LV_USE_MSG          0
#define LV_USE_IME_PINYIN   0

/*==================
 * THEMES
 *==================*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
#define LV_THEME_DEFAULT_DARK 1       /* Start with dark variant */
#define LV_THEME_DEFAULT_GROW 1       /* Enlarge pressed objects */
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif
#define LV_USE_THEME_SIMPLE  0
#define LV_USE_THEME_MONO    0

/*==================
 *  ANIMATION
 *==================*/
#define LV_USE_ANIMATION 1   /* Required for fade and orb drift effects */

/*==================
 * EXAMPLES
 *==================*/
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
#endif /* End of #if 1 "enable" guard */
