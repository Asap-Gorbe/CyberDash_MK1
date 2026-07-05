#include "SerialInput.h"
#include <Arduino.h>

void SerialInput::begin() {
    // Nothing to init — Serial is already begun in main.cpp's setup().
}

InputEvent SerialInput::poll() {
    if (!Serial.available()) return InputEvent::None;

    char c = Serial.read();
    switch (c) {
        case 'n': case 'N': return InputEvent::Next;
        case 's': case 'S': return InputEvent::Select;
        case 'b': case 'B': return InputEvent::Back;
        default:            return InputEvent::None;  // ignore newlines etc.
    }
}