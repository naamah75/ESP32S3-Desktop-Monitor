#pragma once

// Public-safe fallback Wi-Fi configuration.
// Keep real credentials in include/WifiConfig.local.h, which is gitignored.

#if __has_include("WifiConfig.local.h")
#include "WifiConfig.local.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

// Set to 1 to force the captive portal at next boot.
#ifndef FORCE_WIFI_SETUP
#define FORCE_WIFI_SETUP 0
#endif
