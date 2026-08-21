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
    struct ScrollAddressState
    {
        std::uint16_t vramAddress;
        std::uint16_t temporaryAddress;
        std::uint8_t fineX;
        bool writeLatch;
    };

    void ConnectCartridge(Cartridge* cartridge);

    void Clock();
    bool PollNmi();
    bool ConsumeFrameComplete();
    void RenderBackground();
    void RenderSprites();

    const std::array<std::uint32_t, 256 * 240>& FrameBuffer() const;
    ScrollAddressState AddressState() const;

    std::uint8_t CpuRead(std::uint16_t address);
    void CpuWrite(std::uint16_t address, std::uint8_t data);

    std::uint8_t PpuRead(std::uint16_t address);
    void PpuWrite(std::uint16_t address, std::uint8_t data);

private:
    std::uint16_t NormalizeAddress(std::uint16_t address) const;
    std::uint16_t NametableAddress(std::uint16_t address) const;
    std::uint8_t PaletteAddress(std::uint16_t address) const;
    void BeginFrame();
    void RenderBackgroundPixel(std::uint16_t screenY, std::uint16_t screenX);
    void RenderBackgroundScanline(std::uint16_t screenY);
    void RenderSpritesScanline(std::uint16_t screenY);
    void IncrementVramAddress();
    std::uint32_t ColorFromPaletteIndex(std::uint8_t index) const;

    Cartridge* m_cartridge{nullptr};
    Mirroring m_mirroring{Mirroring::Horizontal};

    std::array<std::uint8_t, 2048> m_nametableRam{};
    std::array<std::uint8_t, 32> m_paletteRam{};
    std::array<std::uint8_t, 256> m_oam{};
    std::array<std::uint8_t, 8192> m_chrRam{};
    std::array<std::uint32_t, 256 * 240> m_frameBuffer{};
    std::array<bool, 256 * 240> m_backgroundOpaque{};

    std::uint8_t m_control{0};
    std::uint8_t m_mask{0};
    std::uint8_t m_status{0};
    std::uint8_t m_oamAddress{0};
    std::uint8_t m_dataBuffer{0};
    // The PPU scrolling registers conventionally call these v, t, and x.
    // CPU register writes prepare t and x; only a completed $2006 write
    // copies t into v. The renderer will progressively take ownership of v.
    std::uint16_t m_vramAddress{0}; // v: current VRAM address
    std::uint16_t m_temporaryAddress{0}; // t: temporary VRAM address
    std::uint8_t m_fineX{0}; // x: fine horizontal scroll
    // Retained for the current direct renderer until it is replaced with the
    // live v/t/x background fetch pipeline.
    std::uint8_t m_scrollX{0};
    std::uint8_t m_scrollY{0};
    bool m_writeLatch{false};
    bool m_nmiPending{false};
    bool m_frameComplete{false};
    std::int16_t m_scanline{-1};
    std::int16_t m_cycle{0};
};

} // namespace dendyforge
