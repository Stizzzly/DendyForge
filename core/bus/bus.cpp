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

Zapper& Bus::SecondaryZapper()
{
    return m_zapper;
}

bool Bus::CartridgeIrqPending() const
{
    return m_cartridge && m_cartridge->IrqPending();
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

    // $4017: controller port 2 reads the Zapper light sensor and trigger.
    if (address == 0x4017)
    {
        return m_zapper.ReadPort(
            m_ppu.Scanline(), m_ppu.Cycle(), m_ppu.FrameBuffer());
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
    // OAM DMA transfers one CPU memory page into PPU sprite memory over
    // 512 real CPU cycles (one read and one write cycle per byte) after
    // one or two alignment cycles; the Console stalls the CPU while the
    // PPU and APU keep clocking.
    if (address == 0x4014)
    {
        m_dmaPage = data;
        m_dmaOffset = 0;
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

std::optional<std::uint8_t> Bus::DebugPeekCpu(std::uint16_t address)
{
    if (address <= 0x1FFF)
    {
        return m_cpuRam[address & 0x07FF];
    }
    if (address <= 0x3FFF)
    {
        return m_ppu.DebugPeekCpuRegister(address);
    }
    // Reading APU/controller ports changes their observable state. The
    // debugger intentionally reports these locations as unavailable instead.
    if (address <= 0x401F)
    {
        return std::nullopt;
    }

    std::uint8_t data = 0;
    if (m_cartridge && m_cartridge->CpuRead(address, data))
    {
        return data;
    }
    return std::nullopt;
}

void Bus::ClockPpu()
{
    m_ppu.Clock();
}

bool Bus::PpuNmiLine() const
{
    return m_ppu.NmiLineLevel();
}

void Bus::BeginPendingDma(bool cpuCycleIsOdd)
{
    if (!m_dmaPending)
    {
        return;
    }

    // 513 cycles on an even CPU cycle, 514 on an odd one: the transfer
    // needs 512 cycles and the remainder is the alignment gap.
    m_dmaStallCycles = cpuCycleIsOdd ? 514 : 513;
    m_dmaAlignmentCycles =
        static_cast<std::uint16_t>(m_dmaStallCycles - 512);
    m_dmaPending = false;
}

bool Bus::ConsumeDmaStallCycle()
{
    if (m_dmaStallCycles != 0)
    {
        --m_dmaStallCycles;
        if (m_dmaAlignmentCycles > 0)
        {
            --m_dmaAlignmentCycles;
        }
        else
        {
            // After the alignment gap, alternating cycles read from the
            // CPU page and write the byte into OAM.
            if (m_dmaReadPhase)
            {
                m_dmaValue = CpuRead(
                    (static_cast<std::uint16_t>(m_dmaPage) << 8) |
                    m_dmaOffset);
                m_dmaReadPhase = false;
            }
            else
            {
                m_ppu.CpuWrite(0x2004, m_dmaValue);
                ++m_dmaOffset;
                m_dmaReadPhase = true;
            }
        }
        return true;
    }

    return m_apu.ConsumeDmcDmaStallCycle();
}

} // namespace dendyforge
