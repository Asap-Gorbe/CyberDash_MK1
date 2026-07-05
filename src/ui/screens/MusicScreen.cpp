//
// Created by NoteBook on 7/4/2026.
//
#include "MusicScreen.h"
#include <Arduino.h>

void MusicScreen::onEnter() {
    Serial.println("[Screen] Music (stub)");
}

void MusicScreen::onExit() {}

void MusicScreen::render(Renderer& renderer) {
    renderer.drawCentered(renderer.height() / 2, "MUSIC (stub - M4 wires lyrics)", Theme::TextSize::Hero);
}

bool MusicScreen::handleInput(InputEvent event) {
    if (event == InputEvent::Select) {
        Serial.println("[Screen] Music: Select pressed (no-op stub)");
        return true;
    }
    return false;
}