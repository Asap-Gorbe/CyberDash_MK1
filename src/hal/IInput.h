#pragma once
enum class InputEvent { None, Next, Select, Back };
class IInput {
public:
    virtual ~IInput() = default;
    virtual void begin() = 0;
    virtual InputEvent poll() = 0;
};