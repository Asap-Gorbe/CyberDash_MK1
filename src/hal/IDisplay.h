#pragma once
#include <Arduino.h>

enum class TextAlign { Left, Center, Right };

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual void begin() = 0;
    virtual void clear() = 0;

    virtual int width() = 0;
    virtual int height() = 0;

    virtual void drawText(int x, int y, const String& text, int size, TextAlign align) = 0;

    virtual void poll() {}
};