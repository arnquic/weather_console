# Per-measurement unit settings + persistence — design

## Context

The console currently hardcodes display units: Temperature always shows °F, Pressure always shows hPa. Humidity is inherently a percentage with no common alternate unit. This feature adds a Settings-screen toggle for Temperature (F/C) and Pressure (hPa/mmHg/psi), and — since the Arduino Giga R1's Mbed OS core exposes a KVStore flash API — persists these choices (and the existing Scroll Style setting) across power cycles, replacing today's reset-on-boot behavior for Scroll Style.

## Goals

1. Add a "Temperature Unit" setting with values F (default) and C.
2. Add a "Pressure Unit" setting with values hPa (default), mmHg, and psi — three explicit buttons, not a two-way cycle.
3. Persist all three settings (Scroll Style, Temperature Unit, Pressure Unit) across power cycles via Mbed OS KVStore, replacing Scroll Style's current reset-on-boot behavior.
4. Keep the Settings screen's rendering and touch hit-testing data-driven so adding a future setting doesn't require copy-pasting another near-duplicate block.

## New module: `settings_manager.h` / `settings_manager.cpp`

Owns all persisted-setting state and KVStore I/O — kept separate from `display_manager` so that file stays focused on rendering/touch, not flash I/O.

```cpp
// settings_manager.h
enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };
enum TemperatureUnit { TEMP_UNIT_F, TEMP_UNIT_C };
enum PressureUnit { PRESSURE_UNIT_HPA, PRESSURE_UNIT_MMHG, PRESSURE_UNIT_PSI };

void loadSettings();  // call once from setup(), before first draw

ScrollMode getScrollMode();
void setScrollMode(ScrollMode mode);

TemperatureUnit getTemperatureUnit();
void setTemperatureUnit(TemperatureUnit unit);

PressureUnit getPressureUnit();
void setPressureUnit(PressureUnit unit);
```

- `ScrollMode` moves out of `display_manager.cpp` (currently file-local, non-persisted) into this new header, since it's now persisted identically to the other two settings.
- Each `set*()` updates an in-RAM static immediately (rendering never blocks on flash) and also performs a synchronous `kv_set()` call. Writes only happen on a settings-button tap, so no batching/async is needed.
- `loadSettings()` calls `kv_get()` for each of the 3 keys. Any read that fails (key not found on first boot, or a previously-interrupted write) falls back to the documented default for that setting (Natural / F / hPa) rather than treating it as an error.
- Storage: each value is a single `uint8_t` (the enum's underlying value) under keys `"/kv/scroll_mode"`, `"/kv/temp_unit"`, `"/kv/pressure_unit"`, using `kv_set`/`kv_get`/from `kvstore_global_api.h` (part of the already-installed `arduino:mbed_giga` core — confirmed present at `cores/arduino/mbed/storage/kvstore/kvstore_global_api/include/kvstore_global_api/kvstore_global_api.h`; no new library needed).

## Display-time unit conversion

Underlying sensor data is untouched — `data_manager` continues storing raw °F and raw hPa, and `drawGraph()`'s line-shape/pixel-mapping math continues operating on raw native-unit values (it's a relative scale, so unit choice doesn't affect line shape). Conversion happens only where a number is turned into display text.

Two small static helpers in `display_manager.cpp`:

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
```

`getViewUnit()` becomes unit-aware: returns `"F"`/`"C"` for Temperature and `"hPa"`/`"mmHg"`/`"psi"` for Pressure based on the current setting (currently hardcoded strings). Humidity is unaffected — still plain `data[idx]` and `"%"`.

These apply at the 4 existing display sites in `drawGraph()`/`drawNavBar()` that currently do `String(rawValue, 1)`: the Y-axis min label, the Y-axis max label, the current-value label, and the nav-bar average — each becomes `String(convert...(rawValue), 1)` for Temperature/Pressure views (Humidity's call sites are left as-is).

## Settings screen: data-driven layout

Replaces the current hand-written single "Scroll Style" block with one table walked by a single render loop and a single hit-test loop, so a 2-button and a 3-button row both fall out of the same code:

```cpp
struct SettingRow {
  const char* label;
  const char** options;
  uint8_t optionCount;
  uint8_t currentIndex;
  void (*onSelect)(uint8_t index);
};
```

Built fresh on every draw/touch (cheap — 3 entries) from the live getters, so it can never drift out of sync with the actual setting:

```cpp
const char* scrollOpts[]   = {"Natural", "Classic"};
const char* tempOpts[]     = {"F", "C"};
const char* pressureOpts[] = {"hPa", "mmHg", "psi"};

static void applyScrollMode(uint8_t i)   { setScrollMode((ScrollMode)i); }
static void applyTempUnit(uint8_t i)     { setTemperatureUnit((TemperatureUnit)i); }
static void applyPressureUnit(uint8_t i) { setPressureUnit((PressureUnit)i); }

SettingRow rows[] = {
  {"Scroll Style",     scrollOpts,   2, (uint8_t)getScrollMode(),       applyScrollMode},
  {"Temperature Unit", tempOpts,     2, (uint8_t)getTemperatureUnit(),  applyTempUnit},
  {"Pressure Unit",    pressureOpts, 3, (uint8_t)getPressureUnit(),     applyPressureUnit},
};
```

`drawSettingsScreen()` loops over `rows[]`: draws the label, then draws `optionCount` buttons left-to-right (reusing the existing `SETTINGS_BUTTON_WIDTH/HEIGHT/GAP/X_OFFSET` constants and active/inactive styling), advancing Y by a new `SETTINGS_ROW_HEIGHT` constant (sized so all 3 rows fit comfortably within the ~390px content area — `SETTINGS_ROW_HEIGHT = 90`). Button X position for option `i` in a row is `rowX + i * (SETTINGS_BUTTON_WIDTH + SETTINGS_BUTTON_GAP)`, so row width naturally scales with `optionCount`.

## Touch hit-testing

`handleTouch()`'s Settings-screen branch builds the same `rows[]` table (or calls a shared static helper that returns it, so drawn geometry and hit-test geometry can never disagree — the same "must stay in sync" concern CLAUDE.md already flags for the drawer). Then:

```
for each row r in rows:
  if touchY falls within row r's vertical band:
    for each option i in r.options:
      if touchX falls within button i's horizontal bounds:
        r.onSelect(i)
        setRedrawFlag()
        return
```

The loop is fully generic — it doesn't need a switch on row identity, it just calls `rows[r].onSelect(i)`.

No change to drawer/hamburger/pan/zoom touch logic — this only replaces the current single Natural/Classic hit-test block.

## Wiring & boot sequence

- `weather_console.ino`: add `#include "settings_manager.h"`; call `loadSettings();` in `setup()`, before `drawStaticElements()`, so the first paint already reflects persisted values.
- `display_manager.cpp`: remove the file-local `ScrollMode`/`scrollMode` static. The pan-delta formula switches from reading `scrollMode` to calling `getScrollMode()`. `getViewUnit()` and the two new convert helpers call `getTemperatureUnit()`/`getPressureUnit()`.
- `settings_manager.cpp` includes `kvstore_global_api.h` directly.
- No changes to `data_manager.h`/`data_manager.cpp`.

## Code impact summary

- New: `settings_manager.h`, `settings_manager.cpp`.
- `display_manager.h`: add `SETTINGS_ROW_HEIGHT` constant.
- `display_manager.cpp`: remove file-local `ScrollMode` enum/static; rewrite `drawSettingsScreen()` and the Settings-screen branch of `handleTouch()` to be data-driven per above; add `convertTempForDisplay()`/`convertPressureForDisplay()`; update `getViewUnit()` to be unit-aware.
- `weather_console.ino`: add include + `loadSettings()` call in `setup()`.

## Out of scope

- A Humidity unit toggle (confirmed: % only, no common alternate unit).
- Any settings beyond these three.
- A "reset to defaults" button/action.
- Any change to how Temperature/Humidity/Pressure data is stored, sampled, or plotted — this feature only changes how numbers are formatted for display.
