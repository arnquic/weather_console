#include "display_manager.h"
#include "data_manager.h"

GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touchDetector;

static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;
static bool drawerOpen = false;
static ScreenMode currentScreen = SCREEN_GRAPH;

enum ScrollMode { SCROLL_NATURAL, SCROLL_CLASSIC };
static ScrollMode scrollMode = SCROLL_NATURAL;

// Touch gesture state
static bool gestureActive = false;
static int initialDistance = 0;
static int initialZoomLevel = 0;
static int initialPanX = 0;
static int initialOffset = 0;

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
    case VIEW_TEMP:     return "F";
    case VIEW_HUMIDITY: return "%";
    case VIEW_PRESSURE: return "hPa";
  }
  return "";
}

void initDisplay() {
  display.begin();
  display.setRotation(1);
  display.fillScreen(COLOR_BACKGROUND);
  touchDetector.begin();
}

ViewMode getCurrentView() {
  return currentView;
}

void setCurrentView(ViewMode view) {
  currentView = view;
}

ScreenMode getCurrentScreen() {
  return currentScreen;
}

bool needsRedraw() {
  return redrawNeeded;
}

void clearRedrawFlag() {
  redrawNeeded = false;
}

void setRedrawFlag() {
  redrawNeeded = true;
}

void drawStaticElements() {
  display.fillScreen(COLOR_BACKGROUND);
  
  // Draw navigation bar (no title bar anymore)
  drawNavBar();
  
  // Draw graph frame
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;
  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);
}

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

void drawHamburgerIcon() {
  for(int i = 0; i < 3; i++) {
    int barY = ICON_Y + i * (ICON_BAR_HEIGHT + ICON_BAR_GAP);
    display.fillRect(ICON_MARGIN, barY, ICON_SIZE, ICON_BAR_HEIGHT, COLOR_TEXT);
  }
}

void drawCloseIcon() {
  int iconHeight = 3 * ICON_BAR_HEIGHT + 2 * ICON_BAR_GAP;
  int x1 = ICON_MARGIN;
  int y1 = ICON_Y;
  int x2 = ICON_MARGIN + iconHeight;
  int y2 = ICON_Y + iconHeight;

  for(int offset = -1; offset <= 1; offset++) {
    display.drawLine(x1 + offset, y1, x2 + offset, y2, COLOR_TEXT);
    display.drawLine(x2 + offset, y1, x1 + offset, y2, COLOR_TEXT);
  }
}

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

void drawSettingsScreen() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;

  // Clear settings area, including the margin strip to the left of the
  // frame (the drawer paints all the way to x=0, wider than GRAPH_MARGIN).
  display.fillRect(0, graphY, GRAPH_MARGIN, graphHeight, COLOR_BACKGROUND);
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1,
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT);
  display.setCursor(GRAPH_MARGIN + 20, graphY + 20);
  display.print("Scroll Style");

  int buttonY = graphY + SETTINGS_BUTTON_Y_OFFSET;
  int naturalX = GRAPH_MARGIN + SETTINGS_BUTTON_X_OFFSET;
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

void drawGraph() {
  int graphY = TITLE_HEIGHT + NAV_HEIGHT + GRAPH_MARGIN;
  int graphHeight = SCREEN_HEIGHT - graphY - GRAPH_MARGIN;
  int graphWidth = SCREEN_WIDTH - 2 * GRAPH_MARGIN;
  
  // Clear graph area
  display.fillRect(GRAPH_MARGIN + 1, graphY + 1, 
                   graphWidth - 2, graphHeight - 2, COLOR_BACKGROUND);
  
  int dataCount = getDataCount();
  
  // If no data yet, show waiting message
  if(dataCount == 0) {
    display.setTextSize(2);
    display.setTextColor(COLOR_TEXT);
    display.setCursor(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2);
    display.print("Waiting for data...");
    return;
  }
  
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
  
  // Use zoom level instead of all data
  int zoomLevel = getZoomLevel();
  int viewOffset = getViewOffset();
  int pointsToPlot = min(zoomLevel, dataCount);
  
  float xStep = (float)graphWidth / (pointsToPlot - 1);
  if(pointsToPlot == 1) xStep = 0;
  
  float minValue, maxValue;
  getMinMaxValues(&minValue, &maxValue);
  float valueRange = maxValue - minValue;
  
  // Calculate starting index based on zoom and offset
  int startIdx;
  if(dataCount <= getMaxPoints()) {
    startIdx = max(0, dataCount - zoomLevel - viewOffset);
  } else {
    int currentIndex = getCurrentIndex();
    startIdx = (currentIndex - zoomLevel - viewOffset + getMaxPoints()) % getMaxPoints();
  }
  
  // Draw lines between points
  if(valueRange > 0.001 && pointsToPlot > 1) {
    for(int i = 1; i < pointsToPlot; i++) {
      int idx1 = (startIdx + i - 1) % getMaxPoints();
      int idx2 = (startIdx + i) % getMaxPoints();
      
      int x1 = GRAPH_MARGIN + (i - 1) * xStep;
      int x2 = GRAPH_MARGIN + i * xStep;
      
      int y1 = graphY + graphHeight - 
               ((data[idx1] - minValue) / valueRange * (graphHeight - 2));
      int y2 = graphY + graphHeight - 
               ((data[idx2] - minValue) / valueRange * (graphHeight - 2));
      
      y1 = constrain(y1, graphY + 1, graphY + graphHeight - 1);
      y2 = constrain(y2, graphY + 1, graphY + graphHeight - 1);
      
      display.drawLine(x1, y1, x2, y2, lineColor);
    }
  } else if(pointsToPlot == 1) {
    int idx = startIdx;
    int x = GRAPH_MARGIN;
    int y = graphY + graphHeight - 
            ((data[idx] - minValue) / (valueRange > 0.001 ? valueRange : 1) * (graphHeight - 2));
    y = constrain(y, graphY + 1, graphY + graphHeight - 1);
    display.fillCircle(x, y, 3, lineColor);
  }

  // Restore graph border in case a line/point touched the edge pixels
  display.drawRect(GRAPH_MARGIN, graphY, graphWidth, graphHeight, COLOR_TEXT);

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
  
  // Show zoom indicator if zoomed
  if(zoomLevel < dataCount) {
    display.setTextSize(1);
    display.setTextColor(COLOR_TEXT);
    display.setCursor(SCREEN_WIDTH - 100, graphY + 10);
    display.print("Zoom: ");
    display.print(pointsToPlot);
    display.print("/");
    display.print(dataCount);
  }
}

int calculateDistance(int x1, int y1, int x2, int y2) {
  int dx = x2 - x1;
  int dy = y2 - y1;
  return sqrt(dx * dx + dy * dy);
}

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