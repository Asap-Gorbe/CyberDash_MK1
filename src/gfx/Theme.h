#pragma once
#include <Arduino.h>

namespace Theme {

    namespace Color {
        constexpr uint16_t Background = 0x0000;
        constexpr uint16_t Primary    = 0x07E0;
        constexpr uint16_t Dim        = 0x03E0;
        constexpr uint16_t Accent     = 0xFFE0;
    }

    enum class TextSize {
        Small = 1,
        Body  = 2,
        Large = 3,
        Hero  = 4,
    };

    constexpr int SpacingUnit = 8;

}  // namespace Theme