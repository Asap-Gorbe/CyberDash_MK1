#pragma once
#include "ui/Screen.h"
#include "app/AppState.h"
class NavScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    bool update() override;
    void render(Renderer& renderer) override;
    bool handleInput(InputEvent event) override;

private:
    void refresh();
    void refresh(const AppState::NavSnapshot& snapshot);
    unsigned long _lastSeenVersion = 0;
    String _displayText = "NAV (waiting for phone...)";
};