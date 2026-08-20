#include <doctest/doctest.h>

#include <array>

#include "cpu/cpu6502.hpp"

namespace
{

class MemoryCpuBus final : public dendyforge::CpuBus
{
public:
    std::uint8_t CpuRead(std::uint16_t address) override
    {
        return memory[address];
    }

    void CpuWrite(std::uint16_t address, std::uint8_t data) override
    {
        memory[address] = data;
    }

    std::array<std::uint8_t, 65536> memory{};
};

void CompleteInstruction(dendyforge::CPU6502& cpu)
{
    cpu.Clock();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}

} // namespace

TEST_CASE("CPU6502 works with a platform-specific CpuBus")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x8000] = 0xF8; // SED
    bus.memory[0x8001] = 0x18; // CLC
    bus.memory[0x8002] = 0xA9; // LDA #$45
    bus.memory[0x8003] = 0x45;
    bus.memory[0x8004] = 0x69; // ADC #$55
    bus.memory[0x8005] = 0x55;

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    CompleteInstruction(cpu);
    CompleteInstruction(cpu);
    CompleteInstruction(cpu);
    CompleteInstruction(cpu);

    CHECK(cpu.Accumulator() == 0x00);
    CHECK(cpu.GetFlag(dendyforge::CPU6502::Flags::N));
    CHECK_FALSE(cpu.GetFlag(dendyforge::CPU6502::Flags::Z));
    CHECK(cpu.GetFlag(dendyforge::CPU6502::Flags::V));
    CHECK(cpu.GetFlag(dendyforge::CPU6502::Flags::C));
}
