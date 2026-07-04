# Settings Screen + Natural/Classic Zoomed Scroll Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Flip zoomed-graph pan direction to feel "natural" by default (content follows the finger), keep the old direction available as "Classic," and add a Settings screen — reachable as a 4th item in the existing hamburger drawer — where the user can switch between the two.

**Architecture:** A new `ScreenMode` (`SCREEN_GRAPH`/`SCREEN_SETTINGS`) state in `display_manager.cpp` determines whether `loop()` draws the graph or a new settings panel in the same screen region, and gates pinch-zoom/pan the same way `drawerOpen` already does. A separate `ScrollMode` (`SCROLL_NATURAL`/`SCROLL_CLASSIC`) state, defaulting to Natural, flips the sign of the existing pan-delta formula. The drawer grows from 3 to 4 rows to add a "Settings" entry with a small drawn gear icon.

**Tech Stack:** Arduino sketch (C++), `arduino:mbed_giga:giga` board, `Arduino_GigaDisplay_GFX` / `Arduino_GigaDisplayTouch` libraries (already installed in this environment).

## Global Constraints

- This project has **no automated test framework** — every task's "test" step is `arduino-cli compile --fqbn arduino:mbed_giga:giga .` run from the project root. The final task adds a manual on-hardware verification checklist in place of behavioral tests (physical touch input can't be driven by a subagent).
- Follow the approved spec exactly: `docs/superpowers/specs/2026-07-04-settings-screen-natural-scroll-design.md`. Do not add persistence (EEPROM/KVStore) for the scroll setting, and do not add a dedicated "back" control on the Settings screen — both are explicitly out of scope there.
- Required libraries are already installed. Do not reinstall.
- This is a git repo (`origin` = `git@github.com:arnquic/weather_console.git`, branch `main`). Commit after each task; do not push unless asked. **Do not add a `Co-Authored-By` trailer to any commit in this repo.**

---

### Task 1: Add ScreenMode state and getter

**Files:**
- Modify: `display_manager.h` (add `ScreenMode` enum, `getCurrentScreen()` declaration)
- Modify: `display_manager.cpp` (add `currentScreen` static, `getCurrentScreen()` definition)

**Interfaces:**
- Produces: `enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS };` (header-level — used from `weather_console.ino` in Task 4), `ScreenMode getCurrentScreen()`. Used by Task 3 (`drawDrawer()`, `drawNavBar()`) and Task 4 (`handleTouch()`, `.ino` redraw dispatch).

This task only adds state and a getter — nothing sets `currentScreen` to `SCREEN_SETTINGS` yet, and nothing calls `getCurrentScreen()` yet. That's expected; it compiles fine (an unused function/enum value is not a compile error).

- [ ] **Step 1: Add the `ScreenMode` enum to the header**

In `display_manager.h`, find:

```c
// View modes
enum ViewMode { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };
```

Replace with:

```c
// View modes
enum ViewMode { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };

// Screens
enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS };
```

- [ ] **Step 2: Add the `getCurrentScreen()` declaration**

In `display_manager.h`, find:

```c
ViewMode getCurrentView();
void setCurrentView(ViewMode view);
```

Replace with:

```c
ViewMode getCurrentView();
void setCurrentView(ViewMode view);
ScreenMode getCurrentScreen();
```

- [ ] **Step 3: Add the `currentScreen` state variable**

In `display_manager.cpp`, find:

```cpp
static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;
static bool drawerOpen = false;
```

Replace with:

```cpp
static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;
static bool drawerOpen = false;
static ScreenMode currentScreen = SCREEN_GRAPH;
```

- [ ] **Step 4: Add the `getCurrentScreen()` definition**

In `display_manager.cpp`, find:

```cpp
ViewMode getCurrentView() {
  return currentView;
}

void setCurrentView(ViewMode view) {
  currentView = view;
}
```

Replace with:

```cpp
ViewMode getCurrentView() {
  return currentView;
}

void setCurrentView(ViewMode view) {
  currentView = view;
}

ScreenMode getCurrentScreen() {
  return currentScreen;
}
```

- [ ] **Step 5: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 6: Commit**

```bash
git add display_manager.h display_manager.cpp
git commit -m "Add ScreenMode state for graph/settings screen switching"
```

---

### Task 2: Add Natural/Classic scroll mode and flip the pan formula

**Files:**
- Modify: `display_manager.cpp` (add `ScrollMode` enum + `scrollMode` static, both file-local; update the pan-delta calculation in `handleTouch()`)

**Interfaces:**
- Produces: `enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };` and `static ScrollMode scrollMode` — both file-local to `display_manager.cpp` (no header declaration; nothing outside this file reads or sets them). Task 4 references `scrollMode` and `SCROLL_NATURAL`/`SCROLL_CLASSIC` when it replaces `handleTouch()` in full and when it adds the Settings-screen button handling.

This task alone already fixes the user-reported issue (pan direction) by changing the default to Natural — the Settings screen to switch back to Classic comes in later tasks.

- [ ] **Step 1: Add the `ScrollMode` enum and `scrollMode` state**

In `display_manager.cpp`, find:

```cpp
static bool drawerOpen = false;
static ScreenMode currentScreen = SCREEN_GRAPH;
```

Replace with:

```cpp
static bool drawerOpen = false;
static ScreenMode currentScreen = SCREEN_GRAPH;

enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };
static ScrollMode scrollMode = SCROLL_NATURAL;
```

- [ ] **Step 2: Flip the pan-delta formula based on `scrollMode`**

In `display_manager.cpp`, inside `handleTouch()`, find:

```cpp
          // Pan left/right - drag right = scroll back in time
          int panDelta = -panChange / 3;  // Negative because dragging right should increase offset
```

Replace with:

```cpp
          // Pan left/right - direction depends on scrollMode
          int panDelta = (scrollMode == SCROLL_NATURAL) ? (panChange / 3) : (-panChange / 3);
```

`SCROLL_CLASSIC` reproduces the exact original formula (`-panChange / 3`). `SCROLL_NATURAL` (the new default) is the sign-flipped version, so the visible window moves the same direction as the drag.

- [ ] **Step 3: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 4: Commit**

```bash
git add display_manager.cpp
git commit -m "Add Natural/Classic zoomed-scroll modes, default to Natural"
```

---

### Task 3: Draw the 4-row drawer (with gear icon) and the Settings screen

**Files:**
- Modify: `display_manager.h` (add `GEAR_ICON_RADIUS` and `SETTINGS_BUTTON_*` layout constants, add `drawSettingsScreen()` declaration)
- Modify: `display_manager.cpp` (extend `drawDrawer()` to 4 rows with a drawn gear icon for the Settings row; add `drawSettingsScreen()`; update `drawNavBar()` to show "Settings" when appropriate)

**Interfaces:**
- Consumes: `ScreenMode`/`getCurrentScreen()`-backing state `currentScreen` (Task 1, read directly as the file-local static — this file doesn't need the getter, only `weather_console.ino` does), `ScrollMode`/`scrollMode` (Task 2), `getViewColor()`/`getViewLabel()`/`getViewUnit()` (pre-existing helpers).
- Produces: `void drawSettingsScreen()` — called by Task 4's `handleTouch()` (indirectly, via the `.ino` redraw dispatch) and by the manual verification in Task 5. `GEAR_ICON_RADIUS`, `SETTINGS_BUTTON_WIDTH`, `SETTINGS_BUTTON_HEIGHT`, `SETTINGS_BUTTON_GAP`, `SETTINGS_BUTTON_Y_OFFSET` — Task 4's `handleTouch()` uses these exact same constants to hit-test the buttons this task draws, so the tap targets always match what's rendered.

This task only changes drawing code — no touch-handling changes. `drawSettingsScreen()` has no caller yet after this task (Task 4 wires up the `.ino` dispatch that invokes it); that's expected and compiles fine.

- [ ] **Step 1: Add the new layout constants and `drawSettingsScreen()` declaration to the header**

In `display_manager.h`, find:

```c
// Drawer
#define DRAWER_WIDTH 220
```

Replace with:

```c
// Drawer
#define DRAWER_WIDTH 220
#define GEAR_ICON_RADIUS 10

// Settings screen
#define SETTINGS_BUTTON_WIDTH 150
#define SETTINGS_BUTTON_HEIGHT 50
#define SETTINGS_BUTTON_GAP 20
#define SETTINGS_BUTTON_Y_OFFSET 60
```

Then find:

```c
void drawDrawer();
```

Replace with:

```c
void drawDrawer();
void drawSettingsScreen();
```

- [ ] **Step 2: Extend `drawDrawer()` to 4 rows with a gear icon for Settings**

In `display_manager.cpp`, find the entire existing `drawDrawer()` function:

```cpp
void drawDrawer() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int rowHeight = graphHeight / 3;

  display.fillRect(0, graphY, DRAWER_WIDTH, graphHeight, COLOR_NAV_BG);

  ViewMode views[3] = { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };

  for(int i = 0; i < 3; i++) {
    int rowY = graphY + i * rowHeight;
    bool active = (views[i] == currentView);
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
}
```

Replace it with:

```cpp
void drawGearIcon(int cx, int cy, uint16_t color, uint16_t bgColor) {
  display.fillCircle(cx, cy, GEAR_ICON_RADIUS, color);
  display.fillCircle(cx, cy, GEAR_ICON_RADIUS / 2, bgColor);

  int toothSize = 4;
  display.fillRect(cx - toothSize / 2, cy - GEAR_ICON_RADIUS - toothSize / 2, toothSize, toothSize, color);
  display.fillRect(cx - toothSize / 2, cy + GEAR_ICON_RADIUS - toothSize / 2, toothSize, toothSize, color);
  display.fillRect(cx - GEAR_ICON_RADIUS - toothSize / 2, cy - toothSize / 2, toothSize, toothSize, color);
  display.fillRect(cx + GEAR_ICON_RADIUS - toothSize / 2, cy - toothSize / 2, toothSize, toothSize, color);
}

void drawDrawer() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int rowHeight = graphHeight / 4;

  display.fillRect(0, graphY, DRAWER_WIDTH, graphHeight, COLOR_NAV_BG);

  ViewMode views[3] = { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };

  for(int i = 0; i < 3; i++) {
    int rowY = graphY + i * rowHeight;
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

  // Settings row (4th row) - no accent color of its own, so it uses the
  // plain active/inactive styling plus a gear icon to stand out.
  int settingsRowY = graphY + 3 * rowHeight;
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

Note the `active` check for the 3 sensor rows now also requires `currentScreen == SCREEN_GRAPH` — this prevents a sensor row from appearing highlighted while the Settings screen (which forces `currentView` to `VIEW_TEMP` internally, per Task 4) is actually showing.

- [ ] **Step 3: Add `drawSettingsScreen()`**

In `display_manager.cpp`, find the `drawDrawer()` function you just wrote (ending with `display.print("Settings");\n}`) and add this new function immediately after it:

```cpp
void drawSettingsScreen() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;

  // Clear settings area
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1,
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT);
  display.setCursor(GRAPH_MARGIN + 20, graphY + 20);
  display.print("Zoomed Scroll");

  int buttonY = graphY + SETTINGS_BUTTON_Y_OFFSET;
  int naturalX = GRAPH_MARGIN + 20;
  int classicX = naturalX + SETTINGS_BUTTON_WIDTH + SETTINGS_BUTTON_GAP;

  bool naturalActive = (scrollMode == SCROLL_NATURAL);
  bool classicActive = (scrollMode == SCROLL_CLASSIC);

  display.fillRoundRect(naturalX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                         naturalActive ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG);
  display.drawRoundRect(naturalX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                         naturalActive ? COLOR_NAV_ACTIVE : COLOR_TEXT);

  display.fillRoundRect(classicX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                         classicActive ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG);
  display.drawRoundRect(classicX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                         classicActive ? COLOR_NAV_ACTIVE : COLOR_TEXT);

  display.setTextSize(2);
  display.setTextColor(naturalActive ? COLOR_NAV_ACTIVE : COLOR_TEXT);
  display.setCursor(naturalX + 20, buttonY + 17);
  display.print("Natural");

  display.setTextColor(classicActive ? COLOR_NAV_ACTIVE : COLOR_TEXT);
  display.setCursor(classicX + 20, buttonY + 17);
  display.print("Classic");

  // Restore frame border in case anything touched the edge pixels
  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
}
```

- [ ] **Step 4: Update `drawNavBar()` to show "Settings" when the Settings screen is active**

In `display_manager.cpp`, find the entire existing `drawNavBar()` function:

```cpp
void drawNavBar() {
  display.fillRect(0, TITLE_HEIGHT, SCREEN_WIDTH, NAV_HEIGHT, COLOR_NAV_BG);

  if(drawerOpen) {
    drawCloseIcon();
  } else {
    drawHamburgerIcon();
  }

  uint16_t accentColor = getViewColor(currentView);
  String unit = getViewUnit(currentView);
  float avgValue;
  getAverageValue(&avgValue);

  String label = String(getViewLabel(currentView)) + "  " +
                 String(avgValue, 1) + " " + unit + " avg";

  display.setTextSize(2);
  display.setTextColor(accentColor);
  display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
  display.print(label);
}
```

Replace it with:

```cpp
void drawNavBar() {
  display.fillRect(0, TITLE_HEIGHT, SCREEN_WIDTH, NAV_HEIGHT, COLOR_NAV_BG);

  if(drawerOpen) {
    drawCloseIcon();
  } else {
    drawHamburgerIcon();
  }

  display.setTextSize(2);

  if(currentScreen == SCREEN_SETTINGS) {
    display.setTextColor(COLOR_TEXT);
    display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
    display.print("Settings");
    return;
  }

  uint16_t accentColor = getViewColor(currentView);
  String unit = getViewUnit(currentView);
  float avgValue;
  getAverageValue(&avgValue);

  String label = String(getViewLabel(currentView)) + "  " +
                 String(avgValue, 1) + " " + unit + " avg";

  display.setTextColor(accentColor);
  display.setCursor(ICON_TAP_WIDTH, (NAV_HEIGHT - 16) / 2);
  display.print(label);
}
```

- [ ] **Step 5: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 6: Commit**

```bash
git add display_manager.h display_manager.cpp
git commit -m "Draw 4-row drawer with gear icon and add Settings screen"
```

---

### Task 4: Wire up Settings navigation, screen-aware gesture suppression, and button touch handling

**Files:**
- Modify: `display_manager.cpp` (extend the drawer's row hit-test to 4 rows; suppress pinch/pan when off the graph screen; add Settings-button tap handling)
- Modify: `weather_console.ino` (make the redraw dispatch screen-aware)

**Interfaces:**
- Consumes: `ScreenMode`/`currentScreen`/`getCurrentScreen()` (Task 1), `ScrollMode`/`scrollMode`/`SCROLL_NATURAL`/`SCROLL_CLASSIC` (Task 2), `drawSettingsScreen()`/`SETTINGS_BUTTON_*` constants (Task 3).

This is the task that finally makes the Settings screen reachable and interactive end-to-end.

**Important — a correctness fix bundled into this task:** the original drawer-row-selection code only called `updateMinMaxForCurrentView()` when the selected row's view differed from `currentView` (`if(newView != currentView) { ... }`). Now that entering Settings force-resets `currentView` to `VIEW_TEMP`, that guard creates a stale-data bug: if you're on Pressure, open Settings (which silently sets `currentView = VIEW_TEMP` without recomputing), then later tap the Temperature row again, `newView (VIEW_TEMP) == currentView (VIEW_TEMP)` already, so the guard would skip recomputing — leaving the nav bar's average and the graph's min/max stale from Pressure. The code below removes that guard and always calls `updateMinMaxForCurrentView()` on every drawer-row selection (both Settings and the 3 sensor rows). This is cheap (the function loops over at most `MAX_POINTS` = 100 points) and closes the staleness gap.

- [ ] **Step 1: Replace `handleTouch()` in full**

In `display_manager.cpp`, find the entire existing `handleTouch()` function (starts with `void handleTouch() {`, ends with the final closing `}` in the file) and replace it with:

```cpp
void handleTouch() {
  static bool wasTouched = false;
  static unsigned long lastTouchTime = 0;
  static unsigned long lastMultiTouchTime = 0;
  static unsigned long lastSingleTouchTime = 0;  // Track single touch timing
  static uint8_t lastContactCount = 0;
  static int panStartX = 0;
  static int panStartOffset = 0;
  static bool panActive = false;
  
  GDTpoint_t points[5];
  uint8_t contacts = touchDetector.getTouchPoints(points);
  
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  
  if(contacts >= 2) {
    if(drawerOpen || currentScreen != SCREEN_GRAPH) {
      // Pinch/zoom is suppressed while the drawer covers the graph,
      // or while a non-graph screen (e.g. Settings) is showing.
      lastContactCount = contacts;
      return;
    }

    // Two finger gesture - pinch to zoom only
    lastMultiTouchTime = millis();
    panActive = false;  // Disable pan when doing two-finger gesture
    
    // Transform both touch points
    int touch1X = points[0].y;
    int touch1Y = 480 - points[0].x;
    int touch2X = points[1].y;
    int touch2Y = 480 - points[1].x;
    
    // Check if both touches are in graph area
    bool inGraph = (touch1Y >= graphY && touch1Y <= graphY + graphHeight &&
                    touch2Y >= graphY && touch2Y <= graphY + graphHeight);
    
    if(inGraph) {
      int currentDistance = calculateDistance(touch1X, touch1Y, touch2X, touch2Y);
      
      if(!gestureActive) {
        // Start of gesture
        gestureActive = true;
        initialDistance = currentDistance;
        initialZoomLevel = getZoomLevel();
        
        Serial.print("Zoom gesture started - Initial distance: ");
        Serial.print(initialDistance);
        Serial.print(", Zoom level: ");
        Serial.println(initialZoomLevel);
      } else {
        // Continue existing gesture
        int absDistanceChange = abs(currentDistance - initialDistance);
        
        if(absDistanceChange > 5) {
          Serial.print("Distance change: ");
          Serial.println(absDistanceChange);
        }
        
        // Zoom when there's significant distance change
        if(absDistanceChange > 30) {
          float zoomFactor = (float)initialDistance / currentDistance;
          int newZoom = initialZoomLevel * zoomFactor;
          setZoomLevel(newZoom);
          drawNavBar();

          Serial.print("Zooming - Factor: ");
          Serial.print(zoomFactor);
          Serial.print(", New zoom: ");
          Serial.println(getZoomLevel());
        }
      }
      
      wasTouched = true;
    }
    
    lastContactCount = contacts;
    return;
  }
  
  if(contacts == 1) {
    // Single touch - icon/drawer/settings handling OR pan
    lastSingleTouchTime = millis();  // Update single touch time

    // Only handle if gesture has timed out
    if(gestureActive && millis() - lastMultiTouchTime < 150) {
      lastContactCount = contacts;
      return;
    }

    // Transform coordinates for rotation
    int touchX = points[0].y;
    int touchY = 480 - points[0].x;

    bool onIcon = (touchX >= 0 && touchX < ICON_TAP_WIDTH &&
                   touchY >= 0 && touchY < NAV_HEIGHT);

    if(drawerOpen) {
      // Drawer covers the graph - suppress pan, handle a single discrete tap.
      if(!wasTouched) {
        if(millis() - lastTouchTime < 200) {
          lastContactCount = contacts;
          return;
        }

        wasTouched = true;
        lastTouchTime = millis();

        if(!onIcon) {
          bool inDrawer = (touchX >= 0 && touchX < DRAWER_WIDTH &&
                           touchY >= graphY && touchY < graphY + graphHeight);

          if(inDrawer) {
            int rowHeight = graphHeight / 4;
            int rowIndex = (touchY - graphY) / rowHeight;

            if(rowIndex == 3) {
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

        drawerOpen = false;
        drawNavBar();
        setRedrawFlag();
      }

      lastContactCount = contacts;
      return;
    }

    // Check if touch is in the content area (graph or settings screen)
    bool inGraph = (touchY >= graphY && touchY <= graphY + graphHeight);

    if(inGraph && currentScreen == SCREEN_SETTINGS) {
      // Settings screen - hit-test the Natural/Classic buttons.
      if(!wasTouched) {
        if(millis() - lastTouchTime < 200) {
          lastContactCount = contacts;
          return;
        }

        wasTouched = true;
        lastTouchTime = millis();

        int buttonY = graphY + SETTINGS_BUTTON_Y_OFFSET;
        int naturalX = GRAPH_MARGIN + 20;
        int classicX = naturalX + SETTINGS_BUTTON_WIDTH + SETTINGS_BUTTON_GAP;

        bool inNatural = (touchX >= naturalX && touchX < naturalX + SETTINGS_BUTTON_WIDTH &&
                          touchY >= buttonY && touchY < buttonY + SETTINGS_BUTTON_HEIGHT);
        bool inClassic = (touchX >= classicX && touchX < classicX + SETTINGS_BUTTON_WIDTH &&
                          touchY >= buttonY && touchY < buttonY + SETTINGS_BUTTON_HEIGHT);

        if(inNatural) {
          scrollMode = SCROLL_NATURAL;
          setRedrawFlag();
          Serial.println("Zoomed scroll set to Natural");
        } else if(inClassic) {
          scrollMode = SCROLL_CLASSIC;
          setRedrawFlag();
          Serial.println("Zoomed scroll set to Classic");
        }
      }

      lastContactCount = contacts;
      return;
    }

    if(inGraph && currentScreen == SCREEN_GRAPH) {
      // Panning in graph area
      if(!panActive) {
        // Start pan
        panActive = true;
        panStartX = touchX;
        panStartOffset = getViewOffset();
        
        Serial.print("Pan started at x: ");
        Serial.print(touchX);
        Serial.print(", offset: ");
        Serial.println(panStartOffset);
      } else {
        // Continue pan
        int panChange = touchX - panStartX;
        int absPanChange = abs(panChange);
        
        if(absPanChange > 5) {
          // Pan left/right - direction depends on scrollMode
          int panDelta = (scrollMode == SCROLL_NATURAL) ? (panChange / 3) : (-panChange / 3);
          int newOffset = panStartOffset + panDelta;
          setViewOffset(newOffset);
          drawNavBar();

          if(absPanChange > 10) {  // Only log significant movement
            Serial.print("Panning - Change: ");
            Serial.print(panChange);
            Serial.print(", Delta: ");
            Serial.print(panDelta);
            Serial.print(", New offset: ");
            Serial.print(getViewOffset());
            Serial.print(" / ");
            Serial.println(getMaxViewOffset());
          }
        }
      }
      
      wasTouched = true;
      lastContactCount = contacts;
      return;
    }
    
    // Not in graph area - check for hamburger icon tap
    if(!wasTouched && !panActive) {
      if(millis() - lastTouchTime < 200) {
        lastContactCount = contacts;
        return;
      }

      wasTouched = true;
      lastTouchTime = millis();

      if(onIcon) {
        drawerOpen = true;
        drawNavBar();
        drawDrawer();
        Serial.println("Drawer opened");
      }
    }

    lastContactCount = contacts;
  }
  
  if(contacts == 0) {
    // Only end gesture if we haven't seen multi-touch for 150ms
    if(gestureActive && millis() - lastMultiTouchTime > 150) {
      Serial.println("Zoom gesture ended");
      gestureActive = false;
    }
    
    // Only end pan if we haven't seen single touch for 150ms
    if(panActive && millis() - lastSingleTouchTime > 150) {
      Serial.println("Pan ended");
      panActive = false;
    }
    
    if(millis() - lastTouchTime > 150) {
      wasTouched = false;
    }
    
    lastContactCount = 0;
  }
}
```

- [ ] **Step 2: Make the `.ino` redraw dispatch screen-aware**

In `weather_console.ino`, find:

```cpp
void loop() {
  ArduinoCloud.update();
  handleTouch();
  
  if(needsRedraw()) {
    drawGraph();
    clearRedrawFlag();
  }
}
```

Replace with:

```cpp
void loop() {
  ArduinoCloud.update();
  handleTouch();
  
  if(needsRedraw()) {
    if(getCurrentScreen() == SCREEN_SETTINGS) {
      drawSettingsScreen();
    } else {
      drawGraph();
    }
    clearRedrawFlag();
  }
}
```

- [ ] **Step 3: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 4: Commit**

```bash
git add display_manager.cpp weather_console.ino
git commit -m "Wire up Settings navigation and screen-aware gesture suppression"
```

---

### Task 5: Upload and manually verify on hardware

**Files:** none (verification only)

- [ ] **Step 1: Find the board port**

Run: `arduino-cli board list`
Expected: a line for `Arduino Giga R1` with FQBN `arduino:mbed_giga:giga`, e.g. `/dev/cu.usbmodem1101`.

- [ ] **Step 2: Compile and upload**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga . && arduino-cli upload --fqbn arduino:mbed_giga:giga -p <port> .`

If upload fails with `dfu-util: No DFU capable USB device available`, double-tap the board's reset button to enter bootloader mode, re-run `arduino-cli board list` (the port number typically increments), and retry the upload with the new port.

- [ ] **Step 3: Manually verify each behavior on the device**

Walk through this checklist on the physical touchscreen:

- [ ] With a zoomed-in graph, dragging a finger right scrolls the visible window to the right (content follows the finger) — this is the default Natural behavior.
- [ ] Opening the hamburger drawer now shows 4 rows: Temperature, Humidity, Pressure, and Settings (with a small gear icon next to its label).
- [ ] Tapping Settings replaces the graph area with a "Zoomed Scroll" label and two buttons, "Natural" and "Classic," with "Natural" highlighted as active by default.
- [ ] The top nav bar shows plain "Settings" text (no average, no accent color) while the Settings screen is showing.
- [ ] Tapping "Classic" highlights it instead, and afterward, dragging right on a zoomed graph scrolls left (the original pre-change behavior).
- [ ] Tapping "Natural" again restores the follow-the-finger behavior.
- [ ] While the Settings screen is showing, pinch-to-zoom and drag-to-pan do nothing (no graph is present to interact with).
- [ ] Reopening the drawer while on the Settings screen shows the Settings row highlighted as active, and none of the 3 sensor rows highlighted.
- [ ] Tapping a sensor row (e.g. Temperature) from the Settings screen's open drawer returns to that graph with a correct, non-stale average and min/max (not leftover values from whatever was showing before Settings).
- [ ] Rebooting the board (power cycle or re-upload) resets the scroll mode back to Natural, regardless of what was last selected.

If any check fails, note which one and stop — do not proceed to further changes until it's diagnosed.
