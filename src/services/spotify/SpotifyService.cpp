//
// Created by NoteBook on 7/5/2026.
//
#include "SpotifyService.h"
#include <ArduinoJson.h>
#include "app/Secrets.h"
#include "app/Config.h"
#include "app/AppState.h"

namespace {  // file-private helpers — not part of SpotifyService's public shape, not visible outside this file

String base64Encode(String input) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encoded = "";
    int len = input.length();
    const char* data = input.c_str();
    int i = 0;
    while (i < len) {
        unsigned char b0 = i < len ? data[i++] : 0;
        unsigned char b1 = i < len ? data[i++] : 0;
        unsigned char b2 = i < len ? data[i++] : 0;
        encoded += chars[b0 >> 2];
        encoded += chars[((b0 & 3) << 4) | (b1 >> 4)];
        encoded += chars[((b1 & 15) << 2) | (b2 >> 6)];
        encoded += chars[b2 & 63];
    }
    int pad = (3 - len % 3) % 3;
    for (int p = 0; p < pad; p++) encoded[encoded.length() - 1 - p] = '=';
    return encoded;
}

String urlEncode(String str) {
    str.replace(" ", "+");
    str.replace("&", "%26");
    str.replace("'", "%27");
    str.replace("(", "%28");
    str.replace(")", "%29");
    return str;
}

String cleanName(String s) {
    String lower = s;
    lower.toLowerCase();
    int cut = s.length();

    int idx;
    if ((idx = lower.indexOf(" feat")) != -1 && idx < cut) cut = idx;
    if ((idx = lower.indexOf(" ft."))  != -1 && idx < cut) cut = idx;
    if ((idx = s.indexOf(" ("))        != -1 && idx < cut) cut = idx;
    if ((idx = s.indexOf(" - "))       != -1 && idx < cut) cut = idx;

    s = s.substring(0, cut);
    s.trim();
    return s;
}

String asciiOnly(String s) {
    String out = "";
    for (int i = 0; i < (int)s.length(); i++) {
        char c = s[i];
        if (c >= 32 && c <= 126) out += c;
    }
    return out;
}

String skipHeaders(WiFiClientSecure& c) {
    String statusLine = c.readStringUntil('\n');
    while (c.connected()) {
        String line = c.readStringUntil('\n');
        if (line == "\r") break;
    }
    return statusLine;
}

// from a /api/search array, return syncedLyrics of the result whose duration
// is closest to target (within 2s). "" if none qualify.
String pickByDuration(String& resp, int targetSec) {
    String bestObj = ""; long bestDiff = 999999;
    int n = resp.length();
    int i = resp.indexOf('[');
    if (i < 0) return "";

    while (i < n) {
        int objStart = resp.indexOf('{', i);
        if (objStart < 0) break;
        int depth = 0, objEnd = -1; bool inStr = false, esc = false;
        for (int j = objStart; j < n; j++) {
            char c = resp[j];
            if (esc)       { esc = false; continue; }
            if (c == '\\') { esc = true;  continue; }
            if (c == '"')  { inStr = !inStr; continue; }
            if (inStr) continue;
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) { objEnd = j; break; } }
        }
        if (objEnd < 0) break;

        String obj = resp.substring(objStart, objEnd + 1);
        int k = obj.indexOf("\"duration\":");
        if (k >= 0) {
            long dur  = obj.substring(k + 11).toInt();
            long diff = dur - targetSec; if (diff < 0) diff = -diff;
            if (dur > 0 && diff <= 2 && diff < bestDiff) {
                bestDiff = diff; bestObj = obj;
                if (diff == 0) break;
            }
        }
        i = objEnd + 1;
    }

    if (bestObj.length() == 0) return "";
    JsonDocument doc;
    if (deserializeJson(doc, bestObj)) return "";
    return doc["syncedLyrics"].as<String>();
}

}  // namespace

void SpotifyService::begin() {
    xTaskCreatePinnedToCore(taskTrampoline, "spotify", 16384, this, 1, NULL, 0);
}

void SpotifyService::taskTrampoline(void* pv) {
    static_cast<SpotifyService*>(pv)->networkTaskLoop();
}
bool SpotifyService::refreshAccessToken() {
    Serial.println("[Spotify] Refreshing token...");
    _client.setInsecure();
    if (!_client.connect("accounts.spotify.com", 443)) {
        Serial.println("[Spotify] Token endpoint connection failed");
        return false;
    }

    String auth = base64Encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
    String body = "grant_type=refresh_token&refresh_token=" + String(REFRESH_TOKEN);

    _client.println("POST /api/token HTTP/1.1");
    _client.println("Host: accounts.spotify.com");
    _client.println("Authorization: Basic " + auth);
    _client.println("Content-Type: application/x-www-form-urlencoded");
    _client.println("Content-Length: " + String(body.length()));
    _client.println("Connection: close");
    _client.println();
    _client.print(body);

    skipHeaders(_client);
    String response = _client.readString();
    _client.stop();

    int jsonStart = response.indexOf('{');
    if (jsonStart == -1) { Serial.println("[Spotify] Bad token response"); return false; }
    response = response.substring(jsonStart);

    JsonDocument doc;
    deserializeJson(doc, response);
    _accessToken = doc["access_token"].as<String>();

    if (_accessToken.length() > 0) {
        Serial.println("[Spotify] Token OK");
        return true;
    }
    Serial.println("[Spotify] Token empty");
    return false;
}

SpotifyService::NowPlaying SpotifyService::getNowPlaying() {
NowPlaying np = {"", "", 0, 0, 0, false, false, false};    _client.setInsecure();
    if (!_client.connect("api.spotify.com", 443)) {
        Serial.println("[Spotify] API connection failed");
        return np;
    }

    _client.println("GET /v1/me/player/currently-playing HTTP/1.1");
    _client.println("Host: api.spotify.com");
    _client.println("Authorization: Bearer " + _accessToken);
    _client.println("Connection: close");
    _client.println();
    unsigned long sentAt = millis();

    String statusLine = skipHeaders(_client);

    if (statusLine.indexOf("204") > 0) {
        _client.stop();
        np.nothingPlaying = true ;
        return np;
    }
    if (statusLine.indexOf("401") > 0) {
        Serial.println("[Spotify] Token expired, refreshing...");
        _client.stop();
        refreshAccessToken();
        return np;
    }

    String response = _client.readString();
    np.fetch_latency_ms = (int)(millis() - sentAt);
    _client.stop();

    int jsonStart = response.indexOf('{');
    if (jsonStart == -1) return np;
    response = response.substring(jsonStart);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        Serial.println("[Spotify] JSON error: " + String(err.c_str()));
        return np;
    }

    np.track       = doc["item"]["name"].as<String>();
    np.artist      = doc["item"]["artists"][0]["name"].as<String>();
    np.progress_ms = doc["progress_ms"].as<int>();
    np.duration_ms = doc["item"]["duration_ms"].as<int>();
    np.is_playing  = doc["is_playing"].as<bool>();
    np.valid       = true;
    return np;
}
void SpotifyService::clearLyrics() {
    _lyricsLocked = true;
    _lyricCount  = 0;
    _currentLine = -1;
    _lyricsLocked = false;
}

void SpotifyService::parseSyncedLyrics(String synced) {
    _lyricsLocked = true;          // block tick() on core 1 while we rewrite
    _lyricCount  = 0;
    _currentLine = -1;

    int pos = 0;
    while (pos < (int)synced.length() && _lyricCount < MAX_LINES) {
        int nl = synced.indexOf('\n', pos);
        if (nl == -1) nl = synced.length();
        String line = synced.substring(pos, nl);
        pos = nl + 1;
        line.trim();

        // each line looks like  [mm:ss.xx] text
        if (line.length() < 3 || line[0] != '[') continue;
        int close = line.indexOf(']');
        if (close == -1) continue;

        String stamp = line.substring(1, close);     // "mm:ss.xx"
        String text  = line.substring(close + 1);
        text.trim();

        int colon = stamp.indexOf(':');
        if (colon == -1) continue;
        long mm  = stamp.substring(0, colon).toInt();
        float ss = stamp.substring(colon + 1).toFloat();
        long ms  = mm * 60000L + (long)(ss * 1000.0);

        _lyricTime[_lyricCount] = ms;
        _lyricText[_lyricCount] = asciiOnly(text);   // empty lines kept on purpose (clears display on gaps)
        _lyricCount++;
    }
    Serial.printf("[Lyrics] Parsed %d lines\n", _lyricCount);
    _lyricsLocked = false;         // done, tick() may resume
}

// minimal HTTPS GET to lrclib.net; returns body on 200, "" otherwise
String SpotifyService::lrclibGet(String path) {
    _client.setInsecure();
    if (!_client.connect("lrclib.net", 443)) { _client.stop(); return ""; }
    _client.setTimeout(10000);

    _client.println("GET " + path + " HTTP/1.1");
    _client.println("Host: lrclib.net");
    _client.println("User-Agent: ESP32LyricsDisplay/1.0");
    _client.println("Connection: close");
    _client.println();

    unsigned long t = millis();
    while (_client.available() == 0) {
        if (millis() - t > 8000) { _client.stop(); return ""; }
        delay(10);
    }

    String status = skipHeaders(_client);
    if (status.indexOf("200") < 0) { _client.stop(); return ""; }   // 404 = no match

    const size_t MAX_BYTES = 40000;
    String resp = "";
    resp.reserve(MAX_BYTES + 16);
    t = millis();
    while ((_client.connected() || _client.available()) && millis() - t < 10000) {
        while (_client.available()) {
            char c = (char)_client.read();
            if (resp.length() < MAX_BYTES) resp += c;
            t = millis();
        }
        if (resp.length() >= MAX_BYTES) break;
    }
    _client.stop();
    return resp;
}

void SpotifyService::getLyrics(String track, String artist, int durationMs) {
    clearLyrics();                  // blank immediately while we fetch

    int durSec    = durationMs / 1000;
    String qTrack = cleanName(track);
    Serial.printf("[Lyrics] \"%s\" by \"%s\" (%ds)\n", qTrack.c_str(), artist.c_str(), durSec);

    // 1) exact match by duration — timestamps line up with the playing recording
    String body = lrclibGet("/api/get?track_name=" + urlEncode(qTrack) +
                            "&artist_name=" + urlEncode(artist) +
                            "&duration=" + String(durSec));
    String synced = "";
    if (body.length()) {
        int s = body.indexOf('{');
        if (s >= 0) {
            JsonDocument doc;
            if (!deserializeJson(doc, body.substring(s)))
                synced = doc["syncedLyrics"].as<String>();
        }
    }

    // 2) fallback: search, then keep the result whose duration matches (±2s)
    if (synced.length() == 0) {
        Serial.println("[Lyrics] No exact match, searching...");
        body = lrclibGet("/api/search?track_name=" + urlEncode(qTrack) +
                         "&artist_name=" + urlEncode(artist));
        synced = pickByDuration(body, durSec);
    }

    if (synced.length() == 0) {
        Serial.println("[Lyrics] No synced lyrics matched");
        return;
    }
    parseSyncedLyrics(synced);
    Serial.printf("[Lyrics] Free heap: %d\n", ESP.getFreeHeap());
}
void SpotifyService::networkTaskLoop() {
    refreshAccessToken();
    _lastRefresh  = millis();
    for (;;) {
        unsigned long now = millis();
        if (now - _lastRefresh > Config::TokenRefreshMs) {
            refreshAccessToken();
            _lastRefresh  = now ;
    }

        NowPlaying np = getNowPlaying();
        if (np.valid) {

            // re-anchor the local playback clock (compensate ~half the round trip)
            _baseProgressMs = np.progress_ms + np.fetch_latency_ms / 2;
            _baseAtMs       = millis();
            _playing        = np.is_playing;

            bool trackChanged = (np.track != _lastTrack);
            if (trackChanged) {
                _lastTrack = np.track;
                Serial.println("\n>>> " + np.track + " - " + np.artist);
                AppState::setMusicLyricLine("");  // clear the previous track's line immediately — don't leave stale text up while the new track's lyrics are being fetched
                getLyrics(np.track, np.artist, np.duration_ms);
            }

            bool hasSyncedLyrics = (_lyricCount > 0);

            AppState::MusicSnapshot current = AppState::getMusic();
            if (trackChanged || current.playing != np.is_playing || current.hasSyncedLyrics != hasSyncedLyrics) {
                AppState::setMusicTrack(np.track, np.artist, np.is_playing, hasSyncedLyrics);
            }
        } else if (np.nothingPlaying) {
            // Only act on the transition (was playing -> now isn't) —
            // otherwise this fires every poll and both spams the log
            // and re-bumps AppState::music.version for no reason.
            AppState::MusicSnapshot current = AppState::getMusic();
            if (current.playing) {
                Serial.println("[Spotify] Nothing playing");
                _playing = false;
                _lastTrack = "";  // so the same track starting again later is correctly treated as a change
                AppState::setMusicTrack("", "", false, false);
            }

        }
        vTaskDelay(50 / portTICK_PERIOD_MS);   // yield to idle task / feed the watchdog
    }
}

void SpotifyService::tick() {
    unsigned long now = millis();

    static unsigned long lastLineCheck = 0;
    if (_playing && _lyricCount > 0 && !_lyricsLocked && now - lastLineCheck > 50) {
        lastLineCheck = now;

        long est = _baseProgressMs + (long)(now - _baseAtMs) + Config::SyncOffsetMs;

        int idx = -1;
        for (int i = 0; i < _lyricCount; i++) {
            if (_lyricTime[i] <= est) idx = i;
            else break;                       // timestamps are sorted, so stop early
        }

        if (idx >= 0 && idx != _currentLine) {
            _currentLine = idx;
            AppState::setMusicLyricLine(_lyricText[_currentLine]);
        }
    }
}
