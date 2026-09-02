#pragma once

#include <array>
#include <cstdint>

namespace dendyforge
{

class Controller
{
public:
    enum class Button : std::uint8_t
    {
        A,
        B,
        Select,
        Start,
        Up,
        Down,
        Left,
        Right
    };

    void SetButton(Button button, bool pressed);
    void Write(std::uint8_t data);
    void ClockPutCycle();
    std::uint8_t Read();

private:
    void LatchButtons();

    std::array<bool, 8> m_buttons{};
    std::uint8_t m_shiftRegister{0};
    bool m_strobe{false};
};

} // namespace dendyforge
