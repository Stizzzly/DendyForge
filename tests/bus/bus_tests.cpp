#include "bus_tests.hpp"

#include <iostream>
#include <iomanip>

#include "bus/bus.hpp"

void RunBusTests()
{
    std::cout << "\n=== Bus Tests ===\n";

    dendyforge::Bus bus;

    bus.CpuWrite(0x0000, 0xAA);

    std::cout << "$0000 -> $"
              << std::hex
              << std::uppercase
              << std::setw(2)
              << std::setfill('0')
              << static_cast<int>(bus.CpuRead(0x0000))
              << '\n';

    std::cout << "$0800 -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x0800))
              << '\n';

    std::cout << "$1000 -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x1000))
              << '\n';

    std::cout << "$1800 -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x1800))
              << '\n';

    bus.CpuWrite(0x07FF, 0x55);

    std::cout << "\nMirror upper boundary:\n";

    std::cout << "$07FF -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x07FF))
              << '\n';

    std::cout << "$0FFF -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x0FFF))
              << '\n';

    std::cout << "$17FF -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x17FF))
              << '\n';

    std::cout << "$1FFF -> $"
              << std::setw(2)
              << static_cast<int>(bus.CpuRead(0x1FFF))
              << '\n';
}
