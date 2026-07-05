#pragma once
#include "Screen.h"
#include "hal/IInput.h"
#include "gfx/Renderer.h"
class ScreenManager {
public:
    ScreenManager(Renderer& renderer, IInput& input, Screen** screens, int count);
    void begin();
    void tick();
private:
    void switchTo(int newIndex);
    Renderer& _renderer;
    IInput& _input;
    Screen** _screens;
    int _count;
    int _index;
};