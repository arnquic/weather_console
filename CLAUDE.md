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

- **`weather_console.ino`** — entry point. `setup()` wires up the display, data buffers, and IoT Cloud; `loop()` pumps `ArduinoCloud.update()`, polls touch input, and redraws the graph only when `needsRedraw()` is set (dirty-flag rendering, not redraw-every-frame). The `onXChange()` callbacks are IoT Cloud property-change hooks (registered in `thingProperties.h`) that feed new sensor values into `data_manager`.
- **`thingProperties.h`** — Arduino IoT Cloud codegen: declares the cloud properties (`temp`, `humidity`, `pressure`, `light_alpha`) and `initProperties()`, which registers each property with an `ON_CHANGE` callback. Treat this as generated glue, not hand-authored architecture — if properties change, they're normally regenerated from the Arduino IoT Cloud "Thing" (id in the header comment of the `.ino`).
- **`data_manager.h/.cpp`** — owns all sensor history state: fixed-size circular buffers (`MAX_POINTS = 100`) for temp/humidity/pressure, plus zoom/pan view state (`zoomLevel`, `viewOffset`, `fullyZoomedOut`). Temperature arrives with a new sample index each update (`addDataPoint`); humidity/pressure instead patch the *most recent* slot (`updateHumidityData`/`updatePressureData`) since they change independently of the temp cadence. `updateMinMaxForCurrentView()` recomputes the visible min/max (with 10% padding) *and* the visible-window average whenever data or the view window changes — the average is accumulated inside the same loop that computes min/max (not a separate pass), so it stays in sync with the same `zoomLevel`/`viewOffset` window. `display_manager` reads these via `getMinMaxValues()`/`getAverageValue()` rather than scanning data itself.
- **`display_manager.h/.cpp`** — owns the GFX/touch peripherals (`GigaDisplay_GFX`, `Arduino_GigaDisplayTouch`) and all rendering/input logic. Key things to know before touching this file:
  - The top nav bar is a hamburger-icon + view-name/average readout (`drawNavBar()`), not buttons. Tapping the icon opens a left-side drawer (`drawDrawer()`, 3 rows: Temperature/Humidity/Pressure) that overlays part of the graph; tapping a row switches the view and closes the drawer, tapping elsewhere while open just closes it. The icon itself swaps between a hamburger (closed) and an "X" (open) via `drawHamburgerIcon()`/`drawCloseIcon()`, chosen in `drawNavBar()` based on the `drawerOpen` static flag.
  - Per-view color/label/unit are centralized in three small static helpers (`getViewColor`/`getViewLabel`/`getViewUnit`) near the top of the file — `drawGraph()`, `drawNavBar()`, and `drawDrawer()` all call these rather than each having their own `switch(currentView)`. They're plain file-local statics with no header declaration, so they must stay defined before any function that calls them (C++ single-translation-unit ordering).
  - **The nav bar is not redrawn automatically.** `drawNavBar()` must be called explicitly whenever anything it displays changes — view switch, drawer open/close, *and* zoom/pan (since the average reflects the visible window). `setRedrawFlag()` only causes `drawGraph()` to be redrawn on the next `loop()` iteration; it does not touch the nav bar. Any new touch-handling code that changes the current view, zoom level, or pan offset needs its own explicit `drawNavBar()` call — this was missed for zoom/pan when the average feature was first added and had to be fixed.
  - Touch coordinates are rotated/remapped (`touchX = points[i].y; touchY = 480 - points[i].x`) to match `display.setRotation(1)`. Any new touch-handling code must apply the same transform.
  - `handleTouch()` is a single state machine distinguishing input by contact count, screen region, and timing: two-finger pinch-to-zoom (`gestureActive`), single-finger pan-in-graph (`panActive`), and single-finger tap-on-hamburger-icon/drawer-row (gated by the `drawerOpen` flag) — with small `millis()`-based debounce windows to avoid one gesture bleeding into another. Pinch-zoom and pan are both suppressed outright while `drawerOpen` is true (the drawer covers the graph, so those touches are drawer interactions, not graph interactions).
  - Graph rendering (`drawGraph`) and min/max/average calc (`updateMinMaxForCurrentView`) both independently derive the same "visible window" start index from `zoomLevel`/`viewOffset`/circular-buffer wraparound — if you change the windowing math, update both. The drawer's row hit-testing in `handleTouch()` also duplicates the drawer's row-layout math from `drawDrawer()` (`rowHeight = graphHeight / 3`) — keep those in sync too if the drawer layout changes.
  - Redraws are split into "static" (nav bar + frame, drawn once / on state change) and "graph" (data-dependent, drawn on the dirty flag) to avoid flicker/unnecessary redraw of the whole screen.
  - Colors are raw RGB565 hex constants (`COLOR_*` in `display_manager.h`), not a theme system.
