#include "zapper.hpp"

namespace dendyforge
{

namespace
{

// Duck Hunt's detection frames draw pure white target boxes on black; the
// threshold separates the $20-$30 brights from everything darker.
constexpr std::uint32_t BrightPixelThreshold = 3 * 0x70;

bool IsBright(std::uint32_t pixel)
{
    const std::uint32_t red = (pixel >> 16) & 0xFF;
    const std::uint32_t green = (pixel >> 8) & 0xFF;
    const std::uint32_t blue = pixel & 0xFF;
    return red + green + blue >= BrightPixelThreshold;
}

} // namespace

void Zapper::SetAim(int x, int y)
{
    m_aimValid = x >= 0 && x < ScreenWidth && y >= 0 && y < ScreenHeight;
    m_aimX = static_cast<std::uint16_t>(x);
    m_aimY = static_cast<std::uint16_t>(y);
}

void Zapper::SetTrigger(bool pulled)
{
    m_triggerPulled = pulled;
}

std::uint8_t Zapper::ReadPort(
    std::int16_t beamScanline,
    std::int16_t beamCycle,
    const std::array<std::uint32_t, ScreenWidth * ScreenHeight>&
        frameBuffer) const
{
    std::uint8_t data = 0x08; // no light

    // The sensor fires while the beam sweeps the aimed spot and the
    // phosphor keeps glowing for the rest of that scanline: a read on the
    // aimed scanline, past the aimed column (so the pixel has been drawn
    // this frame), over a bright pixel reports light.
    if (m_aimValid &&
        beamScanline >= 0 && beamScanline < ScreenHeight &&
        static_cast<std::int16_t>(m_aimY) == beamScanline &&
        beamCycle > static_cast<std::int16_t>(m_aimX))
    {
        const std::uint32_t pixel =
            frameBuffer[m_aimY * ScreenWidth + m_aimX];
        if (IsBright(pixel))
        {
            data &= ~0x08;
        }
    }

    if (m_triggerPulled)
    {
        data |= 0x10;
    }

    return data;
}

} // namespace dendyforge
