#include "bus.hpp"

#include "../cartridge/cartridge.hpp"

namespace dendyforge
{

Bus::Bus()
{
    m_cpuRam.fill(0);
    m_apu.SetDmcMemoryReader([this](std::uint16_t address) {
        return CpuRead(address);
    });
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

APU& Bus::AudioProcessor()
{
    return m_apu;
}

Controller& Bus::PrimaryController()
{
    return m_controller1;
}

std::uint8_t Bus::CpuRead(std::uint16_t address)
{
    // Built-in ranges below $6000 can never be claimed by a cartridge
    // (Mapper 0/2 decode only $6000-$FFFF), so answer them directly and
    // skip the virtual cartridge call on the hot RAM/register paths.
    // $0000-$1FFF: 2 KB internal RAM (mirrored every $800 bytes)
    if (address <= 0x1FFF)
    {
        return m_cpuRam[address & 0x07FF];
    }

    // $2000-$3FFF: PPU registers (mirrored every 8 bytes)
    if (address <= 0x3FFF)
    {
        return m_ppu.CpuRead(address);
    }

    if (address == 0x4016)
    {
        return m_controller1.Read();
    }

    if (address == 0x4015)
    {
        return m_apu.CpuRead(address);
    }

    // $4020-$FFFF (and the unclaimed $4018-$401F/$4020-$5FFF open bus
    // when no cartridge decodes them): cartridge space.
    std::uint8_t data = 0x00;
    if (m_cartridge &&
        m_cartridge->CpuRead(address, data))
    {
        return data;
    }

    return data;
}

void Bus::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    // Same ordering as CpuRead: built-in ranges first, cartridge space
    // ($6000-$FFFF for the implemented mappers) last.
    // $0000-$1FFF: 2 KB internal RAM (mirrored every $800 bytes)
    if (address <= 0x1FFF)
    {
        m_cpuRam[address & 0x07FF] = data;
        return;
    }

    // $2000-$3FFF: PPU registers (mirrored every 8 bytes)
    if (address <= 0x3FFF)
    {
        m_ppu.CpuWrite(address, data);
        return;
    }

    // $4014
    // OAM DMA transfers one CPU memory page into PPU sprite memory.
    if (address == 0x4014)
    {
        const std::uint16_t baseAddress = static_cast<std::uint16_t>(data) << 8;
        for (std::uint16_t offset = 0; offset < 256; ++offset)
        {
            m_ppu.CpuWrite(0x2004, CpuRead(baseAddress + offset));
        }
        m_dmaPending = true;
        return;
    }

    if (address == 0x4016)
    {
        m_controller1.Write(data);
        return;
    }

    if (address <= 0x4017)
    {
        m_apu.CpuWrite(address, data);
        return;
    }

    if (m_cartridge &&
        m_cartridge->CpuWrite(address, data))
    {
        return;
    }
}

void Bus::ClockPpu()
{
    m_ppu.Clock();
}

bool Bus::PollPpuNmi()
{
    return m_ppu.PollNmi();
}

void Bus::BeginPendingDma(bool cpuCycleIsOdd)
{
    if (!m_dmaPending)
    {
        return;
    }

    m_dmaStallCycles = cpuCycleIsOdd ? 514 : 513;
    m_dmaPending = false;
}

bool Bus::ConsumeDmaStallCycle()
{
    if (m_dmaStallCycles != 0)
    {
        --m_dmaStallCycles;
        return true;
    }

    return m_apu.ConsumeDmcDmaStallCycle();
}

} // namespace dendyforge
