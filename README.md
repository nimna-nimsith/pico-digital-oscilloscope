# Pico Digital Oscilloscope
 
A small digital oscilloscope built on the Raspberry Pi Pico (RP2040), with live waveform display on a 1.8" ST7735 TFT and a physical RUN/STOP control.
 
## Features
 
- Real-time waveform capture and display via SPI TFT
- Measurements: Vmax, Vmin, Vpp, average voltage, RMS, frequency
- Color-coded stats panel (Vpp/cyan, Freq/yellow, Max-Min/white, RMS/pink, Avg/blue)
- Live RUN/STOP badge with partial-redraw rendering to minimize flicker
- Hardware RUN/STOP toggle via a tactile push button
## Hardware
 
| Component | Notes |
|---|---|
| Raspberry Pi Pico | RP2040, Arduino IDE (Mbed core) |
| ST7735 TFT, 128x160 | SPI display |
| 4-pin tactile push button | RUN/STOP control |
| 100 kΩ resistor | Input divider (top) |
| 33 kΩ resistor | Input divider (bottom) |
| 10 nF capacitor | Front-end low-pass filter |
 
## Wiring
 
### TFT display
 
| TFT | Pico |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL / SCK | GP18 |
| SDA / MOSI | GP19 |
| CS | GP17 |
| DC / A0 | GP7 |
| RES / RST | GP8 |
| LED / BL | 3.3V |
 
### Analog input
 
Signal → 100 kΩ → GP26 (ADC0), with 33 kΩ + 10 nF to GND (voltage divider + low-pass filter).
 
### RUN/STOP button
 
One leg to GP14, the other to GND. Internal pull-up enabled in software (no external resistor needed).
 
## Getting started
 
1. Install the [Arduino-Pico core](https://github.com/earlephilhower/arduino-pico) in the Arduino IDE.
2. Install the `Adafruit_GFX` and `Adafruit_ST7735` libraries via the Library Manager.
3. Wire the hardware as described above.
4. Open `pico_oscilloscope.ino`, select your Pico board, and upload.
## Notes on accuracy
 
This is an educational prototype, not a lab-grade instrument. The input divider gives roughly 0.25x attenuation, and the 10 nF filter caps the usable bandwidth to a few hundred Hz — swap in a smaller capacitor (e.g. 1 nF) if you need to capture kHz-range signals. Don't connect mains or unknown high voltages to the input.
 
## License
 
MIT
 
