#pragma once
#include <Arduino.h>
namespace AppState {
struct MusicSnapshot {
    String track ;
    String artist ;
    String currentLyricLine ;
    bool playing ;
    unsigned long version ;
};
    void setMusicTrack(const String& track, const String& artist, bool playing);
    void setMusicLyricLine(const String& line);
    MusicSnapshot getMusic();

}