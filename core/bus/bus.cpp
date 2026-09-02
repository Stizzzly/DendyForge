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
    m_cpuAccessObserved = true;
    m_lastCpuAccessWasWrite = false;
    m_lastCpuAddress = address;

    // Built-in ranges below $6000 can never be claimed by a cartridge
    // (Mapper 0/2 decode only $6000-$FFFF), so answer them directly and
    // skip the virtual cartridge call on the hot RAM/register paths.
    // $0000-$1FFF: 2 KB internal RAM (mirrored every $800 bytes)
    if (address <= 0x1FFF)
    {
        m_cpuDataBus = m_cpuRam[address & 0x07FF];
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = m_cpuDataBus;
        }
        return m_cpuDataBus;
    }

    // $2000-$3FFF: PPU registers (mirrored every 8 bytes)
    if (address <= 0x3FFF)
    {
        m_cpuDataBus = m_ppu.CpuRead(address);
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = m_cpuDataBus;
        }
        return m_cpuDataBus;
    }

    if (address == 0x4016)
    {
        // The controller port drives the low five lines; D7-D5 retain the
        // CPU bus value. A standard NES controller only asserts D0.
        m_cpuDataBus = static_cast<std::uint8_t>(
            (m_cpuDataBus & 0xE0) | (m_controller1.Read() & 0x1F));
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = m_cpuDataBus;
        }
        return m_cpuDataBus;
    }

    if (address == 0x4015)
    {
        // $4015's bit 5 is undriven. Its status is delivered to the CPU's
        // internal data bus and does not replace the external bus latch.
        const std::uint8_t value = static_cast<std::uint8_t>(
            (m_apu.CpuRead(address) & 0xDF) |
            (m_cpuInternalDataBus & 0x20));
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = value;
        }
        return value;
    }

    // $4017: controller port 2 reads the Zapper light sensor and trigger.
    if (address == 0x4017)
    {
        m_cpuDataBus = static_cast<std::uint8_t>(
            (m_cpuDataBus & 0xE0) |
            (m_zapper.ReadPort(
                 m_ppu.Scanline(), m_ppu.Cycle(), m_ppu.FrameBuffer()) &
             0x1F));
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = m_cpuDataBus;
        }
        return m_cpuDataBus;
    }

    // $4020-$FFFF (and the unclaimed $4018-$401F/$4020-$5FFF open bus
    // when no cartridge decodes them): cartridge space.
    std::uint8_t data = 0x00;
    if (m_cartridge &&
        m_cartridge->CpuRead(address, data))
    {
        m_cpuDataBus = data;
        if (!m_dmaBusAccess)
        {
            m_cpuInternalDataBus = m_cpuDataBus;
        }
        return m_cpuDataBus;
    }

    if (!m_dmaBusAccess)
    {
        m_cpuInternalDataBus = m_cpuDataBus;
    }
    return m_cpuDataBus;
}

void Bus::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    m_cpuAccessObserved = true;
    m_lastCpuAccessWasWrite = true;
    m_lastCpuAddress = address;

    // Every CPU write drives all eight data lines, including writes to an
    // unimplemented or nominally read-only register.
    m_cpuDataBus = data;
    if (!m_dmaBusAccess)
    {
        m_cpuInternalDataBus = data;
    }

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

void Bus::BeginPendingDma()
{
    if (m_dmaPending)
    {
        if (!m_oamDmaActive)
        {
            m_oamDmaActive = true;
            m_oamWritePending = false;
            m_oamBytesTransferred = 0;
            m_dmaOffset = 0;
            m_dmaNeedsHalt = true;
        }
        // A read-modify-write of $4014 writes twice before the first halt
        // succeeds. It updates the page but starts only one transfer.
        m_dmaPending = false;
    }

    std::uint16_t address = 0;
    bool abortOnly = false;
    if (!m_dmcDmaActive &&
        m_apu.ConsumeDmcDmaRequest(address, abortOnly))
    {
        m_dmcDmaActive = true;
        m_dmcDmaAbortOnly = abortOnly;
        m_dmcDmaAddress = address;
        m_dmaNeedsHalt = true;
        m_dmcNeedsDummyRead = !abortOnly;
    }
}

std::uint8_t Bus::DmaRead(
    std::uint16_t address,
    bool preserveInternalBus)
{
    if (!m_dmaInternalReadsEnabled)
    {
        // The APU and controller registers are internal to the 2A03 and do
        // not respond to an ordinary external DMA read.
        if (address >= 0x4000 && address <= 0x401F)
        {
            m_previousDmaReadAddress = address;
            return m_cpuDataBus;
        }

        m_previousDmaReadAddress = address;
        return DmaExternalRead(address, preserveInternalBus);
    }

    // If RDY halted the CPU while it was reading $4000-$401F, the low five
    // address bits driven by DMA can accidentally select an internal 2A03
    // register at the same time as the external DMA source. This is the
    // documented DMC/controller/APU bus-conflict behavior.
    const std::uint16_t internalAddress =
        static_cast<std::uint16_t>(0x4000 | (address & 0x001F));
    const bool sameAddress = internalAddress == address;
    std::uint8_t value = 0;

    if (internalAddress == 0x4015)
    {
        value = DmaExternalRead(internalAddress, preserveInternalBus);
        if (!sameAddress)
        {
            // The external source still drives the CPU data bus, but the
            // DMC receives the internal status-register value.
            DmaExternalRead(address, preserveInternalBus);
        }
    }
    else if (internalAddress == 0x4016 || internalAddress == 0x4017)
    {
        // On the front-loading NES, keeping /OE asserted across two adjacent
        // reads does not clock the controller twice.
        if (m_previousDmaReadAddress == internalAddress)
        {
            value = m_cpuDataBus;
        }
        else
        {
            value = DmaExternalRead(internalAddress, preserveInternalBus);
        }

        if (!sameAddress)
        {
            const std::uint8_t externalValue =
                DmaExternalRead(address, preserveInternalBus);
            constexpr std::uint8_t OpenBusMask = 0xE0;
            value = static_cast<std::uint8_t>(
                (externalValue & OpenBusMask) |
                (value & ~OpenBusMask));

            // Unlike $4015, the controller lines participate in the value
            // left on the CPU-visible data bus. The following open-bus read
            // must therefore observe the merged external/controller value,
            // not the unmodified byte fetched from the sample address.
            m_cpuDataBus = value;
        }
    }
    else
    {
        value = DmaExternalRead(address, preserveInternalBus);
    }

    m_previousDmaReadAddress = internalAddress;
    return value;
}

std::uint8_t Bus::DmaExternalRead(
    std::uint16_t address,
    bool preserveInternalBus)
{
    m_dmaBusAccess = preserveInternalBus;
    const std::uint8_t value = CpuRead(address);
    m_dmaBusAccess = false;
    return value;
}

void Bus::DmaDummyRead()
{
    // NES controller /OE remains asserted through adjacent DMA setup cycles,
    // so only the CPU's original halt read shifts the controller.
    if (m_haltedCpuAddress != 0x4016 && m_haltedCpuAddress != 0x4017)
    {
        CpuRead(m_haltedCpuAddress);
    }
}

void Bus::ClockCpuCycle(CPU6502& cpu, bool getCycle)
{
    const auto clockControllerStrobe = [&]()
    {
        if (getCycle)
        {
            m_controller1.ClockPutCycle();
        }
    };

    if (!m_cpuHaltedForDma && !m_dmaNeedsHalt)
    {
        cpu.ReadyLine(true);
        m_cpuAccessObserved = false;
        cpu.Clock();
        clockControllerStrobe();
        return;
    }

    if (!m_cpuHaltedForDma)
    {
        // RDY stretches reads but cannot stop a write. A failed halt simply
        // remains armed for the next CPU cycle.
        cpu.ReadyLine(false);
        m_cpuAccessObserved = false;
        cpu.Clock();
        if (m_cpuAccessObserved && m_lastCpuAccessWasWrite)
        {
            if (m_dmcDmaActive && m_dmcDmaAbortOnly)
            {
                m_dmcDmaActive = false;
                m_dmcDmaAbortOnly = false;
                m_dmcNeedsDummyRead = false;
                if (!m_oamDmaActive)
                {
                    m_dmaNeedsHalt = false;
                }
            }
            clockControllerStrobe();
            return;
        }

        m_cpuHaltedForDma = true;
        m_haltedCpuAddress = m_lastCpuAddress;
        m_previousDmaReadAddress = m_haltedCpuAddress;
        m_dmaInternalReadsEnabled =
            (m_haltedCpuAddress & 0xFFE0) == 0x4000;
        m_dmaNeedsHalt = false;
        if (m_dmcDmaAbortOnly)
        {
            // An aborted DMC DMA is just this halt attempt. It performs no
            // dummy/alignment/get phases and releases RDY immediately.
            m_dmcDmaActive = false;
            m_dmcDmaAbortOnly = false;
            m_cpuHaltedForDma = false;
            cpu.ReadyLine(true);
        }
        clockControllerStrobe();
        return;
    }

    const bool dmcReadyForGet =
        m_dmcDmaActive && !m_dmaNeedsHalt && !m_dmcNeedsDummyRead;

    if (getCycle)
    {
        if (dmcReadyForGet)
        {
            const std::uint8_t value = DmaRead(m_dmcDmaAddress, true);
            m_apu.CompleteDmcDma(value);
            m_dmcDmaActive = false;
            m_dmcDmaAbortOnly = false;
        }
        else if (m_oamDmaActive && !m_oamWritePending)
        {
            m_dmaValue = DmaRead(
                (static_cast<std::uint16_t>(m_dmaPage) << 8) |
                m_dmaOffset,
                false);
            m_oamWritePending = true;
        }
        else
        {
            DmaDummyRead();
        }
    }
    else if (m_oamDmaActive && m_oamWritePending)
    {
        CpuWrite(0x2004, m_dmaValue);
        m_oamWritePending = false;
        ++m_dmaOffset;
        if (++m_oamBytesTransferred == 256)
        {
            m_oamDmaActive = false;
        }
    }
    else
    {
        DmaDummyRead();
    }

    // OAM accesses overlap the DMC's halt and dummy phases. Decide the
    // action above from the pre-cycle state, then consume one setup phase.
    if (m_dmaNeedsHalt)
    {
        m_dmaNeedsHalt = false;
    }
    else if (m_dmcNeedsDummyRead)
    {
        m_dmcNeedsDummyRead = false;
    }

    if (!m_oamDmaActive && !m_dmcDmaActive)
    {
        m_cpuHaltedForDma = false;
        cpu.ReadyLine(true);
    }

    clockControllerStrobe();
}

bool Bus::DmaActive() const
{
    return m_dmaPending || m_oamDmaActive || m_dmcDmaActive ||
           m_cpuHaltedForDma || m_dmaNeedsHalt;
}

} // namespace dendyforge
