#pragma once

#include <array>
#include <cstdint>

#include "../cpu/cpu_bus.hpp"
#include "../apu/apu.hpp"
#include "../controller/controller.hpp"
#include "../ppu/ppu.hpp"

namespace dendyforge
{

class Cartridge;

class Bus : public CpuBus
{
public:
    Bus();

    void InsertCartridge(Cartridge* cartridge);

    std::uint8_t CpuRead(std::uint16_t address) override;
    void CpuWrite(std::uint16_t address, std::uint8_t data) override;

    void ClockPpu();
    bool PpuNmiLine() const;
    void BeginPendingDma(bool cpuCycleIsOdd);
    bool ConsumeDmaStallCycle();

    PPU& VideoProcessor();
    APU& AudioProcessor();
    Controller& PrimaryController();

private:
    Cartridge* m_cartridge{nullptr};
    PPU m_ppu;
    APU m_apu;
    Controller m_controller1;
    bool m_dmaPending{false};
    std::uint16_t m_dmaStallCycles{0};
    std::uint16_t m_dmaAlignmentCycles{0};
    std::uint8_t m_dmaPage{0};
    std::uint8_t m_dmaOffset{0};
    bool m_dmaReadPhase{true};
    std::uint8_t m_dmaValue{0};

    std::array<std::uint8_t, 2048> m_cpuRam{};
};

}
