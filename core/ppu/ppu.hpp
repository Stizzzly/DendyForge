#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ines/ines.hpp"

namespace dendyforge
{

class Cartridge;

class PPU
{
public:
    PPU();

    struct ScrollAddressState
    {
        std::uint16_t vramAddress;
        std::uint16_t temporaryAddress;
        std::uint8_t fineX;
        bool writeLatch;
    };

    void ConnectCartridge(Cartridge* cartridge);

    void Clock();
    // The /NMI line level: high while the VBlank flag and the NMI enable
    // are both set. The Console samples this once per CPU cycle and the
    // CPU latches an NMI on a rising edge.
    bool NmiLineLevel() const;
    bool ConsumeFrameComplete();
    void RenderBackground();
    void RenderSprites();

    const std::array<std::uint32_t, 256 * 240>& FrameBuffer() const;
    ScrollAddressState AddressState() const;

    // The beam counters as observed between dots: the scanline being
    // rendered (-1 pre-render through 260) and the next dot to execute.
    std::int16_t Scanline() const { return m_scanline; }
    std::int16_t Cycle() const { return m_cycle; }

    std::uint8_t CpuRead(std::uint16_t address);
    void CpuWrite(std::uint16_t address, std::uint8_t data);

    std::uint8_t PpuRead(std::uint16_t address);
    void PpuWrite(std::uint16_t address, std::uint8_t data);

private:
    struct BackgroundFetchState
    {
        std::uint8_t nametableByte{0};
        std::uint8_t attribute{0};
        std::uint8_t lowPlane{0};
        std::uint8_t highPlane{0};
        std::uint16_t patternShiftLow{0};
        std::uint16_t patternShiftHigh{0};
        std::uint16_t attributeShiftLow{0};
        std::uint16_t attributeShiftHigh{0};
    };

    struct ScanlineSprite
    {
        std::uint8_t index{0};
        std::uint8_t x{0};
        std::uint8_t attributes{0};
        std::uint8_t lowPlane{0};
        std::uint8_t highPlane{0};
    };

    std::uint16_t NormalizeAddress(std::uint16_t address) const;
    std::uint16_t NametableAddress(std::uint16_t address) const;
    std::uint8_t PaletteAddress(std::uint16_t address) const;
    void BeginFrame();
    void RenderBackgroundPixel(std::uint16_t screenY, std::uint16_t screenX);
    void RenderBackgroundScanline(std::uint16_t screenY);
    void RenderSpritePixel(std::uint16_t screenY, std::uint16_t screenX);
    void RenderSpritesScanline(std::uint16_t screenY);
    void EvaluateSpritesForScanline(std::uint16_t screenY);
    void FetchScanlineSprites(std::uint16_t screenY);
    // Hardware sprite pipeline: per-dot secondary-OAM clear (dots 1-64),
    // byte-wise evaluation with the overflow bug (dots 65-256) and the
    // eight-dot sprite fetches (dots 257-320) on visible scanlines.
    void ProcessSpriteEvaluation();
    void SpriteEvaluationStart();
    void SpriteEvaluationEnd();
    void LoadSpriteTileInfo();
    void ClockBackgroundFetch();
    void PrimeBackgroundFetch();
    void FetchNametableByte();
    void FetchAttribute();
    void FetchPatternLow();
    void FetchPatternHigh();
    void LoadBackgroundShifters();
    void ShiftBackgroundShifters();
    void IncrementVramAddress();
    void IncrementCoarseX(std::uint16_t& address) const;
    void IncrementY(std::uint16_t& address) const;
    void CopyHorizontalBits(std::uint16_t& destination,
                            std::uint16_t source) const;
    void CopyVerticalBits(std::uint16_t& destination,
                          std::uint16_t source) const;
    bool RenderingEnabled() const;
    std::uint32_t ColorFromPaletteIndex(std::uint8_t index) const;

    Cartridge* m_cartridge{nullptr};
    Mirroring m_mirroring{Mirroring::Horizontal};

    std::array<std::uint8_t, 2048> m_nametableRam{};
    std::array<std::uint8_t, 32> m_paletteRam{};
    std::array<std::uint8_t, 256> m_oam{};
    std::array<std::uint8_t, 8192> m_chrRam{};
    std::array<std::uint32_t, 256 * 240> m_frameBuffer{};
    std::array<bool, 256 * 240> m_backgroundOpaque{};
    BackgroundFetchState m_backgroundFetch{};
    std::array<ScanlineSprite, 8> m_scanlineSprites{};
    std::size_t m_scanlineSpriteCount{0};
    // Secondary OAM (32 bytes) plus the evaluation state machine variables.
    // The sprite address is split into H (OAM entry) and L (byte in entry),
    // mirroring the hardware counters whose interplay produces the sprite
    // overflow bug.
    std::array<std::uint8_t, 32> m_secondaryOam{};
    std::uint8_t m_oamCopyBuffer{0};
    std::uint8_t m_secondaryOamAddress{0};
    std::uint8_t m_spriteAddrH{0};
    std::uint8_t m_spriteAddrL{0};
    bool m_spriteInRange{false};
    bool m_sprite0Added{false};
    bool m_sprite0Visible{false};
    bool m_oamCopyDone{false};
    std::uint8_t m_overflowBugCounter{0};
    std::uint8_t m_spriteFetchIndex{0};

    std::uint8_t m_control{0};
    std::uint8_t m_mask{0};
    std::uint8_t m_status{0};
    std::uint8_t m_oamAddress{0};
    std::uint8_t m_dataBuffer{0};
    // PPU open-bus latch: holds the last value driven onto the CPU data
    // bus by any $2000-$2007 read or write. The capacitor leaks: without
    // an access for about half a second of PPU dots the latch reads zero
    // (blargg ppu_open_bus decay test).
    std::uint8_t m_openBusLatch{0};
    std::uint32_t m_openBusDecayDots{0};
    // The PPU scrolling registers conventionally call these v, t, and x.
    // CPU register writes prepare t and x; only a completed $2006 write
    // copies t into v. The renderer will progressively take ownership of v.
    std::uint16_t m_vramAddress{0}; // v: current VRAM address
    std::uint16_t m_temporaryAddress{0}; // t: temporary VRAM address
    std::uint8_t m_fineX{0}; // x: fine horizontal scroll
    bool m_writeLatch{false};
    // Set by a $2002 read landing one dot before the VBlank flag-set dot;
    // suppresses the flag and the NMI for that frame only.
    bool m_suppressVblank{false};
    bool m_frameComplete{false};
    bool m_oddFrame{false};
    std::int16_t m_scanline{-1};
    std::int16_t m_cycle{0};
};

} // namespace dendyforge
