#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
class SpotifyService {
public:
    void begin();
    void tick();

private:
    static void taskTrampoline(void* pv);
    void networkTaskLoop();  // runs forever on core 0

    struct NowPlaying {
        String track;
        String artist;
        int    progress_ms;
        int    duration_ms;
        int    fetch_latency_ms;
        bool   is_playing;
        bool   valid;
    };

    bool refreshAccessToken();
    NowPlaying getNowPlaying();
    void getLyrics(String track, String artist, int durationMs);
    String lrclibGet(String path);  // minimal HTTPS GET to lrclib.net; returns body on 200, "" otherwise
    void parseSyncedLyrics(String synced);
    void clearLyrics();

    WiFiClientSecure _client;   // used ONLY by networkTaskLoop (core 0)
    String _accessToken = "";
    String _lastTrack = "";

    static const int MAX_LINES = 200;
    long   _lyricTime[MAX_LINES];      // timestamp (ms) of each line
    String _lyricText[MAX_LINES];      // the line text
    volatile int  _lyricCount   = 0;
    volatile int  _currentLine  = -1;
    volatile bool _lyricsLocked = false;  // true while core 0 is rewriting the arrays

    volatile long          _baseProgressMs = 0;  // Spotify progress at last poll (+ latency comp)
    volatile unsigned long _baseAtMs       = 0;  // millis() when we anchored it
    volatile bool          _playing        = false;

    unsigned long _lastPoll    = 0;
    unsigned long _lastRefresh = 0;
};