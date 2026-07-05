#pragma once
#include "ui/Screen.h"

// Stub for M3 — proves screen switching works. M4 wires this up to read
// lyric state (currently tracked by main.cpp's networkTask/loop) through
// the SpotifyService + state model instead of a placeholder string.
class MusicScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void render(Renderer& renderer) override;
    bool handleInput(InputEvent event) override;
};