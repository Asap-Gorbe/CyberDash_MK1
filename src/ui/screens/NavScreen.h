#pragma once
#include "ui/Screen.h"

// Stub for M3 — proves screen switching works. M6 wires this up to
// NavigationService and the mini-map rendering.
class NavScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void render(Renderer& renderer) override;
};