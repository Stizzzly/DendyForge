#pragma once

#include <string>
#include <vector>

#include "ines.hpp"

namespace dendyforge
{

class INesReader
{
public:
    bool Load(const std::string& path);

    const INesHeader& Header() const;

    std::vector<std::uint8_t> TakePRGRom();
    std::vector<std::uint8_t> TakeCHRRom();

private:
    INesHeader m_header{};

    std::vector<std::uint8_t> m_prgRom;
    std::vector<std::uint8_t> m_chrRom;
};

} // namespace dendyforge
