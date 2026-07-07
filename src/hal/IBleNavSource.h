#pragma once
#include <Arduino.h>
struct NavData {
    String eta;               // e.g. "14:32"
    int minutesRemaining = 0;
    int distanceRemainingM = 0;
    String destination;
    String currentStreet;
    int nextTurnDistanceM = -1;  // -1 means "not available" — mirrors the source app's N/A case
    bool valid = false;          // false until at least one real update has arrived
};
class IBleNavSource {
public:
    virtual ~IBleNavSource() = default;
    virtual void begin() = 0;
    virtual NavData poll() = 0;
};