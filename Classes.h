

/*------------CLASSE AQUECEDOR-------------*/

#ifndef _AQUECEDOR_H_
#define _AQUECEDOR_H_

#include <SinricProDevice.h>
#include <Capabilities/RangeController.h>
#include <Capabilities/PowerStateController.h>
#include <Capabilities/ToggleController.h>
#include <Capabilities/PushNotification.h>

class Aquecedor 
: public SinricProDevice
, public RangeController<Aquecedor>
, public PowerStateController<Aquecedor>
, public ToggleController<Aquecedor>
, public PushNotification<Aquecedor> {
  friend class RangeController<Aquecedor>;
  friend class PowerStateController<Aquecedor>;
  friend class ToggleController<Aquecedor>;
  friend class PushNotification<Aquecedor>;
public:
  Aquecedor(const String &deviceId) : SinricProDevice(deviceId, "Aquecedor") {};
};

#endif

/*------------CLASSE UMIDIFICADOR------------*/

#ifndef _UMIDIFICADOR_H_
#define _UMIDIFICADOR_H_

#include <SinricProDevice.h>
#include <Capabilities/RangeController.h>
#include <Capabilities/PowerStateController.h>
#include <Capabilities/ToggleController.h>
#include <Capabilities/PushNotification.h>

class Umidificador 
: public SinricProDevice
, public RangeController<Umidificador>
, public PowerStateController<Umidificador>
, public ToggleController<Umidificador>
, public PushNotification<Umidificador> {
  friend class RangeController<Umidificador>;
  friend class PowerStateController<Umidificador>;
  friend class ToggleController<Umidificador>;
  friend class PushNotification<Umidificador>;
public:
  Umidificador(const String &deviceId) : SinricProDevice(deviceId, "Umidificador") {};
};

#endif


class Dispositivo {
  public:
    int relay;
    String id;
    bool state;
    Dispositivo(int r, String i, bool s){
      relay = r;
      id = i;
      state = s;
    }
    bool swRelay(bool st) {
      digitalWrite(relay, !st);
      state = st;
      return st;
    }
};
