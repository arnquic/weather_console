#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Arduino_GigaDisplay_GFX.h"
#include "Arduino_GigaDisplayTouch.h"

// Layout constants
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#define TITLE_HEIGHT 0       // Removed title bar
#define NAV_HEIGHT 50
#define GRAPH_MARGIN 20

// Button definitions
#define BUTTON_WIDTH 240
#define BUTTON_HEIGHT 40
#define BUTTON_MARGIN 10
#define BUTTON_Y 5           // Buttons now at top of screen

// Colors
#define COLOR_BACKGROUND 0x0000
#define COLOR_TITLE_BG 0x001F
#define COLOR_NAV_BG 0x4208
#define COLOR_BUTTON_BG 0x528A
#define COLOR_BUTTON_ACTIVE 0x001F
#define COLOR_TEMP_LINE 0xF800
#define COLOR_HUMIDITY_LINE 0x07E0
#define COLOR_PRESSURE_LINE 0x07FF
#define COLOR_TEXT 0xFFFF
#define COLOR_NAV_ACTIVE 0xFFE0

// View modes
enum ViewMode { VIEW_TEMP, VIEW_HUMIDITY, VIEW_PRESSURE };

// Function declarations
void initDisplay();
void drawStaticElements();
void drawNavBar();
void drawButton(int x, int y, const char* label, bool active, uint16_t accentColor);
void drawGraph();
void handleTouch();
ViewMode getCurrentView();
void setCurrentView(ViewMode view);
bool needsRedraw();
void clearRedrawFlag();
void setRedrawFlag();

#endif