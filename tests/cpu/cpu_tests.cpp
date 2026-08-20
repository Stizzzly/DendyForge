#include <doctest/doctest.h>

#include <array>
#include <string_view>

#include "cpu/cpu6502.hpp"
#include "cpu/cpu_test_support.hpp"

namespace
{

using dendyforge::CPU6502;
using dendyforge::test::CompleteInstruction;
using dendyforge::test::CompleteReset;
using dendyforge::test::CpuMachine;
using dendyforge::test::ExecuteUntilSelfJump;
using dendyforge::test::LoadCpuMachine;
using dendyforge::test::RomPath;

bool BootAndRun(CpuMachine& machine)
{
    CompleteReset(machine.cpu);
    return ExecuteUntilSelfJump(machine.cpu);
}

void CheckFlags(const CPU6502& cpu,
                bool carry,
                bool zero,
                bool negative,
                bool overflow)
{
    CHECK(cpu.GetFlag(CPU6502::Flags::C) == carry);
    CHECK(cpu.GetFlag(CPU6502::Flags::Z) == zero);
    CHECK(cpu.GetFlag(CPU6502::Flags::N) == negative);
    CHECK(cpu.GetFlag(CPU6502::Flags::V) == overflow);
}

} // namespace

TEST_CASE("CPU status flags can be set and cleared")
{
    CPU6502 cpu;

    cpu.SetFlag(CPU6502::Flags::C, true);
    cpu.SetFlag(CPU6502::Flags::Z, true);

    CHECK(cpu.GetFlag(CPU6502::Flags::C));
    CHECK(cpu.GetFlag(CPU6502::Flags::Z));
    CHECK_FALSE(cpu.GetFlag(CPU6502::Flags::N));

    cpu.SetFlag(CPU6502::Flags::C, false);

    CHECK_FALSE(cpu.GetFlag(CPU6502::Flags::C));
    CHECK(cpu.GetFlag(CPU6502::Flags::Z));
}

TEST_CASE("CPU reset uses the reset vector from the iNES ROM")
{
    const auto path = RomPath("cpu_test.nes");
    auto machine = LoadCpuMachine("cpu_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    CHECK(machine->cpu.ProgramCounter() == 0x8000);
    CHECK(machine->cpu.StackPointer() == 0xFD);
    CHECK(machine->cpu.Status() == 0x24);
    CHECK(machine->cpu.Cycles() == 8);
}

TEST_CASE("CPU fetches and decodes the first instruction")
{
    const auto path = RomPath("cpu_test.nes");
    auto machine = LoadCpuMachine("cpu_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    CompleteReset(machine->cpu);
    machine->cpu.Clock();

    CHECK(machine->cpu.ProgramCounter() == 0x8001);
    CHECK(machine->cpu.Opcode() == 0x78);
    CHECK(std::string_view(machine->cpu.CurrentInstruction()) == "SEI");
    CHECK(machine->cpu.Cycles() == 1);
}

TEST_CASE("CPU load instructions leave the expected registers")
{
    const auto path = RomPath("load/load_test.nes");
    auto machine = LoadCpuMachine("load/load_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->cpu.Accumulator() == 0x42);
    CHECK(machine->cpu.X() == 0x11);
    CHECK(machine->cpu.Y() == 0x22);
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::Z));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::N));
}

TEST_CASE("CPU store instructions write absolute and zero-page addresses")
{
    const auto path = RomPath("store/store_test.nes");
    auto machine = LoadCpuMachine("store/store_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0200) == 0x42);
    CHECK(machine->bus.CpuRead(0x0201) == 0x11);
    CHECK(machine->bus.CpuRead(0x0202) == 0x22);
    CHECK(machine->bus.CpuRead(0x0000) == 0xAA);
    CHECK(machine->bus.CpuRead(0x0001) == 0xBB);
    CHECK(machine->bus.CpuRead(0x0002) == 0xCC);
    CHECK(machine->bus.CpuRead(0x0015) == 0x42);
    CHECK(machine->bus.CpuRead(0x0023) == 0x99);
    CHECK(machine->bus.CpuRead(0x0037) == 0x55);
    CHECK(machine->bus.CpuRead(0x0008) == 0x77);
    CHECK(machine->bus.CpuRead(0x0010) == 0x66);
}

TEST_CASE("CPU increment and decrement instructions update X and Y")
{
    const auto path = RomPath("increment/increment_test.nes");
    auto machine = LoadCpuMachine("increment/increment_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->cpu.X() == 0x2F);
    CHECK(machine->cpu.Y() == 0x3F);
}

TEST_CASE("CPU flag instructions leave all tested flags cleared")
{
    const auto path = RomPath("flags/flag_test.nes");
    auto machine = LoadCpuMachine("flags/flag_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::I));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::D));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::V));
}

TEST_CASE("CPU transfer instructions preserve the expected register values")
{
    const auto path = RomPath("transfer/transfer_test.nes");
    auto machine = LoadCpuMachine("transfer/transfer_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->cpu.Accumulator() == 0x88);
    CHECK(machine->cpu.X() == 0x55);
    CHECK(machine->cpu.Y() == 0x88);
    CHECK(machine->cpu.StackPointer() == 0x55);
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::Z));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::N));
}

TEST_CASE("CPU logical instructions update accumulator and flags")
{
    const auto path = RomPath("logical/logical_test.nes");
    auto machine = LoadCpuMachine("logical/logical_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->cpu.Accumulator() == 0x40);
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::Z));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::N));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::V));
}

TEST_CASE("CPU compare instructions update flags without changing registers")
{
    const auto path = RomPath("compare/compare_test.nes");
    auto machine = LoadCpuMachine("compare/compare_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    BootAndRun(*machine);

    CHECK(machine->cpu.X() == 0x50);
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::Z));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::N));
}

TEST_CASE("CPU ADC updates accumulator and status flags")
{
    struct Case
    {
        const char* name;
        const char* rom;
        std::uint8_t accumulator;
        bool carry;
        bool zero;
        bool negative;
        bool overflow;
    };

    const std::array cases{
        Case{"normal", "adc/adc_normal.nes", 0x30, false, false, false, false},
        Case{"carry", "adc/adc_carry.nes", 0x10, true, false, false, false},
        Case{"zero", "adc/adc_zero.nes", 0x00, true, true, false, false},
        Case{"overflow", "adc/adc_overflow.nes", 0x80, false, false, true, true},
        Case{"carry in", "adc/adc_carry_in.nes", 0x31, false, false, false, false},
    };

    for (const auto& test : cases)
    {
        SUBCASE(test.name)
        {
            const auto path = RomPath(test.rom);
            auto machine = LoadCpuMachine(test.rom);
            REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

            REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

            CHECK(machine->cpu.Accumulator() == test.accumulator);
            CheckFlags(machine->cpu, test.carry, test.zero, test.negative, test.overflow);
        }
    }
}

TEST_CASE("CPU SBC updates accumulator and status flags")
{
    struct Case
    {
        const char* name;
        const char* rom;
        std::uint8_t accumulator;
        bool carry;
        bool zero;
        bool negative;
        bool overflow;
    };

    const std::array cases{
        Case{"normal", "sbc/sbc_normal.nes", 0x20, true, false, false, false},
        Case{"borrow", "sbc/sbc_borrow.nes", 0xF0, false, false, true, false},
        Case{"zero", "sbc/sbc_zero.nes", 0x00, true, true, false, false},
        Case{"overflow", "sbc/sbc_overflow.nes", 0x7F, true, false, false, true},
        Case{"borrow in", "sbc/sbc_borrow_in.nes", 0x1F, true, false, false, false},
    };

    for (const auto& test : cases)
    {
        SUBCASE(test.name)
        {
            const auto path = RomPath(test.rom);
            auto machine = LoadCpuMachine(test.rom);
            REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

            REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

            CHECK(machine->cpu.Accumulator() == test.accumulator);
            CheckFlags(machine->cpu, test.carry, test.zero, test.negative, test.overflow);
        }
    }
}

TEST_CASE("CPU performs BCD arithmetic when decimal mode is enabled")
{
    const auto path = RomPath("decimal/decimal_mode_test.nes");
    auto machine = LoadCpuMachine(
        "decimal/decimal_mode_test.nes",
        {.decimalModeEnabled = true});
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE(machine->cpu.IsDecimalModeEnabled());
    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0000) == 0x00);
    CHECK(machine->bus.CpuRead(0x0001) == 0x49);
    CHECK(machine->cpu.Accumulator() == 0x49);
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::D));
}

TEST_CASE("CPU ignores decimal arithmetic when configured as an NES 2A03")
{
    const auto path = RomPath("decimal/decimal_mode_test.nes");
    auto machine = LoadCpuMachine("decimal/decimal_mode_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_FALSE(machine->cpu.IsDecimalModeEnabled());
    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0000) == 0x9A);
    CHECK(machine->bus.CpuRead(0x0001) == 0x4F);
    CHECK(machine->cpu.Accumulator() == 0x4F);
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK(machine->cpu.GetFlag(CPU6502::Flags::D));
}

TEST_CASE("CPU ASL updates accumulator, memory, and status flags")
{
    const auto path = RomPath("asl/asl_test.nes");
    auto machine = LoadCpuMachine("asl/asl_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->cpu.Accumulator() == 0x10);
    CheckFlags(machine->cpu, false, false, false, false);
    CHECK(machine->bus.CpuRead(0x0010) == 0x02);
    CHECK(machine->bus.CpuRead(0x0011) == 0x08);
    CHECK(machine->bus.CpuRead(0x0200) == 0x10);
    CHECK(machine->bus.CpuRead(0x0201) == 0x20);
}

TEST_CASE("CPU supports indexed, indirect, and relative addressing modes")
{
    const auto path = RomPath("addressing/addressing_test.nes");
    auto machine = LoadCpuMachine("addressing/addressing_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0200) == 0x42);
    CHECK(machine->bus.CpuRead(0x0201) == 0x77);
    CHECK(machine->bus.CpuRead(0x0202) == 0xA1);
    CHECK(machine->bus.CpuRead(0x0203) == 0xB2);
    CHECK(machine->bus.CpuRead(0x0204) == 0xC3);
    CHECK(machine->bus.CpuRead(0x0205) == 0xD4);
    CHECK(machine->bus.CpuRead(0x0206) == 0xE5);
    CHECK(machine->bus.CpuRead(0x0207) == 0xF6);
    CHECK(machine->bus.CpuRead(0x0208) == 0xA7);
}

TEST_CASE("CPU executes stack and subroutine instructions")
{
    const auto path = RomPath("stack/stack_test.nes");
    auto machine = LoadCpuMachine("stack/stack_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0200) == 0x42);
    CHECK(machine->bus.CpuRead(0x0201) == 0x01);
    CHECK(machine->bus.CpuRead(0x0202) == 0x55);
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::C));
    CHECK_FALSE(machine->cpu.GetFlag(CPU6502::Flags::D));
    CHECK(machine->cpu.StackPointer() == 0xFF);
}

TEST_CASE("CPU enters a BRK handler and resumes through RTI")
{
    const auto path = RomPath("interrupts/interrupts_test.nes");
    auto machine = LoadCpuMachine("interrupts/interrupts_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0200) == 0x42);
    CHECK(machine->bus.CpuRead(0x0201) == 0x55);
    CHECK(machine->cpu.StackPointer() == 0xFF);
}

TEST_CASE("CPU updates memory through rotate, shift, increment, and decrement")
{
    const auto path = RomPath("rmw/rmw_test.nes");
    auto machine = LoadCpuMachine("rmw/rmw_test.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << path.string());

    REQUIRE_MESSAGE(BootAndRun(*machine), "ROM did not reach its terminal self-jump");

    CHECK(machine->bus.CpuRead(0x0010) == 0x00);
    CHECK(machine->bus.CpuRead(0x0011) == 0x80);
    CHECK(machine->bus.CpuRead(0x0012) == 0x01);
    CHECK(machine->bus.CpuRead(0x0013) == 0x00);
    CHECK(machine->bus.CpuRead(0x0014) == 0xFF);
    CHECK(machine->bus.CpuRead(0x0021) == 0x00);
    CHECK(machine->bus.CpuRead(0x0022) == 0xFF);
    CHECK(machine->bus.CpuRead(0x0200) == 0x00);
    CHECK(machine->bus.CpuRead(0x0201) == 0x80);
    CHECK(machine->bus.CpuRead(0x0202) == 0x01);
    CHECK(machine->bus.CpuRead(0x0203) == 0x00);
    CHECK(machine->bus.CpuRead(0x0204) == 0xFF);
    CHECK(machine->bus.CpuRead(0x0206) == 0x00);
    CHECK(machine->bus.CpuRead(0x0207) == 0xFF);
}

TEST_CASE("CPU passes nestest in automation mode")
{
    const auto romPath = RomPath("nestest.nes");
    auto machine = LoadCpuMachine("nestest.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << romPath.string());

    CompleteReset(machine->cpu);
    machine->cpu.SetProgramCounter(0xC000);

    constexpr std::size_t MaxInstructions = 5'000'000;
    for (std::size_t instruction = 0; instruction < MaxInstructions; ++instruction)
    {
        CompleteInstruction(machine->cpu);
    }

    CHECK(machine->bus.CpuRead(0x0002) == 0x00);
    CHECK(machine->bus.CpuRead(0x0003) == 0x00);
}
