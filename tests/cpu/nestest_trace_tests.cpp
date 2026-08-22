#include <doctest/doctest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include "cpu/cpu6502.hpp"
#include "cpu/cpu_test_support.hpp"

namespace
{

using dendyforge::test::CompleteReset;
using dendyforge::test::LoadCpuMachine;
using dendyforge::test::RomPath;

// One parsed line of the Nintendulator-format nestest golden log. The
// disassembly and PPU columns are ignored: the trace harness has no PPU.
struct ReferenceLine
{
    std::string programCounter;
    std::string bytes; // opcode and operand bytes as shown, without spaces
    std::string a;
    std::string x;
    std::string y;
    std::string p;
    std::string sp;
    std::string cyc;
};

// Line shape (fixed columns up to the disassembly):
// "C000  4C F5 C5  JMP $C5F5 ... A:00 X:00 Y:00 P:24 SP:FD PPU:  0, 21 CYC:7"
bool ParseReferenceLine(const std::string& line, ReferenceLine& out)
{
    if (line.size() < 48)
    {
        return false;
    }

    out.programCounter = line.substr(0, 4);
    out.bytes.clear();
    for (const int column : {6, 9, 12})
    {
        const std::string byte = line.substr(static_cast<std::size_t>(column), 2);
        if (byte != "  ")
        {
            out.bytes += byte;
        }
    }

    const auto registerValue = [&line](const char* marker) -> std::string {
        const std::size_t position = line.find(marker);
        return position == std::string::npos ? std::string{}
                                              : line.substr(position + 2, 2);
    };
    out.a = registerValue("A:");
    out.x = registerValue("X:");
    out.y = registerValue("Y:");
    out.p = registerValue("P:");
    out.sp = line.substr(line.find("SP:") + 3, 2);

    const std::size_t cyclePosition = line.rfind("CYC:");
    if (cyclePosition == std::string::npos)
    {
        return false;
    }
    out.cyc = line.substr(cyclePosition + 4);

    return !out.a.empty() && !out.x.empty() && !out.y.empty() &&
           !out.p.empty() && !out.sp.empty() && !out.cyc.empty();
}

std::string HexByte(std::uint8_t value)
{
    char buffer[3];
    std::snprintf(buffer, sizeof buffer, "%02X", value);
    return buffer;
}

std::string HexWord(std::uint16_t value)
{
    char buffer[5];
    std::snprintf(buffer, sizeof buffer, "%04X", value);
    return buffer;
}

} // namespace

TEST_CASE("CPU6502 matches the nestest golden trace in automation mode")
{
    const auto romPath = RomPath("nestest.nes");
    auto machine = LoadCpuMachine("nestest.nes");
    REQUIRE_MESSAGE(machine != nullptr, "Unable to load ROM: " << romPath.string());

    std::ifstream reference(RomPath("nestest.log").string());
    REQUIRE_MESSAGE(reference.is_open(), "Unable to open nestest.log");

    auto& cpu = machine->cpu;
    CompleteReset(cpu);
    cpu.SetProgramCounter(0xC000);

    // The golden log starts counting at cycle 7, the cost of the reset
    // sequence, and each line snapshots the state before its instruction.
    std::uint64_t totalCycles = 7;

    std::string line;
    std::size_t lineNumber = 0;
    bool traceMatches = true;
    while (std::getline(reference, line))
    {
        ++lineNumber;
        if (line.empty())
        {
            continue;
        }

        ReferenceLine expected;
        if (!ParseReferenceLine(line, expected))
        {
            CHECK_MESSAGE(false, "Unparseable reference line " << lineNumber);
            traceMatches = false;
            break;
        }

        const std::uint16_t pc = cpu.ProgramCounter();
        std::string actual = HexWord(pc);
        const std::size_t byteCount = expected.bytes.size() / 2;
        for (std::size_t index = 0; index < byteCount; ++index)
        {
            actual += HexByte(
                machine->bus.CpuRead(static_cast<std::uint16_t>(pc + index)));
        }
        actual += " A:" + HexByte(cpu.Accumulator());
        actual += " X:" + HexByte(cpu.X());
        actual += " Y:" + HexByte(cpu.Y());
        actual += " P:" + HexByte(cpu.Status());
        actual += " SP:" + HexByte(cpu.StackPointer());
        actual += " CYC:" + std::to_string(totalCycles);

        const std::string wanted = expected.programCounter + expected.bytes +
                                   " A:" + expected.a + " X:" + expected.x +
                                   " Y:" + expected.y + " P:" + expected.p +
                                   " SP:" + expected.sp + " CYC:" + expected.cyc;

        if (actual != wanted)
        {
            CHECK_MESSAGE(false,
                          "Trace mismatch at line " << lineNumber
                                                    << ":\n  actual:   " << actual
                                                    << "\n  expected: " << wanted);
            traceMatches = false;
            break;
        }

        cpu.Clock();
        ++totalCycles;
        while (cpu.Cycles() > 0)
        {
            cpu.Clock();
            ++totalCycles;
        }
    }

    if (traceMatches)
    {
        // Automation mode reports the last failed test in $02/$03; both
        // must be zero when the whole suite passed.
        CHECK(machine->bus.CpuRead(0x0002) == 0x00);
        CHECK(machine->bus.CpuRead(0x0003) == 0x00);
    }
}
