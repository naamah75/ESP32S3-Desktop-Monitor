# ESP32S3-Desktop-Monitor

Fork of `ESP32-Desktop-Monitor`, adapted for **LilyGo T-Display-S3** with **ESP32-S3** and a **1.9" ST7789 170x320** display.

## Structure

- `platformio.ini` -> PlatformIO configuration
- `src/main.cpp` -> converted receiver sketch
- `include/WifiConfig.h` -> public-safe Wi-Fi fallback and setup options
- `include/WifiConfig.local.h` -> local untracked Wi-Fi credentials
- `include/TFTConfig.h` -> panel resolution, rotation, and extra pins
- `transmitter.py` -> updated Python sender
- `requirements.txt` -> Python dependencies

## Important Notes

This configuration assumes the official **LilyGo T-Display-S3** pinout:

- CS = 6
- DC = 7
- RST = 5
- WR = 8
- RD = 9
- D0..D7 = 39, 40, 41, 42, 45, 46, 47, 48
- BL = 38
- POWER = 15

If your board uses a different revision, adjust `platformio.ini` and `include/TFTConfig.h`.

## Important Difference From The 135x240 Version

The original project used 8-bit display coordinates. On `170x320`, and in `landscape 320x170`, that is not enough to address the full panel, so both the receiver and sender were updated to a newer protocol:

- `PXUP` packets version `0x04`
- `PXUR` packets version `0x02`
- `x` and `y` coordinates are 16-bit little-endian in pixel packets
- `PXOR` packet to synchronize runtime display rotation

This firmware therefore requires the `transmitter.py` included in this repository.

## Wi-Fi And Captive Portal

The firmware supports three Wi-Fi startup modes:

- use fallback credentials from `include/WifiConfig.local.h` or `include/WifiConfig.h`
- if those fail, try credentials saved in flash
- if it still cannot connect, open a captive portal

Captive portal:

- AP: `ESP32S3-Monitor-Setup`
- URL: `http://192.168.4.1`
- setup button: `IO14`, held low during boot

Software-forced portal:

```cpp
#define FORCE_WIFI_SETUP 1
```

Recommended GitHub-safe setup:

- keep public placeholders in `include/WifiConfig.h`
- store real credentials in `include/WifiConfig.local.h`
- `include/WifiConfig.local.h` is gitignored

## Usage

1. Open the folder in VS Code with PlatformIO.
2. If you want a local fallback, create or update `include/WifiConfig.local.h` with your SSID and password.
3. Build and upload the `t-display-s3` environment.
4. Open the serial monitor at 115200 baud.
5. Install Python dependencies on the PC:

```bash
pip install -r requirements.txt
```

6. Start `transmitter.py` on the PC.

Example:

```bash
python transmitter.py
```

If you do not pass `--ip`, the sender automatically discovers the device via mDNS using `_desktopmonitor._tcp.local`.

## Display Orientation

Firmware orientation is configured in `include/TFTConfig.h`:

```cpp
// 0 = portrait, 1 = landscape, 2 = portrait inverted, 3 = landscape inverted
#define DISPLAY_ROTATION 0
```

To keep the PC stream consistent with the display, use the same orientation in `transmitter.py`:

```bash
python transmitter.py --orientation portrait
python transmitter.py --orientation landscape
```

Notes:

- `landscape` is the sender default
- `--orientation` changes the target display resolution and also sends runtime rotation to the firmware
- `--rotate` rotates the captured content before resize

## Sender Capture Modes

The sender supports:

- full monitor capture
- specific monitor with `--monitor-index`
- largest monitor with `--prefer-largest`
- active window with `--active-window` (Windows)
- window title match with `--window-title "Title"` (Windows)
- fixed region with `--region x,y,width,height`

Examples:

```bash
python transmitter.py --active-window
python transmitter.py --window-title "Visual Studio Code"
python transmitter.py --region 100,100,800,600
```

The options `--region`, `--active-window`, and `--window-title` are mutually exclusive.

## mDNS

The firmware publishes:

- hostname: `esp32s3-monitor.local`
- service: `_desktopmonitor._tcp.local`
- port: `8090`

The sender uses `zeroconf` to discover the device automatically on the LAN.

## Panel Configuration

The native panel resolution and some extra pins are configurable in `include/TFTConfig.h`:

```cpp
#define TFT_PANEL_WIDTH 170
#define TFT_PANEL_HEIGHT 320
#define DISPLAY_ROTATION 0
#define SETUP_BUTTON_PIN 14
```

This configuration targets the T-Display-S3 `170x320` ST7789V panel, but it can be adapted to compatible panels using the same library if the pin mapping, offsets, and color order also match.

## Useful Commands

```bash
pio run
pio run -t upload
pio device monitor
pip install -r requirements.txt
python transmitter.py --help
```

## If The Display Does Not Work

Typical causes are:

1. board or pin mapping does not match your hardware revision
2. display power is not enabled (`GPIO15`)
3. wrong color order

In that case you can:

- try `TFT_RGB_ORDER=TFT_BGR`
- verify that `GPIO15` is driven `HIGH`
- verify your board pin mapping
