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

void ExecuteProgram(dendyforge::CPU6502& cpu,
                    int maxInstructions = 100)
{
    for (int i = 0; i < maxInstructions; i++)
    {
        ExecuteInstruction(cpu);
    }
}

dendyforge::CPU6502 CreateCPU(const std::string& romPath,
                              dendyforge::Bus& bus,
                              dendyforge::Cartridge& cartridge)
{
    dendyforge::INesReader reader;

    if (!reader.Load(romPath))
    {
        throw std::runtime_error("Failed to load " + romPath);
    }

    cartridge = dendyforge::Cartridge(
        reader.Header(),
        reader.TakePRGRom(),
        reader.TakeCHRRom());

    bus.InsertCartridge(&cartridge);

    dendyforge::CPU6502 cpu;
    cpu.ConnectBus(&bus);
    cpu.Reset();

    while (cpu.Cycles() > 0)
        cpu.Clock();

    return cpu;
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

void TestLoadInstructions()
{
    std::cout << "\nLoad Instructions\n";

    dendyforge::Bus bus;
    dendyforge::Cartridge cartridge({}, {}, {});

    auto cpu = CreateCPU(
        "tests/cpu/roms/load/load_test.nes",
        bus,
        cartridge);

    ExecuteProgram(cpu);

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
        << "\n\n";

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
        << "\n\n";

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

void TestStoreInstructions()
{
    std::cout << "\nSTA,STX,STY\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/store/store_test.nes"))
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

    ExecuteProgram(cpu);
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

    ExecuteProgram(cpu);
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

    std::cout << "\nSTA,STX,STY (ZPX/ZPY)\n";

    ExecuteProgram(cpu);
    data = bus.CpuRead(0x0015);
    std::cout << "$0015 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0023);
    std::cout << "$0023 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0037);
    std::cout << "$0037 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    std::cout << "\nZero Page Wrap\n";

    ExecuteProgram(cpu);
    data = bus.CpuRead(0x0008);
    std::cout << "$0008 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';

    data = bus.CpuRead(0x0010);
    std::cout << "$0010 = $"
          << std::hex
          << std::uppercase
          << std::setw(2)
          << std::setfill('0')
          << static_cast<int>(data)
          << '\n';
}

void TestIncrementInstructions()
{
    std::cout << "\nINX,INY,DEX,DEY\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/increment/increment_test.nes"))
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

    ExecuteProgram(cpu);
    std::cout
        << "X = $"
        << std::hex
        << std::uppercase
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.X())
        << '\n';

    std::cout
        << "Y = $"
        << std::hex
        << std::uppercase
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Y())
        << '\n';
}

void TestFlagInstructions()
{
    std::cout << "\nFlag Instructions\n";

    dendyforge::INesReader reader;

    if (!reader.Load("tests/cpu/roms/flags/flag_test.nes"))
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

    ExecuteProgram(cpu);

    std::cout << "I=" << cpu.GetFlag(dendyforge::CPU6502::Flags::I)
              << '\n';
    std::cout
        << "Carry = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
        << '\n';

    std::cout
        << "Interrupt = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::I)
        << '\n';

    std::cout
        << "Decimal = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::D)
        << '\n';

    std::cout
        << "Overflow = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::V)
        << '\n';

}

void TestTransferInstructions()
{
    std::cout << "\nTransfer Instructions\n";

    dendyforge::Bus bus;
    dendyforge::Cartridge cartridge({}, {}, {});

    auto cpu = CreateCPU(
        "tests/cpu/roms/transfer/transfer_test.nes",
        bus,
        cartridge);

    ExecuteProgram(cpu);

    std::cout
        << "A = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Accumulator())
        << '\n';

    std::cout
        << "X = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.X())
        << '\n';

    std::cout
        << "Y = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.Y())
        << '\n';

    std::cout
        << "SP = $"
        << std::uppercase
        << std::hex
        << std::setw(2)
        << std::setfill('0')
        << static_cast<int>(cpu.StackPointer())
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

void TestLogicalInstructions()
{
    std::cout << "\nLogical Instructions\n";

    dendyforge::Bus bus;
    dendyforge::Cartridge cartridge({}, {}, {});

    auto cpu = CreateCPU(
        "tests/cpu/roms/logical/logical_test.nes",
        bus,
        cartridge);

    ExecuteProgram(cpu);

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

    std::cout
        << "Overflow = "
        << cpu.GetFlag(dendyforge::CPU6502::Flags::V)
        << '\n';
}

void TestCompareInstructions()
{
    std::cout << "\nCompare Instructions\n";

    dendyforge::Bus bus;
    dendyforge::Cartridge cartridge({}, {}, {});

    auto cpu = CreateCPU(
        "tests/cpu/roms/compare/compare_test.nes",
        bus,
        cartridge);

    ExecuteProgram(cpu);

    std::cout << "Carry = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
              << '\n';

    std::cout << "Zero = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
              << '\n';

    std::cout << "Negative = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
              << '\n';
}

void RunADCTest(
    const std::string& rom,
    const std::string& title)
{
    std::cout << "\n" << title << '\n';

    dendyforge::Bus bus;
    dendyforge::Cartridge cartridge({}, {}, {});

    auto cpu = CreateCPU(rom, bus, cartridge);

    ExecuteProgram(cpu);

    std::cout << "A = $"
              << std::uppercase
              << std::hex
              << std::setw(2)
              << std::setfill('0')
              << static_cast<int>(cpu.Accumulator())
              << '\n';

    std::cout << "Carry = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::C)
              << '\n';

    std::cout << "Zero = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::Z)
              << '\n';

    std::cout << "Negative = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::N)
              << '\n';

    std::cout << "Overflow = "
              << cpu.GetFlag(dendyforge::CPU6502::Flags::V)
              << '\n';
}

void TestADC()
{
    std::cout << "\nADC\n";

    RunADCTest(
        "tests/cpu/roms/adc/adc_normal.nes",
        "Normal");

    RunADCTest(
        "tests/cpu/roms/adc/adc_carry.nes",
        "Carry");

    RunADCTest(
        "tests/cpu/roms/adc/adc_zero.nes",
        "Zero");

    RunADCTest(
        "tests/cpu/roms/adc/adc_overflow.nes",
        "Overflow");

    RunADCTest(
        "tests/cpu/roms/adc/adc_carry_in.nes",
        "Carry In");
}

void RunCpuTests()
{
    std::cout << "\n=== CPU Core ===\n";

    TestFlags();
    TestReset();
    TestFetch();
    TestDecode();

    std::cout << "\n=== Instruction Tests ===\n";

    TestLoadInstructions();
    TestStoreInstructions();
    TestIncrementInstructions();
    TestFlagInstructions();
    TestTransferInstructions();
    TestLogicalInstructions();
    TestCompareInstructions();
    TestADC();
}
