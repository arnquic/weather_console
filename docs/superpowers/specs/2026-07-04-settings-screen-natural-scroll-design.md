# Settings screen + Natural/Classic zoomed-scroll direction — design

## Context

Panning the graph while zoomed in currently always moves the visible window opposite to the drag direction (dragging right moves the view toward more recent/live data). This feels backwards compared to "natural" scrolling (content follows the finger) that most touchscreen users now expect. This feature adds a user-facing setting to choose between the current behavior ("Classic") and an inverted, more intuitive behavior ("Natural," the new default), and adds a Settings screen — reachable from the hamburger drawer — to hold that toggle.

## Goals

1. Add a "Zoomed Scroll" setting with two values, Natural and Classic, defaulting to Natural on every boot (no persistence).
2. Natural inverts the existing pan direction so the graph content follows the drag direction; Classic preserves today's exact behavior.
3. Add a Settings screen, reachable as a 4th item in the existing hamburger drawer, where the user can switch between Natural and Classic immediately.

## Navigation & state model

- New `enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS };` and a `static ScreenMode currentScreen = SCREEN_GRAPH;` in `display_manager.cpp`, alongside the existing `currentView` (Temperature/Humidity/Pressure) and `drawerOpen` state.
- The drawer (`drawDrawer()`) changes from 3 to 4 equal-height rows: Temperature, Humidity, Pressure, Settings. `rowHeight` becomes `graphHeight / 4`. The Settings row shows a small gear icon next to its label (see "Settings screen rendering" below) to visually distinguish it from the three colored sensor rows, since it has no natural accent color of its own — it uses `COLOR_TEXT`/`COLOR_NAV_ACTIVE` for its active/inactive states like the others, just without a colored accent border.
- Tapping the Temperature/Humidity/Pressure row (unchanged behavior, extended to the new 4-row layout): sets `currentView` to that row's view, sets `currentScreen = SCREEN_GRAPH`, closes the drawer, redraws nav bar + graph.
- Tapping the Settings row: sets `currentScreen = SCREEN_SETTINGS` **and** resets `currentView = VIEW_TEMP`, closes the drawer, redraws nav bar + settings screen.
- The `.ino`'s `loop()` redraw dispatch (currently `if(needsRedraw()) { drawGraph(); clearRedrawFlag(); }`) becomes screen-aware: it calls `drawGraph()` when `getCurrentScreen() == SCREEN_GRAPH` and `drawSettingsScreen()` when `SCREEN_SETTINGS`. This requires a new `ScreenMode getCurrentScreen()` getter in `display_manager.h`/`.cpp` (mirroring the existing `getCurrentView()` pattern) so the `.ino` file (which doesn't have direct access to the file-local static) can read it.
- `drawNavBar()`: when `currentScreen == SCREEN_SETTINGS`, shows the plain text "Settings" in `COLOR_TEXT` (no accent color, no average suffix) instead of the view name + average. When `currentScreen == SCREEN_GRAPH`, behavior is unchanged from today.
- `handleTouch()`: pinch-zoom (`contacts >= 2` branch) and pan (single-touch-in-graph-area branch) are suppressed whenever `currentScreen != SCREEN_GRAPH`, using the same early-return pattern already used for `drawerOpen`. (Note: `drawerOpen` and `currentScreen == SCREEN_SETTINGS` are independent flags — the drawer can still be opened while Settings is showing, exactly as it can while a graph view is showing, and doing so takes precedence over Settings' own touch handling the same way it currently takes precedence over graph pan/zoom.)

## Natural vs. Classic scroll

- New `enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };` and `static ScrollMode scrollMode = SCROLL_NATURAL;` in `display_manager.cpp`.
- In the existing pan-continuation code in `handleTouch()`:
  ```cpp
  int panDelta = -panChange / 3;  // Negative because dragging right should increase offset
  ```
  becomes:
  ```cpp
  int panDelta = (scrollMode == SCROLL_NATURAL) ? (panChange / 3) : (-panChange / 3);
  ```
  `SCROLL_CLASSIC` reproduces the exact existing formula (`-panChange / 3`). `SCROLL_NATURAL` is the sign-flipped version (`panChange / 3`), making the visible window move in the same direction as the drag.
- No EEPROM/KVStore persistence — `scrollMode` is a plain static that always initializes to `SCROLL_NATURAL` on power-up, matching "default to Natural on boot."

## Settings screen rendering

- New `void drawSettingsScreen()` in `display_manager.cpp`, drawing into the same screen region `drawGraph()` uses (`GRAPH_MARGIN` frame, same clear-and-redraw pattern: clear the interior, draw content, restore the border).
- Content: a "Zoomed Scroll" label near the top, followed by two side-by-side buttons labeled "Natural" and "Classic" (styled like the drawer's active/inactive rows — active button filled with `COLOR_BUTTON_ACTIVE` and an accent border, e.g. `COLOR_NAV_ACTIVE` text, inactive button filled with `COLOR_BUTTON_BG` and a `COLOR_TEXT` border).
- Tapping either button sets `scrollMode` and immediately redraws the settings screen (via `setRedrawFlag()`, same dirty-flag mechanism as the graph) to reflect the new selection. No confirmation step, no separate "Save" action.
- Gear icon (drawer row + reused nowhere else): a simple ring (two concentric circles, outer filled in `COLOR_TEXT`, inner filled in the row's background color to punch out the middle) with four small square "teeth" at the top/bottom/left/right edges of the ring — cheap to draw with `fillCircle`/`fillRect` calls already used elsewhere in this file, no new drawing primitives needed.

## Touch handling

- `handleTouch()` gains a `currentScreen == SCREEN_SETTINGS` branch, parallel to the existing `drawerOpen` branch: on a single-finger tap (using the same `wasTouched`/`lastTouchTime` debounce already in place), hit-test against the two button regions defined in `drawSettingsScreen()`'s layout, and set `scrollMode` accordingly. Taps outside both buttons while on the Settings screen do nothing (no dismiss action needed — the only way to leave Settings is via the hamburger drawer, same as switching between any two graph views today).
- The drawer's row hit-testing (`rowIndex = (touchY - graphY) / rowHeight`) extends from 3 to 4 rows; `rowIndex == 3` maps to Settings (sets `currentScreen = SCREEN_SETTINGS`, resets `currentView = VIEW_TEMP`) instead of the current fallback-to-Pressure `else` branch, which becomes an explicit `rowIndex == 2` check for Pressure.

## Code impact summary

- `display_manager.h`: add the `ScreenMode` enum (must live in the header — `getCurrentScreen()`'s return type is used from `weather_console.ino`, outside this translation unit), add `ScreenMode getCurrentScreen()` declaration, add `void drawSettingsScreen()` declaration, add a gear-icon drawing constant or two if needed for layout. `ScrollMode` stays file-local to `display_manager.cpp` (no header entry) — nothing outside that file reads or sets it.
- `display_manager.cpp`: add `currentScreen`/`scrollMode` statics (`ScrollMode` enum defined here, file-local), extend `drawDrawer()` to 4 rows + gear icon, add `drawSettingsScreen()`, add `getCurrentScreen()`, update `drawNavBar()` for the Settings label case, update `handleTouch()` for the new screen-aware suppression, the Settings-row hit-test, the Settings-screen button hit-test, and the Natural/Classic pan formula.
- `weather_console.ino`: update `loop()`'s redraw dispatch to be screen-aware.
- No changes needed to `data_manager.h/.cpp` or `thingProperties.h` — this feature is entirely UI/touch state, no sensor data involved.

## Out of scope

- Persisting the scroll-mode setting across power cycles (explicitly deferred — resets to Natural every boot).
- Any other settings beyond "Zoomed Scroll" (this design only adds the one setting; the Settings screen's layout should leave room to add more later, but building for that now is not required).
- A "back" button on the Settings screen distinct from the hamburger drawer (leaving Settings works the same way switching between any two graph views does today — via the drawer).
