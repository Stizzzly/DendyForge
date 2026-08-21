#include "bus.hpp"

#include "../cartridge/cartridge.hpp"

namespace dendyforge
{

Bus::Bus()
{
    m_cpuRam.fill(0);
}

void Bus::InsertCartridge(Cartridge* cartridge)
{
    m_cartridge = cartridge;
    m_ppu.ConnectCartridge(cartridge);
}

PPU& Bus::VideoProcessor()
{
    return m_ppu;
}

std::uint8_t Bus::CpuRead(std::uint16_t address)
{
    std::uint8_t data = 0x00;

    // $4020-$FFFF
    // Cartridge space
    if (m_cartridge &&
        m_cartridge->CpuRead(address, data))
    {
        return data;
    }

    // $0000-$1FFF
    // 2 KB internal RAM (mirrored every $800 bytes)
    if (address <= 0x1FFF)
    {
        return m_cpuRam[address & 0x07FF];
    }

    // $2000-$3FFF
    // PPU registers (mirrored every 8 bytes)
    if (address >= 0x2000 && address <= 0x3FFF)
    {
        return m_ppu.CpuRead(address);
    }

    // $4000-$4017
    // APU + Controllers (TODO)

    return data;
}

void Bus::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    // $4020-$FFFF
    // Cartridge space
    if (m_cartridge &&
        m_cartridge->CpuWrite(address, data))
    {
        return;
    }

    // $0000-$1FFF
    // 2 KB internal RAM (mirrored every $800 bytes)
    if (address <= 0x1FFF)
    {
        m_cpuRam[address & 0x07FF] = data;
        return;
    }

    // $2000-$3FFF
    // PPU registers (mirrored every 8 bytes)
    if (address >= 0x2000 && address <= 0x3FFF)
    {
        m_ppu.CpuWrite(address, data);
        return;
    }

    // $4000-$4017
    // APU + Controllers (TODO)
}

void Bus::ClockPpu()
{
    m_ppu.Clock();
}

bool Bus::PollPpuNmi()
{
    return m_ppu.PollNmi();
}

} // namespace dendyforge
