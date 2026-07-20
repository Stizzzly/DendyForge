#pragma once

#include <string>

#include "ines.hpp"

namespace dendyforge
{

class INesReader
{
public:
    bool Load(const std::string& path);

private:
    INesHeader m_header{};
};

} // namespace dendyforge
