#include "ines_reader.hpp"

#include <fstream>
#include <utility>

namespace dendyforge
{

bool INesReader::Load(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    file.read(reinterpret_cast<char*>(&m_header), sizeof(INesHeader));

    if (!file)
    {
        return false;
    }

    if (m_header.magic[0] != 'N' ||
        m_header.magic[1] != 'E' ||
        m_header.magic[2] != 'S' ||
        m_header.magic[3] != 0x1A)
    {
        return false;
    }
    const std::size_t prgSize =
        static_cast<std::size_t>(m_header.prgRomBanks) * 16 * 1024;
    m_prgRom.resize(prgSize);

    file.read(
        reinterpret_cast<char*>(m_prgRom.data()),
        static_cast<std::streamsize>(prgSize));

    if (!file)
    {
    return false;
    }

    const std::size_t chrSize =
        static_cast<std::size_t>(m_header.chrRomBanks) * 8 * 1024;

    m_chrRom.resize(chrSize);

    if (chrSize > 0)
    {
        file.read(
            reinterpret_cast<char*>(m_chrRom.data()),
            static_cast<std::streamsize>(chrSize));

    if (!file)
    {
        return false;
    }
}

    return true;
}

const INesHeader& INesReader::Header() const
{
    return m_header;
}

std::vector<std::uint8_t> INesReader::TakePRGRom()
{
    return std::move(m_prgRom);
}

std::vector<std::uint8_t> INesReader::TakeCHRRom()
{
    return std::move(m_chrRom);
}

} // namespace dendyforge
