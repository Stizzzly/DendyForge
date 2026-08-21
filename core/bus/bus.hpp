#pragma once

#include <array>
#include <cstdint>

#include "../cpu/cpu_bus.hpp"
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
    bool PollPpuNmi();

    PPU& VideoProcessor();
    Controller& PrimaryController();

private:
    Cartridge* m_cartridge{nullptr};
    PPU m_ppu;
    Controller m_controller1;

    std::array<std::uint8_t, 2048> m_cpuRam{};
};

}
