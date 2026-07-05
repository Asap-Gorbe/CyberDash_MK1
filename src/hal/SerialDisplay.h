// SerialDisplay.h
#pragma once
#include "IDisplay.h"

class SerialDisplay : public IDisplay {
public:
    void begin() override;
    void clear() override;
    int width() override;
    int height() override;
    void drawText(int x, int y, const String& text, int size, TextAlign align) override;
    void poll() override;

private:
    void queryTermSize();
    int _cols = 80;
    int _rows = 24;
    unsigned long _lastSizeQueryMs = 0;
    static constexpr unsigned long kSizeQueryIntervalMs = 2000;
};