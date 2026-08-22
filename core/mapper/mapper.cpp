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

void Mapper::PpuScanlineClock()
{
}

bool Mapper::IrqPending() const
{
    return false;
}

} // namespace dendyforge
