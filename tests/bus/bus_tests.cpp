#include <doctest/doctest.h>

#include "bus/bus.hpp"

namespace
{

int DrainDma(dendyforge::Bus& bus, dendyforge::CPU6502& cpu,
             bool firstCycleIsGet)
{
    int cycles = 0;
    bool getCycle = firstCycleIsGet;
    do
    {
        bus.BeginPendingDma();
        bus.ClockCpuCycle(cpu, getCycle);
        getCycle = !getCycle;
        ++cycles;
    } while (bus.DmaActive());
    return cycles;
}

} // namespace

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

TEST_CASE("CPU open bus retains writes and unmapped reads")
{
    dendyforge::Bus bus;

    bus.CpuWrite(0x0000, 0xA5);
    CHECK(bus.CpuRead(0x4000) == 0xA5);
    CHECK(bus.CpuRead(0x5000) == 0xA5);

    bus.CpuWrite(0x4015, 0x3C);
    CHECK(bus.CpuRead(0x4000) == 0x3C);
}

TEST_CASE("Controller reads preserve the upper CPU open-bus bits")
{
    dendyforge::Bus bus;
    bus.PrimaryController().SetButton(
        dendyforge::Controller::Button::A, true);
    // The strobe latches on a put cycle while it is held high, not on the
    // write itself.
    bus.CpuWrite(0x4016, 0x01);
    bus.PrimaryController().ClockPutCycle();
    bus.CpuWrite(0x4016, 0x00);

    bus.CpuWrite(0x0000, 0xA0);
    CHECK(bus.CpuRead(0x4016) == 0xA1);
}

TEST_CASE("APU status bit 5 is open without replacing the external bus latch")
{
    dendyforge::Bus bus;
    bus.CpuWrite(0x0000, 0x20);

    CHECK((bus.CpuRead(0x4015) & 0x20) != 0);
    CHECK(bus.CpuRead(0x4000) == 0x20);
}

TEST_CASE("Bus transfers a CPU memory page into PPU OAM through DMA")
{
    dendyforge::Bus bus;
    dendyforge::CPU6502 cpu(
        dendyforge::CPU6502::Configuration{.decimalModeEnabled = false});
    cpu.ConnectBus(&bus);
    bus.CpuWrite(0x0200, 0x12);
    bus.CpuWrite(0x02FF, 0x34);

    bus.CpuWrite(0x2003, 0x00);
    bus.CpuWrite(0x4014, 0x02);

    CHECK(DrainDma(bus, cpu, false) == 513);

    bus.CpuWrite(0x2003, 0x00);
    CHECK(bus.CpuRead(0x2004) == 0x12);
    bus.CpuWrite(0x2003, 0xFF);
    CHECK(bus.CpuRead(0x2004) == 0x34);
}

TEST_CASE("OAM DMA stalls the CPU for 513 or 514 cycles based on parity")
{
    dendyforge::Bus bus;
    dendyforge::CPU6502 cpu(
        dendyforge::CPU6502::Configuration{.decimalModeEnabled = false});
    cpu.ConnectBus(&bus);

    bus.CpuWrite(0x4014, 0x00);
    CHECK(DrainDma(bus, cpu, false) == 513);

    bus.CpuWrite(0x4014, 0x00);
    CHECK(DrainDma(bus, cpu, true) == 514);
}
