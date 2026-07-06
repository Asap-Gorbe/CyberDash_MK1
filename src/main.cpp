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
#include "services/spotify/SpotifyService.h"
SerialDisplay display;
Renderer renderer(display);

SerialInput input;
HomeScreen homeScreen;
MusicScreen musicScreen;
NavScreen navScreen;
Screen* screens[] = { &homeScreen, &musicScreen, &navScreen };  // index 0 = Home, by convention
ScreenManager screenManager(renderer, input, screens, 3);
SpotifyService spotifyService;






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
  renderer.begin();
  screenManager.begin();
  spotifyService.begin();  // starts its own background task; handles its own initial token fetch internally
}

// loop() runs on core 1 and does ONLY the advancer — it never blocks on the network
void loop() {
  renderer.poll();
  screenManager.tick();
  spotifyService.tick();
  delay(5);
}