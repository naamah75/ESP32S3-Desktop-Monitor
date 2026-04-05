# ESP32S3-Desktop-Monitor

Fork del progetto `ESP32-Desktop-Monitor`, adattato per **LilyGo T-Display-S3** con **ESP32-S3** e display **ST7789 1.9" 170x320**.

## Struttura

- `platformio.ini` → configurazione PlatformIO
- `src/main.cpp` → sketch convertito
- `include/WifiConfig.h` → fallback pubblico e opzioni setup Wi-Fi
- `include/WifiConfig.local.h` → credenziali locali non tracciate
- `include/TFTConfig.h` → risoluzione, rotazione e pin aggiuntivi
- `transmitter.py` → sender Python aggiornato
- `requirements.txt` → dipendenze Python

## Note importanti

Questa configurazione assume il pinout ufficiale **LilyGo T-Display-S3**:

- CS = 6
- DC = 7
- RST = 5
- WR = 8
- RD = 9
- D0..D7 = 39, 40, 41, 42, 45, 46, 47, 48
- BL = 38
- POWER = 15

Se la tua board usa una revisione diversa, modifica `platformio.ini` e `include/TFTConfig.h`.

## Differenza importante rispetto alla versione 135x240

Il progetto originale usava coordinate display a 8 bit. Su `170x320` e in `landscape 320x170` non bastano per indirizzare tutto il pannello, quindi receiver e sender sono stati aggiornati a un protocollo nuovo:

- pacchetti `PXUP` versione `0x04`
- pacchetti `PXUR` versione `0x02`
- coordinate `x` e `y` a 16 bit little-endian nei pacchetti pixel
- pacchetto `PXOR` per sincronizzare la rotazione runtime del display

Questa versione del firmware richiede quindi anche il `transmitter.py` incluso in questa cartella.

## Wi-Fi e captive portal

Il firmware supporta tre modalita' di avvio Wi-Fi:

- usa le credenziali fallback di `include/WifiConfig.local.h` o `include/WifiConfig.h`
- se non funzionano, prova le credenziali salvate in flash
- se non riesce a collegarsi, apre un captive portal

Captive portal:

- AP: `ESP32S3-Monitor-Setup`
- URL: `http://192.168.4.1`
- pulsante setup: `IO14` tenuto basso al boot

Forzatura software del portal:

```cpp
#define FORCE_WIFI_SETUP 1
```

Configurazione consigliata per GitHub:

- lascia `include/WifiConfig.h` con placeholder pubblici
- salva le credenziali vere in `include/WifiConfig.local.h`
- `include/WifiConfig.local.h` e' ignorato da git

## Come usare

1. Apri la cartella in VS Code con PlatformIO.
2. Se vuoi un fallback locale, crea o aggiorna `include/WifiConfig.local.h` con SSID e password.
3. Compila e carica sull'ambiente `t-display-s3`.
4. Apri il monitor seriale a 115200 baud.
5. Installa le dipendenze Python sul PC:

```bash
pip install -r requirements.txt
```

6. Avvia `transmitter.py` dal PC.

Esempio:

```bash
python transmitter.py
```

Se non passi `--ip`, il sender cerca automaticamente il device via mDNS usando `_desktopmonitor._tcp.local`.

## Orientamento display

L'orientamento del firmware si imposta in `include/TFTConfig.h`:

```cpp
// 0 = portrait, 1 = landscape, 2 = portrait inverted, 3 = landscape inverted
#define DISPLAY_ROTATION 0
```

Per mantenere coerente anche lo stream dal PC, usa la stessa orientazione in `transmitter.py`:

```bash
python transmitter.py --orientation portrait
python transmitter.py --orientation landscape
```

Nota:

- `landscape` e' il default del sender
- `--orientation` cambia la risoluzione target del display e invia anche la rotazione runtime al firmware
- `--rotate` ruota il contenuto catturato prima del resize

## Modalita' di cattura del sender

Il sender supporta:

- monitor intero
- monitor specifico con `--monitor-index`
- monitor piu' grande con `--prefer-largest`
- finestra attiva con `--active-window` (Windows)
- finestra per titolo con `--window-title "Titolo"` (Windows)
- regione con `--region x,y,width,height`

Esempi:

```bash
python transmitter.py --active-window
python transmitter.py --window-title "Visual Studio Code"
python transmitter.py --region 100,100,800,600
```

Le opzioni `--region`, `--active-window` e `--window-title` sono mutuamente esclusive.

## mDNS

Il firmware pubblica:

- hostname: `esp32s3-monitor.local`
- service: `_desktopmonitor._tcp.local`
- porta: `8090`

Il sender usa `zeroconf` per trovare automaticamente il device in LAN.

## Configurazione pannello

La risoluzione nativa del pannello e alcuni pin aggiuntivi sono configurabili in `include/TFTConfig.h`:

```cpp
#define TFT_PANEL_WIDTH 170
#define TFT_PANEL_HEIGHT 320
#define DISPLAY_ROTATION 0
#define SETUP_BUTTON_PIN 14
```

Questa configurazione e' pensata per il pannello ST7789V `170x320` della T-Display-S3, ma puo' essere adattata a pannelli compatibili con la stessa libreria se anche pin, offset e ordine colori sono coerenti.

## Comandi utili

```bash
pio run
pio run -t upload
pio device monitor
pip install -r requirements.txt
python transmitter.py --help
```

## Se il display non va

Le cause classiche sono:

1. board o pin mapping non coerenti con la tua revisione
2. alimentazione display non abilitata (`GPIO15`)
3. ordine colori

In quel caso puoi:

- provare `TFT_RGB_ORDER=TFT_BGR`
- verificare che `GPIO15` venga portato `HIGH`
- verificare i pin della tua board
