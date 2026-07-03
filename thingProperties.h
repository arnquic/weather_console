#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include <Arduino_NetworkConfigurator.h>
#include "configuratorAgents/agents/BLEAgent.h"
#include "configuratorAgents/agents/SerialAgent.h"
void onHumidityChange();
void onPressureChange();
void onTempChange();
void onLightAlphaChange();

float humidity;
float pressure;
CloudTemperatureSensor temp;
int light_alpha;

KVStore kvStore;
BLEAgentClass BLEAgent;
SerialAgentClass SerialAgent;
WiFiConnectionHandler ArduinoIoTPreferredConnection; 
NetworkConfiguratorClass NetworkConfigurator(ArduinoIoTPreferredConnection);

void initProperties(){
  NetworkConfigurator.addAgent(BLEAgent);
  NetworkConfigurator.addAgent(SerialAgent);
  NetworkConfigurator.setStorage(kvStore);
  // For changing the default reset pin uncomment and set your preferred pin. Use DISABLE_PIN for disabling the reset procedure.
  //NetworkConfigurator.setReconfigurePin(your_pin);
  ArduinoCloud.setConfigurator(NetworkConfigurator);

  ArduinoCloud.addProperty(humidity, READWRITE, ON_CHANGE, onHumidityChange);
  ArduinoCloud.addProperty(pressure, READWRITE, ON_CHANGE, onPressureChange);
  ArduinoCloud.addProperty(temp, READWRITE, ON_CHANGE, onTempChange);
  ArduinoCloud.addProperty(light_alpha, READWRITE, ON_CHANGE, onLightAlphaChange);

}
