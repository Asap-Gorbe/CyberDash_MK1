#pragma once
#include "IBleNavSource.h"
class SimulatedBleNavSource : public IBleNavSource {
public:
    void begin() override;
    NavData poll() override;

private:
    unsigned long _lastTickMs = 0;
    int _step = 0;
};