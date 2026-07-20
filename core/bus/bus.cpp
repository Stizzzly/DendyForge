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
}

std::uint8_t Bus::CpuRead(std::uint16_t address)
{
    std::uint8_t data = 0x00;

    if (m_cartridge != nullptr &&
        m_cartridge->CpuRead(address, data))
    {
        return data;
    }

    if (address <= 0x1FFF)
    {
        return m_cpuRam[address & 0x07FF];
    }

    return data;
}

void Bus::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    if (m_cartridge != nullptr &&
        m_cartridge->CpuWrite(address, data))
    {
        return;
    }

    if (address <= 0x1FFF)
    {
        m_cpuRam[address & 0x07FF] = data;
    }
}

}
