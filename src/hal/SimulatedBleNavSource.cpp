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
    data.active = true;
    data.isNavigation = true;
    data.eta = "14:32";
    data.duration = "12 min";

    int remainingM = 4200 - (_step * 10);
    if (remainingM < 0) remainingM = 4200;  // loop back around
    data.distance = String(remainingM / 1000.0, 1) + " km";

    int cycle = (_step / 20) % 3;
    if (cycle == 0) {
        data.directions = "Moenckebergstrasse";
        int turnM = 200 - ((_step % 20) * 10);
        data.title = String(turnM > 0 ? turnM : 0) + " m";
    } else if (cycle == 1) {
        data.directions = "Steinstrasse";
        int turnM = 150 - ((_step % 20) * 7);
        data.title = String(turnM > 0 ? turnM : 0) + " m";
    } else {
        data.directions = "Moenckebergstrasse";
        data.title = "";  // no turn coming up right now
    }

    return data;
}