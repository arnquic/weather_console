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
