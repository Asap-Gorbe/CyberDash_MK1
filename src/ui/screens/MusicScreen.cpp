#include "MusicScreen.h"
#include "app/AppState.h"
#include <Arduino.h>

void MusicScreen::onEnter() {
    Serial.println("[Screen] Music");
    refresh();  // pull whatever's already known immediately — don't wait for a version bump
}

void MusicScreen::onExit() {}

bool MusicScreen::update() {
    if (AppState::music.version == _lastSeenVersion) return false;  // nothing changed, skip the redraw
    refresh();
    return true;
}

void MusicScreen::refresh() {
    _lastSeenVersion = AppState::music.version;

    if (!AppState::music.playing) {
        _displayText = "MUSIC (waiting for playback...)";
    } else if (AppState::music.currentLyricLine.length() > 0) {
        _displayText = AppState::music.currentLyricLine;
    } else if (AppState::music.track.length() > 0) {
        // Playing, but no synced line yet — instrumental gap, or no match found.
        _displayText = AppState::music.artist + " - " + AppState::music.track;
    } else {
        _displayText = "MUSIC (waiting for playback...)";
    }
}

void MusicScreen::render(Renderer& renderer) {
    renderer.drawCentered(renderer.height() / 2, _displayText, Theme::TextSize::Hero);
}

bool MusicScreen::handleInput(InputEvent event) {
    if (event == InputEvent::Select) {
        Serial.println("[Screen] Music: Select pressed (playback controls arrive in M7)");
        return true;
    }
    return false;
}