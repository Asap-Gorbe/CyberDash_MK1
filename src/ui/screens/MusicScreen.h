#pragma once
#include "ui/Screen.h"

class MusicScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    bool update() override;
    void render(Renderer& renderer) override;
    bool handleInput(InputEvent event) override;

private:
    void refresh();  // pulls current AppState::music into _displayText

    unsigned long _lastSeenVersion = 0;
    String _displayText = "MUSIC (waiting for playback...)";
};