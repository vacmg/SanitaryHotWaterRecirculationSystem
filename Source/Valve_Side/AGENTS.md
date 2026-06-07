# AGENTS.md

## Scope
- This folder (`Valve_Side/`) is the AVR valve controller (Arduino Mega 2560) for the sanitary hot-water recirculation system.
- No pre-existing AI instruction files were found via glob search (`.github/copilot-instructions.md`, `AGENTS.md`, `CLAUDE.md`, etc.).

## Architecture at a glance
- Main control loop is in `src/main.cpp`: periodic tasks + button handling + FSM step + comms event processing.
- System behavior is state-driven in `src/FSM.cpp`, with modes/status enums in `include/Types.h`.
- Hardware-facing modules are split by concern:
  - `src/Sensors.cpp`: valve temp + pressure trigger logic.
  - `src/HeaterControl.cpp`: valve relay, pump commands, heater MCU link, watchdog sync.
  - `src/Leds.cpp`: status/animation rendering.
  - `src/Errors.cpp`: persistent error log + fallback transition + reboot.
- Shared mutable runtime state is centralized as globals in `include/Globals.h` and instantiated in `src/main.cpp`.

## Cross-device communication boundaries
- Valve MCU talks to heater MCU using `SimpleComms` over `Serial2` (`src/main.cpp`, `lib/SimpleComms/*`).
- Wire protocol is command/argument framed: `"{HEADER}{CMD$}[ARG$]*"` (`include/Config.h`).
- Header is fixed to `SHWRS_`; key commands are `PUMP`, `TMP`, `WTD-RST`, `SPT`, `OK`, `ERROR`.
- `extras/uart-to-can/` is a separate ESP-IDF bridge (UART<->CAN) used in the full system path, not part of the PlatformIO AVR firmware build.

## Behavioral patterns to preserve
- `raiseError(...)` is terminal by design: logs, stores to EEPROM, enables fallback, then reboots (`src/Errors.cpp`).
- Fallback mode persistence is EEPROM-backed (`EEPROM_FALLBACK_MODE_ENABLED_ADDRESS`, `EEPROM_MODE_ADDRESS` in `include/Config.h`).
- Watchdog policy is dual-sided: AVR WDT + remote watchdog reset command (`resetWatchdogs*` in `src/HeaterControl.cpp`).
- Sensor reads are intentionally non-blocking in loop cadence (`requestValveTempIfNecessary()` then `getValveTempIfNecessary()`).
- Mode/status changes should go through `changeMode()` / `changeStatus()` (`src/Utils.cpp`) to keep LED/status side effects consistent.

## Developer workflows (project-local)
- AVR firmware uses PlatformIO env `megaatmega2560` (`platformio.ini`). Typical commands:
  - `pio run -e megaatmega2560`
  - `pio run -e megaatmega2560 -t upload`
  - `pio device monitor -b 115200`
  - `pio test -e megaatmega2560`
- Runtime console commands are implemented in `serialEvent()` (`src/main.cpp`): `help`, `sensors`, `changemode`, `startpump`, `stoppump`, `openvalve`, `closevalve`, `enable`, `disable`, `reset`, `reseton`, `errorlist`.
- ESP32 bridge in `extras/uart-to-can/` is built with ESP-IDF (`CMakeLists.txt` + `main/main.cpp`), not with the root PlatformIO config.

## Project-specific coding conventions
- Extensive compile-time feature flags in `include/Config.h` (e.g., `DISABLE_WATCHDOGS`, `MOCK_SENSORS`, `PROFILER_ENABLED`) are part of normal workflow.
- Debug output uses `debug(...)` / `debugln(...)` macros (gated by `DEBUG`) rather than raw `Serial` in most internal paths.
- Flash-string literals (`F("...")`) and fixed-size buffers are preferred for AVR memory safety.
- Communication handlers commonly implement retry loops with exponential delay for pump control (`setPump()` in `src/HeaterControl.cpp`).

## High-value starting points for new agents
- Read first: `include/Config.h`, `include/Types.h`, `src/main.cpp`, `src/FSM.cpp`.
- Then inspect: `src/HeaterControl.cpp` and `src/Errors.cpp` for failure semantics and reboot behavior.
- If touching protocol behavior, update both parser/sender assumptions in `lib/SimpleComms/SimpleComms.cpp` and call sites in `src/HeaterControl.cpp`.

