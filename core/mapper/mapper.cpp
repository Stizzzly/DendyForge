#include "mapper.hpp"

namespace dendyforge
{

Mapper::Mapper(
    std::uint8_t prgBanks,
    std::uint8_t chrBanks)
    : m_prgBanks(prgBanks),
      m_chrBanks(chrBanks)
{
}

Mirroring Mapper::MirroringMode(Mirroring headerMirroring) const
{
    return headerMirroring;
}

void Mapper::PpuClock()
{
}

void Mapper::ObservePpuAddress(std::uint16_t)
{
}

bool Mapper::PrgRamEnabled() const
{
    return true;
}

bool Mapper::PrgRamWriteProtected() const
{
    return false;
}

bool Mapper::IrqPending() const
{
    return false;
}

} // namespace dendyforge
