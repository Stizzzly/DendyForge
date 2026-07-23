#include "cpu6502.hpp"
#include "../bus/bus.hpp"
#include <ios>
#include <iostream>
namespace dendyforge
{

const CPU6502::Instruction& CPU6502::GetInstructionConfig(std::uint8_t opcode)
{
    static constexpr auto LookupTable = []() consteval {
        std::array<Instruction, 256> table{};

        table.fill({
            .name = "???",
            .operate = &CPU6502::XXX,
            .addressMode = &CPU6502::IMP,
            .cycles = 0
        });

        table[0x78] = {
            .name = "SEI",
            .operate = &CPU6502::SEI,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };
        table[0x85] = {
            .name = "STA",
            .operate = &CPU6502::STA,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0x86] = {
             .name = "STX",
             .operate = &CPU6502::STX,
             .addressMode = &CPU6502::ZP0,
             .cycles = 3
        };

        table[0x84] = {
            .name = "STY",
            .operate = &CPU6502::STY,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };
        table[0x8D] = {
            .name = "STA",
            .operate = &CPU6502::STA,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0x8E] = {
            .name = "STX",
            .operate = &CPU6502::STX,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0x8C] = {
            .name = "STY",
            .operate = &CPU6502::STY,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0xA0] = {
            .name = "LDY",
            .operate = &CPU6502::LDY,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xA2] = {
            .name = "LDX",
            .operate = &CPU6502::LDX,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xA5] = {
            .name = "LDA",
            .operate = &CPU6502::LDA,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA6] = {
            .name = "LDX",
            .operate = &CPU6502::LDX,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA4] = {
            .name = "LDY",
            .operate = &CPU6502::LDY,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA9] = {
            .name = "LDA",
            .operate = &CPU6502::LDA,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };
        table[0x4C] = {
            .name = "JMP",
            .operate = &CPU6502::JMP,
            .addressMode = &CPU6502::ABS,
            .cycles = 3
        };

        return table;
    }();

    return LookupTable[opcode];
}
CPU6502::CPU6502()
{
}

void CPU6502::ConnectBus(Bus* bus)
{
    m_bus = bus;
}

bool CPU6502::GetFlag(Flags flag) const
{
    return (m_status & static_cast<std::uint8_t>(flag)) != 0;
}

void CPU6502::SetFlag(Flags flag, bool value)
{
    if (value)
    {
        m_status |= static_cast<std::uint8_t>(flag);
    }
    else
    {
        m_status &= ~static_cast<std::uint8_t>(flag);
    }
}

void CPU6502::Reset()
{
    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0xFD;
    m_status = static_cast<std::uint8_t>(Flags::U);

    const std::uint16_t lo = Read(0xFFFC);
    const std::uint16_t hi = Read(0xFFFD);

    m_pc = (hi << 8) | lo;
    m_cycles = 8;
}

void CPU6502::Clock()
{
    if (m_cycles == 0)
    {
        Fetch();

        const auto& instruction = GetInstructionConfig(m_opcode);

        m_cycles = instruction.cycles;

        (this->*instruction.addressMode)();
        (this->*instruction.operate)();
    }

    --m_cycles;
}

std::uint8_t CPU6502::Accumulator() const
{
    return m_a;
}

std::uint8_t CPU6502::Read(std::uint16_t address)
{
    return m_bus->CpuRead(address);
}

void CPU6502::Write(std::uint16_t address, std::uint8_t data)
{
    m_bus->CpuWrite(address, data);
}

void CPU6502::Push(std::uint8_t data)
{
    Write(0x0100 + m_sp, data);
    --m_sp;
}

std::uint8_t CPU6502::Pop()
{
    ++m_sp;
    return Read(0x0100 + m_sp);
}

std::uint16_t CPU6502::ProgramCounter() const
{
    return m_pc;
}

std::uint8_t CPU6502::StackPointer() const
{
    return m_sp;
}

std::uint8_t CPU6502::Status() const
{
    return m_status;
}

std::uint8_t CPU6502::Fetch()
{
    m_opcode = Read(m_pc);
    ++m_pc;
    return m_opcode;
}

std::uint8_t CPU6502::Opcode() const
{
    return m_opcode;
}

const char* CPU6502::CurrentInstruction() const
{
    return GetInstructionConfig(m_opcode).name;
}

std::uint8_t CPU6502::IMP()
{
    m_fetched = m_a;
    return 0;
}

std::uint8_t CPU6502::IMM()
{
    m_addrAbs = m_pc++;
    return 0;
}

std::uint8_t CPU6502::XXX()
{
    return 0;
}

std::uint8_t CPU6502::SEI()
{
    SetFlag(Flags::I, true);
    return 0;
}

std::uint8_t CPU6502::LDA()
{
    FetchData();

    m_a = m_fetched;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, (m_a & 0x80) != 0);

    return 0;
}

std::uint8_t CPU6502::LDX()
{
    FetchData();

    m_x = m_fetched;

    SetFlag(Flags::Z, m_x == 0x00);
    SetFlag(Flags::N, m_x & 0x80);

    return 0;
}

std::uint8_t CPU6502::LDY()
{
    FetchData();

    m_y = m_fetched;

    SetFlag(Flags::Z, m_y == 0x00);
    SetFlag(Flags::N, m_y & 0x80);

    return 0;
}

std::uint8_t CPU6502::STA()
{
    Write(m_addrAbs, m_a);
    return 0;
}

std::uint8_t CPU6502::STX()
{
    std::cout
        << "STX: addr=$"
        << std::hex << m_addrAbs
        << " X=$"
        << static_cast<int>(m_x)
        << '\n';

    Write(m_addrAbs, m_x);
    return 0;
}

std::uint8_t CPU6502::STY()
{
    std::cout
        << "STY: addr=$"
        << std::hex << m_addrAbs
        << " Y=$"
        << static_cast<int>(m_y)
        << '\n';

    Write(m_addrAbs, m_y);
    return 0;
}

std::uint8_t CPU6502::ZPX()
{
    return 0;
}

std::uint8_t CPU6502::ZPY()
{
    return 0;
}

std::uint8_t CPU6502::ABS()
{
    // Читаем младший байт адреса
    const std::uint16_t lo = Read(m_pc);
    ++m_pc;

    // Читаем старший байт адреса
    const std::uint16_t hi = Read(m_pc);
    ++m_pc;

    // Собираем 16-битный адрес
    m_addrAbs = (hi << 8) | lo;

    return 0;
}

std::uint8_t CPU6502::ZP0()
{
    m_addrAbs = Read(m_pc);
    m_pc++;

    m_addrAbs &= 0x00FF;

    return 0;
}

std::uint8_t CPU6502::JMP()
{
    m_pc = m_addrAbs;
    return 0;
}

std::uint8_t CPU6502::Cycles() const
{
    return m_cycles;
}

std::uint8_t CPU6502::FetchData()
{
    m_fetched = Read(m_addrAbs);
    return m_fetched;
}

std::uint8_t CPU6502::X() const
{
    return m_x;
}

std::uint8_t CPU6502::Y() const
{
    return m_y;
}
} // namespace dendyforge
