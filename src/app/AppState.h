#pragma once
#include <Arduino.h>
namespace AppState {
struct Music {
    String track = "";
    String artist = "";
    String currentLyricLine = "";
    bool playing = false;
    unsigned long version = 0 ;
};
extern Music music;
}