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
