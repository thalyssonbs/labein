#ifndef _AQUECEDOR_H_
#define _AQUECEDOR_H_

#include <SinricProDevice.h>
#include <Capabilities/RangeController.h>
#include <Capabilities/ModeController.h>
#include <Capabilities/PowerStateController.h>
#include <Capabilities/ToggleController.h>
#include <Capabilities/PushNotification.h>

class Aquecedor 
: public SinricProDevice
, public RangeController<Aquecedor>
, public ModeController<Aquecedor>
, public PowerStateController<Aquecedor>
, public ToggleController<Aquecedor>
, public PushNotification<Aquecedor> {
  friend class RangeController<Aquecedor>;
  friend class ModeController<Aquecedor>;
  friend class PowerStateController<Aquecedor>;
  friend class ToggleController<Aquecedor>;
  friend class PushNotification<Aquecedor>;
public:
  Aquecedor(const String &deviceId) : SinricProDevice(deviceId, "Aquecedor") {};
};

#endif
