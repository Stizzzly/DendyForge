#pragma once

#include <cstdint>

namespace dendyforge
{

enum class Mirroring
{
    Horizontal,
    Vertical
};

constexpr std::uint8_t FLAG_MIRRORING = 0x01;
constexpr std::uint8_t FLAG_BATTERY = 0x02;
constexpr std::uint8_t FLAG_TRAINER = 0x04;
constexpr std::uint8_t FLAG_FOURSCREEN = 0x08;


struct INesHeader
{
    std::uint8_t magic[4];

    std::uint8_t prgRomBanks;
    std::uint8_t chrRomBanks;

    std::uint8_t flags6;
    std::uint8_t flags7;

    std::uint8_t prgRamSize;

    std::uint8_t flags9;
    std::uint8_t flags10;

    std::uint8_t reserved[5];
};

static_assert(sizeof(INesHeader) == 16,
              "INesHeader must be exactly 16 bytes.");

} // namespace dendyforge
