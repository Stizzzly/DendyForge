#include <doctest/doctest.h>

#include "bus/bus.hpp"

TEST_CASE("Bus mirrors CPU RAM across the 0x0000-0x1FFF range")
{
    dendyforge::Bus bus;

    bus.CpuWrite(0x0000, 0xAA);

    CHECK(bus.CpuRead(0x0000) == 0xAA);
    CHECK(bus.CpuRead(0x0800) == 0xAA);
    CHECK(bus.CpuRead(0x1000) == 0xAA);
    CHECK(bus.CpuRead(0x1800) == 0xAA);

    bus.CpuWrite(0x07FF, 0x55);

    CHECK(bus.CpuRead(0x07FF) == 0x55);
    CHECK(bus.CpuRead(0x0FFF) == 0x55);
    CHECK(bus.CpuRead(0x17FF) == 0x55);
    CHECK(bus.CpuRead(0x1FFF) == 0x55);
}
