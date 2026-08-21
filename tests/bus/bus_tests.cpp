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

TEST_CASE("Bus transfers a CPU memory page into PPU OAM through DMA")
{
    dendyforge::Bus bus;
    bus.CpuWrite(0x0200, 0x12);
    bus.CpuWrite(0x02FF, 0x34);

    bus.CpuWrite(0x2003, 0x00);
    bus.CpuWrite(0x4014, 0x02);

    bus.CpuWrite(0x2003, 0x00);
    CHECK(bus.CpuRead(0x2004) == 0x12);
    bus.CpuWrite(0x2003, 0xFF);
    CHECK(bus.CpuRead(0x2004) == 0x34);
}

TEST_CASE("OAM DMA stalls the CPU for 513 or 514 cycles based on parity")
{
    dendyforge::Bus bus;

    bus.CpuWrite(0x4014, 0x00);
    bus.BeginPendingDma(false);
    int evenCycleStall = 0;
    while (bus.ConsumeDmaStallCycle())
    {
        ++evenCycleStall;
    }
    CHECK(evenCycleStall == 513);

    bus.CpuWrite(0x4014, 0x00);
    bus.BeginPendingDma(true);
    int oddCycleStall = 0;
    while (bus.ConsumeDmaStallCycle())
    {
        ++oddCycleStall;
    }
    CHECK(oddCycleStall == 514);
}
