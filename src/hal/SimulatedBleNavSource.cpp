#include "SimulatedBleNavSource.h"

void SimulatedBleNavSource::begin() {
    _lastTickMs = millis();
}

NavData SimulatedBleNavSource::poll() {
    unsigned long now = millis();
    if (now - _lastTickMs > 1000) {
        _lastTickMs = now;
        _step++;
    }

    NavData data;
    data.valid = true;
    data.destination = "Home";
    data.eta = "14:32";
    data.minutesRemaining = 12;
    data.distanceRemainingM = 4200 - (_step * 10);
    if (data.distanceRemainingM < 0) data.distanceRemainingM = 4200;  // loop back around
    int cycle = (_step / 20) % 3;
    if (cycle == 0) {
        data.currentStreet = "Moenckebergstrasse";
        data.nextTurnDistanceM = 200 - ((_step % 20) * 10);
    } else if (cycle == 1) {
        data.currentStreet = "Steinstrasse";
        data.nextTurnDistanceM = 150 - ((_step % 20) * 7);
    } else {
        data.currentStreet = "Moenckebergstrasse";
        data.nextTurnDistanceM = -1;  // no turn coming up right now
    }
    if (data.nextTurnDistanceM < 0 && cycle != 2) data.nextTurnDistanceM = 0;

    return data;
}