#include "ines_reader.hpp"

#include <fstream>

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

    return true;
}

const INesHeader& INesReader::Header() const
{
    return m_header;
}

} // namespace dendyforge
