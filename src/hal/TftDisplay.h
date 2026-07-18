#pragma once
#include "IDisplay.h"
#include <TFT_eSPI.h>

class TftDisplay : public IDisplay {
public:
    void begin() override;
    void clear() override;
    int width() override;
    int height() override;
    void drawText(int x, int y, const String& text, int size, TextAlign align) override;

private:
    TFT_eSPI _tft;
};