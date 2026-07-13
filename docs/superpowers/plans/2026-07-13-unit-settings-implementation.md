# Per-Measurement Unit Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Temperature (F/C) and Pressure (hPa/mmHg/psi) unit toggles to the Settings screen, and persist these plus the existing Scroll Style setting across power cycles via Mbed OS KVStore.

**Architecture:** A new `settings_manager.h/.cpp` module owns the three setting enums and all KVStore load/save I/O, kept separate from `display_manager.cpp`'s rendering/touch responsibilities. `display_manager.cpp`'s Settings screen becomes data-driven (one small table of rows, walked by one render loop and one hit-test loop) instead of one hand-written block per setting. Sensor data storage (`data_manager`) and the graph's line-shape math are untouched — unit conversion happens only at the point numbers are formatted into display strings.

**Tech Stack:** Arduino CLI project (arduino:mbed_giga:giga), Mbed OS `kvstore_global_api.h` (already present in the installed core at `~/Library/Arduino15/packages/arduino/hardware/mbed_giga/4.4.1/cores/arduino/mbed/storage/kvstore/kvstore_global_api/include/kvstore_global_api/kvstore_global_api.h` — no new library install needed).

## Global Constraints

- No automated test framework exists for this project (per `CLAUDE.md`) — every task's "test" step is `arduino-cli compile` followed by `arduino-cli upload` and manual verification on the physical board (double-tap reset to enter DFU mode if upload fails with "No DFU capable USB device available"; port may increment, e.g. `/dev/cu.usbmodem101` → `/dev/cu.usbmodem102`).
- Target board FQBN: `arduino:mbed_giga:giga`.
- Humidity is out of scope for unit toggling — leave its rendering exactly as-is.
- No persistence beyond the 3 settings (Scroll Style, Temperature Unit, Pressure Unit); no "reset to defaults" action; no change to how sensor data is sampled, stored, or plotted.
- Never include a `Co-Authored-By: Claude` trailer in any git commit.

---

### Task 1: `settings_manager` module + boot wiring

**Files:**
- Create: `settings_manager.h`
- Create: `settings_manager.cpp`
- Modify: `weather_console.ino` (add include + `loadSettings()` call in `setup()`)

**Interfaces:**
- Produces (used by Task 2 and Task 3):
  - `enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };`
  - `enum TemperatureUnit { TEMP_UNIT_F, TEMP_UNIT_C };`
  - `enum PressureUnit { PRESSURE_UNIT_HPA, PRESSURE_UNIT_MMHG, PRESSURE_UNIT_PSI };`
  - `void loadSettings();`
  - `ScrollMode getScrollMode(); void setScrollMode(ScrollMode mode);`
  - `TemperatureUnit getTemperatureUnit(); void setTemperatureUnit(TemperatureUnit unit);`
  - `PressureUnit getPressureUnit(); void setPressureUnit(PressureUnit unit);`

- [ ] **Step 1: Write `settings_manager.h`**

```cpp
#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };
enum TemperatureUnit { TEMP_UNIT_F, TEMP_UNIT_C };
enum PressureUnit { PRESSURE_UNIT_HPA, PRESSURE_UNIT_MMHG, PRESSURE_UNIT_PSI };

void loadSettings();

ScrollMode getScrollMode();
void setScrollMode(ScrollMode mode);

TemperatureUnit getTemperatureUnit();
void setTemperatureUnit(TemperatureUnit unit);

PressureUnit getPressureUnit();
void setPressureUnit(PressureUnit unit);

#endif
```

- [ ] **Step 2: Write `settings_manager.cpp`**

```cpp
#include "settings_manager.h"
#include "kvstore_global_api.h"
#include <Arduino.h>

static const char* KEY_SCROLL_MODE = "/kv/scroll_mode";
static const char* KEY_TEMP_UNIT = "/kv/temp_unit";
static const char* KEY_PRESSURE_UNIT = "/kv/pressure_unit";

static ScrollMode scrollMode = SCROLL_NATURAL;
static TemperatureUnit temperatureUnit = TEMP_UNIT_F;
static PressureUnit pressureUnit = PRESSURE_UNIT_HPA;

static uint8_t loadU8(const char* key, uint8_t defaultValue) {
  uint8_t value = defaultValue;
  size_t actualSize = 0;
  int result = kv_get(key, &value, sizeof(value), &actualSize);
  if(result != 0 || actualSize != sizeof(value)) {
    return defaultValue;
  }
  return value;
}

static void saveU8(const char* key, uint8_t value) {
  kv_set(key, &value, sizeof(value), 0);
}

void loadSettings() {
  scrollMode = (ScrollMode)loadU8(KEY_SCROLL_MODE, SCROLL_NATURAL);
  temperatureUnit = (TemperatureUnit)loadU8(KEY_TEMP_UNIT, TEMP_UNIT_F);
  pressureUnit = (PressureUnit)loadU8(KEY_PRESSURE_UNIT, PRESSURE_UNIT_HPA);

  Serial.print("Settings loaded - scroll: ");
  Serial.print(scrollMode);
  Serial.print(", temp unit: ");
  Serial.print(temperatureUnit);
  Serial.print(", pressure unit: ");
  Serial.println(pressureUnit);
}

ScrollMode getScrollMode() {
  return scrollMode;
}

void setScrollMode(ScrollMode mode) {
  scrollMode = mode;
  saveU8(KEY_SCROLL_MODE, (uint8_t)mode);
}

TemperatureUnit getTemperatureUnit() {
  return temperatureUnit;
}

void setTemperatureUnit(TemperatureUnit unit) {
  temperatureUnit = unit;
  saveU8(KEY_TEMP_UNIT, (uint8_t)unit);
}

PressureUnit getPressureUnit() {
  return pressureUnit;
}

void setPressureUnit(PressureUnit unit) {
  pressureUnit = unit;
  saveU8(KEY_PRESSURE_UNIT, (uint8_t)unit);
}
```

- [ ] **Step 3: Wire `loadSettings()` into `weather_console.ino`**

In `weather_console.ino`, add the include near the top alongside the other includes:

```cpp
#include "thingProperties.h"
#include "display_manager.h"
#include "data_manager.h"
#include "settings_manager.h"
```

And call `loadSettings()` in `setup()`, before `initDisplay()` so the very first paint (`drawStaticElements()`) can reflect persisted values once Task 2 wires the getters into rendering:

```cpp
void setup() {
  Serial.begin(9600);
  delay(1500); 
  
  // Load persisted settings before first draw
  loadSettings();
  
  // Initialize display and touch
  initDisplay();
  
  // Initialize data buffers
  initDataBuffers();
  
  // Initialize IoT Cloud
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
  
  // Draw initial UI
  drawStaticElements();
}
```

- [ ] **Step 4: Compile**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: compiles with no errors (nothing yet calls the new getters/setters outside `settings_manager.cpp` itself, so this only proves the new module builds and links).

- [ ] **Step 5: Upload and verify boot log**

Run: `arduino-cli upload --fqbn arduino:mbed_giga:giga -p <port> .` (use the port from `arduino-cli board list`; double-tap the board's reset button first if you get "No DFU capable USB device available").

Open the Serial Monitor at 9600 baud. On first boot (or after a previous KVStore erase), expect a line like:

```
Settings loaded - scroll: 0, temp unit: 0, pressure unit: 0
```

(`0` for each is the default: `SCROLL_NATURAL`, `TEMP_UNIT_F`, `PRESSURE_UNIT_HPA`.) This confirms `kv_get` correctly falls back to defaults when no key exists yet.

- [ ] **Step 6: Commit**

```bash
git add settings_manager.h settings_manager.cpp weather_console.ino
git commit -m "Add settings_manager module with KVStore-backed persistence"
```

---

### Task 2: Data-driven Settings screen (rendering + touch) using `settings_manager`

**Files:**
- Modify: `display_manager.h` (add `SETTINGS_ROW_HEIGHT` constant)
- Modify: `display_manager.cpp` (remove file-local `ScrollMode`; rewrite `drawSettingsScreen()` and the Settings-screen branch of `handleTouch()` to be data-driven over all 3 settings)

**Interfaces:**
- Consumes: `settings_manager.h`'s `ScrollMode`/`TemperatureUnit`/`PressureUnit` enums and all 6 getters/setters (Task 1).
- Produces: no new public interface — this task only changes internals of `display_manager.cpp`. `getViewUnit()`'s signature (`static String getViewUnit(ViewMode view)`) is unchanged here; Task 3 changes its *body*.

- [ ] **Step 1: Add `SETTINGS_ROW_HEIGHT` to `display_manager.h`**

In `display_manager.h`, in the "Settings screen" constants block, add one line after the existing `SETTINGS_BUTTON_X_OFFSET`:

```cpp
// Settings screen
#define SETTINGS_BUTTON_WIDTH 150
#define SETTINGS_BUTTON_HEIGHT 50
#define SETTINGS_BUTTON_GAP 20
#define SETTINGS_BUTTON_Y_OFFSET 60
#define SETTINGS_BUTTON_X_OFFSET 20
#define SETTINGS_ROW_HEIGHT 90
```

- [ ] **Step 2: Include `settings_manager.h` and remove the file-local `ScrollMode` in `display_manager.cpp`**

At the top of `display_manager.cpp`, change:

```cpp
#include "display_manager.h"
#include "data_manager.h"
```

to:

```cpp
#include "display_manager.h"
#include "data_manager.h"
#include "settings_manager.h"
```

Then delete these two lines (the file-local enum/static being replaced by `settings_manager`):

```cpp
enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };
static ScrollMode scrollMode = SCROLL_NATURAL;
```

- [ ] **Step 3: Add a shared row-table builder, placed directly above `drawSettingsScreen()`**

This one function is called by both the render step and the touch-handling step, so drawn geometry and hit-test geometry can never disagree (the array parameter must be sized 3 to match the exact number of settings rows):

```cpp
struct SettingRow {
  const char* label;
  const char** options;
  uint8_t optionCount;
  uint8_t currentIndex;
  void (*onSelect)(uint8_t index);
};

static void applyScrollMode(uint8_t i)   { setScrollMode((ScrollMode)i); }
static void applyTempUnit(uint8_t i)     { setTemperatureUnit((TemperatureUnit)i); }
static void applyPressureUnit(uint8_t i) { setPressureUnit((PressureUnit)i); }

static void getSettingRows(SettingRow rows[3]) {
  static const char* scrollOpts[]   = {"Natural", "Classic"};
  static const char* tempOpts[]     = {"F", "C"};
  static const char* pressureOpts[] = {"hPa", "mmHg", "psi"};

  rows[0] = { "Scroll Style",     scrollOpts,   2, (uint8_t)getScrollMode(),      applyScrollMode };
  rows[1] = { "Temperature Unit", tempOpts,     2, (uint8_t)getTemperatureUnit(), applyTempUnit };
  rows[2] = { "Pressure Unit",    pressureOpts, 3, (uint8_t)getPressureUnit(),    applyPressureUnit };
}
```

- [ ] **Step 4: Replace `drawSettingsScreen()`**

Replace the entire existing `drawSettingsScreen()` function body with a version that loops over `getSettingRows()`:

```cpp
void drawSettingsScreen() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;

  // Clear settings area, including the margin strip to the left of the
  // frame (the drawer paints all the way to x=0, wider than GRAPH_MARGIN).
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1,
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);

  SettingRow rows[3];
  getSettingRows(rows);

  for(int r = 0; r < 3; r++) {
    int rowTop = graphY + r * SETTINGS_ROW_HEIGHT;

    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT);
    display.setCursor(GRAPH_MARGIN + 20, rowTop + 20);
    display.print(rows[r].label);

    int buttonY = rowTop + SETTINGS_BUTTON_Y_OFFSET;

    for(int i = 0; i < rows[r].optionCount; i++) {
      int buttonX = GRAPH_MARGIN + SETTINGS_BUTTON_X_OFFSET +
                    i * (SETTINGS_BUTTON_WIDTH + SETTINGS_BUTTON_GAP);
      bool active = (i == rows[r].currentIndex);

      display.fillRoundRect(buttonX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                             active ? COLOR_BUTTON_ACTIVE : COLOR_BUTTON_BG);
      display.drawRoundRect(buttonX, buttonY, SETTINGS_BUTTON_WIDTH, SETTINGS_BUTTON_HEIGHT, 8,
                             active ? COLOR_NAV_ACTIVE : COLOR_TEXT);

      display.setTextSize(2);
      display.setTextColor(active ? COLOR_NAV_ACTIVE : COLOR_TEXT);
      display.setCursor(buttonX + 20, buttonY + 17);
      display.print(rows[r].options[i]);
    }
  }

  // Restore frame border in case anything touched the edge pixels
  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
}
```

- [ ] **Step 5: Replace the Settings-screen touch branch in `handleTouch()`**

In `handleTouch()`, find this block (the `if(inGraph && currentScreen == SCREEN_SETTINGS)` branch):

```cpp
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
        int naturalX = GRAPH_MARGIN + SETTINGS_BUTTON_X_OFFSET;
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
```

and replace it with:

```cpp
    if(inGraph && currentScreen == SCREEN_SETTINGS) {
      // Settings screen - hit-test each row's option buttons.
      if(!wasTouched) {
        if(millis() - lastTouchTime < 200) {
          lastContactCount = contacts;
          return;
        }

        wasTouched = true;
        lastTouchTime = millis();

        SettingRow rows[3];
        getSettingRows(rows);

        for(int r = 0; r < 3; r++) {
          int rowTop = graphY + r * SETTINGS_ROW_HEIGHT;
          int buttonY = rowTop + SETTINGS_BUTTON_Y_OFFSET;

          if(touchY < buttonY || touchY >= buttonY + SETTINGS_BUTTON_HEIGHT) {
            continue;
          }

          for(int i = 0; i < rows[r].optionCount; i++) {
            int buttonX = GRAPH_MARGIN + SETTINGS_BUTTON_X_OFFSET +
                          i * (SETTINGS_BUTTON_WIDTH + SETTINGS_BUTTON_GAP);

            if(touchX >= buttonX && touchX < buttonX + SETTINGS_BUTTON_WIDTH) {
              rows[r].onSelect(i);
              setRedrawFlag();
              Serial.print("Settings row ");
              Serial.print(r);
              Serial.print(" set to option ");
              Serial.println(i);
              break;
            }
          }

          break;
        }
      }

      lastContactCount = contacts;
      return;
    }
```

- [ ] **Step 6: Update the pan-delta formula to read `getScrollMode()`**

In `handleTouch()`, find:

```cpp
          int panDelta = (scrollMode == SCROLL_NATURAL) ? (panChange / 3) : (-panChange / 3);
```

and replace with:

```cpp
          int panDelta = (getScrollMode() == SCROLL_NATURAL) ? (panChange / 3) : (-panChange / 3);
```

- [ ] **Step 7: Compile**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: compiles with no errors. (If you see "scrollMode was not declared", search `display_manager.cpp` for any remaining reference to the old file-local `scrollMode` variable and switch it to `getScrollMode()`.)

- [ ] **Step 8: Upload and verify on hardware**

Run: `arduino-cli upload --fqbn arduino:mbed_giga:giga -p <port> .`

Manually verify on the board:
1. Open the hamburger drawer, tap Settings. Confirm you now see **three** rows: "Scroll Style" (Natural/Classic), "Temperature Unit" (F/C), "Pressure Unit" (hPa/mmHg/psi), all fitting within the frame with no overlap or clipping.
2. Tap each row's non-default option (e.g. Classic, C, mmHg) and confirm the active-button highlight moves immediately.
3. Power-cycle the board (unplug/replug or press the physical reset button — not just re-uploading). Reopen Settings and confirm all three choices you set are still selected (proves KVStore persistence works end-to-end).
4. Go to a graph view, pinch-zoom and pan; confirm panning direction still matches whichever Scroll Style you selected.

- [ ] **Step 9: Commit**

```bash
git add display_manager.h display_manager.cpp
git commit -m "Make Settings screen data-driven and persist all settings via KVStore"
```

---

### Task 3: Display-time unit conversion for Temperature and Pressure

**Files:**
- Modify: `display_manager.cpp` (`getViewUnit()`, add two conversion helpers, apply conversion at 4 call sites in `drawGraph()`/`drawNavBar()`)

**Interfaces:**
- Consumes: `getTemperatureUnit()`/`getPressureUnit()` from `settings_manager.h` (already included as of Task 2).
- Produces: none consumed by later tasks — this is the final task.

- [ ] **Step 1: Update `getViewUnit()` to be unit-aware**

Replace:

```cpp
static String getViewUnit(ViewMode view) {
  switch(view) {
    case VIEW_TEMP:     return "F";
    case VIEW_HUMIDITY: return "%";
    case VIEW_PRESSURE: return "hPa";
  }
  return "";
}
```

with:

```cpp
static String getViewUnit(ViewMode view) {
  switch(view) {
    case VIEW_TEMP:
      return (getTemperatureUnit() == TEMP_UNIT_C) ? "C" : "F";
    case VIEW_HUMIDITY:
      return "%";
    case VIEW_PRESSURE:
      switch(getPressureUnit()) {
        case PRESSURE_UNIT_MMHG: return "mmHg";
        case PRESSURE_UNIT_PSI:  return "psi";
        default:                 return "hPa";
      }
  }
  return "";
}
```

- [ ] **Step 2: Add the two conversion helpers directly below `getViewUnit()`**

```cpp
static float convertTempForDisplay(float rawF) {
  return (getTemperatureUnit() == TEMP_UNIT_C) ? (rawF - 32) * 5.0 / 9.0 : rawF;
}

static float convertPressureForDisplay(float rawHpa) {
  switch(getPressureUnit()) {
    case PRESSURE_UNIT_MMHG: return rawHpa * 0.750062;
    case PRESSURE_UNIT_PSI:  return rawHpa * 0.0145038;
    default:                 return rawHpa; // hPa
  }
}

static float convertForCurrentView(ViewMode view, float rawValue) {
  switch(view) {
    case VIEW_TEMP:     return convertTempForDisplay(rawValue);
    case VIEW_PRESSURE: return convertPressureForDisplay(rawValue);
    default:            return rawValue; // Humidity: no conversion
  }
}
```

- [ ] **Step 3: Apply conversion in `drawNavBar()`'s average text**

Find, in `drawNavBar()`:

```cpp
  uint16_t accentColor = getViewColor(currentView);
  String unit = getViewUnit(currentView);
  float avgValue;
  getAverageValue(&avgValue);

  String label = String(getViewLabel(currentView)) + "  " +
                 String(avgValue, 1) + " " + unit + " avg";
```

Replace with:

```cpp
  uint16_t accentColor = getViewColor(currentView);
  String unit = getViewUnit(currentView);
  float avgValue;
  getAverageValue(&avgValue);
  float displayAvg = convertForCurrentView(currentView, avgValue);

  String label = String(getViewLabel(currentView)) + "  " +
                 String(displayAvg, 1) + " " + unit + " avg";
```

- [ ] **Step 4: Apply conversion in `drawGraph()`'s min/max/current-value labels**

Find, in `drawGraph()`:

```cpp
  // Draw Y-axis labels
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT);
  display.setCursor(5, graphY);
  display.print(String(maxValue, 1));
  display.setCursor(5, graphY + graphHeight - 10);
  display.print(String(minValue, 1));
  
  // Display current value (rightmost visible point)
  int latestIdx = (startIdx + pointsToPlot - 1) % getMaxPoints();
  display.setTextSize(3);
  display.setCursor(GRAPH_MARGIN + 10, graphY + 10);
  display.setTextColor(lineColor);
  display.print(String(data[latestIdx], 1) + " " + unit);
```

Replace with:

```cpp
  // Draw Y-axis labels
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT);
  display.setCursor(5, graphY);
  display.print(String(convertForCurrentView(currentView, maxValue), 1));
  display.setCursor(5, graphY + graphHeight - 10);
  display.print(String(convertForCurrentView(currentView, minValue), 1));
  
  // Display current value (rightmost visible point)
  int latestIdx = (startIdx + pointsToPlot - 1) % getMaxPoints();
  display.setTextSize(3);
  display.setCursor(GRAPH_MARGIN + 10, graphY + 10);
  display.setTextColor(lineColor);
  display.print(String(convertForCurrentView(currentView, data[latestIdx]), 1) + " " + unit);
```

Note: `minValue`/`maxValue`/`data[]` remain raw (native-unit) throughout the rest of `drawGraph()` — the line-plotting math a few lines above this block (`y1`/`y2` calculations) is untouched, since it only cares about relative position within the raw min/max range, not the displayed unit.

- [ ] **Step 5: Compile**

Run: `arduino-cli compile --fqbn arduino:mbed_giga:giga .`
Expected: compiles with no errors.

- [ ] **Step 6: Upload and verify on hardware**

Run: `arduino-cli upload --fqbn arduino:mbed_giga:giga -p <port> .`

Manually verify:
1. On the Temperature graph, note the current-value reading in °F (e.g. "72.0 F"). Go to Settings, switch Temperature Unit to C. Return to the Temperature graph and confirm the current value, min/max labels, and nav-bar average all now show in °C and the unit suffix reads "C" — and that `(72.0°F - 32) * 5/9 ≈ 22.2°C` matches what's displayed (within the sensor's natural fluctuation between reads).
2. On the Pressure graph, note the current value in hPa. Switch Pressure Unit to mmHg, confirm the displayed value is roughly `hPa_value * 0.750062`. Switch to psi, confirm roughly `hPa_value * 0.0145038`.
3. Confirm the Humidity graph is completely unaffected — always "%", unchanged values, regardless of Temperature/Pressure unit settings.
4. Confirm the graph *line shape* itself doesn't jump or distort when switching units (it shouldn't, since it plots off raw values) — only the printed numbers/unit labels change.

- [ ] **Step 7: Commit**

```bash
git add display_manager.cpp
git commit -m "Convert Temperature and Pressure values to selected unit at display time"
```

---

## Post-Plan Note

This plan does not include a `CLAUDE.md` update task — per this project's established pattern, `CLAUDE.md` documentation updates happen as a separate explicit step after the feature is verified and merged, not as part of the implementation plan itself.
