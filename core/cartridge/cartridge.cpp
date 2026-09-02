#include "cartridge.hpp"
#include "../mapper/mapper0.hpp"
#include "../mapper/mapper1.hpp"
#include "../mapper/mapper2.hpp"
#include "../mapper/mapper3.hpp"
#include "../mapper/mapper4.hpp"
#include "../mapper/mapper7.hpp"

#include <algorithm>
#include <utility>
#include <stdexcept>

namespace dendyforge
{

Cartridge::Cartridge(
    const INesHeader& header,
    std::vector<std::uint8_t>&& prgRom,
    std::vector<std::uint8_t>&& chrRom)
    : m_info(header),
      m_prgRom(std::move(prgRom)),
      m_prgRam(static_cast<std::size_t>(
          header.prgRamSize == 0 ? 1 : header.prgRamSize) * 8 * 1024),
      m_chrRom(std::move(chrRom))
{
    switch (m_info.Mapper())
    {
    case 0:
        m_mapper = std::make_unique<Mapper0>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    case 1:
        m_mapper = std::make_unique<Mapper1>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    case 2:
        m_mapper = std::make_unique<Mapper2>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    case 3:
        m_mapper = std::make_unique<Mapper3>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    case 4:
        m_mapper = std::make_unique<Mapper4>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    case 7:
        m_mapper = std::make_unique<Mapper7>(
            PRGRomBanks(),
            CHRRomBanks());
        break;

    default:
        throw std::runtime_error("Unsupported mapper");
    }

    if (m_chrRom.empty())
    {
        m_chrRam.resize(8 * 1024);
    }
}

const CartridgeInfo& Cartridge::Info() const
{
    return m_info;
}

Mirroring Cartridge::CurrentMirroring() const
{
    return m_mapper->MirroringMode(m_info.MirroringMode());
}

void Cartridge::PpuClock()
{
    m_mapper->PpuClock();
}

void Cartridge::ObservePpuAddress(std::uint16_t address)
{
    m_mapper->ObservePpuAddress(address);
}

bool Cartridge::MonitorsPpuBus() const noexcept
{
    return m_mapper->MonitorsPpuBus();
}

bool Cartridge::IrqPending() const
{
    return m_mapper->IrqPending();
}

const std::vector<std::uint8_t>& Cartridge::PRGRom() const
{
    return m_prgRom;
}

const std::vector<std::uint8_t>& Cartridge::CHRRom() const
{
    return m_chrRom;
}

bool Cartridge::HasBatteryBackedPrgRam() const
{
    return m_info.HasBattery();
}

std::span<const std::uint8_t> Cartridge::BatteryBackedPrgRam() const
{
    return HasBatteryBackedPrgRam() ? std::span<const std::uint8_t>(m_prgRam)
                                    : std::span<const std::uint8_t>{};
}

bool Cartridge::RestoreBatteryBackedPrgRam(std::span<const std::uint8_t> data)
{
    if (!HasBatteryBackedPrgRam() || data.size() != m_prgRam.size())
    {
        return false;
    }
    std::copy(data.begin(), data.end(), m_prgRam.begin());
    return true;
}

bool Cartridge::CpuRead(
    std::uint16_t address,
    std::uint8_t& data)
{
    if (address >= 0x6000 && address <= 0x7FFF)
    {
        if (!m_mapper->PrgRamEnabled())
        {
            return false;
        }
        data = m_prgRam[address & 0x1FFF];
        return true;
    }

    std::uint32_t mappedAddress;

    if (!m_mapper->CpuRead(address, mappedAddress))
    {
        return false;
    }

    if (mappedAddress >= m_prgRom.size())
{
    return false;
}

    data = m_prgRom[mappedAddress];
    return true;
}

bool Cartridge::CpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    if (address >= 0x6000 && address <= 0x7FFF)
    {
        if (!m_mapper->PrgRamEnabled())
        {
            return false;
        }
        if (!m_mapper->PrgRamWriteProtected())
        {
            m_prgRam[address & 0x1FFF] = data;
        }
        return true;
    }

    std::uint32_t mappedAddress;

    if (!m_mapper->CpuWrite(address, data, mappedAddress))
    {
        return false;
    }

    if (mappedAddress == Mapper::NoMappedAddress)
    {
        return true;
    }

    if (mappedAddress >= m_prgRom.size())
    {
        return false;
    }

    m_prgRom[mappedAddress] = data;
    return true;
}

bool Cartridge::PpuRead(
    std::uint16_t address,
    std::uint8_t& data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->PpuRead(address, mappedAddress))
    {
        return false;
    }

    const std::vector<std::uint8_t>& chrMemory =
        m_chrRom.empty() ? m_chrRam : m_chrRom;
    if (mappedAddress >= chrMemory.size())
    {
        return false;
    }

    data = chrMemory[mappedAddress];
    return true;
}

bool Cartridge::PpuWrite(
    std::uint16_t address,
    std::uint8_t data)
{
    std::uint32_t mappedAddress;

    if (!m_mapper->PpuWrite(address, mappedAddress))
    {
        return false;
    }

    std::vector<std::uint8_t>& chrMemory =
        m_chrRom.empty() ? m_chrRam : m_chrRom;
    if (mappedAddress >= chrMemory.size())
    {
        return false;
    }

    chrMemory[mappedAddress] = data;
    return true;
}

std::uint8_t Cartridge::PRGRomBanks() const
{
    return static_cast<std::uint8_t>(m_prgRom.size() / (16 * 1024));
}

std::uint8_t Cartridge::CHRRomBanks() const
{
    return static_cast<std::uint8_t>(m_chrRom.size() / (8 * 1024));
}


} // namespace dendyforge
