#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#define MAX_POINTS 100
#define MIN_ZOOM_POINTS 10

void initDataBuffers();
void addDataPoint(float tempValue, float humidityValue, float pressureValue);
void updateHumidityData(float value);
void updatePressureData(float value);
void updateMinMaxForCurrentView();
void getMinMaxValues(float* minVal, float* maxVal);
void getAverageValue(float* avg);
void getFullBufferAverages(float* tempAvg, float* humidityAvg, float* pressureAvg);

float* getTempData();
float* getHumidityData();
float* getPressureData();
int getDataCount();
int getCurrentIndex();
int getMaxPoints();

// Zoom and pan functions
void setZoomLevel(int points);
int getZoomLevel();
void setViewOffset(int offset);
int getViewOffset();
int getMaxViewOffset();
bool isFullyZoomedOut();  // New function

#endif