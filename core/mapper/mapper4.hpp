#pragma once

#include <array>
#include <cstdint>

#include "mapper.hpp"

namespace dendyforge
{

// MMC3 (iNES mapper 4). Eight internal registers selected by bits 2-0 of
// the bank-select register: two switchable 8 KiB PRG banks plus two fixed
// at the end (order chosen by the PRG mode bit), six CHR banks in 1/2 KiB
// units (arrangement chosen by the CHR mode bit), nametable mirroring,
// and a scanline IRQ counter clocked once per rendering scanline by the
// PPU sprite-fetch phase.
class Mapper4 : public Mapper
{
public:
    Mapper4(std::uint8_t prgBanks, std::uint8_t chrBanks);

    bool CpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool CpuWrite(std::uint16_t address, std::uint8_t data,
                  std::uint32_t& mappedAddress) override;
    bool PpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool PpuWrite(std::uint16_t address,
                  std::uint32_t& mappedAddress) override;

    Mirroring MirroringMode(Mirroring headerMirroring) const override;
    void PpuScanlineClock() override;
    bool IrqPending() const override;

private:
    std::uint8_t m_bankSelect{0};
    std::array<std::uint8_t, 8> m_registers{};
    std::uint8_t m_irqLatch{0};
    std::uint8_t m_irqCounter{0};
    bool m_irqReload{false};
    bool m_irqEnabled{false};
    bool m_irqActive{false};
    bool m_prgMode{false};
    bool m_chrMode{false};
    bool m_controlLoaded{false};
    bool m_verticalMirroring{false};
};

} // namespace dendyforge
