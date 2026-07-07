#pragma once
#include "hal/IBleNavSource.h"
class NavigationService {
public:
    explicit NavigationService(IBleNavSource& source) : _source(source) {}
    void begin();
    void tick();
private:
    IBleNavSource& _source;
    NavData _lastPublished;  // for change detection
};