#pragma once
#include "ui/Screen.h"
#include "app/AppState.h"
class HomeScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    bool update() override;
    void render(Renderer& renderer) override;

private:
    void refresh();
    String _timeText = "--:--";
    String _musicSummary = "";
    unsigned long _lastSeenMusicVersion = 0;
};