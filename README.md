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
| Bowl-lift detection | Implemented in `bowlLifted(int i)` |
| Pace-matching companion | Implemented in `src/main.cpp` (`beginCompanion`, `updateCompanion`, `companionGlow`) |
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
| `bool bowlLifted(int i)` | True when bowl `i` is lifted; implemented |
| `void showLevel(int pct)` | Shows the level on the LED bar |
| `void pulseShared()` | Warm centerpiece pulse; shared-bite implementation is incomplete |
| `void companionNod()` | Servo nod; pace matching is implemented |
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

## Pace-Matching Companion

The companion behaviour is implemented in `src/main.cpp`. A successful build confirms that the Arduino code compiles; the checks below are Wokwi test instructions, not documented test results.

### Behaviour

- **Sleeping:** Before the first detected bowl lift, the servo rests at 90 degrees and the RGB LED uses a dim, four-second warm breathing cycle.
- **Wake-up:** The first lift detected after `SENSOR_DEBOUNCE` moves the servo to 110 degrees for 350 ms, then returns it to 90 degrees. The serial monitor prints `A bowl lifted` or `B bowl lifted`, followed by `Companion wakes`.
- **Pacing:** In the default two-diner mode, the companion uses the longest measured interval among participating diners, with a 3000 ms minimum. It estimates an interval from bowl lifts and nudges toward a slower eater. A second lift within 400 ms of the previous lift is ignored.
- **Amber cue:** After both diners have usable intervals, the RGB LED turns soft amber if the slower interval is more than 1.5 times the faster interval. The servo continues following the longer interval.
- **Conversation pause:** If one participating diner has been idle for 20 seconds, the companion stops regular nods, holds the servo at 65 degrees, prints `Waiting for the table`, and gives the RGB LED a softer warm pulse.
- **Playful wiggle and chime:** When all participating diners have been idle for 45 seconds, the companion alternates the servo between 75 and 105 degrees for 1.2 seconds, toggles the buzzer for 80 ms, and prints `Playful wiggle + chime`. It performs this wiggle once until another bite resets the condition.
- **Resuming:** When no diner is paused, the companion returns the servo to 90 degrees, resumes the normal pace, and prints `Eating resumes`.
- **Solo mode:** Set `SOLO_MODE` to `true` to read only bowl A and nod immediately after each detected lift. It is currently `false`, so the default is two-diner mode.

The dish-clean threshold is evaluated separately: when the dish level is 5% or below, `companionGlow()` gives control to the dish lantern instead of the companion breathing or amber behaviour.

### Relevant hardware and pins

| Item | Pin or value |
|---|---|
| Companion servo | D9, 90 degrees at rest |
| Bowl A photoresistor | A0 |
| Bowl B photoresistor | A1 |
| Centerpiece RGB LED | D3 (R), D5 (G), D6 (B) |
| Active buzzer | D13 |

The Wokwi circuit identifies the servo as `buddy`, bowl A as `bowlA`, bowl B as `bowlB`, and the RGB LED as `rgb`.

### Wokwi test instructions

1. Build the project and start the Wokwi simulator.
2. Open the serial monitor at 9600 baud.
3. Find the `buddy` servo, `bowlA` and `bowlB` photoresistor sensors, and `rgb` LED.
4. In each sensor, change lux from `10` to `400`, hold for about one second, then change it back to `10` and hold for about one second. The code considers a reading below 450 as a lift.
5. Watch the serial monitor and the servo/RGB LED after each lift.

This `10 -> 400 -> 10` sequence is needed because 10 lux initially represents the bowl in place, while 400 lux represents a lifted bowl. One sensor change from `10` to `400` alone changes the raw state, but the code counts the stable change back to the 10 lux lifted state as a bowl lift.

### Test table

| Action | Expected servo movement | Expected light | Sound / serial output |
|---|---:|---|---|
| Start simulator; no bowl lift | Rest at 90 degrees | Dim warm breathing | `Companion ready`, then `Dish-level lantern ready` |
| Lift bowl A (`10 -> 400 -> 10`, about 1 second per position) | 110 degrees for 350 ms, then 90 | Normal dish glow or companion state | `A bowl lifted`, then `Companion wakes` |
| Lift bowl B the same way | 110 degrees for 350 ms, then 90 | Amber may appear after both diners have usable intervals | `B bowl lifted` |
| Leave one diner idle for 20 seconds | Moves or holds at 65 degrees | Soft warm pulse | `Waiting for the table` |
| Leave all participating diners idle for 45 seconds | Alternates 75/105 degrees for 1.2 seconds | Waiting pulse remains if paused | 80 ms buzzer toggle and `Playful wiggle + chime` |
| Resume bowl lifts after a pause | Returns to 90 degrees, then resumes measured nods | Normal pace or amber cue | `Eating resumes`, bowl lift message |
| Set `SOLO_MODE = true`, rebuild, and lift bowl A | Nods immediately with each detected lift | Amber pacing comparison is disabled | `A bowl lifted` |

These are the expected state transitions from the implementation. The 20-second, 45-second, and interval transitions depend on timing and must be watched in the simulator to verify.

### Adjustable settings

| Setting | Current value | Effect |
|---|---:|---|
| `SOLO_MODE` | `false` | `false` reads bowls A and B; `true` reads only bowl A |
| `DEFAULT_INTERVAL` | 6000 ms | Starting interval used if no measured interval is available |
| `WAIT_AFTER` | 20000 ms | Idle time that enters conversation-pause state |
| `PLAY_AFTER` | 45000 ms | Idle time that allows the playful wiggle |
| `SENSOR_DEBOUNCE` | 80 ms | Stability time required before accepting a sensor change |
| Lift threshold | Analog reading below 450 | The `10` lux state is lifted; `400` lux is the bowl-down state |
| Bite guard | 400 ms | Ignores repeated lift transitions inside this window |
| Group pace floor | 3000 ms | Minimum regular companion nod interval |
| Amber ratio | 1.5 times faster interval | Uses the amber cue when the slower diner is that much slower |

### Integration considerations

- The `Servo` library claims servo use on D9 and disables PWM on D9/D10. The RGB LED uses D3, D5, and D6, and should not be moved to D9 or D10.
- `companionGlow()` and the existing dish lantern both write to D3/D5/D6. Companion behaviour replaces dish warmth while the dish is above 5%, except when pace is equal and `lanternGlow(pct)` is used.
- `beep()` and the dish chime share D13. The playful wiggle calls `beep(80)`; the companion must therefore coexist with `chime()` without changing shared buzzer ownership.
- Bowl-lift detection uses `analogRead()`. The same sensor path does not currently call `pulseShared()`, so the shared-bite RGB pulse is not implemented.
- The companion servo and Leftover Roulette both use servo-related hardware patterns. Roulette remains a stub and must not be merged into D9 without coordination.

### Known companion limitations

- The bowl sensors are analog lux simulations with a fixed threshold of 450; real bowls may need a calibrated threshold or debounced analog range.
- Bowl-lift intervals depend on the 400 ms duplicate guard and 80 ms sensor debounce, so very quick or noisy movements may be missed.
- Only one interval estimate is maintained for each diner, using a 70/30 weighted moving average after the first measured interval.
- After the 20-second pause threshold, existing intervals are reset to the default if a later lift is detected after `WAIT_AFTER`.
- The 45-second wiggle triggers once per `playfulDone` reset; a new bite sets `playfulDone = false`.
- `beep()` and the dish-level `chime()` use blocking delays, and `medianDistance()` also blocks briefly. This can delay servo and serial timing.
- Calibration and companion state remain in memory and are lost after reset.
- The shared-bite pulse, diner LEDs, roulette button, and Leftover Roulette are still stubs.