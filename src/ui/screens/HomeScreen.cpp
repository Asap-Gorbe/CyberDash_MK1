//
// Created by NoteBook on 7/4/2026.
//
#include "HomeScreen.h"
#include <Arduino.h>

void HomeScreen::onEnter() {
    Serial.println("[Screen] Home");
}

void HomeScreen::onExit() {}

void HomeScreen::render(Renderer& renderer) {
    renderer.drawCentered(renderer.height() / 2, "HOME (n:next b:back)", Theme::TextSize::Hero);
}