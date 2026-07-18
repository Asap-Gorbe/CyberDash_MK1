#include "TftDisplay.h"
#include "gfx/Theme.h"

void TftDisplay::begin() {
    _tft.init();
    _tft.setRotation(1);  // landscape — matches the color test
    _tft.fillScreen(Theme::Color::Background);
}

void TftDisplay::clear() {
    _tft.fillScreen(Theme::Color::Background);
}

int TftDisplay::width()  { return _tft.width(); }
int TftDisplay::height() { return _tft.height(); }

void TftDisplay::drawText(int x, int y, const String& text, int size, TextAlign align) {
    _tft.setTextColor(Theme::Color::Primary, Theme::Color::Background);
    _tft.setTextSize(size);
    _tft.setTextWrap(false);  // we wrap manually below so each wrapped line
                               // can be centered on its own, instead of
                               // TFT_eSPI's default wrap (which restarts
                               // every overflow line at the left margin —
                               // that's what broke centering on long lines)

    const int charW = 6 * size;  // GLCD font base cell is 6x8px before scaling
    const int lineH = 8 * size + 2;
    int maxChars = width() / charW;
    if (maxChars < 1) maxChars = 1;

    // First pass: split into wrapped lines before drawing anything, so we
    // know the total block height and can center it around y.
    static constexpr int kMaxLines = 8;  // plenty for HUD/lyric-length text
    String lines[kMaxLines];
    int lineCount = 0;

    int start = 0;
    while (start < (int)text.length() && lineCount < kMaxLines) {
        int lineEnd = start;
        int lastSpace = -1;
        while (lineEnd < (int)text.length() && (lineEnd - start) < maxChars) {
            if (text[lineEnd] == ' ') lastSpace = lineEnd;
            lineEnd++;
        }
        if (lineEnd < (int)text.length() && lastSpace > start) lineEnd = lastSpace;

        String line = text.substring(start, lineEnd);
        line.trim();
        lines[lineCount++] = line;

        start = lineEnd;
        while (start < (int)text.length() && text[start] == ' ') start++;
    }

    // Second pass: now that we know lineCount, draw the whole block
    // centered around y instead of growing down from it.
    int row = y - (lineCount * lineH) / 2;
    for (int i = 0; i < lineCount; i++) {
        int textW = _tft.textWidth(lines[i]);
        int col = x;
        if (align == TextAlign::Center)      col = (width() - textW) / 2;
        else if (align == TextAlign::Right)  col = width() - textW;
        if (col < 0) col = 0;

        _tft.setCursor(col, row);
        _tft.print(lines[i]);
        row += lineH;
    }
}