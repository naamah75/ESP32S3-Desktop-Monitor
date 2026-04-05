#pragma once

// Optional compile-time display power/backlight configuration.
// Change these only if your specific T-Display-S3 revision uses different wiring.

// Native panel resolution for the selected TFT_eSPI driver.
// Keep the current 170x320 defaults for this ST7789V panel, or override them
// for compatible displays wired to the same bus setup.

#ifndef TFT_PANEL_WIDTH
#define TFT_PANEL_WIDTH 170
#endif

#ifndef TFT_PANEL_HEIGHT
#define TFT_PANEL_HEIGHT 320
#endif

#ifndef TFT_WIDTH
#define TFT_WIDTH TFT_PANEL_WIDTH
#endif

#ifndef TFT_HEIGHT
#define TFT_HEIGHT TFT_PANEL_HEIGHT
#endif

#ifndef TFT_BL
#define TFT_BL 38
#endif

#ifndef TFT_POWER
#define TFT_POWER 15
#endif

// Boot/setup button used to force Wi-Fi captive portal on startup.
#ifndef SETUP_BUTTON_PIN
#define SETUP_BUTTON_PIN 14
#endif

// 0 = portrait, 1 = landscape, 2 = portrait inverted, 3 = landscape inverted
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 0
#endif
