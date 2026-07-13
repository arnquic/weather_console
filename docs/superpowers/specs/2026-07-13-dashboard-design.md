# Dashboard page — design

## Context

The hamburger drawer currently offers 4 rows: Temperature, Humidity, Pressure, Settings. This feature adds a new "Dashboard" page as the new top row in the drawer, showing at-a-glance averages of all three measurements across the entire buffered history (not just the current graph's zoom/pan window).

## Goals

1. Add a "Dashboard" row as the new top option in the hamburger drawer (5 rows total: Dashboard, Temperature, Humidity, Pressure, Settings).
2. Dashboard screen shows 3 large cards — Temperature, Humidity, Pressure — each displaying the average of that measurement across all currently buffered samples (up to the full 100-point buffer), independent of the graph's zoom level or pan offset.
3. Each card's value respects that measurement's currently configured display unit (Temperature F/C, Pressure hPa/mmHg/psi), using the same settings and conversion helpers the graph already uses.

## Data layer: full-buffer averages

`data_manager.h`/`data_manager.cpp` gains one new function:

```cpp
void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg);
```

Implementation sums `tempData[0..dataCount-1]`, `humidityData[0..dataCount-1]`, `pressureData[0..dataCount-1]` and divides each by `dataCount`. No circular-index math is needed here (unlike `updateMinMaxForCurrentView()`): whether or not the buffer has wrapped, indices `0..dataCount-1` are always exactly the valid, currently-buffered slots — wraparound only affects *insertion order*, not which slots hold valid data. If `dataCount == 0`, all three outputs are set to `0` and the caller is expected to check `getDataCount() == 0` itself before displaying (matching how `drawGraph()` already handles the empty-buffer case).

This is deliberately a separate code path from `avgValue`/`getAverageValue()`, which remain tied to `currentView`/`zoomLevel`/`viewOffset` for the nav bar's windowed average. Dashboard's average is unaffected by whatever the graph screen's zoom/pan/view state happens to be.

## Screen plumbing

- `display_manager.h`: add `SCREEN_DASHBOARD` to the `ScreenMode` enum (`SCREEN_GRAPH, SCREEN_SETTINGS, SCREEN_DASHBOARD`). Add `void drawDashboardScreen();` declaration. Add `#define DASHBOARD_CARD_GAP 20`.
- `weather_console.ino`'s `loop()`: dispatch on `getCurrentScreen()` across all three screens:
  ```cpp
  if(needsRedraw()) {
    if(getCurrentScreen() == SCREEN_SETTINGS) {
      drawSettingsScreen();
    } else if(getCurrentScreen() == SCREEN_DASHBOARD) {
      drawDashboardScreen();
    } else {
      drawGraph();
    }
    clearRedrawFlag();
  }
  ```
- Pinch-zoom/pan suppression in `handleTouch()` already gates on `currentScreen != SCREEN_GRAPH`, so Dashboard is automatically covered — no changes needed to that gating logic.

## Drawer changes (`drawDrawer()`)

Rows grow from 4 to 5. Row height becomes `graphHeight / 5`. New row order:

- Row 0: **Dashboard** — plain label (no accent color, no icon), active-highlighted when `currentScreen == SCREEN_DASHBOARD`. Styled like the existing sensor rows (fill/border toggle between `COLOR_BUTTON_BG`/`COLOR_BUTTON_ACTIVE`, text toggle between `COLOR_TEXT`/`COLOR_NAV_ACTIVE`) but without a per-view accent color, since Dashboard has no single `ViewMode`.
- Rows 1–3: Temperature, Humidity, Pressure (unchanged content, shifted down one row).
- Row 4: Settings (unchanged content, shifted down one row).

`handleTouch()`'s drawer-tap branch updates its row math to match:

```cpp
int rowHeight = graphHeight / 5;
int rowIndex = (touchY - graphY) / rowHeight;

if(rowIndex >= 4) {
  currentScreen = SCREEN_SETTINGS;
  currentView = VIEW_TEMP;
  updateMinMaxForCurrentView();
} else if(rowIndex == 0) {
  currentScreen = SCREEN_DASHBOARD;
} else {
  ViewMode newView = currentView;
  if(rowIndex == 1) newView = VIEW_TEMP;
  else if(rowIndex == 2) newView = VIEW_HUMIDITY;
  else newView = VIEW_PRESSURE;
  currentScreen = SCREEN_GRAPH;
  currentView = newView;
  updateMinMaxForCurrentView();
}
```

`rowIndex >= 4` (rather than `== 4`) preserves the existing safety margin so any pixel past the last exact row still resolves to Settings.

## Nav bar (`drawNavBar()`)

Add a branch for Dashboard, following the same pattern as the existing Settings branch — plain title text, no accent color, no avg readout (there's no single "current view" to average):

```cpp
if(currentScreen == SCREEN_DASHBOARD) {
  display.setTextColor(COLOR_TEXT);
  display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
  display.print("Dashboard");
  return;
}
```

## Dashboard screen rendering (`drawDashboardScreen()`)

Layout mirrors `drawSettingsScreen()`'s content-area setup: same `graphY`/`graphHeight`/`graphWidth`, same requirement to clear the `x = 0..GRAPH_MARGIN` strip itself (the drawer paints past that boundary — CLAUDE.md's documented invariant for any full-screen draw function) plus the interior.

If `getDataCount() == 0`: show the same "Waiting for data..." centered message `drawGraph()` shows, skip card rendering.

Otherwise: call `getFullBufferAverages()` once, then draw 3 cards left-to-right:

- Card width: `(graphWidth - 2 * DASHBOARD_CARD_GAP) / 3`. Card height: full `graphHeight` (minus a small top/bottom margin consistent with existing frame conventions).
- Card X for card `i`: `GRAPH_MARGIN + i * (cardWidth + DASHBOARD_CARD_GAP)`.
- Each card: `drawRoundRect` border in that measurement's `getViewColor()`, `getViewLabel()` near the top in `COLOR_TEXT`, then the converted average (`convertForCurrentView(view, rawAvg)`) formatted as `String(value, 1) + " " + getViewUnit(view)`, large text (size 3, matching the graph's current-value label), centered in the card, in that measurement's accent color.
- Restore the outer frame border afterward, same as `drawGraph()`/`drawSettingsScreen()` already do defensively.

## Out of scope

- No min/max, or "current value" on Dashboard cards — averages only, per the request.
- No historical/trend indicator on Dashboard (e.g., up/down arrows).
- No new persisted settings — Dashboard has no configuration of its own.
- No changes to `updateMinMaxForCurrentView()`, `getAverageValue()`, or any existing graph/settings behavior.
