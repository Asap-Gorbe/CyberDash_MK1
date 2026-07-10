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
#include "hal/Clock.h"
SerialDisplay display;
#include "hal/SimulatedBleNavSource.h"
#include "services/nav/NavigationService.h"
Renderer renderer(display);

SerialInput input;
HomeScreen homeScreen;
MusicScreen musicScreen;
NavScreen navScreen;
Screen* screens[] = { &homeScreen, &musicScreen, &navScreen };  // index 0 = Home, by convention
ScreenManager screenManager(renderer, input, screens, 3);
SpotifyService spotifyService;
SimulatedBleNavSource navSource;
NavigationService navigationService(navSource);





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
  Clock::begin();  // NTP sync starts here — WiFi must already be connected
  renderer.begin();
  screenManager.begin();
  spotifyService.begin();  // starts its own background task; handles its own initial token fetch internally
  navigationService.begin();
}

// loop() runs on core 1 and does ONLY the advancer — it never blocks on the network
void loop() {
  renderer.poll();
  screenManager.tick();
  spotifyService.tick();
  navigationService.tick();
  delay(5);
}