# Pass It On

Pass It On is an interactive centerpiece for a shared meal. It combines a pace-matching companion, shared-bite lighting, a dish-level lantern, and a Leftover Roulette interaction. Together, these features explore how physical computing can support companionship, awareness of the shared meal, and taking care of remaining food.

The companion is a small tabletop creature that responds through head movements, coloured light, and sound. Bowl lifts provide signals that represent eating activity. With two diners, the companion uses the slower estimated rhythm to determine its nodding pace. When a participating diner pauses, the companion tilts its head and waits. After a longer pause involving everyone, it gives one playful movement and sound.

The dish-level lantern gives the table a shared sense of how much food is left, without singling anyone out. An ultrasonic sensor mounted on the centerpiece measures the distance down to the food in the shared bowl, and that distance becomes a group meter. “Plenty left” glows warm and full. As the dish empties, the glow softens. When the dish is clean, the table gets a short chime. The design goal was to make finishing the food feel like a small celebration, so that less food ends up in the trash, and to avoid any per-person tracking or comparison.

The intended physical form places a servo behind the creature’s head and an RGB LED inside its body. The current prototype represents these components in Wokwi. Whether the experience improves companionship or reduces food waste remains a question for future evaluation.

This repository is an Arduino Uno prototype, simulated in Wokwi and built with PlatformIO.

## The product

| Feature | What it does | Hardware |
| --- | --- | --- |
| **Shared-bite glow** | When diners lift their bowls within 20 seconds of each other, the centerpiece pulses warm for everyone. If you eat alone, it pulses with your bites. Nobody is compared; it only rewards synchrony. | 2 photoresistors, RGB LED |
| **Pace-matching companion** | A servo buddy nods along with bites but matches the slowest eater. If someone pauses to talk, it waits and the lights soften. | Servo |
| **Dish-level lantern** | An ultrasonic sensor over the shared dish drives the lantern's warmth and a text meter on the LCD. "Plenty left" glows full, "nearly done" softens, and a clean dish gets a celebration chime. It is a group meter, not per person. | HC-SR04, LCD1602, RGB LED, buzzer |
| **Leftover Roulette** | Press Play on the remote and the centrepiece assigns three roles: who takes the leftovers home, who has dish duty, and who has cleanup duty. The same remote adds or removes players (up to 5), sets immunity, and re-spins, and the LCD shows each role. | IR receiver + remote, LCD1602, RGB LED, buzzer |

### Status

| Part | State |
| --- | --- |
| Dish-level lantern | Implemented in `src/main.cpp` |
| Shared-bite glow | Implemented in `pulseShared` and `serviceSharedGlow` |
| Bowl-lift detection | Implemented in `bowlLifted(int i)` |
| Pace-matching companion | Implemented in `src/main.cpp` (`beginCompanion`, `updateCompanion`, `companionGlow`) |
| Leftover Roulette | Implemented in `src/main.cpp` (`spinRoulette`, `updateRoulette`) |

## Pin map

| Part | Pin |
| --- | --- |
| Ultrasonic TRIG / ECHO | D2 / D4 |
| Centerpiece RGB LED (R/G/B) | D3 / D5 / D6 (PWM) |
| LCD1602: RS / E | D7 / D8 |
| LCD1602: D4 / D5 / D6 / D7 | D10 / D11 / D12 / A2 |
| LCD1602: RW / V0 | GND / 10K contrast pot wiper |
| Servo companion | D9 |
| IR receiver DAT | A3 |
| Solo switch | A4 |
| Passive buzzer | D13 |
| Photoresistors (bowl A / bowl B) | A0 / A1 |

The servo library disables PWM on pins 9 and 10, so the RGB LED stays off those pins. D10 is now an LCD data line, not a PWM output. Do not use `tone()`: it takes Timer2 and breaks PWM on D3.

A5 is unused. The roulette has no physical button; every roulette action comes from the IR remote.

The 10K pot is not an input to the firmware. It only sets the LCD's `V0` contrast, so it has no effect in Wokwi (which does not simulate contrast) but matters on the real display.

The 74HC595 and LED bar are gone. The LCD needs six data/control pins in 4-bit parallel mode, and those pins came from the old bar, the shield pot, and the diner LEDs.

## Function contract

| Function | Returns / does |
| --- | --- |
| `int readDishLevel()` | Dish fullness, 0 to 100 |
| `bool bowlLifted(int i)` | True when bowl `i` is lifted; implemented |
| `void showLevel(int pct)` | Shows the level as text on the LCD |
| `void pulseShared()` | Warm centerpiece pulse for a shared bite |
| `void companionNod()` | Servo nod; pace matching is implemented |
| `void spinRoulette()` | Starts a spin and assigns leftovers, dish duty, and cleanup duty, honouring immunity |
| `void updateRoulette()` | Drives the spin/reveal animation, polls the IR remote, and refreshes the LCD |

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
3. Watch the LCD percentage step down, and the RGB lantern soften as the dish empties.
4. When the level reaches 5% or below, the buzzer plays a three-beep chime. It will not repeat until the level rises above 15% (a refill).
5. Calibrate: click the IR remote, set the ultrasonic slider to the "empty" distance and press **Test (T)**, then set it to the "full" distance and press **Test** again. The LCD flashes `Calibration` / `complete!` for 3 seconds, and the serial monitor prints both distances.

Defaults are 5 cm (full) and 30 cm (empty). Replace them with measurements from the real dish.

### Try the Leftover Roulette

1. Start the simulator and click the IR remote once so keyboard shortcuts go to it.
2. Press **+** twice to fill the roster to four players, **-** to remove one, or **1**–**5** to toggle a specific player. Players are labelled P1–P5 on the LCD.
3. Press **M** to enter immunity mode. The LCD shows `Imm 1-5  M=done` and `Imm:none`. Press **2** and **4** to make P2 and P4 immune (`Imm:P2P4`), then press **M** again to exit. The idle screen now shows `Imm:2`. Re-enter the mode any time to rewrite the set. Trying to make the last non-immune player immune fails: the LCD flashes `Immunity denied` / `1 must stay free` for 4 seconds and the setting does not change.
4. Press **P** (Play). The RGB LED cycles colours, the LCD shows a moving marker, and the buzzer ticks for about 2.4 seconds. The servo stays still during the roll.
5. The LCD reveals `Dish duty`, `Cleanup duty`, then `Leftovers`, each with `Player Pn`, a solid RGB colour, and a beep. The servo points at the selected player. Immune players are never assigned dish or cleanup duty while a non-immune player is available.
6. Press **P** during a reveal to re-spin. When the result screen is showing, press **P** again for a fresh spin.
7. With two players, the two duties are drawn independently, so the leftovers winner can also get dish duty, cleanup duty, or both. With three or more, the roles go to different people unless immunity leaves only one eligible player.

The remote's keyboard shortcuts are **+**, **-**, **1**–**5**, **P** (Play), **T** (Test), and **M** (Menu/immunity). The keys only work while the remote is focused; click it first.

## How to run (real hardware)

1. Wire the parts to the pin map above. Put a 220 ohm resistor on every RGB channel.
2. Wire the LCD1602 in 4-bit mode (`RW` to GND, `D0`–`D3` unconnected), and wire the 10K pot as the contrast control on `V0`.
3. Connect the IR receiver: `VCC` to 5V, `GND` to GND, `DAT` to A3.
4. Use the **passive** buzzer on D13; the firmware toggles it at about 2 kHz.
5. Connect the Uno over USB and use PlatformIO: Upload.
6. Calibrate with the remote's **Test** key, using the real dish when it is empty first, then when it is full.
7. Tune the thresholds `CLEAN_PCT` and `REARM_PCT` at the top of `src/main.cpp` to suit the real dish.

## Known limitations

- The chime and the median distance reading block the main loop. They need `millis()`-based timing before the parts are merged into one state machine (idle, eating, low, roulette).
- Wokwi's buzzer only sounds when the pin toggles quickly, so the chime toggles the buzzer pin at about 2 kHz instead of holding it high.
- The calibration defaults are placeholders, and calibration is lost on reset.
- The LCD replaced the LED bar, so the bar itself is no longer part of the prototype. Dish level is shown as text.
- Only one RGB LED is used. The second RGB LED in the kit would need three more PWM pins.
- The roulette roster supports up to five players, but the companion still senses only two bowls.
- Wokwi does not simulate the LCD's `V0` contrast input, so the contrast pot only has a visible effect on real hardware.
- The roulette is remote-only. Without the IR remote (or a phone IR blaster) there is no way to start a spin.
- The command codes match Wokwi's IR remote. A different physical remote sends different codes; watch the `IR:` lines in the serial monitor and update `IR_CMD_*` and `IR_NUMBER_CMD`.
- Immunity and the roster are held in RAM and cleared on reset. Removing a player also clears that player's immunity, and the firmware keeps at least one active player non-immune.

## Troubleshooting

| Problem | Fix |
| --- | --- |
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
| --- | --- |
| Companion servo | D9, 90 degrees at rest |
| Bowl A photoresistor | A0 |
| Bowl B photoresistor | A1 |
| Centerpiece RGB LED | D3 (R), D5 (G), D6 (B) |
| Passive buzzer | D13 |

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
| --- | ---: | --- | --- |
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
| --- | ---: | --- |
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

- The `Servo` library claims servo use on D9 and disables PWM on D9/D10. The RGB LED uses D3, D5, and D6, and should not be moved to D9 or D10. D10 is an LCD data line.
- `companionGlow()` and the existing dish lantern both write to D3/D5/D6. Companion behaviour replaces dish warmth while the dish is above 5%, except when pace is equal and `lanternGlow(pct)` is used.
- `beep()` and the dish chime share D13. The playful wiggle calls `beep(80)`; the companion must therefore coexist with `chime()` without changing shared buzzer ownership.
- Bowl-lift detection uses `analogRead()`. `serviceSharedGlow()` calls `pulseShared()` when both bowls are lifted within `SYNC_MS`, and the pulse lights the RGB LED only while the dish is above the clean threshold.
- The companion servo and Leftover Roulette share D9. `writeServo()` mutes companion movements while `rouletteActive` is true, and the roulette points the servo at each revealed player.
- The LCD uses the built-in `LiquidCrystal` library in 4-bit mode. The IR receiver uses `TinyIRReceiver` from the `IRremote` library, which uses pin-change interrupts and no hardware timer, so it is not disturbed by `analogWrite()` or the buzzer.
- Calibration runs from the remote's **Test** key and captures EMPTY before FULL. The success message flashes for 3 seconds.
- Roster and immunity editing are remote-only. **Menu (M)** opens immunity edit mode; numbers toggle immunity there and roster membership outside it. A toggle that would make everyone immune is refused with a 4-second flashing warning.

### Known companion limitations

- The bowl sensors are analog lux simulations with a fixed threshold of 450; real bowls may need a calibrated threshold or debounced analog range.
- Bowl-lift intervals depend on the 400 ms duplicate guard and 80 ms sensor debounce, so very quick or noisy movements may be missed.
- Only one interval estimate is maintained for each diner, using a 70/30 weighted moving average after the first measured interval.
- After the 20-second pause threshold, existing intervals are reset to the default if a later lift is detected after `WAIT_AFTER`.
- The 45-second wiggle triggers once per `playfulDone` reset; a new bite sets `playfulDone = false`.
- `beep()` and the dish-level `chime()` use blocking delays, and `medianDistance()` also blocks briefly. This can delay servo and serial timing.
- Calibration and companion state remain in memory and are lost after reset.
- The roulette holds the outputs for about 9 seconds. Bowl sensors keep updating, but the companion and ultrasonic readings pause until the reveal ends.
- The companion still tracks two bowls, while the roulette roster can hold up to five players. The roster is independent of the bowl sensors.
- With two players, the two duties are drawn independently from the eligible players, so the leftovers winner can also receive dish, cleanup, or both. With three or more players the roles go to different people unless immunity leaves only one eligible player.

## Leftover Roulette

The roulette turns the end of a shared meal into a short, playful ceremony. One press assigns three roles: **leftovers** (the prize), **dish duty**, and **cleanup duty**. The remote manages the roster, and the LCD shows each role as it is revealed.

### Physical interaction

| Control | Behaviour |
| --- | --- |
| IR remote **+ / -** | Add the next player (P1–P5) or remove the last active player. The roster cannot go below 2 or above 5. |
| IR remote **1–5** (roster mode) | Toggle a specific player on or off, so rosters like P1 P3 P5 are possible. |
| IR remote **M / Menu** | Enter immunity edit mode. Inside the mode, **1–5** toggle immunity for that player and **M** exits. A toggle that would make every active player immune is refused. Pressing **M** again later rewrites the set. |
| IR remote **P / Play** | Spin, or re-spin from a reveal or the result screen. Ignored during the roll itself. |
| IR remote **T / Test** | Calibrate: capture EMPTY first, then FULL. The LCD flashes `Calibration` / `complete!` for 3 seconds. |
| RGB LED | Cycles through colours during the roll, then shows a solid colour per role: blue for dish, green for cleanup, gold for leftovers. |
| LCD | Shows `Dish N%`, the P1–P5 roster and the immunity count while idle, the immunity set while editing, a moving marker during the roll, then the role and player for each reveal, followed by a summary. |
| Buzzer | Ticks during the roll and beeps at each reveal. |
| Servo | Stays still during the roll, then points at the selected player during each reveal. It returns to 90 degrees when the result is done. |

Immunity protects a player from dish duty and cleanup duty, not from the leftovers prize. At least one active player must stay non-immune: a toggle that would make everyone immune fails, leaves the setting unchanged, and flashes `Immunity denied` / `1 must stay free` for 4 seconds. Removing a player clears that player's immunity, and if a removal would leave everyone immune, the firmware clears the highest active player's immunity. Enter immunity mode again to rewrite the set.

### Assignment rules

1. The leftovers winner is drawn at random from all active players, immune or not.
2. Dish and cleanup duty are drawn only from active players who are not immune.
3. With enough eligible players, the leftovers winner is skipped, so the roles go to different people.
4. If only one eligible player remains, that player takes both duties, even if they also won the leftovers.
5. The immunity editor refuses to make every active player immune. The failed toggle flashes `Immunity denied` / `1 must stay free` for 4 seconds and leaves the setting unchanged.
6. Removing players never leaves the table with no eligible player: if it would, the firmware clears the highest active player's immunity.
7. With exactly two active players, the two duties are drawn independently from the eligible players. One person can take both, and the leftovers winner can also hold a duty.
8. The leftovers winner is announced last, after the two chores.
9. Roster and immunity changes are ignored while a spin or reveal is running. They are accepted again from the result screen.
10. Pressing **P** during a reveal cancels that result and starts a new spin.

### Wokwi test instructions

1. Build and start the simulator, then open the serial monitor at 9600 baud.
2. Click the IR remote once. The LCD shows `Dish N%` and `P1 P2 P=spin`. The spin hint only fits when three or fewer players are active.
3. Press **+** to add P3, then **4** to add P4, then **5** to add P5. Press **-** or a number to remove one. The serial monitor prints `Players: N (P1 P2 ...)`.
4. Press **M**. The LCD shows `Imm 1-5  M=done` and `Imm:none`. Press **2** and **4**; the second line becomes `Imm:P2P4`. Press **M** again to exit; the idle screen shows `Imm:2`.
5. Press **P**. The RGB LED cycles colours, the LCD shows a moving marker, and the buzzer ticks for about 2.4 seconds. The servo stays still.
6. Watch the reveal. The LCD shows one role at a time and the serial monitor prints them, chores first and the prize last:
   - `Dish duty` / `Player P3` with blue light.
   - `Cleanup duty` / `Player P5` with green light.
   - `Leftovers` / `Player P2` with gold light and the servo pointing at the winner. P2 can win the prize even while immune, but never appears as dish or cleanup here.
7. Press **P** during a reveal to re-spin. Wait for the final summary and press **P** again.
8. Re-enter immunity mode with **M**, press **2**, then **M** to clear P2's immunity. Spin again: P2 can now receive a duty. With three or four players and one immune player, that player never appears as dish or cleanup while another eligible player exists. If only one player is eligible, both duties land on that player.
9. Try to make everyone immune: with five players active, make four of them immune, then press the remaining number. The LCD flashes `Immunity denied` / `1 must stay free` for 4 seconds, returns to the edit screen, and the player stays non-immune (`Imm:4`).
10. Press **M** to leave immunity mode, then **T** to calibrate. The LCD shows `Calibrate` / `T: set EMPTY`. Set the ultrasonic distance to the empty value and press **T**, then set it to the full value and press **T** again. `Calibration` / `complete!` flashes for 3 seconds before the dish screen returns.

### Adjustable settings

| Setting | Current value | Effect |
| --- | ---: | --- |
| `MAX_PLAYERS` | 5 | Maximum active players; the remote cannot add beyond this. |
| `MIN_PLAYERS` | 2 | Minimum active players; the remote cannot remove below this. |
| `ROULETTE_SPIN_MS` | 2400 ms | Length of the colour roll before the first reveal. |
| `ROULETTE_REVEAL_MS` | 2000 / 2000 / 2600 ms | Time spent on dish duty, cleanup duty, and leftovers. |
| `CAL_DONE_MS` | 3000 ms | How long the calibration success message flashes. |
| `IMM_FAIL_MS` | 4000 ms | How long the failed-immunity warning flashes. |
| Servo angles | 30–150 degrees | `playerAngle()` spreads the active players across the pointer arc. |
| LCD summary hold | 8000 ms | How long the final result stays on screen before the dish screen returns. |

### Wiring on real hardware

- Wire the LCD1602 in 4-bit mode: `RS` to D7, `E` to D8, `D4`–`D7` to D10–D12 and A2, `RW` to GND. Leave `D0`–`D3` unconnected.
- Wire the 10K pot as the LCD contrast control: ends to 5V and GND, wiper to `V0`. Connect the LCD backlight `A` to 5V and `K` to GND.
- Wire the IR receiver: `VCC` to 5V, `GND` to GND, `DAT` to A3.
- Use the **passive** buzzer on D13.
- There is no roulette button. The IR remote is the only control; A5 is unused.
- The roulette shares D9 with the companion. `writeServo()` mutes companion movements while the roulette is active, and `serviceSharedGlow()` keeps polling the bowl sensors but leaves the RGB LED to the roulette.

