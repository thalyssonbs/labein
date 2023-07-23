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
#include <ESP8266Ping.h>
#include <Pinger.h>
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
}

// Set global to avoid object removing after setup() routine
Pinger pinger;



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

bool  irSignal;
unsigned int irDelay = 0;
int irCom = 1;
bool wifiCom;
char buff[32];

String hostName = "www.sinric.pro";
int late, notificar;


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
 /*----------------------------*/


switch (ESP.getResetInfoPtr()->reason) {
    
    case REASON_DEFAULT_RST: 
      // do something at normal startup by power on
      strcpy_P(buff, PSTR("Power on"));
      break;
      
    case REASON_WDT_RST:
      // do something at hardware watch dog reset
      strcpy_P(buff, PSTR("Hardware Watchdog"));     
      break;
      
    case REASON_EXCEPTION_RST:
      // do something at exception reset
      strcpy_P(buff, PSTR("Exception"));      
      break;
      
    case REASON_SOFT_WDT_RST:
      // do something at software watch dog reset
      strcpy_P(buff, PSTR("Software Watchdog"));
      break;
      
    case REASON_SOFT_RESTART: 
      // do something at software restart ,system_restart 
      strcpy_P(buff, PSTR("Software/System restart"));
      break;
      
    case REASON_DEEP_SLEEP_AWAKE:
      // do something at wake up from deep-sleep
      strcpy_P(buff, PSTR("Deep-Sleep Wake"));
      break;
      
    case REASON_EXT_SYS_RST:
      // do something at external system reset (assertion of reset pin)
      strcpy_P(buff, PSTR("External System"));
      break;
      
    default:  
      // do something when reset occured for unknown reason
      strcpy_P(buff, PSTR("Unknown"));     
      break;
  }



 /*----------------------------*/
  EEPROM.begin(32);
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
  //acordou = true;
  StartTimer(5, 120000UL, wakeUP);
  //rstTime = millis();
  StartTimer(6, 259200000UL, espRestart);
  //transmissaoTS = millis();
  StartTimer(7, 30000UL, atualizaThingSpeak);
  
}


const byte TIMER_COUNT = 10;

unsigned long TimerStartTimes[TIMER_COUNT];
unsigned long TimerIntervals[TIMER_COUNT];
void (*TimerCallbacks[TIMER_COUNT])(void);  // Callback functions

void StartTimer(int index, unsigned long interval, void (*callback)(void)) {
    if (TimerStartTimes[index] == 0UL) {  // Unused timer
      TimerStartTimes[index] = millis();
      TimerIntervals[index] = interval;
      TimerCallbacks[index] = callback;
  }
}

void CheckTimers() {
  unsigned long currentTime = millis();
  for (byte i = 0; i < TIMER_COUNT; i++) {
    if ((TimerStartTimes[i] != 0UL) &&
        (currentTime - TimerStartTimes[i] >= TimerIntervals[i])) {
      TimerStartTimes[i] = 0;  // Unused Timer
      (TimerCallbacks[i])();
    }
  }
}


void loop() {
  ESP.wdtFeed();
  
  if(!wifiCom){
    wifiCom = setupWiFi();
  }
  else{
    SinricPro.handle();
  }
  
  handleTemperaturesensor();
  if(tempAnFlag) tempAnalyze();
  if(humiAnFlag) humiAnalyze();
  swHandle();

  CheckTimers();
  
  if(irSignal & (millis()-irDelay)>2000) humidifierON(irCom);
  
}


void espRestart(void) {
  ESP.restart();
  }


void wakeUP(void){
  if(wifiCom){
    umidificador.sendPushNotification("Reinicialização: "+ String(buff));
    ThingSpeak.setStatus("Reinicialização: "+ String(buff));
  }
  else {
    StartTimer(5, 120000UL, wakeUP);
  }
}

void ligou(int device){
  switch (device) {
    case 3:
      StartTimer(1, 1800000UL, heaterTimeOut);
      break;
    case 0:
      StartTimer(2, 1800000UL, umidifierTimeOut);
      break;
  }
}

void desligou(int device){
  switch (device) {
    case 3:
      TimerStartTimes[1] = 0;
      break;
    case 0:
      TimerStartTimes[2] = 0;
      break;
  }
}


void heaterTimeOut(void) {
  devicePowerState[AQUECEDOR_ID] = false;
  if(wifiCom) {
    ThingSpeak.setStatus("Heater Timeout!");
    aquecedor.sendPushNotification("Tempo Ligado Excedido!");
  }
  delay(1000);
  swHandle();
}

void umidifierTimeOut(void) {
  devicePowerState[UMIDIFICADOR_ID] = false;
  if(wifiCom) {
    ThingSpeak.setStatus("Humidifier Timeout!");
    umidificador.sendPushNotification("Tempo Ligado Excedido!");
  }
  delay(1000);
  swHandle();
}


void swHandle(){
  if (devicePowerState[AQUECEDOR_ID]!=relayPowerState[AQUECEDOR_ID]){
    relayPowerState[AQUECEDOR_ID] = Heater.swRelay(devicePowerState[AQUECEDOR_ID]);
    wrEeprom(eeprAdr["Aquecedor"], devicePowerState[AQUECEDOR_ID]);
    devicePowerState[AQUECEDOR_ID] ? ligou(Heater.relay) : desligou(Heater.relay);
    aquecedor.sendPowerStateEvent(relayPowerState[AQUECEDOR_ID]);
  }

  if (devicePowerState[UMIDIFICADOR_ID]!=relayPowerState[UMIDIFICADOR_ID]){
    relayPowerState[UMIDIFICADOR_ID] = Humidifier.swRelay(devicePowerState[UMIDIFICADOR_ID]);
    irSignal = relayPowerState[UMIDIFICADOR_ID];
    if(irSignal) irDelay = millis();
    wrEeprom(eeprAdr["Umidificador"], devicePowerState[UMIDIFICADOR_ID]);
    devicePowerState[UMIDIFICADOR_ID] ? ligou(Humidifier.relay) : desligou(Humidifier.relay);
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

  wifi = latencia();
  
  return wifi;

}

bool latencia() {
  if(pinger.Ping(hostName)) {
  wifiCom = true;
  }
  else {
  wifiCom = false;
  }

  pinger.OnEnd([](const PingerResponse& response){
  if (response.TotalReceivedResponses > 0) {
    late = response.AvgResponseTime;
  }
  return true;
  });
  return wifiCom;
}



//-------------------DHT Handle Sinric Pro---------------------//
void handleTemperaturesensor() {
  //if (deviceIsOn == false) return; // device is off...do nothing

  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) return; //only check every EVENT_WAIT_TIME milliseconds


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

  if(temperature < (globalRangeValues["rangeAquecedor"]-2) | temperature > (globalRangeValues["rangeAquecedor"]+2) | humidity < (globalRangeValues["rangeUmidificador"]-5) | humidity > (globalRangeValues["rangeUmidificador"]+3)) {
    notificar++;
    alerta();
  }
  else{
    notificar = 0;
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
      irDelay = millis();
      sendIrSignal(3);
      irCom = 4;
      //irSignal = false;
      break;
    }
    case 4: {
      sendIrSignal(4);
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
  uint16_t irTimer[67] = {8930,4470, 580,1670, 580,520, 580,570, 580,520, 580,570, 530,570, 580,570, 530,570, 580,570, 530,1670, 580,1670, 630,1620, 580,1670, 580,1670, 580,1670, 580,1620, 630,1620, 580,570, 580,1670, 530,1670, 580,570, 580,570, 530,570, 580,570, 580,520, 580,1670, 580,520, 580,570, 580,1670, 580,1670, 580,1620, 580,1670, 580};  // Protocol=NEC Address=0x1 Command=0xD Raw-Data=0xF20DFE01 32 bits LSB first
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
    case 4: {
      IrSender.sendRaw(irTimer, sizeof(irTimer) / sizeof(irTimer[0]), NEC_KHZ);
      break;
    }
  }
  return;
}

//void rstNotif() {
//  umidificador.sendPushNotification("Reinicialização: "+ String(buff));
//  rstFlag = false;
//  return;
//}

void alerta(){
  if(notificar < 30) return;
  umidificador.sendPushNotification("ALERTA! Umidade: "+ String(humidity));
  aquecedor.sendPushNotification("ALERTA! Temperatura: "+ String(temperature));
  notificar = 0;
}

void atualizaThingSpeak(void){
  wifiCom = latencia();
  //ESP.wdtFeed();
  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  ThingSpeak.setField(3, relayPowerState[AQUECEDOR_ID]);
  ThingSpeak.setField(4, relayPowerState[UMIDIFICADOR_ID]);
  ThingSpeak.setField(5, late);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  StartTimer(7, 60000UL, atualizaThingSpeak);
  //transmissaoTS = millis();
}
