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
// read/write order of the sequenced CPU, including hardware dummy reads and
// the values written by read-modify-write instructions.
class RecordingCpuBus final : public dendyforge::CpuBus
{
public:
    struct Transaction
    {
        bool write;
        std::uint16_t address;
        std::uint8_t data;
    };

    std::vector<Transaction> log;
    std::array<std::uint8_t, 65536> memory{};

    std::uint8_t CpuRead(std::uint16_t address) override
    {
        log.push_back({false, address, 0});
        return memory[address];
    }

    void CpuWrite(std::uint16_t address, std::uint8_t data) override
    {
        log.push_back({true, address, data});
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
        char buffer[16];
        std::snprintf(buffer, sizeof buffer,
                      transaction.write ? "W%04X=%02X" : "R%04X",
                      transaction.address, transaction.data);
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

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0202 R0282 W0282=77");
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

TEST_CASE("Read-modify-write zero page writes the old value before the new one")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xE6; // INC $30
    machine.bus.memory[0x0201] = 0x30;
    machine.bus.memory[0x0030] = 0x42;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0030 W0030=42 W0030=43");
    CHECK(clocks == 5);
    CHECK(machine.bus.memory[0x0030] == 0x43);
}

TEST_CASE("Read-modify-write zero page indexed dummy-reads the base")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$05
    machine.bus.memory[0x0201] = 0x05;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x16; // ASL $10,X
    machine.bus.memory[0x0201] = 0x10;
    machine.bus.memory[0x0015] = 0xC0;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0010 R0015 W0015=C0 W0015=80");
    CHECK(clocks == 6);
    CHECK(machine.bus.memory[0x0015] == 0x80);
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::C));
}

TEST_CASE("Read-modify-write indexed absolute reads the unfixed page first")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA2; // LDX #$F0
    machine.bus.memory[0x0201] = 0xF0;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0300);

    machine.bus.memory[0x0300] = 0xFE; // INC $0310,X -> effective $0400
    machine.bus.memory[0x0301] = 0x10;
    machine.bus.memory[0x0302] = 0x03;
    machine.bus.memory[0x0400] = 0x10;

    const int clocks = machine.RunInstruction();

    // The unfixed dummy read goes to $0300 (wrong page); the data read,
    // both writes and the increment use the fixed address.
    CHECK(FormatLog(machine.bus.log) == "R0300 R0301 R0302 R0300 R0400 W0400=10 W0400=11");
    CHECK(clocks == 7);
    CHECK(machine.bus.memory[0x0400] == 0x11);
}

TEST_CASE("Read-modify-write absolute keeps its six cycles")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xEE; // INC $0400
    machine.bus.memory[0x0201] = 0x00;
    machine.bus.memory[0x0202] = 0x04;
    machine.bus.memory[0x0400] = 0xFF;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0202 R0400 W0400=FF W0400=00");
    CHECK(clocks == 6);
    CHECK(machine.bus.memory[0x0400] == 0x00);
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::Z));
}

TEST_CASE("Unofficial SLO follows the same read, write-old, write-new pattern")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$40
    machine.bus.memory[0x0201] = 0x40;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x07; // SLO $30
    machine.bus.memory[0x0201] = 0x30;
    machine.bus.memory[0x0030] = 0xC1;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R0030 W0030=C1 W0030=82");
    CHECK(clocks == 5);
    CHECK(machine.bus.memory[0x0030] == 0x82);
    CHECK(machine.cpu.Accumulator() == 0xC2); // A |= shifted value
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::C));
}

TEST_CASE("PHA dummy-reads the next opcode and writes the stack last")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$AA
    machine.bus.memory[0x0201] = 0xAA;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x48; // PHA
    machine.bus.memory[0x0201] = 0xEA; // next opcode, dummy-read

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 W01FD=AA");
    CHECK(clocks == 3);
    CHECK(machine.cpu.StackPointer() == 0xFC);
    CHECK(machine.bus.memory[0x01FD] == 0xAA);
}

TEST_CASE("PHP pushes the status with break and unused bits set")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0x08; // PHP
    machine.bus.memory[0x0201] = 0xEA;

    const int clocks = machine.RunInstruction();

    // Status after reset is I|U ($24); the pushed byte forces B|U ($34).
    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 W01FD=34");
    CHECK(clocks == 3);
    CHECK(machine.cpu.StackPointer() == 0xFC);
}

TEST_CASE("PLA dummy-reads the stack location before pulling")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$5A
    machine.bus.memory[0x0201] = 0x5A;
    machine.RunInstruction();
    machine.cpu.SetProgramCounter(0x0200);
    machine.bus.memory[0x0200] = 0x48; // PHA
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x68; // PLA
    machine.bus.memory[0x0201] = 0xEA;

    const int clocks = machine.RunInstruction();

    // Cycle 3 dummy-reads $01FC (pre-increment SP); cycle 4 pulls $01FD.
    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R01FC R01FD");
    CHECK(clocks == 4);
    CHECK(machine.cpu.Accumulator() == 0x5A);
    CHECK(machine.cpu.StackPointer() == 0xFD);
}

TEST_CASE("PLP restores the pulled status byte")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$FF (sets N)
    machine.bus.memory[0x0201] = 0xFF;
    machine.RunInstruction();
    machine.cpu.SetProgramCounter(0x0200);
    machine.bus.memory[0x0200] = 0x08; // PHP (pushes N|I|U plus B|U)
    machine.RunInstruction();
    machine.cpu.SetProgramCounter(0x0200);
    machine.bus.memory[0x0200] = 0xA9; // LDA #$00 (clears N)
    machine.bus.memory[0x0201] = 0x00;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x28; // PLP
    machine.bus.memory[0x0201] = 0xEA;

    const int clocks = machine.RunInstruction();

    // Cycle 3 dummy-reads $01FC (pre-increment SP); cycle 4 pulls $01FD.
    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R01FC R01FD");
    CHECK(clocks == 4);
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::N));
    CHECK_FALSE(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::B));
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::U));
}

TEST_CASE("JSR pushes the return address before reading the target high byte")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0x20; // JSR $0500
    machine.bus.memory[0x0201] = 0x00;
    machine.bus.memory[0x0202] = 0x05;

    const int clocks = machine.RunInstruction();

    // The dummy stack read, both pushes of the return address $0202 (the
    // address of the high operand byte), then the high-byte fetch at $0202.
    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 R01FD W01FD=02 W01FC=02 R0202");
    CHECK(clocks == 6);
    CHECK(machine.cpu.ProgramCounter() == 0x0500);
    CHECK(machine.cpu.StackPointer() == 0xFB);
    CHECK(machine.bus.memory[0x01FD] == 0x02);
    CHECK(machine.bus.memory[0x01FC] == 0x02);
}

TEST_CASE("RTS pulls the return address and increments internally")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0x20; // JSR $0500
    machine.bus.memory[0x0201] = 0x00;
    machine.bus.memory[0x0202] = 0x05;
    machine.RunInstruction();
    machine.bus.log.clear();

    machine.bus.memory[0x0500] = 0x60; // RTS

    const int clocks = machine.RunInstruction();

    // The sixth cycle only increments the pulled PC; no bus transaction.
    CHECK(FormatLog(machine.bus.log) == "R0500 R0501 R01FB R01FC R01FD");
    CHECK(clocks == 6);
    CHECK(machine.cpu.ProgramCounter() == 0x0203);
    CHECK(machine.cpu.StackPointer() == 0xFD);
}

TEST_CASE("RTI pulls status and return address across six cycles")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0x08; // PHP
    machine.RunInstruction();
    machine.cpu.SetProgramCounter(0x0200);
    machine.bus.memory[0x0200] = 0x08; // PHP (SP now $FB)
    machine.RunInstruction();
    machine.bus.memory[0x01FC] = 0x24; // pulled status (I|U)
    machine.bus.memory[0x01FD] = 0x00; // return PCL
    machine.bus.memory[0x01FE] = 0x03; // return PCH
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0600);

    machine.bus.memory[0x0600] = 0x40; // RTI
    machine.bus.memory[0x0601] = 0xEA;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0600 R0601 R01FB R01FC R01FD R01FE");
    CHECK(clocks == 6);
    CHECK(machine.cpu.ProgramCounter() == 0x0300);
    CHECK(machine.cpu.StackPointer() == 0xFE);
    CHECK(machine.cpu.Status() == 0x24);
}

TEST_CASE("BRK pushes the padded return address and reads the IRQ vector")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0x00; // BRK
    machine.bus.memory[0x0201] = 0xEA; // padding byte, consumed by cycle 2
    machine.bus.memory[0xFFFE] = 0x34; // IRQ vector $1234
    machine.bus.memory[0xFFFF] = 0x12;

    const int clocks = machine.RunInstruction();

    // Status after reset is I|U ($24); BRK pushes it with B|U forced ($34).
    CHECK(FormatLog(machine.bus.log) == "R0200 R0201 W01FD=02 W01FC=02 W01FB=34 RFFFE RFFFF");
    CHECK(clocks == 7);
    CHECK(machine.cpu.ProgramCounter() == 0x1234);
    CHECK(machine.cpu.StackPointer() == 0xFA);
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::I));
    CHECK_FALSE(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::B));
}

TEST_CASE("Accumulator shifts perform the implied dummy read")
{
    CycleMachine machine;
    machine.bus.memory[0x0200] = 0xA9; // LDA #$40
    machine.bus.memory[0x0201] = 0x40;
    machine.RunInstruction();
    machine.bus.log.clear();
    machine.cpu.SetProgramCounter(0x0200);

    machine.bus.memory[0x0200] = 0x0A; // ASL A
    machine.bus.memory[0x0201] = 0xEA;

    const int clocks = machine.RunInstruction();

    CHECK(FormatLog(machine.bus.log) == "R0200 R0201");
    CHECK(clocks == 2);
    CHECK(machine.cpu.Accumulator() == 0x80);
    CHECK(machine.cpu.GetFlag(dendyforge::CPU6502::Flags::N));
}

TEST_CASE("Sequenced stores perform one bus transaction per cycle")
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

    int clocks = 0;
    std::size_t previousSize = 0;
    do
    {
        machine.cpu.Clock();
        ++clocks;
        CHECK(machine.bus.log.size() - previousSize == 1);
        previousSize = machine.bus.log.size();
    } while (machine.cpu.Cycles() > 0);

    CHECK(clocks == 5);
    CHECK(machine.bus.log.size() == 5);
    CHECK(machine.bus.log.back().write);
    CHECK(machine.bus.memory[0x0282] == 0x77);
}
