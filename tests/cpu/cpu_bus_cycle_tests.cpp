#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "cpu/cpu6502.hpp"

namespace
{

// Records every bus transaction so tests can assert the exact per-cycle
// read/write order of the sequenced CPU, including hardware dummy reads.
class RecordingCpuBus final : public dendyforge::CpuBus
{
public:
    struct Transaction
    {
        bool write;
        std::uint16_t address;
    };

    std::vector<Transaction> log;
    std::array<std::uint8_t, 65536> memory{};

    std::uint8_t CpuRead(std::uint16_t address) override
    {
        log.push_back({false, address});
        return memory[address];
    }

    void CpuWrite(std::uint16_t address, std::uint8_t data) override
    {
        log.push_back({true, address});
        memory[address] = data;
    }
};

struct CycleMachine
{
    RecordingCpuBus bus;
    dendyforge::CPU6502 cpu;

    CycleMachine()
    {
        cpu.ConnectBus(&bus);
        bus.memory[0xFFFC] = 0x00;
        bus.memory[0xFFFD] = 0x02; // reset into the program page $0200
        cpu.Reset();
        while (cpu.Cycles() > 0)
        {
            cpu.Clock();
        }
        bus.log.clear();
    }

    // Runs one full instruction and returns the number of cycles it took.
    int RunInstruction()
    {
        int clocks = 0;
        cpu.Clock();
        ++clocks;
        while (cpu.Cycles() > 0)
        {
            cpu.Clock();
            ++clocks;
        }
        return clocks;
    }
};

std::string FormatLog(const std::vector<RecordingCpuBus::Transaction>& log)
{
    std::string text;
    for (const auto& transaction : log)
    {
        if (!text.empty())
        {
            text += ' ';
        }
        char buffer[8];
        std::snprintf(buffer, sizeof buffer, transaction.write ? "W%04X" : "R%04X",
                      transaction.address);
        text += buffer;
    }
    return text;
}

} // namespace

TEST_CASE("Indexed absolute reads touch the unfixed address when crossing")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$01
    machine.bus.memory[0x0201] = 0x01;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0xBD; // LDA $02FF,X
    machine.bus.memory[0x0201] = 0xFF;
    machine.bus.memory[0x0202] = 0x02;
    machine.bus.memory[0x0300] = 0x5A;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0202 R0200 R0300");
    CHECK(clocks == 5);
    CHECK(machine.cpu.Accumulator() == 0x5A);
}

TEST_CASE("Indexed absolute reads fetch data once without a page cross")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$01
    machine.bus.memory[0x0201] = 0x01;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0xBD; // LDA $0300,X
    machine.bus.memory[0x0201] = 0x00;
    machine.bus.memory[0x0202] = 0x03;
    machine.bus.memory[0x0301] = 0x42;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0202 R0301");
    CHECK(clocks == 4);
    CHECK(machine.cpu.Accumulator() == 0x42);
}

TEST_CASE("Indexed absolute stores dummy-read then write the fixed address")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$77
    machine.bus.memory[0x0201] = 0x77;
    machine.RunInstruction();
    machine.cpu.SetProgramCounter(0x0200);
    machine.bus.memory[0x0200] = 0xA2; // LDX #$02
    machine.bus.memory[0x0201] = 0x02;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x9D; // STA $0280,X
    machine.bus.memory[0x0201] = 0x80;
    machine.bus.memory[0x0202] = 0x02;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0202 R0282 W0282");
    CHECK(clocks == 5);
    CHECK(machine.bus.memory[0x0282] == 0x77);
}

TEST_CASE("Indirect Y reads the wrong page before the fixed address")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA0; // LDY #$01
    machine.bus.memory[0x0201] = 0x01;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0xB1; // LDA ($10),Y
    machine.bus.memory[0x0201] = 0x10;
    machine.bus.memory[0x0010] = 0xFF; // pointer lo
    machine.bus.memory[0x0011] = 0x02; // pointer hi -> base $02FF
    machine.bus.memory[0x0300] = 0x66;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0010 R0011 R0200 R0300");
    CHECK(clocks == 6);
    CHECK(machine.cpu.Accumulator() == 0x66);
}

TEST_CASE("Zero page indexed reads dummy-read the unindexed base")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$05
    machine.bus.memory[0x0201] = 0x05;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0xB5; // LDA $10,X
    machine.bus.memory[0x0201] = 0x10;
    machine.bus.memory[0x0015] = 0x3C;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0010 R0015");
    CHECK(clocks == 4);
    CHECK(machine.cpu.Accumulator() == 0x3C);
}

TEST_CASE("Implied instructions dummy-read the next opcode address")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xE8; // INX
    machine.bus.memory[0x0201] = 0xE8; // INX (next)

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201");
    CHECK(clocks == 2);
    CHECK(machine.cpu.ProgramCounter() == 0x0201);
    CHECK(machine.cpu.X() == 0x01);
}

TEST_CASE("Indexed X indirect reads dummy-read the unindexed pointer")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$04
    machine.bus.memory[0x0201] = 0x04;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0xA1; // LDA ($20,X)
    machine.bus.memory[0x0201] = 0x20;
    machine.bus.memory[0x0024] = 0x34; // pointer lo at ($20+$04)
    machine.bus.memory[0x0025] = 0x05; // pointer hi -> $0534
    machine.bus.memory[0x0534] = 0x99;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0020 R0024 R0025 R0534");
    CHECK(clocks == 6);
    CHECK(machine.cpu.Accumulator() == 0x99);
}

TEST_CASE("JMP indirect wraps the pointer within its page")
{
    CycleMachine machine;
    machine.bus.memory[0x0210] = 0x6C; // JMP ($02FF)
    machine.bus.memory[0x0211] = 0xFF;
    machine.bus.memory[0x0212] = 0x02;
    machine.bus.memory[0x02FF] = 0x40; // target lo
    machine.bus.memory[0x0200] = 0x12; // wrapped high byte comes from $0200
    machine.bus.memory[0x0300] = 0x90; // wrong page, must not be used
    machine.cpu.SetProgramCounter(0x0210);

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0210 R0211 R0212 R02FF R0200");
    CHECK(clocks == 5);
    CHECK(machine.cpu.ProgramCounter() == 0x1240);
}
