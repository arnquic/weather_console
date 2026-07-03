# Hamburger menu + top-bar average — design

## Context

The current top bar is a row of 3 always-visible buttons (Temperature / Humidity / Pressure) that switch `currentView`. This is being replaced with a hamburger-menu pattern to modernize the UI and free up top-bar space, plus a new at-a-glance average reading for the current view.

## Goals

1. Replace the 3-button nav bar with a hamburger icon + current view name in the top bar.
2. Tapping the icon opens/closes a left-side drawer listing the same 3 view options.
3. Show the average of the currently visible (zoomed/panned) data window next to the view name in the top bar.

## Layout & visuals

- Top bar (same `NAV_HEIGHT` strip) shows, left to right:
  - Hamburger icon (3 horizontal bars) in the top-left corner.
  - Current view name (e.g. "Temperature"), colored with that view's accent color (`COLOR_TEMP_LINE` / `COLOR_HUMIDITY_LINE` / `COLOR_PRESSURE_LINE`).
  - The average of the visible window for that view, with its unit, e.g. `"22.3 C avg"`, immediately after the name.
- Drawer: left-aligned panel ~220px wide, spanning from below the nav bar to the bottom margin (same vertical span as the graph area). Drawn on top of the graph when open.
  - Contains 3 stacked full-drawer-width rows: Temperature / Humidity / Pressure.
  - The active view's row is highlighted using its accent color, following the same active/inactive visual treatment the old buttons used (`COLOR_BUTTON_ACTIVE` background + accent border for active, `COLOR_BUTTON_BG` + `COLOR_TEXT` border for inactive).
- No animation: the drawer is drawn or erased instantly on toggle.

## State

- New: `drawerOpen` (bool, static in `display_manager.cpp`), alongside existing `currentView` / `redrawNeeded`.
- New: `avgValue` (float, static in `data_manager.cpp`), alongside existing `minValue` / `maxValue`.

## Touch interaction (`handleTouch` in `display_manager.cpp`)

- Tap on the hamburger icon → toggle `drawerOpen`; redraw drawer (open) or full graph (close).
- While `drawerOpen`:
  - Tap on a drawer row → `setCurrentView(...)`, close drawer, redraw nav bar + full graph.
  - Tap anywhere else (top bar or graph area) → close drawer, redraw graph. No view change.
  - Pinch-zoom and pan gestures are suppressed entirely (the drawer covers the graph, so touches there are drawer-related, not graph-related).
- While `!drawerOpen`: existing pinch-zoom/pan behavior in the graph area is unchanged. The old button-hit-testing code path is removed (no buttons remain outside the drawer).

## Average calculation (`data_manager.cpp`)

- Computed over the **currently visible window** (respecting `zoomLevel` and `viewOffset`), not the full buffer — same scope as the existing min/max calculation.
- To avoid a third copy of the window-index logic (there are already two: `updateMinMaxForCurrentView()` and `drawGraph()`), the average is folded into the existing loop in `updateMinMaxForCurrentView()`: accumulate a running sum alongside the existing min/max tracking, then set `avgValue = sum / pointsToCheck` at the end.
- New getter `getAverageValue()` mirrors the existing `getMinMaxValues()` pattern.
- Recompute/redraw triggers are already in place: `updateMinMaxForCurrentView()` is already called on view switch, zoom, pan, and new data arrival, and each of those paths already sets the redraw flag. No new triggers are needed — the nav bar draw function just needs to read `getAverageValue()` when it redraws.

## Code impact summary

- `display_manager.h`:
  - Remove `BUTTON_WIDTH`, `BUTTON_HEIGHT`, `BUTTON_MARGIN`, `BUTTON_Y`, `drawButton()`.
  - Add hamburger icon and drawer layout constants (icon size/position, drawer width).
  - Add `COLOR_NAV_ACTIVE`/`COLOR_BUTTON_*` reuse for drawer row styling (no new colors needed).
- `display_manager.cpp`:
  - Remove old button-drawing and button-hit-testing code.
  - Add `drawerOpen` state, `drawHamburgerIcon()`, `drawDrawer()`, updated `drawNavBar()` (icon + view name + average), and updated `handleTouch()` hit-testing (icon tap, drawer row tap, outside-tap-to-close, gesture suppression while drawer is open).
- `data_manager.h` / `data_manager.cpp`:
  - Add `avgValue` static, extend `updateMinMaxForCurrentView()`'s loop to accumulate a sum, add `getAverageValue()` getter.
- `thingProperties.h`: no changes.

## Out of scope

- Drawer open/close animation (explicitly deferred — instant show/hide only).
- Average over the full buffer (explicitly not needed — visible-window average only, per approval above).
