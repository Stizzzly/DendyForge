#include "cpu6502.hpp"

#include "../bus/bus.hpp"

namespace dendyforge
{

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

void CPU6502::SetFlag(
    Flags flag,
    bool value)
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

    // Read reset vector ($FFFC-$FFFD)
    const std::uint16_t lo = Read(0xFFFC);
    const std::uint16_t hi = Read(0xFFFD);

    m_pc = (hi << 8) | lo;
}

void CPU6502::Clock()
{
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

std::uint8_t CPU6502::Fetch()
{
    m_opcode = Read(m_pc);
    ++m_pc;

    return m_opcode;
}

} // namespace dendyforge
