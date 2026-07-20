#include <iostream>

#include "ines/ines_reader.hpp"

int main()
{
    dendyforge::INesReader reader;

    if (!reader.Load("roms/BattleCity.nes"))
    {
        std::cout << "Failed to load ROM\n";
        return 1;
    }

    std::cout << "ROM loaded successfully\n";

    return 0;
}
