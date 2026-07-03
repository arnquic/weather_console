#include "display_manager.h"
#include "data_manager.h"

GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touchDetector;

static ViewMode currentView = VIEW_TEMP;
static bool redrawNeeded = true;

// Touch gesture state
static bool gestureActive = false;
static int initialDistance = 0;
static int initialZoomLevel = 0;
static int initialPanX = 0;
static int initialOffset = 0;

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
    // Single touch - button handling OR pan
    lastSingleTouchTime = millis();  // Update single touch time
    
    // Only handle if gesture has timed out
    if(gestureActive && millis() - lastMultiTouchTime < 150) {
      lastContactCount = contacts;
      return;
    }
    
    // Transform coordinates for rotation
    int touchX = points[0].y;
    int touchY = 480 - points[0].x;
    
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
    
    // Not in graph area - check for button press
    if(!wasTouched && !panActive) {
      if(millis() - lastTouchTime < 200) {
        lastContactCount = contacts;
        return;
      }
      
      wasTouched = true;
      lastTouchTime = millis();
      
      Serial.print("Single touch at x: ");
      Serial.print(touchX);
      Serial.print(", y: ");
      Serial.println(touchY);
      
      if(touchY >= BUTTON_Y && touchY <= BUTTON_Y + BUTTON_HEIGHT) {
        ViewMode oldView = currentView;
        
        if(touchX >= BUTTON_MARGIN && touchX < BUTTON_MARGIN + BUTTON_WIDTH) {
          currentView = VIEW_TEMP;
          Serial.println("Temperature button pressed");
        }
        else if(touchX >= BUTTON_MARGIN * 2 + BUTTON_WIDTH && 
                touchX < BUTTON_MARGIN * 2 + BUTTON_WIDTH * 2) {
          currentView = VIEW_HUMIDITY;
          Serial.println("Humidity button pressed");
        }
        else if(touchX >= BUTTON_MARGIN * 3 + BUTTON_WIDTH * 2 && 
                touchX < BUTTON_MARGIN * 3 + BUTTON_WIDTH * 3) {
          currentView = VIEW_PRESSURE;
          Serial.println("Pressure button pressed");
        }
        
        if(oldView != currentView) {
          drawNavBar();
          updateMinMaxForCurrentView();
          redrawNeeded = true;
          
          Serial.print("Switched to view: ");
          Serial.println(currentView);
        }
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