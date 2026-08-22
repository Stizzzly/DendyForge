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
    const std::uint8_t banks8k = static_cast<std::uint8_t>(m_prgBanks * 2);
    const auto wrap = [banks8k](std::uint8_t bank) {
        return static_cast<std::uint8_t>(bank % banks8k);
    };
    const std::uint8_t last = static_cast<std::uint8_t>(banks8k - 1);
    const std::uint8_t penultimate =
        static_cast<std::uint8_t>(last - 1);

    std::uint8_t bank;
    if (address < 0xA000)
    {
        bank = m_prgMode ? penultimate : wrap(m_registers[6] & 0x0F);
    }
    else if (address < 0xC000)
    {
        bank = wrap(m_registers[7] & 0x0F);
    }
    else if (address < 0xE000)
    {
        bank = m_prgMode ? wrap(m_registers[6] & 0x0F) : penultimate;
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
        // odd addresses are the WRAM protect register, which the
        // cartridge's always-enabled PRG RAM ignores.
        if ((address & 0x01) == 0)
        {
            m_controlLoaded = true;
            m_verticalMirroring = (data & 0x01) != 0;
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

    if (m_chrBanks == 0)
    {
        // CHR RAM boards are not banked.
        mappedAddress = address;
        return true;
    }

    const std::uint8_t banks1k = static_cast<std::uint8_t>(m_chrBanks * 2);
    const auto wrap = [banks1k](std::uint8_t bank) {
        return static_cast<std::uint8_t>(bank % banks1k);
    };

    std::uint8_t bank1k;
    if (!m_chrMode)
    {
        // 2 KiB banks from R0/R1 at $0000, 1 KiB banks R2-R5 at $1000.
        if (address < 0x1000)
        {
            const std::uint8_t bank2k = static_cast<std::uint8_t>(
                m_registers[address >> 11] & 0x1E);
            bank1k = static_cast<std::uint8_t>(
                bank2k | ((address >> 10) & 0x01));
        }
        else
        {
            bank1k = static_cast<std::uint8_t>(
                m_registers[2 + ((address - 0x1000) >> 10)] & 0x1F);
        }
    }
    else
    {
        // 1 KiB banks R2-R5 at $0000, 2 KiB banks from R0/R1 at $1000.
        if (address < 0x1000)
        {
            bank1k = static_cast<std::uint8_t>(
                m_registers[2 + (address >> 10)] & 0x1F);
        }
        else
        {
            const std::uint8_t bank2k = static_cast<std::uint8_t>(
                m_registers[(address - 0x1000) >> 11] & 0x1E);
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

    mappedAddress = address;
    return true;
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

void Mapper4::PpuScanlineClock()
{
    // A clocked counter reloads silently on request, or asserts the IRQ
    // and reloads when it has counted down to zero; the asserted line
    // lasts until acknowledged through $E000.
    if (m_irqReload)
    {
        m_irqCounter = m_irqLatch;
        m_irqReload = false;
    }
    else if (m_irqCounter == 0)
    {
        m_irqActive = m_irqEnabled;
        m_irqCounter = m_irqLatch;
    }
    else
    {
        --m_irqCounter;
    }
}

bool Mapper4::IrqPending() const
{
    return m_irqActive;
}

} // namespace dendyforge
