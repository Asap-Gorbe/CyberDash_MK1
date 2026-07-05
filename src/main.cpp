#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "app/Secrets.h"
#include "app/Config.h"
#include "hal/SerialDisplay.h"
#include "hal/SerialInput.h"
#include "gfx/Renderer.h"
#include "ui/ScreenManager.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/MusicScreen.h"
#include "ui/screens/NavScreen.h"
SerialDisplay display;
Renderer renderer(display);

SerialInput input;
HomeScreen homeScreen;
MusicScreen musicScreen;
NavScreen navScreen;
Screen* screens[] = { &homeScreen, &musicScreen, &navScreen };  // index 0 = Home, by convention
ScreenManager screenManager(renderer, input, screens, 3);

WiFiClientSecure client;          // used ONLY by the network task (core 0)
String accessToken  = "";
String lastTrack    = "";

// ---- lyric buffer (written by core 0, read by core 1) ----
const int MAX_LINES = 200;
long   lyricTime[MAX_LINES];      // timestamp (ms) of each line
String lyricText[MAX_LINES];      // the line text
volatile int  lyricCount   = 0;   // how many lines parsed
volatile int  currentLine  = -1;  // line currently being shown
volatile bool lyricsLocked = false; // true while core 0 is rewriting the arrays

// ---- playback clock (written by core 0, read by core 1) ----
volatile long          baseProgressMs = 0;  // Spotify progress at last poll (+ latency comp)
volatile unsigned long baseAtMs       = 0;  // millis() when we anchored it
volatile bool          playing        = false;

// ---- timing config ----
unsigned long lastPoll    = 0;
unsigned long lastRefresh = 0;
const unsigned long POLL_INTERVAL    = 2000;     // check now-playing every 2s
const long          SYNC_OFFSET_MS   = 300;      // the ONE knob: raise if lyrics lag, lower if they rush
const unsigned long REFRESH_INTERVAL = 3300000;  // refresh token every 55 min

void clearLyrics() {
  lyricsLocked = true;
  lyricCount  = 0;
  currentLine = -1;
  lyricsLocked = false;
}

// ---------- helpers ----------

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
    if (c >= 32 && c <= 126) out += c;   // keep only printable ASCII
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

// ---------- Spotify token ----------

bool refreshAccessToken() {
  Serial.println("[Spotify] Refreshing token...");
  client.setInsecure();
  if (!client.connect("accounts.spotify.com", 443)) {
    Serial.println("[Spotify] Token endpoint connection failed");
    return false;
  }

  String auth = base64Encode(String(CLIENT_ID) + ":" + String(CLIENT_SECRET));
  String body = "grant_type=refresh_token&refresh_token=" + String(REFRESH_TOKEN);

  client.println("POST /api/token HTTP/1.1");
  client.println("Host: accounts.spotify.com");
  client.println("Authorization: Basic " + auth);
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(body.length()));
  client.println("Connection: close");
  client.println();
  client.print(body);

  skipHeaders(client);
  String response = client.readString();
  client.stop();

  int jsonStart = response.indexOf('{');
  if (jsonStart == -1) { Serial.println("[Spotify] Bad token response"); return false; }
  response = response.substring(jsonStart);

  JsonDocument doc;
  deserializeJson(doc, response);
  accessToken = doc["access_token"].as<String>();

  if (accessToken.length() > 0) {
    Serial.println("[Spotify] Token OK");
    return true;
  }
  Serial.println("[Spotify] Token empty");
  return false;
}

// ---------- now playing ----------

struct NowPlaying {
  String track;
  String artist;
  int    progress_ms;
  int    duration_ms;
  int    fetch_latency_ms;
  bool   is_playing;
  bool   valid;
};

NowPlaying getNowPlaying() {
  NowPlaying np = {"", "", 0, 0, 0, false, false};

  client.setInsecure();
  if (!client.connect("api.spotify.com", 443)) {
    Serial.println("[Spotify] API connection failed");
    return np;
  }

  client.println("GET /v1/me/player/currently-playing HTTP/1.1");
  client.println("Host: api.spotify.com");
  client.println("Authorization: Bearer " + accessToken);
  client.println("Connection: close");
  client.println();
  unsigned long sentAt = millis();

  String statusLine = skipHeaders(client);

  if (statusLine.indexOf("204") > 0) {
    Serial.println("[Spotify] Nothing playing");
    client.stop();
    return np;
  }
  if (statusLine.indexOf("401") > 0) {
    Serial.println("[Spotify] Token expired, refreshing...");
    client.stop();
    refreshAccessToken();
    return np;
  }

  String response = client.readString();
  np.fetch_latency_ms = (int)(millis() - sentAt);
  client.stop();

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

// ---------- lyrics ----------

void parseSyncedLyrics(String synced) {
  lyricsLocked = true;          // block the reader on core 1 while we rewrite
  lyricCount  = 0;
  currentLine = -1;

  int pos = 0;
  while (pos < (int)synced.length() && lyricCount < MAX_LINES) {
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

    lyricTime[lyricCount] = ms;
    lyricText[lyricCount] = asciiOnly(text);   // empty lines kept on purpose (clears display on gaps)
    lyricCount++;
  }
  Serial.printf("[Lyrics] Parsed %d lines\n", lyricCount);
  lyricsLocked = false;         // done, reader may resume
}

// minimal HTTP GET to LRCLIB; returns body on 200, "" otherwise
String lrclibGet(String path) {
  client.setInsecure();
  if (!client.connect("lrclib.net", 443)) { client.stop(); return ""; }
  client.setTimeout(10000);

  client.println("GET " + path + " HTTP/1.1");
  client.println("Host: lrclib.net");
  client.println("User-Agent: ESP32LyricsDisplay/1.0");
  client.println("Connection: close");
  client.println();

  unsigned long t = millis();
  while (client.available() == 0) {
    if (millis() - t > 8000) { client.stop(); return ""; }
    delay(10);
  }

  String status = skipHeaders(client);
  if (status.indexOf("200") < 0) { client.stop(); return ""; }   // 404 = no match

  const size_t MAX_BYTES = 40000;
  String resp = "";
  resp.reserve(MAX_BYTES + 16);
  t = millis();
  while ((client.connected() || client.available()) && millis() - t < 10000) {
    while (client.available()) {
      char c = (char)client.read();
      if (resp.length() < MAX_BYTES) resp += c;
      t = millis();
    }
    if (resp.length() >= MAX_BYTES) break;
  }
  client.stop();
  return resp;
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
    if (objEnd < 0) break;   // truncated tail, stop

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

void getLyrics(String track, String artist, int durationMs) {
  clearLyrics();                  // blank the display immediately while we fetch

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

// ---------- network task (core 0): all blocking I/O lives here ----------

void networkTask(void* pv) {
  for (;;) {
    unsigned long now = millis();

    if (now - lastRefresh > REFRESH_INTERVAL) {
      refreshAccessToken();
      lastRefresh = now;
    }

    if (now - lastPoll > POLL_INTERVAL) {
      lastPoll = now;
      NowPlaying np = getNowPlaying();
      if (np.valid) {


        // re-anchor the local playback clock (compensate ~half the round trip)
        baseProgressMs = np.progress_ms + np.fetch_latency_ms / 2;
        baseAtMs       = millis();
        playing        = np.is_playing;

        if (np.track != lastTrack) {
          lastTrack = np.track;
          Serial.println("\n>>> " + np.track + " - " + np.artist);
          getLyrics(np.track, np.artist, np.duration_ms);
        }
      }
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);   // yield to idle task / feed the watchdog
  }
}

// ---------- main ----------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\nESP32 Spotify Lyrics Display");
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  refreshAccessToken();
  lastRefresh = millis();
renderer.begin();
screenManager.begin();
  // all networking runs on core 0; core 1 (loop) stays free for smooth lyric timing
  xTaskCreatePinnedToCore(networkTask, "net", 16384, NULL, 1, NULL, 0);
}

// loop() runs on core 1 and does ONLY the advancer — it never blocks on the network
void loop() {
  unsigned long now = millis();

  renderer.poll();       // was: throttled queryTermSize() — backend now owns its own housekeeping cadence
  screenManager.tick();  // polls input, updates + renders whichever screen is active

  // Lyric-line tracking keeps running in the background even though
  // MusicScreen is a stub — currentLine stays correct and ready for M4,
  // which will move this computation into services/spotify/LyricsSync and
  // have MusicScreen read it through the state model instead of main.cpp
  // reaching into these globals directly.
  static unsigned long lastLineCheck = 0;
  if (playing && lyricCount > 0 && !lyricsLocked && now - lastLineCheck > 50) {
    lastLineCheck = now;

    long est = baseProgressMs + (long)(now - baseAtMs) + Config::SyncOffsetMs;

    int idx = -1;
    for (int i = 0; i < lyricCount; i++) {
      if (lyricTime[i] <= est) idx = i;
      else break;                       // timestamps are sorted, so stop early
    }

    if (idx >= 0 && idx != currentLine) {
      currentLine = idx;
      // Drawing removed here on purpose — MusicScreen (M4) will read
      // currentLine/lyricText via the state model and draw itself.
    }
  }
  delay(5);
}