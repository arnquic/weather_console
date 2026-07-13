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

// Hamburger icon
#define ICON_MARGIN 10
#define ICON_SIZE 30
#define ICON_BAR_HEIGHT 4
#define ICON_BAR_GAP 4
#define ICON_Y 15
#define ICON_TAP_WIDTH 60

// Drawer
#define DRAWER_WIDTH 220
#define GEAR_ICON_RADIUS 10

// Settings screen
#define SETTINGS_BUTTON_WIDTH 150
#define SETTINGS_BUTTON_HEIGHT 50
#define SETTINGS_BUTTON_GAP 20
#define SETTINGS_BUTTON_Y_OFFSET 60
#define SETTINGS_BUTTON_X_OFFSET 20
#define SETTINGS_ROW_HEIGHT 120

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

// Screens
enum ScreenMode { SCREEN_GRAPH, SCREEN_SETTINGS };

// Function declarations
void initDisplay();
void drawStaticElements();
void drawNavBar();
void drawHamburgerIcon();
void drawCloseIcon();
void drawDrawer();
void drawSettingsScreen();
void drawGraph();
void handleTouch();
ViewMode getCurrentView();
void setCurrentView(ViewMode view);
ScreenMode getCurrentScreen();
bool needsRedraw();
void clearRedrawFlag();
void setRedrawFlag();

#endif