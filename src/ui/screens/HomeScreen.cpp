#include "HomeScreen.h"
#include "hal/Clock.h"
#include <Arduino.h>

void HomeScreen::onEnter() {
    Serial.println("[Screen] Home");
    refresh();  // pull current time + now-playing immediately, don't wait for a tick
}

void HomeScreen::onExit() {}

bool HomeScreen::update() {
    String newTime = Clock::nowFormatted();
    AppState::MusicSnapshot snapshot = AppState::getMusic();

    bool timeChanged = (newTime != _timeText);
    bool musicChanged = (snapshot.version != _lastSeenMusicVersion);

    if (!timeChanged && !musicChanged) return false;  // nothing to redraw

    _timeText = newTime;
    if (musicChanged) {
        _lastSeenMusicVersion = snapshot.version;
        _musicSummary = snapshot.playing
            ? (snapshot.artist + " - " + snapshot.track)
            : "Not playing";
    }
    return true;
}

void HomeScreen::refresh() {
    _timeText = Clock::nowFormatted();

    AppState::MusicSnapshot snapshot = AppState::getMusic();
    _lastSeenMusicVersion = snapshot.version;
    _musicSummary = snapshot.playing
        ? (snapshot.artist + " - " + snapshot.track)
        : "Not playing";
}

void HomeScreen::render(Renderer& renderer) {
    renderer.beginFrame();
    renderer.drawCentered(1, _timeText, Theme::TextSize::Hero);
    renderer.drawCentered(3, _musicSummary, Theme::TextSize::Body);
}