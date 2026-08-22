#pragma once

#include "mapper.hpp"

namespace dendyforge
{

// MMC1 (iNES mapper 1). Registers are loaded through a five-write serial
// shift register addressed by CPU address bits 14-13:
// $8000-$9FFF control, $A000-$BFFF CHR bank 0, $C000-$DFFF CHR bank 1,
// $E000-$FFFF PRG bank.
class Mapper1 : public Mapper
{
public:
    Mapper1(std::uint8_t prgBanks, std::uint8_t chrBanks);

    bool CpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool CpuWrite(std::uint16_t address, std::uint8_t data,
                  std::uint32_t& mappedAddress) override;
    bool PpuRead(std::uint16_t address,
                 std::uint32_t& mappedAddress) override;
    bool PpuWrite(std::uint16_t address,
                  std::uint32_t& mappedAddress) override;

    Mirroring MirroringMode(Mirroring headerMirroring) const override;

private:
    void LoadControl(std::uint8_t value);

    std::uint8_t m_shiftRegister{0};
    std::uint8_t m_writeCount{0};
    // PRG bank mode 3 at power-on; the nametable arrangement stays taken
    // from the iNES header until the game loads the control register.
    std::uint8_t m_control{0x0C};
    std::uint8_t m_chrBank0{0};
    std::uint8_t m_chrBank1{0};
    std::uint8_t m_prgBank{0};
    bool m_controlLoaded{false};
};

} // namespace dendyforge
