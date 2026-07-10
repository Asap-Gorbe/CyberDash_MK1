#pragma once
#include <Arduino.h>

namespace AppState {

    struct MusicSnapshot {
        String track;
        String artist;
        String currentLyricLine;
        bool playing;
        bool hasSyncedLyrics;  // false if no synced lyrics were found for this track
        unsigned long version;
    };
    void setMusicTrack(const String& track, const String& artist, bool playing, bool hasSyncedLyrics);
    void setMusicLyricLine(const String& line);
    MusicSnapshot getMusic();

    struct NavSnapshot {
        bool active = false;
        bool isNavigation = false;
        String eta;
        String duration;
        String distance;
        String title;
        String directions;
        String speed;
        bool valid = false;
        unsigned long version = 0;
    };

    void setNav(const NavSnapshot& nav);
    NavSnapshot getNav();

}  // namespace AppState