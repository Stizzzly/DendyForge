#pragma once

#include <array>
#include <cstdint>

namespace dendyforge
{

// The Zapper light gun on controller port 2. Games read $4017 repeatedly
// during rendering: bit 3 is the light sensor (0 = light detected, 1 = no
// light) and bit 4 is the trigger (1 = pulled).
//
// The photodiode sees the CRT beam sweeping the aimed spot: light is
// reported only while the beam is on the aimed scanline past the aimed
// column and the pixel there is bright. Games that poll every scanline
// (and multiple times per scanline) reconstruct the aim position from
// exactly these conditions.
class Zapper
{
public:
    static constexpr std::uint16_t ScreenWidth = 256;
    static constexpr std::uint16_t ScreenHeight = 240;

    // Aims at a pixel; coordinates outside the screen mean the gun points
    // away and no light is ever reported.
    void SetConnected(bool connected);
    void SetAim(int x, int y);
    void SetTrigger(bool pulled);

    std::uint8_t ReadPort(
        std::int16_t beamScanline,
        std::int16_t beamCycle,
        const std::array<std::uint32_t, ScreenWidth * ScreenHeight>&
            frameBuffer) const;

private:
    bool m_connected{false};
    bool m_aimValid{false};
    std::uint16_t m_aimX{0};
    std::uint16_t m_aimY{0};
    bool m_triggerPulled{false};
};

} // namespace dendyforge
