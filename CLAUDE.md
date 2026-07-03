# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Arduino sketch for a Giga Display touchscreen weather console. It connects to Arduino IoT Cloud, receives temperature/humidity/pressure/light readings, buffers history in RAM, and renders an interactive scrolling/zoomable line graph with touch-driven navigation.

Target board: Arduino Giga (fqbn `arduino:mbed_giga:giga`, see `sketch.json`).

## Build / upload / verify

This is an Arduino CLI project (no npm/make build system). Use the `arduino-cli` skills already configured in this environment (arduino-build, arduino-upload, arduino-boards, arduino-libs) rather than invoking `arduino-cli` manually when possible.

- Compile: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
- Upload: `arduino-cli compile --fqbn arduino:mbed_giga:giga --upload -p <port> .`
- List connected boards/ports: `arduino-cli board list`

There are no automated tests (no test framework is set up for this sketch) — verification is by compiling and by exercising the touchscreen UI on real hardware.

## Architecture

The sketch is split into three cooperating modules, all sharing global state via extern-style getter/setter functions (no classes/instances):

- **`weather_console_dec12a.ino`** — entry point. `setup()` wires up the display, data buffers, and IoT Cloud; `loop()` pumps `ArduinoCloud.update()`, polls touch input, and redraws the graph only when `needsRedraw()` is set (dirty-flag rendering, not redraw-every-frame). The `onXChange()` callbacks are IoT Cloud property-change hooks (registered in `thingProperties.h`) that feed new sensor values into `data_manager`.
- **`thingProperties.h`** — Arduino IoT Cloud codegen: declares the cloud properties (`temp`, `humidity`, `pressure`, `light_alpha`) and `initProperties()`, which registers each property with an `ON_CHANGE` callback. Treat this as generated glue, not hand-authored architecture — if properties change, they're normally regenerated from the Arduino IoT Cloud "Thing" (id in the header comment of the `.ino`).
- **`data_manager.h/.cpp`** — owns all sensor history state: fixed-size circular buffers (`MAX_POINTS = 100`) for temp/humidity/pressure, plus zoom/pan view state (`zoomLevel`, `viewOffset`, `fullyZoomedOut`). Temperature arrives with a new sample index each update (`addDataPoint`); humidity/pressure instead patch the *most recent* slot (`updateHumidityData`/`updatePressureData`) since they change independently of the temp cadence. `updateMinMaxForCurrentView()` recomputes the visible min/max (with 10% padding) whenever data or the view window changes, and `display_manager` reads those via `getMinMaxValues()` rather than scanning data itself.
- **`display_manager.h/.cpp`** — owns the GFX/touch peripherals (`GigaDisplay_GFX`, `Arduino_GigaDisplayTouch`) and all rendering/input logic. Key things to know before touching this file:
  - Touch coordinates are rotated/remapped (`touchX = points[i].y; touchY = 480 - points[i].x`) to match `display.setRotation(1)`. Any new touch-handling code must apply the same transform.
  - `handleTouch()` is a single state machine distinguishing three input modes by contact count and timing: two-finger pinch-to-zoom (`gestureActive`), single-finger pan-in-graph (`panActive`), and single-finger tap-on-button — with small `millis()`-based debounce windows to avoid one gesture bleeding into another.
  - Graph rendering (`drawGraph`) and min/max calc (`updateMinMaxForCurrentView`) both independently derive the same "visible window" start index from `zoomLevel`/`viewOffset`/circular-buffer wraparound — if you change the windowing math, update both.
  - Redraws are split into "static" (nav bar + frame, drawn once / on view change) and "graph" (data-dependent, drawn on the dirty flag) to avoid flicker/unnecessary redraw of the whole screen.
  - Colors are raw RGB565 hex constants (`COLOR_*` in `display_manager.h`), not a theme system.
