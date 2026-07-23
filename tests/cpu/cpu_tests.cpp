#include "cpu_tests.hpp"
#include <iostream>
#include <iomanip>

#include "bus/bus.hpp"
#include "cartridge/cartridge.hpp"
#include "ines/ines_reader.hpp"
#include "cpu/cpu6502.hpp"

namespace
{

void ExecuteInstruction(dendyforge::CPU6502& cpu)
{
    cpu.Clock();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}
}
void TestFlags()
{
    std::cout << "\nFlags\n";

    dendyforge::CPU6502 cpu;

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, true);
    cpu.SetFlag(dendyforge::CPU6502::Flags::Z, true);

    std::cout
        << "Carry    : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
        << '\n';

    std::cout
        << "Zero     : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';

    cpu.SetFlag(dendyforge::CPU6502::Flags::C, false);

    std::cout << "\nAfter clearing Carry\n";

    std::cout
        << "Carry    : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
        << '\n';

    std::cout
        << "Zero     : "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';
}

void TestReset()
{
    std::cout << "\nReset\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    std::cout
        << "PC      = $"
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << cpu.ProgramCounter()
        << '\n';

    std::cout
        << "SP      = $"
        << std::setw(2)
        << static_cast<int>(cpu.StackPointer())
        << '\n';

    std::cout
        << "Status  = $"
        << std::setw(2)
        << static_cast<int>(cpu.Status())
        << '\n';

    std::cout
        << "Cycles  = "
        << std::dec
        << static_cast<int>(cpu.Cycles())
        << '\n';
}

void TestFetch()
{
    std::cout << "\nFetch\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    //
    // Сбрасываем стартовые 8 циклов Reset
    //
    while (cpu.Cycles() > 0)
        cpu.Clock();

    //
    // Выполняем Fetch первой инструкции
    //
    cpu.Clock();

    std::cout
        << "PC = $"
        << std::uppercase
        << std::hex
        << std::setw(4)
        << std::setfill('0')
        << cpu.ProgramCounter()
        << '\n';

    std::cout
        << "Opcode = $"
        << std::setw(2)
        << static_cast<int>(cpu.Opcode())
        << '\n';
}

void TestDecode()
{
    std::cout << "\nDecode\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    cpu.Clock();

    std::cout
        << "Opcode      = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Opcode())
        << '\n';

    std::cout
        << "Instruction = "
        << cpu.CurrentInstruction()
        << '\n';

    std::cout
        << "Cycles left = "
        << std::dec
        << static_cast<int>(cpu.Cycles())
        << '\n';
}

void TestLDA()
{
    std::cout << "\nLDA\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
    ExecuteInstruction(cpu); // SEI
    ExecuteInstruction(cpu); // CLD
    ExecuteInstruction(cpu); // LDX #$FF
    ExecuteInstruction(cpu); // TXS
    ExecuteInstruction(cpu); // JMP Main
    //
    // LDA #$42
    //
    ExecuteInstruction(cpu);

    std::cout
        << "A = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Accumulator())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestLDX()
{
    std::cout << "\nLDX\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstruction(cpu); // SEI
    ExecuteInstruction(cpu); // CLD
    ExecuteInstruction(cpu); // LDX #$FF
    ExecuteInstruction(cpu); // TXS
    ExecuteInstruction(cpu); // JMP Main

    ExecuteInstruction(cpu); // LDA
    ExecuteInstruction(cpu); // STA
    ExecuteInstruction(cpu); // LDX
    std::cout
        << "X = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.X())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestLDY()
{
    std::cout << "\nLDY\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstruction(cpu); // SEI
    ExecuteInstruction(cpu); // CLD
    ExecuteInstruction(cpu); // LDX #$FF
    ExecuteInstruction(cpu); // TXS
    ExecuteInstruction(cpu); // JMP Main

    ExecuteInstruction(cpu); // LDA
    ExecuteInstruction(cpu); // STA
    ExecuteInstruction(cpu); // LDX
    ExecuteInstruction(cpu); // STX
    ExecuteInstruction(cpu); // LDY
    std::cout
        << "Y = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Y())
        << '\n';

    std::cout
        << "Zero = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
        << '\n';

    std::cout
        << "Negative = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
        << '\n';
}

void TestStore()
{
    std::cout << "\nSTA,STX,STY\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/cpu_test.nes"))
    {
        std::cout << "Failed to load cpu_test.nes\n";
        return;
    }

    dendyforge::Cartridge cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    dendyforge::Bus bus;
    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);

    cpu.Reset();

    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }

    ExecuteInstruction(cpu); // SEI
    ExecuteInstruction(cpu); // CLD
    ExecuteInstruction(cpu); // LDX #$FF
    ExecuteInstruction(cpu); // TXS
    ExecuteInstruction(cpu); // JMP Main

    ExecuteInstruction(cpu); // LDA
    ExecuteInstruction(cpu); // STA

    ExecuteInstruction(cpu); // LDX #$11
    ExecuteInstruction(cpu); // STX

    ExecuteInstruction(cpu); // LDY #$22
    ExecuteInstruction(cpu); // STY

    std::uint8_t data = 0;

    data = bus.CpuRead(0x0200);

    std::cout
        << "$0200 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    data = bus.CpuRead(0x0201);

    std::cout
        << "$0201 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    data = bus.CpuRead(0x0202);

    std::cout
        << "$0202 = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(data)
        << '\n';

    std::cout << "\nSTA,STX,STY (ZP0)\n";

    ExecuteInstruction(cpu); // LDA #$AA
    ExecuteInstruction(cpu); // STA $00

    ExecuteInstruction(cpu); // LDX #$BB
    ExecuteInstruction(cpu); // STX $01

    ExecuteInstruction(cpu); // LDY #$CC
    ExecuteInstruction(cpu); // STY $02

    data = bus.CpuRead(0x0000);
    std::cout << "$0000 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0001);
    std::cout << "$0001 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0002);
    std::cout << "$0002 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';
}
void RunCpuTests()
{
    std::cout << "\n=== CPU Tests ===\n";

    TestFlags();
    TestReset();
    TestFetch();
    TestDecode();
    TestLDA();
    TestLDX();
    TestLDY();
    TestStore();
}

