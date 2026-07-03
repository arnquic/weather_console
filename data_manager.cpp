#include "data_manager.h"
#include "display_manager.h"

static float tempData[MAX_POINTS];
static float humidityData[MAX_POINTS];
static float pressureData[MAX_POINTS];
static int dataCount = 0;
static int currentIndex = 0;

static float minValue = 0;
static float maxValue = 100;

// Zoom and pan state
static int zoomLevel = MAX_POINTS;  // Number of points to show
static int viewOffset = 0;          // Offset from the latest data point
static bool fullyZoomedOut = true;  // Track if user is at max zoom

void initDataBuffers() {
  for(int i = 0; i < MAX_POINTS; i++) {
    tempData[i] = 0;
    humidityData[i] = 0;
    pressureData[i] = 0;
  }
  dataCount = 0;
  currentIndex = 0;
  zoomLevel = MAX_POINTS;
  viewOffset = 0;
  fullyZoomedOut = true;
}

void addDataPoint(float tempValue, float humidityValue, float pressureValue) {
  tempData[currentIndex] = tempValue;
  humidityData[currentIndex] = humidityValue;
  pressureData[currentIndex] = pressureValue;
  currentIndex = (currentIndex + 1) % MAX_POINTS;
  
  if(dataCount < MAX_POINTS) {
    dataCount++;
    
    // If fully zoomed out, adjust zoom level to match new data count
    if(fullyZoomedOut && viewOffset == 0) {
      zoomLevel = dataCount;
    }
  }
  
  // Reset view offset when new data arrives if we're at the "live" view
  if(viewOffset == 0) {
    updateMinMaxForCurrentView();
    setRedrawFlag();
  }
}

void updateHumidityData(float value) {
  int latestIdx = (currentIndex - 1 + MAX_POINTS) % MAX_POINTS;
  humidityData[latestIdx] = value;
  
  if(getCurrentView() == VIEW_HUMIDITY) {
    updateMinMaxForCurrentView();
    setRedrawFlag();
  }
}

void updatePressureData(float value) {
  int latestIdx = (currentIndex - 1 + MAX_POINTS) % MAX_POINTS;
  pressureData[latestIdx] = value;
  
  if(getCurrentView() == VIEW_PRESSURE) {
    updateMinMaxForCurrentView();
    setRedrawFlag();
  }
}

void updateMinMaxForCurrentView() {
  if(dataCount == 0) {
    return;
  }
  
  float* data;
  
  switch(getCurrentView()) {
    case VIEW_TEMP:
      data = tempData;
      break;
    case VIEW_HUMIDITY:
      data = humidityData;
      break;
    case VIEW_PRESSURE:
      data = pressureData;
      break;
  }
  
  // Calculate min/max for the visible window
  int pointsToCheck = min(zoomLevel, dataCount);
  int startIdx;
  
  if(dataCount <= MAX_POINTS) {
    // Buffer hasn't wrapped
    startIdx = max(0, dataCount - zoomLevel - viewOffset);
  } else {
    // Buffer has wrapped
    startIdx = (currentIndex - zoomLevel - viewOffset + MAX_POINTS) % MAX_POINTS;
  }
  
  minValue = data[startIdx];
  maxValue = data[startIdx];
  
  for(int i = 0; i < pointsToCheck; i++) {
    int idx = (startIdx + i) % MAX_POINTS;
    if(data[idx] < minValue) minValue = data[idx];
    if(data[idx] > maxValue) maxValue = data[idx];
  }
  
  float range = maxValue - minValue;
  if(range < 0.1) range = 10;
  minValue -= range * 0.1;
  maxValue += range * 0.1;
}

void getMinMaxValues(float* minVal, float* maxVal) {
  *minVal = minValue;
  *maxVal = maxValue;
}

void setZoomLevel(int points) {
  int maxZoom = min(MAX_POINTS, dataCount);
  zoomLevel = constrain(points, MIN_ZOOM_POINTS, maxZoom);
  
  // Check if we're fully zoomed out
  fullyZoomedOut = (zoomLevel >= maxZoom);
  
  // Adjust view offset if it's now out of bounds
  int maxOffset = getMaxViewOffset();
  if(viewOffset > maxOffset) {
    viewOffset = maxOffset;
  }
  
  updateMinMaxForCurrentView();
  setRedrawFlag();
}

int getZoomLevel() {
  return zoomLevel;
}

void setViewOffset(int offset) {
  int maxOffset = getMaxViewOffset();
  viewOffset = constrain(offset, 0, maxOffset);
  
  // If offset changes from 0, we're no longer at "live" view
  if(viewOffset > 0) {
    fullyZoomedOut = false;
  }
  
  updateMinMaxForCurrentView();
  setRedrawFlag();
}

int getViewOffset() {
  return viewOffset;
}

int getMaxViewOffset() {
  // Maximum offset is the number of points we can scroll back
  return max(0, dataCount - zoomLevel);
}

bool isFullyZoomedOut() {
  return fullyZoomedOut && viewOffset == 0;
}

float* getTempData() { return tempData; }
float* getHumidityData() { return humidityData; }
float* getPressureData() { return pressureData; }
int getDataCount() { return dataCount; }
int getCurrentIndex() { return currentIndex; }
int getMaxPoints() { return MAX_POINTS; }