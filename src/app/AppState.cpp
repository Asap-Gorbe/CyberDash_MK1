#include "AppState.h"

namespace AppState {

    namespace {
        MusicSnapshot musicState;                        // the real, protected data
        portMUX_TYPE musicMux = portMUX_INITIALIZER_UNLOCKED;
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
        MusicSnapshot copy = musicState;   // String copies happen while locked
        portEXIT_CRITICAL(&musicMux);
        return copy;                        // caller uses this copy lock-free
    }

}  // namespace AppState