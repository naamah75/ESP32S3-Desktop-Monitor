#pragma once

// Optional compile-time display power/backlight configuration.
// Change these only if your specific T-Display-S3 revision uses different wiring.

#ifndef TFT_BL
#define TFT_BL 38
#endif

#ifndef TFT_POWER
#define TFT_POWER 15
#endif

// 0 = portrait, 1 = landscape, 2 = portrait inverted, 3 = landscape inverted
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 0
#endif
