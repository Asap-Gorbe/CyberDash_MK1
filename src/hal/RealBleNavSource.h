#pragma once
#include "IBleNavSource.h"
#include <ChronosESP32.h>

// Wraps ChronosESP32 as a BLE peripheral (ESP32 = GATT server, phone = client,
// per the original architecture decision — ChronosESP32 just gave us that
// protocol pre-built instead of having to extract it from CarPlayBLE by hand).
class RealBleNavSource : public IBleNavSource {
public:
    void begin() override;
    NavData poll() override;

private:
    ChronosESP32 _watch{"Cyberdeck Nav"};  // BLE name advertised to the Chronos app
};