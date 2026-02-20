# Pico LCD Microphone Board  
### Reprogramming Instructions & Base Firmware  
SimpleCustomPCBs

This page contains setup instructions and factory firmware for the Raspberry Pi Pico Audio Visualizer development platform.

Hardware included on this board:

- Raspberry Pi Pico (RP2040)
- 2.4" 320×240 ILI9341 TFT LCD
- ICS43434 digital I2S MEMS microphone
- Preconfigured SPI + I2S wiring

---

## IMPORTANT

This board requires the **Earle Philhower RP2040 core**.

Do NOT use:
- The Arduino “Arduino Pico” core
- Third-party RP2040 I2S libraries

Using the wrong core will cause microphone or compilation errors.

---

## Step 1 — Install Arduino IDE

Download and install the latest Arduino IDE (2.x recommended).

---

## Step 2 — Add RP2040 Board Manager URL

Open Arduino IDE → File → Preferences

In **Additional boards manager URLs**, paste:

```
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```

Click OK.

---

## Step 3 — Install RP2040 Board Package

Go to:

Tools → Board → Boards Manager

Search for:

Raspberry Pi Pico

Install:

Raspberry Pi Pico/RP2040/RP2350  
by Earle F. Philhower, III  
(Version 5.5.0 or newer recommended)

After installation, select:

Tools → Board → Raspberry Pi Pico

---

## Step 4 — Install Required Libraries

Go to:

Tools → Manage Libraries

Install:

- TFT_eSPI by Bodmer (v2.5.43 or newer)
- arduinoFFT by Enrique Condes (v2.0.4 or newer)

---

## Step 5 — Configure TFT_eSPI (Required)

Open this file:

### Windows
Documents\Arduino\libraries\TFT_eSPI\User_Setup.h

### macOS / Linux
~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h

Update the pin definitions:

```c
#define TFT_MISO 16
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   17
#define TFT_DC   20
#define TFT_RST  21
```

Enable backlight and set to -1:

```c
#define TFT_BL   -1
```

Set SPI frequency to 40 MHz:

```c
#define SPI_FREQUENCY  40000000
```

Enable RP2040 PIO SPI (remove // if present):

```c
#define RP2040_PIO_SPI
```

Save the file.

Close and reopen Arduino IDE.

---

## Recommended Board Settings

After selecting Raspberry Pi Pico, verify:

Tools → CPU Speed → 133 MHz  
Tools → Flash Size → 2MB  
Tools → Optimize → Default  

---

## Step 6 — Enter BOOTSEL Mode

1. Hold down the BOOTSEL button.
2. Plug the board into your computer.
3. Release BOOTSEL.

---

## Step 7 — Select USB Port

Tools → Port → Select Raspberry Pi Pico

If no port appears:
- Try a different USB cable.
- Try a different USB port.

---

## Step 8 — Generate Custom Code (Optional)

You may use an AI assistant to generate new sketches.

Paste this prompt into your AI tool:

```
You are my programming assistant for a custom hardware device that I own.
The device is a USB-powered Raspberry Pi Pico (RP2040) connected to a 2.4" 320×240 ILI9341 color LCD and an ICS43434 digital I2S microphone. It is designed to be programmed using the Arduino IDE and uses Earle Philhower’s Raspberry Pi Pico core (not the Arduino Pico core).

The display uses the TFT_eSPI library by Bodmer, configured for ILI9341 with PIO SPI at 40 MHz. The pin wiring is:
• TFT SCK = GP18
• TFT MOSI (SDI) = GP19
• TFT MISO (SDO) = GP16
• TFT CS = GP17
• TFT DC = GP20
• TFT RESET = GP21

The microphone uses I2S and is wired:
• BCLK = GP10
• LRCLK = GP11
• DOUT = GP9
• SEL is tied to GND (left channel)

The SPI bus is shared between the display, SD card, and optional touch controller. Each device has its own CS pin.

The Arduino libraries installed are:
• TFT_eSPI by Bodmer
• arduinoFFT by Enrique Condes

I am using the Earle Philhower RP2040 core which provides the I2S interface. Do not use Arduino Pico I2S APIs or third-party I2S libraries.

When generating code, assume all pin mappings and library configuration already exist.

First greet me briefly, then ask:
"What do you want me to make?"

After that, generate complete Arduino sketches that compile for this board and use the LCD and microphone correctly.
```

---

## Step 9 — Upload Code

1. Paste your sketch into Arduino IDE.
2. Click Upload.
3. Wait for the board to reboot and run your code.

If it does not work:
- Check TFT_eSPI configuration.
- Confirm correct board core is installed.
- Confirm correct USB port is selected.

---

## Factory Firmware

Factory firmware is located in:

firmware/base/

Upload it at any time to restore the board.

---

## Audio Behavior Note

In very quiet rooms, minor internal electrical noise may cause slight visual activity.

This is normal for high-gain microphone circuits.

The device performs best with active audio input.

---

## License

MIT License (see LICENSE file).
