# Pass It On

A centerpiece dish that glows when the table eats together, keeps everyone at the same pace, and makes sure nothing ends up in the trash.

This repository is an Arduino Uno prototype, simulated in Wokwi and built with PlatformIO.

## The product

| Feature | What it does | Hardware |
|---|---|---|
| **Shared-bite glow** | When diners lift their bowls within a few seconds of each other, the centerpiece pulses warm for everyone. If you eat alone, it pulses with your bites. Nobody is compared; it only rewards synchrony. | 2 photoresistors, RGB LED |
| **Pace-matching companion** | A servo buddy nods along with bites but matches the slowest eater. If someone pauses to talk, it waits and the lights soften. | Servo |
| **Dish-level lantern** | An ultrasonic sensor over the shared dish drives an LED bar and the lantern's warmth. "Plenty left" glows full, "nearly done" softens, and a clean dish gets a celebration chime. It is a group meter, not per person. | HC-SR04, 74HC595 + LED bar, RGB LED, buzzer |
| **Leftover Roulette** | When the dish is low but not empty, or someone presses the button, the roulette spins and picks who takes the leftovers home as a "lucky leftovers" prize. Dish duty is a secondary role. | Button, servo, buzzer |

### Status

| Part | State |
|---|---|
| Dish-level lantern | Implemented in `src/main.cpp` |
| Shared-bite glow | Stub (`bowlLifted`, `pulseShared`) |
| Companion servo | Stub (`companionNod`) |
| Leftover Roulette | Stub (`spinRoulette`) |

## Pin map (team contract)

| Part | Pin |
|---|---|
| Ultrasonic TRIG / ECHO | D2 / D4 |
| Centerpiece RGB LED (R/G/B) | D3 / D5 / D6 (PWM) |
| 74HC595 LED bar (data/latch/clock) | D7 / D8 / D12 |
| Servo companion | D9 |
| Roulette button / Calibrate button | D10 / D11 |
| Active buzzer | D13 |
| Photoresistors (bowl A / bowl B) | A0 / A1 |
| Diner LEDs (A / B) | A2 / A3 |
| Hunger dial pots (optional) | A4 / A5 |

The servo library disables PWM on pins 9 and 10, so the RGB LED stays off those pins. Do not use `tone()`: it takes Timer2 and breaks PWM on D3 and D11.

## Function contract

| Function | Returns / does |
|---|---|
| `int readDishLevel()` | Dish fullness, 0 to 100 |
| `bool bowlLifted(int i)` | True if bowl `i` is lifted |
| `void showLevel(int pct)` | Shows the level on the LED bar |
| `void pulseShared()` | Warm centerpiece pulse |
| `void companionNod()` | Servo nod, paced to the slowest eater |
| `void spinRoulette()` | Spins the roulette and picks who takes leftovers |

## Project layout

```
platformio.ini   PlatformIO build config (Uno, Arduino framework)
wokwi.toml       Tells the Wokwi extension where the compiled firmware is
diagram.json     Wokwi circuit (parts and wiring)
src/main.cpp     Firmware
```

## How to run (simulator)

### One-time setup

1. Install [VS Code](https://code.visualstudio.com/).
2. Install the **PlatformIO IDE** extension and let it finish its first-time setup.
3. Install the **Wokwi Simulator** extension.
4. Run **Wokwi: Request a New License** from the command palette (Ctrl+Shift+P, or Cmd+Shift+P on Mac) and follow the browser prompts. The license is free.
5. Apple Silicon Macs only: install Rosetta so the AVR compiler can run.
   ```
   softwareupdate --install-rosetta --agree-to-license
   ```

### Build and simulate

1. Open this folder in VS Code (File > Open Folder, choosing the folder that contains `platformio.ini`).
2. Build: click the checkmark in the bottom status bar, or press Ctrl+Alt+B (Cmd+Option+B on Mac). A successful build creates `.pio/build/uno/firmware.hex`.
3. Start the simulator: run **Wokwi: Start Simulator** from the command palette.
4. Rebuild after every code change. Wokwi runs the last compiled firmware, not the source.

### Try the dish-level lantern

1. Open the serial monitor in the simulator. It prints `level%: N`.
2. Drag the HC-SR04 distance slider. A small distance means a full dish (about 100%), and a large distance means an empty dish (about 0%).
3. Watch the LED bar step down, and the RGB lantern soften as the dish empties.
4. When the level reaches 5% or below, the buzzer plays a three-beep chime. It will not repeat until the level rises above 15% (a refill).
5. Calibrate: set the slider to the "full" distance and press the calibrate button, then set the slider to "empty" and press it again. The serial monitor confirms each value.

Defaults are 5 cm (full) and 30 cm (empty). Replace them with measurements from the real dish.

## How to run (real hardware)

1. Wire the parts to the pin map above. Put a 220 ohm resistor on every LED segment and every RGB channel. Connect the 74HC595 with OE to GND and MR to 5V.
2. Connect the Uno over USB and use PlatformIO: Upload.
3. Calibrate with the button, using the real dish when it is full and when it is empty.
4. Tune the thresholds `CLEAN_PCT` and `REARM_PCT` at the top of `src/main.cpp` to suit the real dish.

## Known limitations

- The chime and the median distance reading block the main loop. They need `millis()`-based timing before the parts are merged into one state machine (idle, eating, low, roulette).
- Wokwi's buzzer only sounds when the pin toggles quickly, so the chime toggles the buzzer pin at about 2 kHz instead of holding it high.
- The calibration defaults are placeholders, and calibration is lost on reset.

## Troubleshooting

| Problem | Fix |
|---|---|
| No checkmark in the status bar | PlatformIO has not loaded. Open the folder that contains `platformio.ini` and wait for the extension to finish. |
| Wokwi cannot find the firmware | The build did not finish, or the paths in `wokwi.toml` are wrong. Check that `.pio/build/uno/firmware.hex` exists. |
| A wire or part is missing in the simulator | A pin name in `diagram.json` is wrong. Hover over the pin in Wokwi to see its exact name. |
| No sound from the buzzer in the simulator | Rebuild, and make sure the volume is on. The chime must toggle the pin; a steady HIGH is silent in Wokwi. |