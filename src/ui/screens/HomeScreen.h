#pragma once
#include "ui/Screen.h"

class HomeScreen : public Screen {
public:
    void onEnter() override;
    void onExit() override;
    void render(Renderer& renderer) override;
};