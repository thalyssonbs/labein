#ifndef _UMIDIFICADOR_H_
#define _UMIDIFICADOR_H_

#include <SinricProDevice.h>
#include <Capabilities/RangeController.h>
#include <Capabilities/PowerStateController.h>
#include <Capabilities/ModeController.h>
#include <Capabilities/ToggleController.h>
#include <Capabilities/PushNotification.h>

class Umidificador 
: public SinricProDevice
, public RangeController<Umidificador>
, public PowerStateController<Umidificador>
, public ToggleController<Umidificador>
, public ModeController<Umidificador>
, public PushNotification<Umidificador> {
  friend class RangeController<Umidificador>;
  friend class PowerStateController<Umidificador>;
  friend class ToggleController<Umidificador>;
  friend class ModeController<Umidificador>;
  friend class PushNotification<Umidificador>;
public:
  Umidificador(const String &deviceId) : SinricProDevice(deviceId, "Umidificador") {};
};

#endif
