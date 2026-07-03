# Hamburger Menu + Top-Bar Average Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 3-button nav bar with a hamburger icon that opens a left-side drawer listing Temperature/Humidity/Pressure, and show the current view's visible-window average next to its name in the top bar.

**Architecture:** `display_manager.cpp` owns a new `drawerOpen` state flag alongside the existing `currentView`/`redrawNeeded` state. The nav bar is redrawn to show an icon + view name + average instead of 3 buttons; a new `drawDrawer()` paints a 3-row panel over the left side of the graph when open. `handleTouch()` gates pinch-zoom/pan behind `!drawerOpen` and adds icon-tap/drawer-tap handling. `data_manager.cpp` gains a per-window average computed inside the existing min/max loop (no new windowing-math copy).

**Tech Stack:** Arduino sketch (C++), `arduino:mbed_giga:giga` board, `Arduino_GigaDisplay_GFX` / `Arduino_GigaDisplayTouch` libraries (already installed in this environment).

## Global Constraints

- This project has **no automated test framework** — it's an Arduino sketch with no host-side unit tests (confirmed in `CLAUDE.md`). Every task's "test" step is `arduino-cli compile --fqbn arduino:mbed_giga:giga .` run from the project root; the final task adds a manual on-hardware verification checklist in place of behavioral tests.
- Follow the approved spec exactly: `docs/superpowers/specs/2026-07-03-hamburger-menu-design.md`. Do not add drawer animation or a full-buffer average — both are explicitly out of scope there.
- Required libraries are already installed (`ArduinoIoTCloud`, `Arduino_GigaDisplayTouch`, `Arduino_GigaDisplay_GFX`, `Arduino_NetworkConfigurator`, and dependencies). Do not reinstall.
- This is a git repo (`origin` = `git@github.com:arnquic/weather_console.git`, branch `main`). Commit after each task; do not push unless asked.

---

### Task 1: Add per-view color/label/unit helpers

**Files:**
- Modify: `display_manager.cpp` (insert new helpers near the top; simplify `drawGraph()`'s view switch)

**Interfaces:**
- Produces: `static uint16_t getViewColor(ViewMode view)`, `static const char* getViewLabel(ViewMode view)`, `static String getViewUnit(ViewMode view)` — used by Task 3 (`drawNavBar`, `drawDrawer`) and this task's `drawGraph()` update.

This removes triplication: without it, the color/label/unit mapping for each `ViewMode` would be copy-pasted independently into the nav bar, the drawer, and `drawGraph()`.

- [ ] **Step 1: Insert the three helper functions**

In `display_manager.cpp`, insert this block right after the existing touch-gesture state variables and before `void initDisplay() {` (i.e., after the line `static int initialOffset = 0;`):

```cpp

static uint16_t getViewColor(ViewMode view) {
  switch(view) {
    case VIEW_TEMP:     return COLOR_TEMP_LINE;
    case VIEW_HUMIDITY: return COLOR_HUMIDITY_LINE;
    case VIEW_PRESSURE: return COLOR_PRESSURE_LINE;
  }
  return COLOR_TEXT;
}

static const char* getViewLabel(ViewMode view) {
  switch(view) {
    case VIEW_TEMP:     return "Temperature";
    case VIEW_HUMIDITY: return "Humidity";
    case VIEW_PRESSURE: return "Pressure";
  }
  return "";
}

static String getViewUnit(ViewMode view) {
  switch(view) {
    case VIEW_TEMP:     return "C";
    case VIEW_HUMIDITY: return "%";
    case VIEW_PRESSURE: return "hPa";
  }
  return "";
}
```

They must go here (before any function that will call them) because they're file-local `static` functions with no header declaration — C++ requires the definition to appear before first use in the same translation unit.

- [ ] **Step 2: Simplify `drawGraph()`'s view switch to use the new helpers**

Find this block inside `drawGraph()`:

```cpp
  // Get data for current view
  float* data;
  uint16_t lineColor;
  String unit;
  
  switch(currentView) {
    case VIEW_TEMP:
      data = getTempData();
      lineColor = COLOR_TEMP_LINE;
      unit = "C";
      break;
    case VIEW_HUMIDITY:
      data = getHumidityData();
      lineColor = COLOR_HUMIDITY_LINE;
      unit = "%";
      break;
    case VIEW_PRESSURE:
      data = getPressureData();
      lineColor = COLOR_PRESSURE_LINE;
      unit = "hPa";
      break;
  }
```

Replace it with:

```cpp
  // Get data for current view
  float* data;
  switch(currentView) {
    case VIEW_TEMP:
      data = getTempData();
      break;
    case VIEW_HUMIDITY:
      data = getHumidityData();
      break;
    case VIEW_PRESSURE:
      data = getPressureData();
      break;
  }
  uint16_t lineColor = getViewColor(currentView);
  String unit = getViewUnit(currentView);
```

- [ ] **Step 3: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: `Sketch uses ... bytes` summary, exit code 0, no errors.

- [ ] **Step 4: Commit**

```bash
git add display_manager.cpp
git commit -m "Add per-view color/label/unit helpers, dedupe drawGraph switch"
```

---

### Task 2: Add visible-window average to data_manager

**Files:**
- Modify: `data_manager.h:12` (add getter declaration)
- Modify: `data_manager.cpp` (add `avgValue` state, accumulate sum in the existing min/max loop, add getter)

**Interfaces:**
- Consumes: nothing new — reuses the existing `pointsToCheck`/`startIdx` window computed inside `updateMinMaxForCurrentView()`.
- Produces: `void getAverageValue(float* avg)` — used by Task 3's `drawNavBar()`.

- [ ] **Step 1: Add the getter declaration to the header**

In `data_manager.h`, find:

```c
void getMinMaxValues(float* minVal, float* maxVal);
```

Add immediately after it:

```c
void getAverageValue(float* avg);
```

- [ ] **Step 2: Add the `avgValue` static and accumulate it in the existing loop**

In `data_manager.cpp`, find:

```cpp
static float minValue = 0;
static float maxValue = 100;
```

Replace with:

```cpp
static float minValue = 0;
static float maxValue = 100;
static float avgValue = 0;
```

Then, inside `updateMinMaxForCurrentView()`, find:

```cpp
  minValue = data[startIdx];
  maxValue = data[startIdx];
  
  for(int i = 0; i < pointsToCheck; i++) {
    int idx = (startIdx + i) % MAX_POINTS;
    if(data[idx] < minValue) minValue = data[idx];
    if(data[idx] > maxValue) maxValue = data[idx];
  }
  
  float range = maxValue - minValue;
```

Replace with:

```cpp
  minValue = data[startIdx];
  maxValue = data[startIdx];
  float sum = 0;
  
  for(int i = 0; i < pointsToCheck; i++) {
    int idx = (startIdx + i) % MAX_POINTS;
    if(data[idx] < minValue) minValue = data[idx];
    if(data[idx] > maxValue) maxValue = data[idx];
    sum += data[idx];
  }
  
  avgValue = sum / pointsToCheck;
  
  float range = maxValue - minValue;
```

(`pointsToCheck` is always >= 1 here — the function returns early above when `dataCount == 0`, so no division-by-zero risk.)

- [ ] **Step 3: Add the getter function**

In `data_manager.cpp`, find:

```cpp
void getMinMaxValues(float* minVal, float* maxVal) {
  *minVal = minValue;
  *maxVal = maxValue;
}
```

Add immediately after it:

```cpp
void getAverageValue(float* avg) {
  *avg = avgValue;
}
```

- [ ] **Step 4: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 5: Commit**

```bash
git add data_manager.h data_manager.cpp
git commit -m "Add visible-window average calculation to data_manager"
```

---

### Task 3: Replace button drawing with hamburger icon + drawer drawing

**Files:**
- Modify: `display_manager.h:14-18` (button constants → icon/drawer constants)
- Modify: `display_manager.h:39` (drawButton declaration → drawHamburgerIcon/drawDrawer declarations)
- Modify: `display_manager.cpp` (replace `drawNavBar()`/`drawButton()` with `drawNavBar()`/`drawHamburgerIcon()`/`drawDrawer()`)

**Interfaces:**
- Consumes: `getViewColor()`, `getViewLabel()`, `getViewUnit()` (Task 1), `getAverageValue()` (Task 2).
- Produces: `void drawHamburgerIcon()`, `void drawDrawer()` — called by Task 4's `handleTouch()`. `currentView` (existing file-local static) is read directly by both.

This task only changes drawing code — no touch handling changes yet, so `drawDrawer()` isn't invoked from anywhere until Task 4. That's expected; it will compile fine (an unused function is not an error, only unreferenced *variables* are, and this task adds no unused variables).

- [ ] **Step 1: Replace the button constants in the header**

In `display_manager.h`, find:

```c
// Button definitions
#define BUTTON_WIDTH 240
#define BUTTON_HEIGHT 40
#define BUTTON_MARGIN 10
#define BUTTON_Y 5           // Buttons now at top of screen
```

Replace with:

```c
// Hamburger icon
#define ICON_MARGIN 10
#define ICON_SIZE 30
#define ICON_BAR_HEIGHT 4
#define ICON_BAR_GAP 4
#define ICON_Y 15
#define ICON_TAP_WIDTH 60

// Drawer
#define DRAWER_WIDTH 220
```

- [ ] **Step 2: Replace the `drawButton` declaration**

In `display_manager.h`, find:

```c
void drawButton(int x, int y, const char* label, bool active, uint16_t accentColor);
```

Replace with:

```c
void drawHamburgerIcon();
void drawDrawer();
```

- [ ] **Step 3: Replace `drawNavBar()`/`drawButton()` in the .cpp with `drawNavBar()`/`drawHamburgerIcon()`/`drawDrawer()`**

In `display_manager.cpp`, find this whole block:

```cpp
void drawNavBar() {
  display.fillRect(0, TITLE_HEIGHT, SCREEN_WIDTH, NAV_HEIGHT, COLOR_NAV_BG);
  
  drawButton(BUTTON_MARGIN, BUTTON_Y, "Temperature", 
             currentView == VIEW_TEMP, COLOR_TEMP_LINE);
  drawButton(BUTTON_MARGIN * 2 + BUTTON_WIDTH, BUTTON_Y, "Humidity", 
             currentView == VIEW_HUMIDITY, COLOR_HUMIDITY_LINE);
  drawButton(BUTTON_MARGIN * 3 + BUTTON_WIDTH * 2, BUTTON_Y, "Pressure", 
             currentView == VIEW_PRESSURE, COLOR_PRESSURE_LINE);
}

void drawButton(int x, int y, const char* label, bool active, uint16_t accentColor) {
  if(active) {
    display.fillRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 8, COLOR_BUTTON_ACTIVE);
    display.drawRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 8, accentColor);
  } else {
    display.fillRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 8, COLOR_BUTTON_BG);
    display.drawRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 8, COLOR_TEXT);
  }
  
  display.setTextSize(2);
  display.setTextColor(active ? COLOR_NAV_ACTIVE : COLOR_TEXT);
  
  int textWidth = strlen(label) * 12;
  int textX = x + (BUTTON_WIDTH - textWidth) / 2;
  int textY = y + (BUTTON_HEIGHT - 16) / 2;
  
  display.setCursor(textX, textY);
  display.print(label);
}
```

Replace it with:

```cpp
void drawNavBar() {
  display.fillRect(0, TITLE_HEIGHT, SCREEN_WIDTH, NAV_HEIGHT, COLOR_NAV_BG);

  drawHamburgerIcon();

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

void drawHamburgerIcon() {
  for(int i = 0; i < 3; i++) {
    int barY = ICON_Y + i * (ICON_BAR_HEIGHT + ICON_BAR_GAP);
    display.fillRect(ICON_MARGIN, barY, ICON_SIZE, ICON_BAR_HEIGHT, COLOR_TEXT);
  }
}

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

- [ ] **Step 4: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors. (Unused-function warnings for `drawDrawer`, if any, are fine at this stage — it's wired up in Task 4.)

- [ ] **Step 5: Commit**

```bash
git add display_manager.h display_manager.cpp
git commit -m "Draw hamburger icon and drawer panel in place of nav buttons"
```

---

### Task 4: Wire up drawer open/close/select touch handling

**Files:**
- Modify: `display_manager.cpp` (add `drawerOpen` state; rewrite `handleTouch()`)

**Interfaces:**
- Consumes: `drawDrawer()`, `drawHamburgerIcon()` (Task 3, via `drawNavBar()`), `ICON_TAP_WIDTH`/`DRAWER_WIDTH` (Task 3 header constants).
- Produces: `drawerOpen` (file-local static bool) — no other file needs it.

- [ ] **Step 1: Add the `drawerOpen` state variable**

In `display_manager.cpp`, find:

```cpp
static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;
```

Replace with:

```cpp
static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;
static bool drawerOpen = false;
```

- [ ] **Step 2: Replace `handleTouch()` in full**

Find the entire existing `handleTouch()` function (it starts with `void handleTouch() {` and ends with the closing `}` that matches it — the last function in the file) and replace it with:

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
    if(drawerOpen) {
      // Pinch/zoom is suppressed while the drawer covers the graph.
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
    // Single touch - icon/drawer handling OR pan
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
            int rowHeight = graphHeight / 3;
            int rowIndex = (touchY - graphY) / rowHeight;
            ViewMode newView = currentView;

            if(rowIndex == 0) newView = VIEW_TEMP;
            else if(rowIndex == 1) newView = VIEW_HUMIDITY;
            else newView = VIEW_PRESSURE;

            if(newView != currentView) {
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
    
    // Check if touch is in graph area for panning
    bool inGraph = (touchY >= graphY && touchY <= graphY + graphHeight);
    
    if(inGraph) {
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
          // Pan left/right - drag right = scroll back in time
          int panDelta = -panChange / 3;  // Negative because dragging right should increase offset
          int newOffset = panStartOffset + panDelta;
          setViewOffset(newOffset);
          
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

- [ ] **Step 3: Compile to verify**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: exit code 0, no errors.

- [ ] **Step 4: Commit**

```bash
git add display_manager.cpp
git commit -m "Wire up hamburger icon and drawer touch handling"
```

---

### Task 5: Upload and manually verify on hardware

**Files:** none (verification only)

- [ ] **Step 1: Find the board port**

Run: `arduino-cli board list`
Expected: a line for `Arduino Giga R1` with FQBN `arduino:mbed_giga:giga`, e.g. `/dev/cu.usbmodem1101`.

- [ ] **Step 2: Compile and upload**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga . && arduino-cli upload --fqbn arduino:mbed_giga:giga -p <port> .`

If upload fails with `dfu-util: No DFU capable USB device available`, double-tap the board's reset button to enter bootloader mode, re-run `arduino-cli board list` (the port number typically increments, e.g. `1101` → `1102`), and retry the upload with the new port.

- [ ] **Step 3: Manually verify each behavior on the device**

Walk through this checklist on the physical touchscreen:

- [ ] Top bar shows a hamburger icon (top-left) and, next to it, the current view's name plus its average (e.g. "Temperature  22.3 C avg").
- [ ] Tapping the hamburger icon opens a drawer on the left listing Temperature / Humidity / Pressure, covering the left part of the graph.
- [ ] The currently active view's row in the drawer is visually highlighted (accent-colored background/border).
- [ ] Tapping a different row in the drawer switches the view, closes the drawer, and the top bar/graph update to the new view (including its average).
- [ ] Tapping the hamburger icon again while the drawer is open closes it without changing the view.
- [ ] Tapping elsewhere (graph area to the right of the drawer, or the top bar) while the drawer is open closes it without changing the view.
- [ ] While the drawer is open, pinch-to-zoom and drag-to-pan on the graph do nothing (no zoom/pan indicator changes).
- [ ] While the drawer is closed, pinch-to-zoom and drag-to-pan work as before (unchanged from pre-existing behavior).
- [ ] Panning/zooming the graph changes the top-bar average (since it reflects the visible window, per the approved spec).
- [ ] The graph border is intact after switching views/opening-closing the drawer repeatedly (no leftover visual artifacts from the drawer or graph lines).

If any check fails, note which one and stop — do not proceed to further changes until it's diagnosed.
