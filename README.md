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

    The device is a USB-powered Raspberry Pi Pico / RP2040 connected to a 2.4 inch 320x240 ILI9341 color LCD and an ICS43434 digital I2S microphone. It is programmed using the Arduino IDE with Earle Philhower’s Raspberry Pi Pico core. Do not use the Arduino “Arduino Pico” core APIs and do not use third-party I2S libraries.

    The display uses the TFT_eSPI library by Bodmer, configured for ILI9341 with RP2040 PIO SPI at 40 MHz. The TFT_eSPI pin configuration already exists.

    TFT wiring:
    - TFT SCK = GP18
    - TFT MOSI / SDI = GP19
    - TFT MISO / SDO = GP16
    - TFT CS = GP17
    - TFT DC = GP20
    - TFT RESET = GP21

    Microphone wiring:
    - I2S BCLK = GP10
    - I2S LRCLK / WS = GP11
    - I2S DOUT = GP9
    - SEL is tied to GND, left channel

    The SPI bus may be shared between the display, SD card, and optional touch controller. Each SPI device has its own chip select pin.

    Installed Arduino libraries:
    - TFT_eSPI by Bodmer
    - arduinoFFT by Enrique Condes

    Important display rule:
    For animations, visualizers, moving graphics, waveforms, FFT displays, meters, games, or any frequently changing screen content, do not draw directly to the LCD every frame. Direct LCD erase/redraw can cause visible flicker. Instead, render each frame into a TFT_eSprite framebuffer first, then push the completed frame to the display with pushSprite(0, 0). Use an 8-bit 320x240 sprite by default unless the sketch specifically needs more color depth. Direct tft drawing is okay for static startup screens, menus, and error messages.

    Recommended display pattern:
    - Create TFT_eSPI tft = TFT_eSPI();
    - Create TFT_eSprite frame = TFT_eSprite(&tft);
    - In setup, use frame.setColorDepth(8);
    - In setup, use frame.createSprite(320, 240);
    - In loop, draw to frame, not directly to tft
    - End each frame with frame.pushSprite(0, 0);

    Important microphone rule:
    For microphone-based projects, include a reusable audio input section instead of scattering raw I2S reads throughout the visual code. Use the Earle Philhower RP2040 I2S interface with:
    - #include <I2S.h>
    - I2S i2s(INPUT);
    - i2s.setDATA(9);
    - i2s.setBCLK(10);
    - i2s.setBitsPerSample(32);
    - i2s.begin(sampleRate);

    Recommended baseline audio settings:
    - Sample rate: 16000 Hz
    - Block size: 256 or 512 samples
    - Start with SHIFT_AMOUNT = 12
    - Start with INPUT_GAIN = 4.0
    - Include DC offset removal
    - Include RMS level
    - Include peak level
    - Include smoothing
    - Include soft limiting or clamping
    - Include a clipping indicator when useful

    For oscilloscope-style displays:
    - Draw a continuous line trace, not vertical bars
    - Average or smooth samples when mapping them to screen pixels
    - Use a silence gate so room noise is not over-amplified
    - Avoid aggressive auto-scaling during silence

    For FFT visualizers:
    - Start from the reusable audio input block first
    - Remove DC offset before FFT
    - Apply a window function
    - Smooth FFT bin values between frames
    - Use gain/compression carefully so quiet rooms stay calm and loud sounds do not max everything out

    When generating code, assume all pin mappings and TFT_eSPI configuration already exist.

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
