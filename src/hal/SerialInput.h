#pragma once
#include "IInput.h"

class SerialInput : public IInput {
  public:
    void begin() override ;
    InputEvent poll() override ;
};