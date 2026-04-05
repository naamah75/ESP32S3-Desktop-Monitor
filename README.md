# ESP32S3-Desktop-Monitor

Fork del progetto `ESP32-Desktop-Monitor`, adattato per **LilyGo T-Display-S3** con **ESP32-S3** e display **ST7789 1.9" 170x320**.

## Struttura

- `platformio.ini` → configurazione PlatformIO
- `src/main.cpp` → sketch convertito
- `include/WifiConfig.h` → credenziali Wi-Fi
- `include/TFTConfig.h` → configurazione minima aggiuntiva
- `transmitter.py` → sender Python originale
- `requirements.txt` → dipendenze Python originali

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

Se la tua board usa una revisione diversa, modifica `platformio.ini`.

## Differenza importante rispetto alla versione 135x240

Il progetto originale usava coordinate display a 8 bit. Su `170x320` non bastano per indirizzare tutta l'altezza, quindi receiver e sender sono stati aggiornati a un protocollo nuovo:

- pacchetti `PXUP` versione `0x03`
- pacchetti `PXUR` versione `0x02`
- coordinata `y` ora a 16 bit little-endian

Questa versione del firmware richiede quindi anche il `transmitter.py` incluso in questa cartella.

## Come usare

1. Apri la cartella in VS Code con PlatformIO.
2. Modifica `include/WifiConfig.h` con SSID e password.
3. Compila e carica sull'ambiente `t-display-s3`.
4. Apri il monitor seriale a 115200 baud.
5. Installa le dipendenze Python sul PC:

```bash
pip install -r requirements.txt
```

6. Avvia `transmitter.py` dal PC.

Esempio:

```bash
python transmitter.py --ip 192.168.1.100
```

## Orientamento display

L'orientamento del firmware si imposta in `include/TFTConfig.h`:

```cpp
// 0 = portrait, 1 = landscape, 2 = portrait inverted, 3 = landscape inverted
#define DISPLAY_ROTATION 0
```

Per mantenere coerente anche lo stream dal PC, usa la stessa orientazione in `transmitter.py`:

```bash
python transmitter.py --ip 192.168.1.100 --orientation portrait
python transmitter.py --ip 192.168.1.100 --orientation landscape
```

Nota: `--orientation` cambia la risoluzione target del display, mentre `--rotate` ruota il contenuto catturato prima del resize.

## Comandi utili

```bash
pio run
pio run -t upload
pio device monitor
pip install -r requirements.txt
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
