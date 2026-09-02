#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "../cpu/cpu_bus.hpp"
#include "../cpu/cpu6502.hpp"
#include "../apu/apu.hpp"
#include "../controller/controller.hpp"
#include "../zapper/zapper.hpp"
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
    // Debug-only observation that must not trigger CPU-visible side effects
    // such as clearing PPU status or shifting controller input.
    std::optional<std::uint8_t> DebugPeekCpu(std::uint16_t address);

    void ClockPpu();
    bool PpuNmiLine() const;
    void BeginPendingDma();
    void ClockCpuCycle(CPU6502& cpu, bool getCycle);
    bool DmaActive() const;

    PPU& VideoProcessor();
    APU& AudioProcessor();
    Controller& PrimaryController();
    Zapper& SecondaryZapper();
    bool CartridgeIrqPending() const;

private:
    std::uint8_t DmaRead(std::uint16_t address, bool preserveInternalBus);
    std::uint8_t DmaExternalRead(
        std::uint16_t address, bool preserveInternalBus);
    void DmaDummyRead();

    Cartridge* m_cartridge{nullptr};
    PPU m_ppu;
    APU m_apu;
    Controller m_controller1;
    Zapper m_zapper;
    bool m_dmaPending{false};
    bool m_oamDmaActive{false};
    bool m_oamWritePending{false};
    std::uint16_t m_oamBytesTransferred{0};
    std::uint8_t m_dmaPage{0};
    std::uint8_t m_dmaOffset{0};
    std::uint8_t m_dmaValue{0};
    bool m_dmcDmaActive{false};
    bool m_dmcDmaAbortOnly{false};
    bool m_dmaNeedsHalt{false};
    bool m_dmcNeedsDummyRead{false};
    std::uint16_t m_dmcDmaAddress{0};
    bool m_cpuHaltedForDma{false};
    std::uint16_t m_haltedCpuAddress{0};
    std::uint16_t m_previousDmaReadAddress{0};
    bool m_dmaInternalReadsEnabled{false};
    bool m_cpuAccessObserved{false};
    bool m_lastCpuAccessWasWrite{false};
    std::uint16_t m_lastCpuAddress{0};

    // The 2A03 data bus is a dynamic latch. Unmapped reads retain this
    // value, while partially driven I/O registers replace only the bits
    // that they physically drive.
    std::uint8_t m_cpuDataBus{0};
    std::uint8_t m_cpuInternalDataBus{0};
    bool m_dmaBusAccess{false};

    std::array<std::uint8_t, 2048> m_cpuRam{};
};

}
