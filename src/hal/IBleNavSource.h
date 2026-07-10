#pragma once
#include <Arduino.h>
struct NavData {
    bool active = false;        // true while turn-by-turn nav is running on the phone
    bool isNavigation = false;  // true = real turn-by-turn data; Chronos can push
    String eta;                 // e.g. "14:32"
    String duration;            // e.g. "12 min"
    String distance;            // distance remaining to destination, e.g. "1.2 km"
    String title;                // dual-purpose per Chronos docs (next-point distance
    String directions;          // current street / turn instruction text
    String speed;                // may be blank depending on the source app
    bool valid = false;
};
class IBleNavSource {
public:
    virtual ~IBleNavSource() = default;
    virtual void begin() = 0;
    virtual NavData poll() = 0;
};