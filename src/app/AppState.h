#pragma once
#include <Arduino.h>

namespace AppState {

    struct MusicSnapshot {
        String track;
        String artist;
        String currentLyricLine;
        bool playing;
        unsigned long version;
    };

    void setMusicTrack(const String& track, const String& artist, bool playing);
    void setMusicLyricLine(const String& line);
    MusicSnapshot getMusic();

    struct NavSnapshot {
        String eta;
        int minutesRemaining = 0;
        int distanceRemainingM = 0;
        String destination;
        String currentStreet;
        int nextTurnDistanceM = -1;
        bool valid = false;
        unsigned long version = 0;
    };

    void setNav(const NavSnapshot& nav);
    NavSnapshot getNav();

}  // namespace AppState