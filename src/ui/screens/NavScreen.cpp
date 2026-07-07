#include "NavScreen.h"
#include <Arduino.h>

void NavScreen::onEnter() {
    Serial.println("[Screen] Nav");
    refresh();
}

void NavScreen::onExit() {}

bool NavScreen::update() {
    AppState::NavSnapshot snapshot = AppState::getNav();
    if (snapshot.version == _lastSeenVersion) return false;
    refresh(snapshot);
    return true;
}

void NavScreen::refresh() {
    refresh(AppState::getNav());
}

void NavScreen::refresh(const AppState::NavSnapshot& snapshot) {
    _lastSeenVersion = snapshot.version;

    if (!snapshot.valid) {
        _displayText = "NAV (waiting for phone...)";
    } else if (snapshot.nextTurnDistanceM >= 0) {
        // A turn is coming up — that's the single most useful thing to
        // show while driving, ahead of ETA or destination.
        _displayText = "-> " + String(snapshot.nextTurnDistanceM) + "m " + snapshot.currentStreet;
    } else {
        // No turn pending — show where we are and when we'll arrive instead.
        _displayText = snapshot.currentStreet + " (ETA " + snapshot.eta + ")";
    }
}

void NavScreen::render(Renderer& renderer) {
    renderer.drawCentered(renderer.height() / 2, _displayText, Theme::TextSize::Hero);
}

bool NavScreen::handleInput(InputEvent event) {
    if (event == InputEvent::Select) {
        Serial.println("[Screen] Nav: Select pressed (no-op for now)");
        return true;
    }
    return false;
}