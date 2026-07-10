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
    } else if (snapshot.directions.length() > 0) {
        // Deliberately dumb for the first bench test — just surface whatever
        // text arrived. `title`'s exact meaning is app-dependent per Chronos's
        // own docs; real branching logic waits for a real drive's BLE traffic.
        _displayText = snapshot.directions;
    } else if (snapshot.title.length() > 0) {
        _displayText = snapshot.title;
    } else {
        _displayText = "ETA " + snapshot.eta;
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