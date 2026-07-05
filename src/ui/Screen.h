#pragma once
#include "gfx/Renderer.h"
#include "hal/IInput.h"
class Screen {
public:
    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual void onExit() {}
    virtual bool update() {return false;}
    virtual void render(Renderer& renderer) = 0;
    virtual bool handleInput(InputEvent event) { (void)event; return false; }
};