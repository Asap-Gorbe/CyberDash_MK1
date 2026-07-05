#pragma once
#include "hal/IDisplay.h"
#include "Theme.h"

class Renderer {
public:
    explicit Renderer(IDisplay& display) : _display(display) {}

    void begin() { _display.begin(); }
    void clear() { _display.clear(); }
    void poll()  { _display.poll(); }

    void drawCentered(int row, const String& text, Theme::TextSize size = Theme::TextSize::Hero) {
        _display.drawText(0, row, text, static_cast<int>(size), TextAlign::Center);
    }

    int width()  { return _display.width(); }
    int height() { return _display.height(); }

private:
    IDisplay& _display;
};