#include <Arduino.h>
//#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProTemperaturesensor.h"
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "Classes.h"
#include "Secrets.h"
//#include "Reset.h"
#include "Definitions.h"
#include <EEPROM.h>
#include <IRremote.hpp>
#include "ThingSpeak.h"




//const uint8_t ir20degree[67] = {180,84, 15,11, 13,12, 12,11, 13,11, 13,11, 12,11, 13,33, 13,33, 13,32, 13,11, 13,11, 13,12, 12,32, 13,33, 13,12, 12,11, 13,11, 13,32, 13,33, 13,11, 13,32, 13,12, 12,11, 13,12, 12,12, 12,11, 13,11, 13,32, 13,33, 13,32, 13,33, 13,33, 13};


Aquecedor &aquecedor = SinricPro[AQUECEDOR_ID];
Umidificador &umidificador = SinricPro[UMIDIFICADOR_ID];

//ESP8266WiFiMulti wifiMulti;
//WebSocketsClient webSocket;
WiFiClient  client;
const uint32_t connectTimeoutMs = 7000;

#define DHTPIN           sensorPin
   
#define DHTTYPE    DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);                                      // DHT sensor


bool deviceIsOn;                            // Temeprature sensor on/off state
float temperature;                            // actual temperature
float humidity;                               // actual humidity
float lastTemperature;                        // last known temperature (for compare)
float lastHumidity;                           // last known humidity (for compare)
unsigned long lastEvent = (-EVENT_WAIT_TIME); // last time event has been sent
//char  rst;
bool  irSignal;
unsigned int irDelay = 0;
unsigned int runing;
int irCom = 1;
//char rst;
bool wifiCom;



bool tempAnFlag = false;
bool humiAnFlag = false;

Dispositivo Heater(heaterPin, AQUECEDOR_ID, false);
Dispositivo Humidifier(humidifierPin, UMIDIFICADOR_ID, false);


// RangeController
std::map<String, int> globalRangeValues;

// PowerStateController
bool globalPowerState;

// ToggleController
std::map<String, bool> globalToggleStates;

std::map<String, bool> devicePowerState;
std::map<String, bool> relayPowerState;

std::map<String, int> eeprAdr;


int* tempTarget = &globalRangeValues["rangeAquecedor"];
int* umidTarget = &globalRangeValues["rangeUmidificador"];



/*************
 * Callbacks *
 *************/

// RangeController
bool onRangeValue(const String &deviceId, const String& instance, int &rangeValue) {
  //Serial.printf("[Device: %s]: Value for \"%s\" changed to %d\r\n", deviceId.c_str(), instance.c_str(), rangeValue);
  wrEeprom(eeprAdr[instance], rangeValue);
  globalRangeValues[instance] = rangeValue;
  return true;
}

bool onAdjustRangeValue(const String &deviceId, const String& instance, int &valueDelta) {
  globalRangeValues[instance] += valueDelta;
  //Serial.printf("[Device: %s]: Value for \"%s\" changed about %d to %d\r\n", deviceId.c_str(), instance.c_str(), valueDelta, globalRangeValues[instance]);
  globalRangeValues[instance] = valueDelta;
  return true;
}

// ToggleController
bool onToggleState(const String& deviceId, const String& instance, bool &state) {
  //Serial.printf("[Device: %s]: State for \"%s\" set to %s\r\n", deviceId.c_str(), instance.c_str(), state ? "on" : "off");
  globalToggleStates[instance] = state;
  wrEeprom(eeprAdr[instance], state);
  return true;
}

bool onPowerState(const String &deviceId, bool &state) {
  globalPowerState = state;
  devicePowerState[deviceId] = state;
  return true;
}




void setup() {

  ESP.wdtDisable();
  ESP.wdtFeed();
  EEPROM.begin(32);
  runing = millis();
  eeprAdr["Aquecedor"] = 0;
  eeprAdr["Umidificador"] = 1;
  eeprAdr["toggleAquecedor"] = 2;
  eeprAdr["toggleUmidificador"] = 3;
  eeprAdr["rangeAquecedor"] = 4;
  eeprAdr["rangeUmidificador"] = 5;
  pinMode(0, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(1, OUTPUT);
  dht.begin();
  IrSender.begin();
  ESP.wdtFeed();
  setupWiFi();
  ESP.wdtFeed();
  setupSinricPro();
  ESP.wdtFeed();
  ThingSpeak.begin(client);

  initEeprom();
  ESP.wdtFeed();
  //aquecedor.sendPowerStateEvent(Heater.swRelay(false));
  //umidificador.sendPowerStateEvent(Humidifier.swRelay(false));
  digitalWrite(0, HIGH);
  digitalWrite(3, HIGH);
  ESP.wdtFeed();
  relayPowerState[AQUECEDOR_ID] = false;
  relayPowerState[UMIDIFICADOR_ID] = false;
  rstNotif();
  
}

void loop() {
  ESP.wdtFeed();
  if (wifiCom) {
    SinricPro.handle();
    if(millis() - lastEvent < EVENT_WAIT_TIME+30000) atualizaThingSpeak();
  }
  else {
    wifiCom = setupWiFi();
  }
  
  handleTemperaturesensor();
  if(tempAnFlag) tempAnalyze();
  if(humiAnFlag) humiAnalyze();
  swHandle();
  //if(rstFlag){
  //  if (millis()<120000 & millis()>60000) rstNotif(rst_Reason());
  //}
  if(irSignal & (millis()-irDelay)>2000) humidifierON(irCom);
  
}


void swHandle(){
  if (devicePowerState[AQUECEDOR_ID]!=relayPowerState[AQUECEDOR_ID]){
    relayPowerState[AQUECEDOR_ID] = Heater.swRelay(devicePowerState[AQUECEDOR_ID]);
    wrEeprom(eeprAdr["Aquecedor"], devicePowerState[AQUECEDOR_ID]);
    aquecedor.sendPowerStateEvent(relayPowerState[AQUECEDOR_ID]);
  }

  if (devicePowerState[UMIDIFICADOR_ID]!=relayPowerState[UMIDIFICADOR_ID]){
    relayPowerState[UMIDIFICADOR_ID] = Humidifier.swRelay(devicePowerState[UMIDIFICADOR_ID]);
    irSignal = relayPowerState[UMIDIFICADOR_ID];
    if(irSignal) irDelay = millis();
    wrEeprom(eeprAdr["Umidificador"], devicePowerState[UMIDIFICADOR_ID]);
    umidificador.sendPowerStateEvent(relayPowerState[UMIDIFICADOR_ID]);
  }
}

void tempAnalyze() {
  float target = globalRangeValues["rangeAquecedor"];
  if (!relayPowerState[AQUECEDOR_ID]) {
      devicePowerState[AQUECEDOR_ID] = (temperature<=(target-tempHist));
      }
  if (relayPowerState[AQUECEDOR_ID]) {
      devicePowerState[AQUECEDOR_ID] = !(temperature>=(target+tempHist));
  }
  tempAnFlag = false;
}

void humiAnalyze() {
  float target = globalRangeValues["rangeUmidificador"];
  if (!relayPowerState[UMIDIFICADOR_ID]) {
      devicePowerState[UMIDIFICADOR_ID] = (humidity<=(target-umidHist));
      }
  if (relayPowerState[UMIDIFICADOR_ID]) {
      devicePowerState[UMIDIFICADOR_ID] = !(humidity>=(target+umidHist));
  }
  humiAnFlag = false;
}




//---------------Configurações Sinric Pro----------------//

//void setupWiFi() {
//  WiFi.persistent(false);
//  WiFi.mode(WIFI_STA);
//
//  wifiMulti.addAP(ssid1, pass1);
//  wifiMulti.addAP(ssid2, pass2);
//  wifiMulti.addAP(ssid3, pass3);
//  wifiMulti.addAP(ssid4, pass4);
//
//  int tempo = millis();
//  while (wifiMulti.run(connectTimeoutMs) != WL_CONNECTED & millis()-tempo < 15000) {
//    delay(1000);
//    ESP.wdtFeed();
//  }
//
//  if (WiFi.status() == WL_CONNECTED) {
//    bool wifiNotif = true;
//  }
//  
//}

bool setupWiFi() {
  bool wifi;
  //WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  //wifiMulti.addAP(ssid1, pass1);
  //wifiMulti.addAP(ssid2, pass2);
  //wifiMulti.addAP(ssid3, pass3);
  //wifiMulti.addAP(ssid4, pass4);

  WiFi.begin(ssid3, pass3);
  unsigned int tempo = 16;
  while (WiFi.status() != WL_CONNECTED && tempo != 0) {
    //wifi = true;
    ESP.wdtFeed();
    --tempo;
    delay(1000);
  }

  if(WiFi.status() == WL_CONNECTED) {
    wifi = true;
  }
  else {
    wifi = false;
  }
  return wifi;

}

//-------------------DHT Handle Sinric Pro---------------------//
void handleTemperaturesensor() {
  //if (deviceIsOn == false) return; // device is off...do nothing

  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) return; //only check every EVENT_WAIT_TIME milliseconds
  //aquecedor.sendPushNotification("Temperatura selecionada: "+ String(globalRangeValues["rangeAquecedor"]) + "°C.");
  //umidificador.sendPushNotification("Umidade selecionada: "+ String(globalRangeValues["rangeUmidificador"]) + "%.");
  //aquecedor.sendPushNotification("Valor na memória EEPROM: "+String(rdEeprom(4)));
//  if (millis()-runing >= 86400000) {
//    ESP.restart();
//  }

  sensors_event_t event;
  dht.temperature().getEvent(&event);
  temperature = event.temperature + tempOffset;          // get actual temperature in °C
  dht.humidity().getEvent(&event);
  humidity = event.relative_humidity + umidOffset;                // get actual humidity

  if (isnan(temperature) || isnan(humidity)) { // reading failed... 
    devicePowerState[AQUECEDOR_ID] = false;
    devicePowerState[UMIDIFICADOR_ID] = false;
    aquecedor.sendPushNotification("FALHA NO SENSOR DHT!");
    return;
  }

  if (temperature == lastTemperature & humidity == lastHumidity) return; // if no values changed do nothing...
  if (globalToggleStates["toggleAquecedor"]) tempAnFlag = true;
  if (globalToggleStates["toggleUmidificador"]) humiAnFlag = true;
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];  // get temperaturesensor device
  bool success = mySensor.sendTemperatureEvent(temperature, humidity); // send event
  if (success) {  // if event was sent successfuly, print temperature and humidity to serial
    
  } else {  // if sending event failed, print error message
    
  }

  if(temperature < 25 | temperature > 29 | humidity < 70 | humidity > 80) {
    alerta();
  }

  lastTemperature = temperature;  // save actual temperature for next compare
  lastHumidity = humidity;        // save actual humidity for next compare
  lastEvent = actualMillis;       // save actual time for next compare
}
//--------------------------------------------------------------//


void setupSinricPro() {
  // add device to SinricPro
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  mySensor.onPowerState(onPowerState);


    // RangeController
  aquecedor.onRangeValue("rangeAquecedor", onRangeValue);
  aquecedor.onAdjustRangeValue("rangeAquecedor", onAdjustRangeValue);

  umidificador.onRangeValue("rangeUmidificador", onRangeValue);
  umidificador.onAdjustRangeValue("rangeUmidificador", onAdjustRangeValue);


  // PowerStateController
  aquecedor.onPowerState(onPowerState);

  // ToggleController
  aquecedor.onToggleState("toggleAquecedor", onToggleState);


    // PowerStateController
  umidificador.onPowerState(onPowerState);

  // ToggleController
  umidificador.onToggleState("toggleUmidificador", onToggleState);

  // setup SinricPro
  //SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  //SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  //SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
     
  SinricPro.begin(APP_KEY, APP_SECRET);  
}






/**********
 * Events *
 *************************************************
 * Examples how to update the server status when *
 * you physically interact with your device or a *
 * sensor reading changes.                       *
 *************************************************/

// RangeController
void updateRangeValue(String instance, int value) {
  aquecedor.sendRangeValueEvent(instance, value);
  umidificador.sendRangeValueEvent(instance, value);
}

// PowerStateController
void updatePowerState(bool state) {
  aquecedor.sendPowerStateEvent(state);
  umidificador.sendPowerStateEvent(state);
  //devicePowerState[AQUECEDOR_ID] = state;
  //devicePowerState[UMIDIFICADOR_ID] = state;
}

// ToggleController
void updateToggleState(String instance, bool state) {
  aquecedor.sendToggleStateEvent(instance, state);
  umidificador.sendToggleStateEvent(instance, state);
}

// PushNotificationController
void sendPushNotification(String notification) {
  aquecedor.sendPushNotification(notification);
  umidificador.sendPushNotification(notification);
}

bool initEeprom() {
  devicePowerState[AQUECEDOR_ID] = rdEeprom(eeprAdr["Aquecedor"]);
  devicePowerState[UMIDIFICADOR_ID] = rdEeprom(eeprAdr["Umidificador"]);
  globalToggleStates["toggleAquecedor"] = rdEeprom(eeprAdr["toggleAquecedor"]);
  globalToggleStates["toggleUmidificador"] = rdEeprom(eeprAdr["toggleUmidificador"]);
  globalRangeValues["rangeAquecedor"] = rdEeprom(eeprAdr["rangeAquecedor"]);
  globalRangeValues["rangeUmidificador"] = rdEeprom(eeprAdr["rangeUmidificador"]);
  aquecedor.sendRangeValueEvent("rangeAquecedor", globalRangeValues["rangeAquecedor"]);
  delay(100);
  umidificador.sendRangeValueEvent("rangeUmidificador", globalRangeValues["rangeUmidificador"]);
  delay(100);
  aquecedor.sendToggleStateEvent("toggleAquecedor", globalToggleStates["toggleAquecedor"]);
  delay(100);
  umidificador.sendToggleStateEvent("toggleUmidificador", globalToggleStates["toggleUmidificador"]);
  delay(100);
  aquecedor.sendPowerStateEvent(devicePowerState[AQUECEDOR_ID]);
  delay(100);
  umidificador.sendPowerStateEvent(devicePowerState[UMIDIFICADOR_ID]);
  return true;
}



float rdEeprom(int address) {
  byte value = EEPROM.read(address);
  return (value);
}

void wrEeprom(int address, int value) {
  EEPROM.write(address, value);
  EEPROM.commit();
}



void humidifierON(int command) {
  switch(command){
    case 1:{
      irDelay = millis();
      sendIrSignal(1);
      irCom = 2;
      break;
    }
    case 2: {
      irDelay = millis();
      sendIrSignal(2);
      irCom = 3;
      break;
    }
    case 3: {
      sendIrSignal(3);
      irCom = 1;
      irSignal = false;
      break;
    }
  }
  
}

void sendIrSignal(int command) {
  //uint16_t ir22degree[67] = {8980,4220, 730,570, 630,620, 580,570, 630,570, 630,570, 580,570, 630,1670, 630,1670, 630,1620, 630,570, 630,570, 630,620, 580,1620, 630,1670, 630,620, 580,570, 630,570, 630,1620, 630,1670, 630,570, 630,1620, 630,620, 580,570, 630,620, 580,620, 580,570, 630,570, 630,1620, 630,1670, 630,1620, 630,1670, 630,1670, 630};
  //uint16_t ir20degree[67] = {8880,4420, 580,670, 530,670, 480,670, 530,670, 530,670, 530,670, 530,1720, 530,1720, 580,1720, 580,620, 530,670, 530,670, 530,1720, 580,1720, 530,670, 530,670, 530,670, 530,670, 530,1720, 530,670, 530,1720, 580,670, 530,670, 530,670, 480,670, 530,1720, 580,670, 530,1720, 530,1720, 580,1720, 580,1720, 530,1720, 580};
  //uint16_t ir21degree[67] = {9130,4320, 630,620, 580,570, 630,620, 580,570, 630,570, 580,620, 580,1670, 580,1720, 580,1670, 630,620, 580,570, 630,570, 630,1620, 630,1670, 580,670, 580,570, 630,1620, 630,620, 580,1670, 580,620, 630,1620, 630,620, 580,570, 630,570, 630,1670, 580,620, 580,620, 580,1670, 580,1720, 580,1620, 680,1670, 580,1670, 630};

  uint16_t irPower[67] = {8880,4520, 530,1720, 530,570, 530,620, 530,570, 530,620, 530,570, 530,620, 530,570, 530,620, 530,1720, 530,1720, 530,1670, 530,1720, 530,1720, 530,1720, 530,1720, 530,1670, 580,570, 530,1720, 530,1720, 530,1670, 580,570, 530,620, 530,570, 530,620, 530,1670, 530,620, 530,570, 530,620, 530,1720, 530,1670, 580,1670, 530};  // Protocol=NEC Address=0x1 Command=0x1D Raw-Data=0xE21DFE01 32 bits LSB first
  uint16_t irOscil[67] = {8930,4470, 580,1670, 530,620, 530,570, 530,620, 530,570, 580,570, 530,570, 580,570, 530,570, 580,1670, 530,1720, 530,1720, 530,1720, 530,1720, 530,1670, 580,1670, 530,620, 530,570, 580,570, 530,1720, 530,1720, 530,570, 530,620, 530,570, 530,1720, 530,1720, 530,1720, 530,570, 530,620, 530,1670, 580,1670, 530,1720, 530};  // Protocol=NEC Address=0x1 Command=0x18 Raw-Data=0xE718FE01 32 bits LSB first
  //uint16_t irBrisa[67] = {8930,4470, 580,1670, 530,570, 580,570, 530,570, 580,570, 530,570, 580,570, 530,570, 580,570, 530,1720, 530,1670, 580,1670, 580,1670, 580,1670, 580,1670, 530,1670, 580,1670, 580,570, 580,1570, 630,570, 580,570, 530,570, 580,570, 530,570, 580,570, 530,1720, 530,570, 580,1670, 580,1670, 530,1720, 530,1670, 580,1670, 580};  // Protocol=NEC Address=0x1 Command=0x5 Raw-Data=0xFA05FE01 32 bits LSB first
  uint16_t irClima[67] = {8930,4470, 580,1670, 530,570, 580,570, 530,570, 530,620, 530,570, 580,570, 530,570, 580,570, 530,1720, 530,1670, 580,1670, 530,1720, 530,1720, 580,1670, 530,1670, 580,570, 580,1670, 580,520, 580,1670, 530,620, 530,570, 580,570, 530,570, 580,1670, 580,570, 530,1670, 580,570, 580,1670, 530,1720, 530,1670, 580,1670, 580};  // Protocol=NEC Address=0x1 Command=0xA Raw-Data=0xF50AFE01 32 bits LSB first
  //uint16_t irVeloc[67] = {8980,4470, 580,1620, 630,520, 580,520, 630,520, 580,570, 580,520, 580,570, 580,520, 580,570, 580,1620, 630,1620, 630,1620, 580,1670, 580,1670, 580,1620, 630,1620, 630,520, 580,1670, 580,520, 580,1670, 580,1670, 580,520, 630,520, 580,520, 630,1620, 580,570, 580,1670, 580,520, 580,570, 580,1620, 630,1620, 580,1670, 580};  // Protocol=NEC Address=0x1 Command=0x1A Raw-Data=0xE51AFE01 32 bits LSB first
  //uint16_t irTimer[67] = {8930,4470, 580,1670, 580,520, 580,570, 580,520, 580,570, 530,570, 580,570, 530,570, 580,570, 530,1670, 580,1670, 630,1620, 580,1670, 580,1670, 580,1670, 580,1620, 630,1620, 580,570, 580,1670, 530,1670, 580,570, 580,570, 530,570, 580,570, 580,520, 580,1670, 580,520, 580,570, 580,1670, 580,1670, 580,1620, 580,1670, 580};  // Protocol=NEC Address=0x1 Command=0xD Raw-Data=0xF20DFE01 32 bits LSB first
  //uint16_t irModo[67] = {8930,4470, 580,1620, 630,520, 580,570, 580,520, 580,570, 580,520, 580,570, 580,520, 580,570, 580,1670, 580,1620, 630,1620, 580,1670, 580,1670, 580,1670, 530,1670, 580,570, 580,520, 580,570, 580,1670, 580,520, 580,570, 580,520, 580,570, 580,1670, 530,1670, 580,1670, 580,570, 580,1670, 530,1670, 580,1620, 580,1720, 530};  // Protocol=NEC Address=0x1 Command=0x8 Raw-Data=0xF708FE01 32 bits LSB first




  switch (command){
    case 1:{
      IrSender.sendRaw(irPower, sizeof(irPower) / sizeof(irPower[0]), NEC_KHZ);
      break;
    }
    case 2: {
      IrSender.sendRaw(irOscil, sizeof(irOscil) / sizeof(irOscil[0]), NEC_KHZ);
      break;
    }
    case 3: {
      IrSender.sendRaw(irClima, sizeof(irClima) / sizeof(irClima[0]), NEC_KHZ);
      break;
    }
  }
  return;
}

void rstNotif() {
  char buff[32];
  switch (ESP.getResetInfoPtr()->reason) {
    
    case REASON_DEFAULT_RST: 
      // Inicialização normal
      strcpy_P(buff, PSTR("Power on"));
      break;
      
    case REASON_WDT_RST:
      // Estouro do Hardware Watchdog
      strcpy_P(buff, PSTR("Hardware Watchdog"));
      break;
      
    case REASON_EXCEPTION_RST:
      // Exception Reset
      strcpy_P(buff, PSTR("Exception"));      
      break;
      
    case REASON_SOFT_WDT_RST:
      // Estouro do Software Watchdog
      strcpy_P(buff, PSTR("Software Watchdog"));
      break;
      
    case REASON_SOFT_RESTART: 
      // Software/System Restart
      strcpy_P(buff, PSTR("Software/System restart"));
      break;
      
    case REASON_DEEP_SLEEP_AWAKE:
      // Despertar do modo suspensão
      strcpy_P(buff, PSTR("Deep-Sleep Wake"));
      break;
      
    case REASON_EXT_SYS_RST:
      // Reinicialização externa (Reset Pin)
      strcpy_P(buff, PSTR("External System"));
      break;
      
    default:  
      // Reinicialização desconhecida
      strcpy_P(buff, PSTR("Unknown"));     
      break;
  }
  aquecedor.sendPushNotification("Reinicialização: "+ String(buff));
  //rstFlag = false;
}

void alerta(){
  umidificador.sendPushNotification("ALERTA! Umidade: "+ String(humidity));
  aquecedor.sendPushNotification("ALERTA! Temperatura: "+ String(temperature));
}

void atualizaThingSpeak(){

  //ESP.wdtFeed();
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  ThingSpeak.setField(3, relayPowerState[AQUECEDOR_ID]);
  ThingSpeak.setField(4, relayPowerState[UMIDIFICADOR_ID]);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
}
