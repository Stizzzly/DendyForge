#include "controller.hpp"

namespace dendyforge
{

void Controller::SetButton(Button button, bool pressed)
{
    m_buttons[static_cast<std::size_t>(button)] = pressed;
}

void Controller::Write(std::uint8_t data)
{
    const bool strobe = (data & 0x01) != 0;
    if (strobe)
    {
        LatchButtons();
    }
    else if (m_strobe)
    {
        LatchButtons();
    }

    m_strobe = strobe;
}

std::uint8_t Controller::Read()
{
    if (m_strobe)
    {
        return m_buttons[static_cast<std::size_t>(Button::A)] ? 1 : 0;
    }

    const std::uint8_t value = m_shiftRegister & 0x01;
    m_shiftRegister = (m_shiftRegister >> 1) | 0x80;
    return value;
}

void Controller::LatchButtons()
{
    m_shiftRegister = 0;
    for (std::size_t index = 0; index < m_buttons.size(); ++index)
    {
        m_shiftRegister |= static_cast<std::uint8_t>(m_buttons[index]) << index;
    }
}

} // namespace dendyforge
