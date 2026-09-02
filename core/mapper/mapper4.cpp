#include "mapper4.hpp"

namespace dendyforge
{

Mapper4::Mapper4(std::uint8_t prgBanks, std::uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks)
{
}

bool Mapper4::CpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address < 0x8000 || m_prgBanks == 0)
    {
        return false;
    }

    // PRG is mapped in 8 KiB units; the mapper counts 16 KiB banks, so
    // the fixed banks are the last two 8 KiB banks.
    const std::uint16_t banks8k =
        static_cast<std::uint16_t>(m_prgBanks) * 2;
    const auto wrap = [banks8k](std::uint8_t bank) {
        return static_cast<std::uint16_t>(bank % banks8k);
    };
    const std::uint16_t last = banks8k - 1;
    const std::uint16_t penultimate = last - 1;

    std::uint16_t bank;
    if (address < 0xA000)
    {
        bank = m_prgMode ? penultimate : wrap(m_registers[6] & 0x3F);
    }
    else if (address < 0xC000)
    {
        bank = wrap(m_registers[7] & 0x3F);
    }
    else if (address < 0xE000)
    {
        bank = m_prgMode ? wrap(m_registers[6] & 0x3F) : penultimate;
    }
    else
    {
        bank = last;
    }

    mappedAddress = static_cast<std::uint32_t>(bank) * 0x2000 |
                    (address & 0x1FFF);
    return true;
}

bool Mapper4::CpuWrite(std::uint16_t address, std::uint8_t data,
                       std::uint32_t& mappedAddress)
{
    if (address < 0x8000)
    {
        return false;
    }

    switch ((address >> 13) & 0x03)
    {
    case 0:
        // $8000-$9FFF: even addresses select the register, PRG mode
        // (bit 6) and CHR mode (bit 7); odd addresses load the selected
        // bank register.
        if ((address & 0x01) == 0)
        {
            m_bankSelect = data;
            m_prgMode = (data & 0x40) != 0;
            m_chrMode = (data & 0x80) != 0;
        }
        else
        {
            m_registers[m_bankSelect & 0x07] = data;
        }
        break;
    case 1:
        // $A000-$BFFF: even addresses switch the nametable arrangement;
        // odd addresses control PRG-RAM: bit 7 enables the chip and bit 6
        // blocks writes while preserving reads.
        if ((address & 0x01) == 0)
        {
            m_controlLoaded = true;
            // MMC3 drives CIRAM A10 directly when the bit is clear
            // (vertical arrangement); setting it selects horizontal.
            m_verticalMirroring = (data & 0x01) == 0;
        }
        else
        {
            m_prgRamEnabled = (data & 0x80) != 0;
            m_prgRamWriteProtected = (data & 0x40) != 0;
        }
        break;
    case 2:
        // $C000-$DFFF: IRQ latch, then IRQ reload request.
        if ((address & 0x01) == 0)
        {
            m_irqLatch = data;
        }
        else
        {
            m_irqReload = true;
        }
        break;
    default:
        // $E000-$FFFF: IRQ disable-and-acknowledge, then enable.
        if ((address & 0x01) == 0)
        {
            m_irqEnabled = false;
            m_irqActive = false;
        }
        else
        {
            m_irqEnabled = true;
        }
        break;
    }

    mappedAddress = NoMappedAddress;
    return true;
}

bool Mapper4::PpuRead(std::uint16_t address,
                      std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF)
    {
        return false;
    }

    // Both CHR ROM and CHR RAM are selected by the MMC3's 1 KiB bank
    // registers. iNES stores CHR-ROM capacity in 8 KiB units.
    const std::uint16_t banks1k = m_chrBanks == 0
        ? 8
        : static_cast<std::uint16_t>(m_chrBanks) * 8;
    const auto wrap = [banks1k](std::uint8_t bank) {
        return static_cast<std::uint16_t>(bank % banks1k);
    };

    std::uint8_t bank1k;
    if (!m_chrMode)
    {
        // 2 KiB banks from R0/R1 at $0000, 1 KiB banks R2-R5 at $1000.
        if (address < 0x1000)
        {
            const std::uint8_t bank2k = static_cast<std::uint8_t>(
                m_registers[address >> 11] & 0xFE);
            bank1k = static_cast<std::uint8_t>(
                bank2k | ((address >> 10) & 0x01));
        }
        else
        {
            bank1k = static_cast<std::uint8_t>(
                m_registers[2 + ((address - 0x1000) >> 10)]);
        }
    }
    else
    {
        // 1 KiB banks R2-R5 at $0000, 2 KiB banks from R0/R1 at $1000.
        if (address < 0x1000)
        {
            bank1k = static_cast<std::uint8_t>(
                m_registers[2 + (address >> 10)]);
        }
        else
        {
            const std::uint8_t bank2k = static_cast<std::uint8_t>(
                m_registers[(address - 0x1000) >> 11] & 0xFE);
            bank1k = static_cast<std::uint8_t>(
                bank2k | ((address >> 10) & 0x01));
        }
    }

    mappedAddress = static_cast<std::uint32_t>(wrap(bank1k)) * 0x400 |
                    (address & 0x03FF);
    return true;
}

bool Mapper4::PpuWrite(std::uint16_t address,
                       std::uint32_t& mappedAddress)
{
    if (address > 0x1FFF || m_chrBanks != 0)
    {
        return false;
    }

    // CHR-RAM boards still pass through the MMC3 CHR bank registers.
    return PpuRead(address, mappedAddress);
}

Mirroring Mapper4::MirroringMode(Mirroring headerMirroring) const
{
    if (!m_controlLoaded)
    {
        return headerMirroring;
    }
    return m_verticalMirroring ? Mirroring::Vertical
                               : Mirroring::Horizontal;
}

void Mapper4::PpuClock()
{
    if (!m_a12High && m_a12LowCycles < 8)
    {
        ++m_a12LowCycles;
    }
}

void Mapper4::ObservePpuAddress(std::uint16_t address)
{
    const bool a12High = (address & 0x1000) != 0;
    if (a12High && !m_a12High && m_a12LowCycles >= 8)
    {
        ClockIrqCounter();
    }
    else if (!a12High && m_a12High)
    {
        m_a12LowCycles = 0;
    }
    m_a12High = a12High;
}

bool Mapper4::MonitorsPpuBus() const noexcept
{
    return true;
}

bool Mapper4::PrgRamEnabled() const
{
    return m_prgRamEnabled;
}

bool Mapper4::PrgRamWriteProtected() const
{
    return m_prgRamWriteProtected;
}

void Mapper4::ClockIrqCounter()
{
    // On each qualified rising A12 edge, a zero counter or a pending
    // reload loads the latch. Otherwise it decrements. Reaching zero on
    // this very edge raises /IRQ; it remains asserted until $E000.
    if (m_irqCounter == 0 || m_irqReload)
    {
        m_irqCounter = m_irqLatch;
    }
    else
    {
        --m_irqCounter;
    }

    if (m_irqCounter == 0 && m_irqEnabled)
    {
        m_irqActive = true;
    }
    m_irqReload = false;
}

bool Mapper4::IrqPending() const
{
    return m_irqActive;
}

} // namespace dendyforge
