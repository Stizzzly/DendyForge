#pragma once

#include <array>
#include <cstdint>

#include "ines/ines.hpp"

namespace dendyforge
{

class Cartridge;

class PPU
{
public:
    void ConnectCartridge(Cartridge* cartridge);

    void Clock();
    bool PollNmi();

    std::uint8_t CpuRead(std::uint16_t address);
    void CpuWrite(std::uint16_t address, std::uint8_t data);

    std::uint8_t PpuRead(std::uint16_t address);
    void PpuWrite(std::uint16_t address, std::uint8_t data);

private:
    std::uint16_t NormalizeAddress(std::uint16_t address) const;
    std::uint16_t NametableAddress(std::uint16_t address) const;
    std::uint8_t PaletteAddress(std::uint16_t address) const;
    void IncrementVramAddress();

    Cartridge* m_cartridge{nullptr};
    Mirroring m_mirroring{Mirroring::Horizontal};

    std::array<std::uint8_t, 2048> m_nametableRam{};
    std::array<std::uint8_t, 32> m_paletteRam{};
    std::array<std::uint8_t, 256> m_oam{};

    std::uint8_t m_control{0};
    std::uint8_t m_mask{0};
    std::uint8_t m_status{0};
    std::uint8_t m_oamAddress{0};
    std::uint8_t m_dataBuffer{0};
    std::uint16_t m_vramAddress{0};
    std::uint16_t m_temporaryAddress{0};
    std::uint8_t m_fineX{0};
    bool m_writeLatch{false};
    bool m_nmiPending{false};
    std::int16_t m_scanline{-1};
    std::int16_t m_cycle{0};
};

} // namespace dendyforge
