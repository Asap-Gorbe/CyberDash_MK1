#pragma once
#include <Arduino.h>

namespace Clock {
    void begin();
    bool isSynced();
    String nowFormatted();
}