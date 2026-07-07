#include "AppState.h"

namespace AppState {

    namespace {
        MusicSnapshot musicState;
        portMUX_TYPE musicMux = portMUX_INITIALIZER_UNLOCKED;

        NavSnapshot navState;
        portMUX_TYPE navMux = portMUX_INITIALIZER_UNLOCKED;
    }

    void setMusicTrack(const String& track, const String& artist, bool playing) {
        portENTER_CRITICAL(&musicMux);
        musicState.track = track;
        musicState.artist = artist;
        musicState.playing = playing;
        musicState.version++;
        portEXIT_CRITICAL(&musicMux);
    }

    void setMusicLyricLine(const String& line) {
        portENTER_CRITICAL(&musicMux);
        musicState.currentLyricLine = line;
        musicState.version++;
        portEXIT_CRITICAL(&musicMux);
    }

    MusicSnapshot getMusic() {
        portENTER_CRITICAL(&musicMux);
        MusicSnapshot copy = musicState;
        portEXIT_CRITICAL(&musicMux);
        return copy;
    }

    void setNav(const NavSnapshot& nav) {
        portENTER_CRITICAL(&navMux);
        unsigned long preservedVersion = navState.version + 1;  // caller doesn't set version — AppState owns it
        navState = nav;
        navState.version = preservedVersion;
        portEXIT_CRITICAL(&navMux);
    }

    NavSnapshot getNav() {
        portENTER_CRITICAL(&navMux);
        NavSnapshot copy = navState;
        portEXIT_CRITICAL(&navMux);
        return copy;
    }

}  // namespace AppState