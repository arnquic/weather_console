# Dashboard Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Dashboard page — a new top row in the hamburger drawer — showing 3 cards with the all-time buffered average of Temperature, Humidity, and Pressure, each in its currently configured display unit.

**Architecture:** A new `data_manager` function computes averages across the full buffer independent of the graph's zoom/pan state. A new `SCREEN_DASHBOARD` screen mode is wired through the drawer (now 5 rows), nav bar, and `weather_console.ino`'s redraw dispatch, following the exact same plumbing pattern already used for `SCREEN_SETTINGS`. A new `drawDashboardScreen()` renders 3 bordered cards using the existing per-view color/label/unit/conversion helpers.

**Tech Stack:** Arduino CLI project (`arduino:mbed_giga:giga`), no automated test framework — verification is `arduino-cli compile` plus manual on-hardware checks, per this project's established convention (see CLAUDE.md).

## Global Constraints

- Drawer has 5 rows total, in this order: Dashboard (row 0), Temperature (row 1), Humidity (row 2), Pressure (row 3), Settings (row 4).
- Drawer row height is `graphHeight / 5` (was `graphHeight / 4`). Hit-test uses `rowIndex >= 4` (not `== 4`) to resolve to Settings, preserving the existing "any pixel past the last exact row still resolves to Settings" safety margin.
- Dashboard's averages come from a new `getFullBufferAverages()` function that sums buffered samples across indices `0..dataCount-1` — this is independent of `zoomLevel`/`viewOffset`/`currentView`, and is a distinct code path from the existing windowed `getAverageValue()`.
- Each card's displayed value is converted via the existing `convertForCurrentView(ViewMode, float)` helper so it reflects the currently configured Temperature (F/C) and Pressure (hPa/mmHg/psi) units — same settings the graph screen already uses.
- New constant: `DASHBOARD_CARD_GAP = 20` (matches the existing `SETTINGS_BUTTON_GAP` value).
- Dashboard cards show only the average value — no min/max, no current-value, no trend indicators. No new persisted settings.
- Any full-screen draw function must clear the `x = 0..GRAPH_MARGIN` strip itself (documented CLAUDE.md invariant — the drawer paints past that boundary).

---

### Task 1: Full-buffer averages in data_manager

**Files:**
- Modify: `data_manager.h:13` (add declaration after `getAverageValue`)
- Modify: `data_manager.cpp:129-131` (add implementation after `getAverageValue`)

**Interfaces:**
- Produces: `void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg);` — sums `tempData[0..dataCount-1]`, `humidityData[0..dataCount-1]`, `pressureData[0..dataCount-1]` and divides each by `dataCount`; sets all three outputs to `0` if `dataCount == 0`. Consumed by Task 3's `drawDashboardScreen()`.

- [ ] **Step 1: Add the declaration to `data_manager.h`**

Current content around line 13:

```cpp
void updateMinMaxForCurrentView();
void getMinMaxValues(float* minVal, float* maxVal);
void getAverageValue(float* avg);
```

Change to:

```cpp
void updateMinMaxForCurrentView();
void getMinMaxValues(float* minVal, float* maxVal);
void getAverageValue(float* avg);
void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg);
```

- [ ] **Step 2: Add the implementation to `data_manager.cpp`**

Current content around lines 129-131:

```cpp
void getAverageValue(float* avg) {
  *avg = avgValue;
}
```

Change to:

```cpp
void getAverageValue(float* avg) {
  *avg = avgValue;
}

void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg) {
  if(dataCount == 0) {
    *tempAvg = 0;
    *humidityAvg = 0;
    *pressureAvg = 0;
    return;
  }

  float tempSum = 0, humiditySum = 0, pressureSum = 0;
  for(int i = 0; i < dataCount; i++) {
    tempSum += tempData[i];
    humiditySum += humidityData[i];
    pressureSum += pressureData[i];
  }

  *tempAvg = tempSum / dataCount;
  *humidityAvg = humiditySum / dataCount;
  *pressureAvg = pressureSum / dataCount;
}
```

No circular-index math is needed here (unlike `updateMinMaxForCurrentView()`): whether or not the buffer has wrapped, indices `0..dataCount-1` are always exactly the valid, currently-buffered slots — wraparound only changes insertion order, not which slots hold valid data.

- [ ] **Step 3: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`

Expected: compiles cleanly. This function isn't called from anywhere yet (it's wired up in Task 3), so there's no functional behavior to exercise on hardware at this point — a clean compile is the full verification for this task.

- [ ] **Step 4: Commit**

```bash
git add data_manager.h data_manager.cpp
git commit -m "Add full-buffer average calculation to data_manager"
```

---

### Task 2: Wire the Dashboard screen mode (drawer, nav bar, dispatch)

**Files:**
- Modify: `display_manager.h:32` (add `DASHBOARD_CARD_GAP` constant)
- Modify: `display_manager.h:50` (add `SCREEN_DASHBOARD` to `ScreenMode`)
- Modify: `display_manager.h:59` (add `drawDashboardScreen()` declaration)
- Modify: `display_manager.cpp` (`drawNavBar()`, `drawDrawer()`, `handleTouch()`'s drawer branch; add placeholder `drawDashboardScreen()`)
- Modify: `weather_console.ino:37-44` (`loop()` redraw dispatch)

**Interfaces:**
- Consumes: nothing from Task 1 yet — this task only wires the new screen mode with a placeholder body.
- Produces: `SCREEN_DASHBOARD` enum value; `void drawDashboardScreen();` declared and given a minimal placeholder body (clears the content area, prints "Dashboard"). Task 3 replaces the body with the real card rendering — the wiring done here (drawer row, nav bar title, `loop()` dispatch) does not change in Task 3.

- [ ] **Step 1: Add `DASHBOARD_CARD_GAP` to `display_manager.h`**

Current content around line 32:

```cpp
#define SETTINGS_ROW_HEIGHT 120
```

Change to:

```cpp
#define SETTINGS_ROW_HEIGHT 120

// Dashboard screen
#define DASHBOARD_CARD_GAP 20
```

- [ ] **Step 2: Add `SCREEN_DASHBOARD` to the `ScreenMode` enum**

Current content at line 50:

```cpp
enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS };
```

Change to:

```cpp
enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS, SCREEN_DASHBOARD };
```

- [ ] **Step 3: Declare `drawDashboardScreen()`**

Current content around line 59:

```cpp
void drawSettingsScreen();
void drawGraph();
```

Change to:

```cpp
void drawSettingsScreen();
void drawDashboardScreen();
void drawGraph();
```

- [ ] **Step 4: Add a Dashboard branch to `drawNavBar()` in `display_manager.cpp`**

Current content (the Settings branch, lines ~129-134):

```cpp
  if(currentScreen == SCREEN_SETTINGS) {
    display.setTextColor(COLOR_TEXT);
    display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
    display.print("Settings");
    return;
  }
```

Change to:

```cpp
  if(currentScreen == SCREEN_SETTINGS) {
    display.setTextColor(COLOR_TEXT);
    display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
    display.print("Settings");
    return;
  }

  if(currentScreen == SCREEN_DASHBOARD) {
    display.setTextColor(COLOR_TEXT);
    display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
    display.print("Dashboard");
    return;
  }
```

- [ ] **Step 5: Rewrite `drawDrawer()` for 5 rows**

Replace the entire existing `drawDrawer()` function (currently lines 181-224) with:

```cpp
void drawDrawer() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int rowHeight = graphHeight / 5;

  display.fillRect(0, graphY, DRAWER_WIDTH, graphHeight, COLOR_NAV_BG);

  // Row 0: Dashboard - plain label, no per-view accent color (Dashboard
  // has no single ViewMode to color itself with).
  bool dashboardActive = (currentScreen == SCREEN_DASHBOARD);
  display.fillRect(0, graphY, DRAWER_WIDTH, rowHeight,
                    dashboardActive ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG);
  display.drawRect(0, graphY, DRAWER_WIDTH, rowHeight, COLOR_TEXT);
  display.setTextSize(2);
  display.setTextColor(dashboardActive ? COLOR_NAV_ACTIVE : COLOR_TEXT);
  display.setCursor(20, graphY + rowHeight / 2 - 8);
  display.print("Dashboard");

  ViewMode views[3] = { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };

  for(int i = 0; i < 3; i++) {
    int rowY = graphY + (i + 1) * rowHeight;
    bool active = (currentScreen == SCREEN_GRAPH && views[i] == currentView);
    uint16_t accentColor = getViewColor(views[i]);

    display.fillRect(0, rowY, DRAWER_WIDTH, rowHeight,
                      active ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG);
    display.drawRect(0, rowY, DRAWER_WIDTH, rowHeight,
                      active ? accentColor : COLOR_TEXT);

    display.setTextSize(2);
    display.setTextColor(active ? COLOR_NAV_ACTIVE : COLOR_TEXT);
    display.setCursor(20, rowY + rowHeight / 2 - 8);
    display.print(getViewLabel(views[i]));
  }

  // Settings row (5th row) - no accent color of its own, so it uses the
  // plain active/inactive styling plus a gear icon to stand out.
  int settingsRowY = graphY + 4 * rowHeight;
  bool settingsActive = (currentScreen == SCREEN_SETTINGS);
  uint16_t settingsBg = settingsActive ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG;
  uint16_t settingsFg = settingsActive ? COLOR_NAV_ACTIVE : COLOR_TEXT;

  display.fillRect(0, settingsRowY, DRAWER_WIDTH, rowHeight, settingsBg);
  display.drawRect(0, settingsRowY, DRAWER_WIDTH, rowHeight, COLOR_TEXT);

  int gearCx = 20 + GEAR_ICON_RADIUS;
  int gearCy = settingsRowY + rowHeight / 2;
  drawGearIcon(gearCx, gearCy, settingsFg, settingsBg);

  display.setTextSize(2);
  display.setTextColor(settingsFg);
  display.setCursor(gearCx + GEAR_ICON_RADIUS + 10, settingsRowY + rowHeight / 2 - 8);
  display.print("Settings");
}
```

- [ ] **Step 6: Update the drawer hit-test in `handleTouch()`**

Current content (inside the `drawerOpen` branch, currently around lines 524-550):

```cpp
        if(!onIcon) {
          bool inDrawer = (touchX >= 0 && touchX < DRAWER_WIDTH &&
                           touchY >= graphY && touchY < graphY + graphHeight);

          if(inDrawer) {
            int rowHeight = graphHeight / 4;
            int rowIndex = (touchY - graphY) / rowHeight;

            if(rowIndex >= 3) {
              currentScreen = SCREEN_SETTINGS;
              currentView = VIEW_TEMP;
              updateMinMaxForCurrentView();
              Serial.println("Switched to Settings");
            } else {
              ViewMode newView = currentView;

              if(rowIndex == 0) newView = VIEW_TEMP;
              else if(rowIndex == 1) newView = VIEW_HUMIDITY;
              else newView = VIEW_PRESSURE;

              currentScreen = SCREEN_GRAPH;
              currentView = newView;
              updateMinMaxForCurrentView();
              Serial.print("Switched to view: ");
              Serial.println(currentView);
            }
          }
        }
```

Change to:

```cpp
        if(!onIcon) {
          bool inDrawer = (touchX >= 0 && touchX < DRAWER_WIDTH &&
                           touchY >= graphY && touchY < graphY + graphHeight);

          if(inDrawer) {
            int rowHeight = graphHeight / 5;
            int rowIndex = (touchY - graphY) / rowHeight;

            if(rowIndex >= 4) {
              currentScreen = SCREEN_SETTINGS;
              currentView = VIEW_TEMP;
              updateMinMaxForCurrentView();
              Serial.println("Switched to Settings");
            } else if(rowIndex == 0) {
              currentScreen = SCREEN_DASHBOARD;
              Serial.println("Switched to Dashboard");
            } else {
              ViewMode newView = currentView;

              if(rowIndex == 1) newView = VIEW_TEMP;
              else if(rowIndex == 2) newView = VIEW_HUMIDITY;
              else newView = VIEW_PRESSURE;

              currentScreen = SCREEN_GRAPH;
              currentView = newView;
              updateMinMaxForCurrentView();
              Serial.print("Switched to view: ");
              Serial.println(currentView);
            }
          }
        }
```

- [ ] **Step 7: Add a placeholder `drawDashboardScreen()` to `display_manager.cpp`**

Add this new function immediately after `drawSettingsScreen()` and before `drawGraph()`:

```cpp
void drawDashboardScreen() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;

  // Clear dashboard area, including the margin strip to the left of the
  // frame (the drawer paints all the way to x=0, wider than GRAPH_MARGIN).
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1,
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT);
  display.setCursor(SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2);
  display.print("Dashboard");

  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
}
```

This is intentionally minimal — Task 3 replaces this body with the real 3-card rendering. Its job in this task is only to prove the screen-mode wiring works end-to-end.

- [ ] **Step 8: Update the `loop()` dispatch in `weather_console.ino`**

Current content (lines 37-44):

```cpp
  if(needsRedraw()) {
    if(getCurrentScreen() == SCREEN_SETTINGS) {
      drawSettingsScreen();
    } else {
      drawGraph();
    }
    clearRedrawFlag();
  }
```

Change to:

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

- [ ] **Step 9: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`

Expected: compiles cleanly.

- [ ] **Step 10: Manual hardware verification**

Upload (`arduino-cli compile --fqbn arduino:mbed_giga:giga --upload -p <port> .`), then on the device:
- Tap the hamburger icon — drawer should now show 5 rows: Dashboard, Temperature, Humidity, Pressure, Settings.
- Tap "Dashboard" (top row) — drawer closes, nav bar shows "Dashboard", content area shows the placeholder "Dashboard" text inside the frame.
- Re-open the drawer, tap "Temperature" — confirm it still switches to the graph screen showing Temperature (rows shifted down one but still functional).
- Re-open the drawer, tap "Settings" (now the 5th/bottom row) — confirm it still opens the Settings screen correctly.

- [ ] **Step 11: Commit**

```bash
git add display_manager.h display_manager.cpp weather_console.ino
git commit -m "Wire Dashboard screen mode: drawer row, nav bar, placeholder screen"
```

---

### Task 3: Render Dashboard cards with full-buffer averages

**Files:**
- Modify: `display_manager.cpp` (replace the placeholder `drawDashboardScreen()` body from Task 2)
- Modify: `CLAUDE.md` (update architecture notes to reflect the 5-row drawer and new Dashboard screen)

**Interfaces:**
- Consumes: `void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg);` (Task 1, `data_manager.h`); existing file-local statics `getViewColor(ViewMode)`, `getViewLabel(ViewMode)`, `getViewUnit(ViewMode)`, `convertForCurrentView(ViewMode, float)`; `getDataCount()` (`data_manager.h`).
- Produces: nothing further — this is the final task.

- [ ] **Step 1: Replace the placeholder `drawDashboardScreen()` body**

Replace the entire `drawDashboardScreen()` function added in Task 2 with:

```cpp
void drawDashboardScreen() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;

  // Clear dashboard area, including the margin strip to the left of the
  // frame (the drawer paints all the way to x=0, wider than GRAPH_MARGIN).
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1,
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);

  if(getDataCount() == 0) {
    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT);
    display.setCursor(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2);
    display.print("Waiting for data...");
    display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
    return;
  }

  float tempAvg, humidityAvg, pressureAvg;
  getFullBufferAverages(&tempAvg, &humidityAvg, &pressureAvg);

  ViewMode views[3] = { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };
  float rawAverages[3] = { tempAvg, humidityAvg, pressureAvg };

  int cardWidth = (graphWidth - 2 * DASHBOARD_CARD_GAP) / 3;
  int cardHeight = graphHeight - 2 * GRAPH_MARGIN;
  int cardTop = graphY + GRAPH_MARGIN;

  for(int i = 0; i < 3; i++) {
    int cardX = GRAPH_MARGIN + i * (cardWidth + DASHBOARD_CARD_GAP);
    uint16_t accentColor = getViewColor(views[i]);

    display.drawRoundRect(cardX, cardTop, cardWidth, cardHeight, 8, accentColor);

    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT);
    display.setCursor(cardX + 20, cardTop + 20);
    display.print(getViewLabel(views[i]));

    String valueText = String(convertForCurrentView(views[i], rawAverages[i]), 1) +
                        " " + getViewUnit(views[i]);

    display.setTextSize(3);
    display.setTextColor(accentColor);
    int textX = cardX + (cardWidth - (int)valueText.length() * 18) / 2;
    if(textX < cardX + 10) textX = cardX + 10;
    display.setCursor(textX, cardTop + cardHeight / 2);
    display.print(valueText);
  }

  // Restore frame border in case anything touched the edge pixels
  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
}
```

Text centering uses an approximate 18px-per-character width (the GFX default font at `setTextSize(3)` is 6px base char width × 3), matching the level of precision the rest of this file already uses for positioning — no true text-measurement API is used elsewhere in this codebase.

- [ ] **Step 2: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`

Expected: compiles cleanly.

- [ ] **Step 3: Manual hardware verification**

Upload, then on the device:
- Open the drawer, tap "Dashboard".
- Confirm 3 cards render side-by-side, each with a rounded border in its measurement's accent color (red = Temperature, green = Humidity, cyan = Pressure).
- Confirm each card shows a plausible average value with the correct unit suffix.
- Go to Settings, change Temperature Unit to C (or Pressure Unit to mmHg/psi), return to Dashboard, and confirm the displayed average updates to the new unit and conversion.

- [ ] **Step 4: Update CLAUDE.md architecture notes**

In the `display_manager.h/.cpp` bullet, find this sentence:

```
`ScreenMode` (`SCREEN_GRAPH`/`SCREEN_SETTINGS`, header-level enum + `currentScreen` static + `getCurrentScreen()` getter) picks which screen `weather_console.ino`'s `loop()` draws (`drawGraph()` vs. `drawSettingsScreen()`) and gates pinch-zoom/pan the same way `drawerOpen` does
```

Change to:

```
`ScreenMode` (`SCREEN_GRAPH`/`SCREEN_SETTINGS`/`SCREEN_DASHBOARD`, header-level enum + `currentScreen` static + `getCurrentScreen()` getter) picks which screen `weather_console.ino`'s `loop()` draws (`drawGraph()` vs. `drawSettingsScreen()` vs. `drawDashboardScreen()`) and gates pinch-zoom/pan the same way `drawerOpen` does
```

Find this sentence (in the "three independent places" bullet):

```
(2) the drawer's row hit-testing in `handleTouch()` duplicates `drawDrawer()`'s row-layout math (`rowHeight = graphHeight / 4`, with the drawer-row hit-test using `rowIndex >= 3` rather than `== 3` so any pixel past the last exact row still resolves to Settings rather than falling through);
```

Change to:

```
(2) the drawer's row hit-testing in `handleTouch()` duplicates `drawDrawer()`'s row-layout math (5 rows — Dashboard/Temperature/Humidity/Pressure/Settings — `rowHeight = graphHeight / 5`, with the drawer-row hit-test using `rowIndex >= 4` rather than `== 4` so any pixel past the last exact row still resolves to Settings rather than falling through);
```

Find this sentence (in the `data_manager.h/.cpp` bullet):

```
`display_manager` reads these via `getMinMaxValues()`/`getAverageValue()` rather than scanning data itself.
```

Change to:

```
`display_manager` reads these via `getMinMaxValues()`/`getAverageValue()` rather than scanning data itself. A separate `getFullBufferAverages()` sums every currently-buffered sample (indices `0..dataCount-1`, independent of `zoomLevel`/`viewOffset`/`currentView`) for the Dashboard screen's all-time averages.
```

- [ ] **Step 5: Commit**

```bash
git add display_manager.cpp CLAUDE.md
git commit -m "Render Dashboard cards with full-buffer averages"
```
