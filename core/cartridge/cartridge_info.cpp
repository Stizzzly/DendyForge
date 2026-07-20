#include "cartridge_info.hpp"

namespace dendyforge
{

CartridgeInfo::CartridgeInfo(const INesHeader& header)
    : m_header(header)
{
}

std::uint8_t CartridgeInfo::Mapper() const
{
    return (m_header.flags6 >> 4) |
           (m_header.flags7 & 0xF0);
}

Mirroring CartridgeInfo::MirroringMode() const
{
    if (m_header.flags6 & FLAG_MIRRORING)
    {
        return Mirroring::Vertical;
    }

    return Mirroring::Horizontal;
}

bool CartridgeInfo::HasBattery() const
{
    return (m_header.flags6 & FLAG_BATTERY) != 0;
}

bool CartridgeInfo::HasTrainer() const
{
    return (m_header.flags6 & FLAG_TRAINER) != 0;
}

} // namespace dendyforge
