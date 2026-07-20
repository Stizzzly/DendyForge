#pragma once

#include <cstdint>

namespace dendyforge
{

struct INesHeader
{
    char magic[4];

    std::uint8_t prgRomBanks;
    std::uint8_t chrRomBanks;

    std::uint8_t flags6;
    std::uint8_t flags7;

    std::uint8_t prgRamSize;

    std::uint8_t flags9;
    std::uint8_t flags10;

    std::uint8_t reserved[5];
};

} // namespace dendyforge
