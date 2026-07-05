// SerialDisplay.cpp
#include "SerialDisplay.h"

void SerialDisplay::begin() {
    Serial.print("\033[?25l");  // hide cursor
    queryTermSize();
}

void SerialDisplay::clear() {
    Serial.print("\033[2J\033[H");
}

int SerialDisplay::width()  { return _cols; }
int SerialDisplay::height() { return _rows; }

void SerialDisplay::drawText(int x, int y, const String& text, int size, TextAlign align) {
    (void)size;  // no-op on a terminal
    Serial.print("\033[2J");

    if (text.length() == 0) {
        Serial.print("\033[H");
        return;
    }

    int col = x;
    if (align == TextAlign::Center)      col = (_cols - (int)text.length()) / 2;
    else if (align == TextAlign::Right)  col = _cols - (int)text.length();
    if (col < 1) col = 1;

    int row = y + 1;
    if (row < 1) row = 1;

    Serial.printf("\033[%d;%dH", row, col);
    Serial.print(text);
}

void SerialDisplay::poll() {
    unsigned long now = millis();
    if (now - _lastSizeQueryMs > kSizeQueryIntervalMs) {
        _lastSizeQueryMs = now;
        queryTermSize();
    }
}

void SerialDisplay::queryTermSize() {
    while (Serial.available()) Serial.read();
    Serial.print("\033[999;999H\033[6n");

    unsigned long t = millis();
    String buf = "";
    while (millis() - t < 120) {
        if (Serial.available()) {
            char c = Serial.read();
            buf += c;
            if (c == 'R') break;
        }
    }

    int lb = buf.indexOf('['), sep = buf.indexOf(';'), end = buf.indexOf('R');
    if (lb >= 0 && sep > lb && end > sep) {
        int r = buf.substring(lb + 1, sep).toInt();
        int c = buf.substring(sep + 1, end).toInt();
        if (r > 5 && c > 5) { _rows = r; _cols = c; }
    }
}