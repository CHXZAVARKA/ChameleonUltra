# Chameleon Ultra LED capabilities

## Conclusion

Chameleon Ultra can produce yellow. Each of its eight user indicators is a physical RGB LED. The official schematic shows separate red, green, and blue dies in every package. All eight positions share three color buses (`LED_R_H`, `LED_G_H`, and `LED_B_H`), while each position has its own enable line (`LED1` through `LED8`).

The stock firmware exposes seven additive colors: red, green, blue, magenta, yellow, cyan, and white. Yellow is produced by enabling red and green together. The observed blue color for an empty or fully disabled slot is stock firmware behavior, not a hardware limitation.

## Primary-source evidence

1. The official hardware documentation states that Chameleon Ultra has eight LEDs used to indicate the active slot:
   - https://github.com/RfidResearchGroup/ChameleonUltra/wiki/hardware
2. The official Ultra schematic shows eight `MHPA0606RGBDT` RGB packages. The red, green, and blue lines are shared across all eight LEDs, and `LED1` through `LED8` select individual positions:
   - https://github.com/RfidResearchGroup/ChameleonUltra/blob/main/hardware/ultra/Chameleon_nrf52_ultra_V1.0.pdf
3. The official firmware defines seven supported additive colors, including `RGB_YELLOW`:
   - https://github.com/RfidResearchGroup/ChameleonUltra/blob/main/firmware/common/hw_connect.h
4. The official color driver implements yellow by enabling the red and green channels together. It implements cyan as green plus blue, magenta as red plus blue, and white as all three channels:
   - https://github.com/RfidResearchGroup/ChameleonUltra/blob/main/firmware/common/hw_connect.c
5. Stock slot-color selection returns red for enabled dual-frequency data, green for enabled HF data, and blue for every other case. Therefore LF-only, empty, disabled, and partially disabled cases can all fall into the blue branch:
   - https://github.com/RfidResearchGroup/ChameleonUltra/blob/main/firmware/application/src/rfid_main.c
6. The current official firmware already contains a function named `field_generator_rainbow_loop`. It steps all eight LEDs through six discrete colors every 100 ms: red, yellow, green, cyan, blue, and magenta:
   - https://github.com/RfidResearchGroup/ChameleonUltra/blob/main/firmware/application/src/app_main.c
7. The nRF52840 provides four four-channel PWM units. PWM can therefore vary the effective intensity of the shared red, green, and blue buses in custom firmware:
   - https://docs.nordicsemi.com/r/bundle/ps_nrf52840/page/keyfeatures_html5.html

## What kind of rainbow is possible

### With the current firmware color API

Only seven non-black colors are available. The existing rainbow is a six-step animation, not a smooth hue transition.

### With a custom PWM color driver

A smooth perceived hue wheel is feasible by applying independent PWM duty cycles to the shared red, green, and blue buses. Gamma correction and per-channel calibration are needed because the three dies and their series resistors do not have equal perceived brightness.

This is additive RGB mixing, so it approximates visible rainbow hues. It does not generate every spectral wavelength. In practical UI terms it can still produce smooth orange, yellow, lime, turquoise, violet, and intermediate gradients.

### Per-slot color limitation

All simultaneously lit positions normally share one hue because the RGB buses are common. The hardware cannot statically hold eight independent hues at the same instant.

Different apparent hues per position could be simulated by fast multiplexing: select one slot, set its RGB duty values, then repeat for the other slots faster than the eye can follow. This reduces duty cycle and brightness and adds timing complexity around BLE and RFID work. A reliable alternative is a shared smooth hue with a moving per-slot brightness tail.

## Why the tested empty slot is blue

The connected device is still running firmware with the stock fallback branch. That branch returns blue whenever the slot is not an enabled dual-frequency or enabled HF slot. The local custom firmware changes the empty or fully disabled state to `RGB_YELLOW`, but its DFU package has not been installed on the physical device.
