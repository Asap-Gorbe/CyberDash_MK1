#include "MusicScreen.h"
#include "app/AppState.h"
#include <Arduino.h>

void MusicScreen::onEnter() {
    Serial.println("[Screen] Music");
    refresh();  // pull whatever's already known immediately — don't wait for a version bump
}

void MusicScreen::onExit() {}

bool MusicScreen::update() {
    AppState::MusicSnapshot snapshot = AppState::getMusic();
    if (snapshot.version == _lastSeenVersion) return false;  // nothing changed, skip the redraw
    refresh(snapshot);
    return true;
}

void MusicScreen::refresh() {
    refresh(AppState::getMusic());
}

void MusicScreen::refresh(const AppState::MusicSnapshot& snapshot) {
    _lastSeenVersion = snapshot.version;

    if (!snapshot.playing) {
        _displayText = "MUSIC (waiting for playback...)";
    } else if (!snapshot.hasSyncedLyrics) {
        _displayText = snapshot.artist + " - " + snapshot.track;
    } else {
        _displayText = snapshot.currentLyricLine;
    }
}

void MusicScreen::render(Renderer& renderer) {
    renderer.beginFrame();
    renderer.drawCentered(renderer.height() / 2, _displayText, Theme::TextSize::Large);
}

bool MusicScreen::handleInput(InputEvent event) {
    if (event == InputEvent::Select) {
        Serial.println("[Screen] Music: Select pressed (playback controls arrive in M7)");
        return true;
    }
    return false;
}