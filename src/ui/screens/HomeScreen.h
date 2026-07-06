#pragma once
#include "ui/Screen.h"

// Home is always index 0 in the screens array — ScreenManager's Back
// behavior depends on that convention.
class HomeScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void render(Renderer& renderer) override;
};