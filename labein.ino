#include <EEPROM.h>
#include <Arduino.h>
#include <ESP8266WiFiMulti.h> 
#include "SinricPro.h"
#include "Aquecedor.h"
#include "Umidificador.h"
#include "SinricProTemperaturesensor.h"
#include "DHT.h"
#include "ThingSpeak.h"
#include "Secrets.h"

#define BAUD_RATE         9600  // Not using serial!
#define EVENT_WAIT_TIME   30000              

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 2    
#define DHTTYPE    DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);    

ESP8266WiFiMulti wifiMulti;
WebSocketsClient webSocket;
WiFiClient  client;
const uint32_t connectTimeoutMs = 10000;

Aquecedor &aquecedor = SinricPro[DEVICE_ID];
Umidificador &umidificador = SinricPro[UMIDIF_ID];

bool deviceIsOn = true;                       // Temeprature sensor on/off state
float temperature;                            // actual temperature
float humidity;                               // actual humidity
float lastTemperature;                        // last known temperature (for compare)
float lastHumidity;                           // last known humidity (for compare)
unsigned long lastEvent = (-EVENT_WAIT_TIME); // last time event has been sent
float tempOffset = 0.5;
float umiOffset = 0;
bool internetOn;

// ToggleController
std::map<String, bool> globalToggleStates;

// RangeController
std::map<String, int> globalRangeValues;

// ModeController
std::map<String, String> globalModes;

// RelayStatus
std::map<String, bool> releStatus;

bool globalPowerState;
unsigned int tempo;

bool rele1, rele2, tog1, tog2, failSensor;
int tem, umi;


void rele(const String& idDev, bool stats) {
  if (idDev == UMIDIF_ID) {
    if (!stats) {
      digitalWrite(0, HIGH);
    }
    else {
      digitalWrite(0, LOW);
    }
    releStatus[UMIDIF_ID] = stats;
    EEPROM.write(2, stats);
    EEPROM.commit();
  }

  if (idDev == DEVICE_ID) {
    if (!stats) {
      digitalWrite(3, HIGH);
    }
    else {
      digitalWrite(3, LOW);
    }
    releStatus[DEVICE_ID] = stats;
    EEPROM.write(1, stats);
    EEPROM.commit();
  }
}

void analiseReles(bool onLine) {
  if(!onLine) {
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    temperature = event.temperature;
    dht.humidity().getEvent(&event);
    humidity = event.relative_humidity;

    if (isnan(temperature) || isnan(humidity)) {
      failSensor = true;
      return;
    }
    else {
      failSensor = false; 
    }
    temperature += tempOffset;
    humidity += umiOffset;
    if(globalToggleStates["toggleAquecedor"] == false || globalToggleStates["toggleUmidificador"] == false) {
      rele(UMIDIF_ID, false);
      rele(DEVICE_ID, false);
    }
  }

  
  if (globalToggleStates["toggleAquecedor"] == true & failSensor == false){
    if (releStatus[DEVICE_ID] == false){
      if (temperature - globalRangeValues["rangeAquecedor"] <= -0.2){
        rele(DEVICE_ID, true);
        if(onLine) {
          aquecedor.sendPowerStateEvent(true);
        }
      }
    }
    if (releStatus[DEVICE_ID] == true) {
      if (temperature - globalRangeValues["rangeAquecedor"] >= 0.2){
        rele(DEVICE_ID, false);
        if(onLine) {
          aquecedor.sendPowerStateEvent(false);
        }
      }
    }
    
  }
  if (globalToggleStates["toggleUmidificador"] == true & failSensor == false){
    if (releStatus[UMIDIF_ID] == false){
      if (humidity - globalRangeValues["rangeUmidificador"] <= -0.5){
        rele(UMIDIF_ID, true);
        if(onLine) {
          umidificador.sendPowerStateEvent(true);
        }
      }
    }
    if (releStatus[UMIDIF_ID] == true) {
      if (humidity - globalRangeValues["rangeUmidificador"] >= 0.5){
        rele(UMIDIF_ID, false);
        if(onLine) {
          umidificador.sendPowerStateEvent(false);
        }
      }
    } 
  }
  
  if(failSensor == true) {
    rele(UMIDIF_ID, false);
    rele(DEVICE_ID, false);
  }

}


// ToggleController
bool onToggleState(const String& deviceId, const String& instance, bool state) {
  globalToggleStates[instance] = state;
  if (instance == "toggleAquecedor"){
    EEPROM.write(3, state);
    EEPROM.commit();
  }
  if (instance == "toggleUmidificador"){
    EEPROM.write(4, state);
    EEPROM.commit();
  }
  return true;
}

// RangeController
bool onRangeValue(const String &deviceId, const String& instance, int &rangeValue) {
  globalRangeValues[instance] = rangeValue;
  if (deviceId == DEVICE_ID) {
    updateToggleState("toggleAquecedor", true);
    delay(1000);
    onToggleState(DEVICE_ID, "toggleAquecedor", true);
    EEPROM.write(5, rangeValue);
    EEPROM.commit();
  }
  if (deviceId == UMIDIF_ID) {
    updateToggleState("toggleUmidificador", true);
    delay(1000);
    onToggleState(DEVICE_ID, "toggleUmidificador", true);
    EEPROM.write(6, rangeValue);
    EEPROM.commit();
  }
  return true;
}

bool onAdjustRangeValue(const String &deviceId, const String& instance, int &valueDelta) {
  globalRangeValues[instance] += valueDelta;
  globalRangeValues[instance] = valueDelta;
  return true;
}

// ModeController
bool onSetMode(const String& deviceId, const String& instance, String &mode) {
  globalModes[instance] = mode;
  return true;
}

// RangeController
void updateRangeValue(String instance, int value) {
  aquecedor.sendRangeValueEvent(instance, value);
  umidificador.sendRangeValueEvent(instance, value);
}

bool onPowerState(const String &deviceId, bool &state) {
  rele(deviceId, state);
  globalPowerState = state;
  releStatus[deviceId] = state; 
  return true;
}

// ToggleController
void updateToggleState(String instance, bool state) {
  aquecedor.sendToggleStateEvent(instance, state);
  umidificador.sendToggleStateEvent(instance, state);
  if (instance == "toggleAquecedor"){
    EEPROM.write(3, state);
    EEPROM.commit();
  }
  if (instance == "toggleUmidificador"){
    EEPROM.write(4, state);
    EEPROM.commit();
  }
}

void sendPushNotification(String instance, String notification) {
  if(instance == "aquecedor") {
    aquecedor.sendPushNotification(notification);
  }
  if(instance == "umidificador") {
    umidificador.sendPushNotification(notification);
  }
}

// ModeController
void updateMode(String instance, String mode) {
  aquecedor.sendModeEvent(instance, mode, "PHYSICAL_INTERACTION");
  umidificador.sendModeEvent(instance, mode, "PHYSICAL_INTERACTION");
}

void updatePowerState(bool state) {
  aquecedor.sendPowerStateEvent(state);
  umidificador.sendPowerStateEvent(state);
}

void handleTemperaturesensor() {
  if (deviceIsOn == false) return;
  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) return;
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  temperature = event.temperature;
  dht.humidity().getEvent(&event);
  humidity = event.relative_humidity;

  if (isnan(temperature) || isnan(humidity)) {
    failSensor = true;
    return;
  }
  else {
    failSensor = false; 
  }

  temperature += tempOffset;
  humidity += umiOffset;
  if (temperature == lastTemperature & humidity == lastHumidity) return; // if no values changed do nothing...
  lastTemperature = temperature;  // save actual temperature for next compare
  lastHumidity = humidity;        // save actual humidity for next compare
  lastEvent = actualMillis;       // save actual time for next compare
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];  // get temperaturesensor device
  bool success = mySensor.sendTemperatureEvent(temperature, humidity); // send event
  if (success) {
    atualizaThingSpeak();
  }
}


void setupWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  wifiMulti.addAP(ssid1, pass1);
  wifiMulti.addAP(ssid2, pass2);
  wifiMulti.addAP(ssid3, pass3);
  wifiMulti.addAP(ssid4, pass4);

  tempo = millis();
  while (wifiMulti.run(connectTimeoutMs) != WL_CONNECTED & millis()-tempo < 28000) {
    //Serial.printf(".");
    delay(250);
  }
}

// setup function for SinricPro
void setupSinricPro() {

  umidificador.onPowerState(onPowerState);
  aquecedor.onPowerState(onPowerState);
  aquecedor.onSetMode("modeAquecedor", onSetMode);
  umidificador.onSetMode("modeUmidificador", onSetMode);
  umidificador.onRangeValue("rangeUmidificador", onRangeValue);
  umidificador.onAdjustRangeValue("rangeUmidificador", onAdjustRangeValue);
  aquecedor.onToggleState("toggleAquecedor", onToggleState);
  umidificador.onToggleState("toggleUmidificador", onToggleState);
  aquecedor.onRangeValue("rangeAquecedor", onRangeValue);
  aquecedor.onAdjustRangeValue("rangeAquecedor", onAdjustRangeValue);
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  mySensor.onPowerState(onPowerState);

  // setup SinricPro
  SinricPro.onConnected([](){}); 
  SinricPro.onDisconnected([](){});
  SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
  SinricPro.begin(APP_KEY, APP_SECRET);  
}

void setup() {
  pinMode(0, OUTPUT);
  pinMode(3, OUTPUT);
  rele(DEVICE_ID, false);
  rele(UMIDIF_ID, false);
  dht.begin();
  
  EEPROM.begin(6); 

  rele1 = EEPROM.read(1);
  rele2 = EEPROM.read(2);
  tog1 = EEPROM.read(3);
  tog2 = EEPROM.read(4);
  tem = EEPROM.read(5);
  umi = EEPROM.read(6);

  setupWiFi();

  rele(DEVICE_ID, rele1);
  rele(UMIDIF_ID, rele2);
  delay(1000);

  globalToggleStates["toggleAquecedor"] = tog1;
  delay(1000);
  globalToggleStates["toggleUmidificador"] = tog2;
  delay(1000);
  globalRangeValues["rangeUmidificador"] = umi;
  delay(1000);
  globalRangeValues["rangeAquecedor"] = tem;
  delay(1000);

  while (WiFi.status() != WL_CONNECTED) {
    internetOn = false;
    analiseReles(false);
    setupWiFi();
    delay(2000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    internetOn = true;
    setupSinricPro();
    delay(1000);
    aquecedor.sendRangeValueEvent("rangeAquecedor", tem);
    delay(1000);
    umidificador.sendRangeValueEvent("rangeUmidificador", umi);
    delay(1000);
    aquecedor.sendPowerStateEvent(rele1);
    delay(1000);
    umidificador.sendPowerStateEvent(rele2);
    delay(1000);
    updateToggleState("toggleAquecedor", tog1);
    delay(1000);
    updateToggleState("toggleUmidificador", tog2);
    delay(1000);
    aquecedor.sendToggleStateEvent("toggleAquecedor", tog1);
    delay(1000);
    umidificador.sendToggleStateEvent("toggleUmidificador", tog2);
    ThingSpeak.begin(client);
  }
  tempo = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    semCon();
  }
  else {
    SinricPro.handle();
    handleTemperaturesensor();
    analiseReles(true);

    if(millis()-tempo > 300000) {
      if (humidity - globalRangeValues["rangeUmidificador"] > 5 || humidity - globalRangeValues["rangeUmidificador"] < -4) {
        sendPushNotification("umidificador", "Umidade fora do intervalo! " + String(humidity) + "%");
    }
      delay(500);
      if (temperature - globalRangeValues["rangeAquecedor"] < -2 || temperature - globalRangeValues["rangeAquecedor"] > 2) {
        sendPushNotification("aquecedor", "Temperatura fora do intervalo! " + String(temperature) + " C");
      }
      tempo = millis();
    }
  }
}


void semCon() {
  while(WiFi.status() != WL_CONNECTED) {
    analiseReles(false);
    setupWiFi();
    delay(2000);
  }
  setupSinricPro();
  return;
}


void atualizaThingSpeak(){
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  ThingSpeak.setField(3, releStatus[DEVICE_ID]);
  ThingSpeak.setField(4, releStatus[UMIDIF_ID]);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x != 200) {
    internetOn = false;
  }
  else {
    internetOn = true;
  }
}
