#pragma once

#include <cstdint>

#include "ines/ines.hpp"

namespace dendyforge
{

class CartridgeInfo
{
public:
    explicit CartridgeInfo(const INesHeader& header);

    std::uint8_t Mapper() const;

    Mirroring MirroringMode() const;

    bool HasBattery() const;

    bool HasTrainer() const;

private:
    INesHeader m_header{};
};

} // namespace dendyforge
