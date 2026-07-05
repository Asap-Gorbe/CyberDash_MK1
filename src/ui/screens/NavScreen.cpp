//
// Created by NoteBook on 7/4/2026.
//
#include "NavScreen.h"
#include <Arduino.h>

void NavScreen::onEnter() {
    Serial.println("[Screen] Nav (stub)");
}

void NavScreen::onExit() {}

void NavScreen::render(Renderer& renderer) {
    renderer.drawCentered(renderer.height() / 2, "NAV (stub - M6 wires map)", Theme::TextSize::Hero);
}