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

TEST_CASE("CPU6502 applies page-crossing and branch cycle penalties")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0xF9;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x80F9] = 0xA9; // LDA #$01
    bus.memory[0x80FA] = 0x01;
    bus.memory[0x80FB] = 0xD0; // BNE $8100
    bus.memory[0x80FC] = 0x03;
    bus.memory[0x8100] = 0xA2; // LDX #$01
    bus.memory[0x8101] = 0x01;
    bus.memory[0x8102] = 0xBD; // LDA $02FF,X
    bus.memory[0x8103] = 0xFF;
    bus.memory[0x8104] = 0x02;
    bus.memory[0x0300] = 0x5A;

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    CompleteInstruction(cpu);
    cpu.Clock();
    CHECK(cpu.ProgramCounter() == 0x8100);
    CHECK(cpu.Cycles() == 3);
    CompleteInstruction(cpu);

    CompleteInstruction(cpu);
    cpu.Clock();
    CHECK(cpu.Cycles() == 4);
    CompleteInstruction(cpu);
    CHECK(cpu.Accumulator() == 0x5A);
}

TEST_CASE("CPU6502 wraps indirect JMP reads at page boundaries")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x8000] = 0x6C; // JMP ($00FF)
    bus.memory[0x8001] = 0xFF;
    bus.memory[0x8002] = 0x00;
    bus.memory[0x00FF] = 0x34;
    bus.memory[0x0000] = 0x90;
    bus.memory[0x0100] = 0x91;

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    CompleteInstruction(cpu);
    CHECK(cpu.ProgramCounter() == 0x9034);
}

TEST_CASE("CPU6502 gives unknown opcodes a finite execution time")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x8000] = 0x02;

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    cpu.Clock();

    CHECK(cpu.ProgramCounter() == 0x8001);
    CHECK(cpu.Cycles() == 1);
}

TEST_CASE("CPU6502 stack and subroutine instructions preserve their stack contract")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x8000] = 0xA9; // LDA #$42
    bus.memory[0x8001] = 0x42;
    bus.memory[0x8002] = 0x48; // PHA
    bus.memory[0x8003] = 0x08; // PHP
    bus.memory[0x8004] = 0x20; // JSR $800A
    bus.memory[0x8005] = 0x0A;
    bus.memory[0x8006] = 0x80;
    bus.memory[0x8007] = 0xEA; // NOP
    bus.memory[0x800A] = 0x60; // RTS

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    CompleteInstruction(cpu);

    cpu.Clock();
    CHECK(cpu.Cycles() == 2);
    CHECK(bus.memory[0x01FD] == 0x42);
    CompleteInstruction(cpu);

    CompleteInstruction(cpu);
    CHECK((bus.memory[0x01FC] & 0x30) == 0x30);

    cpu.Clock();
    CHECK(cpu.Cycles() == 5);
    CHECK(bus.memory[0x01FB] == 0x80);
    CHECK(bus.memory[0x01FA] == 0x06);
    CompleteInstruction(cpu);
    CHECK(cpu.ProgramCounter() == 0x800A);

    CompleteInstruction(cpu);
    CHECK(cpu.ProgramCounter() == 0x8007);
    CHECK(cpu.StackPointer() == 0xFB);
}

TEST_CASE("CPU6502 services BRK, IRQ, and NMI through the stack and vectors")
{
    MemoryCpuBus bus;
    bus.memory[0xFFFC] = 0x00;
    bus.memory[0xFFFD] = 0x80;
    bus.memory[0x8000] = 0x00; // BRK
    bus.memory[0x8001] = 0xEA; // BRK padding byte
    bus.memory[0xFFFE] = 0x00;
    bus.memory[0xFFFF] = 0x90;
    bus.memory[0xFFFA] = 0x00;
    bus.memory[0xFFFB] = 0xA0;
    bus.memory[0x9000] = 0x40; // RTI

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    cpu.Clock();
    CHECK(cpu.Cycles() == 6);
    CHECK(cpu.ProgramCounter() == 0x9000);
    CHECK(cpu.StackPointer() == 0xFA);
    CHECK(bus.memory[0x01FD] == 0x80);
    CHECK(bus.memory[0x01FC] == 0x02);
    CHECK((bus.memory[0x01FB] & 0x30) == 0x30);
    CHECK(cpu.GetFlag(dendyforge::CPU6502::Flags::I));

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    CompleteInstruction(cpu);
    CHECK(cpu.ProgramCounter() == 0x8002);
    CHECK(cpu.StackPointer() == 0xFD);
    CHECK_FALSE(cpu.GetFlag(dendyforge::CPU6502::Flags::I));
    CHECK_FALSE(cpu.GetFlag(dendyforge::CPU6502::Flags::B));
    CHECK(cpu.GetFlag(dendyforge::CPU6502::Flags::U));

    cpu.IRQ();
    CHECK(cpu.Cycles() == 7);
    CHECK(cpu.ProgramCounter() == 0x9000);
    CHECK(bus.memory[0x01FD] == 0x80);
    CHECK(bus.memory[0x01FC] == 0x02);
    CHECK((bus.memory[0x01FB] & 0x30) == 0x20);

    cpu.Reset();
    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    cpu.SetFlag(dendyforge::CPU6502::Flags::I, true);
    cpu.IRQ();
    CHECK(cpu.Cycles() == 0);
    CHECK(cpu.ProgramCounter() == 0x8000);

    cpu.NMI();
    CHECK(cpu.Cycles() == 7);
    CHECK(cpu.ProgramCounter() == 0xA000);
    CHECK(bus.memory[0x01FD] == 0x80);
    CHECK(bus.memory[0x01FC] == 0x00);
    CHECK((bus.memory[0x01FB] & 0x30) == 0x20);
}
