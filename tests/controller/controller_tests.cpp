#include <doctest/doctest.h>

#include "bus/bus.hpp"

TEST_CASE("Controller serializes buttons through the CPU controller port")
{
    dendyforge::Bus bus;
    auto& controller = bus.PrimaryController();
    controller.SetButton(dendyforge::Controller::Button::A, true);
    controller.SetButton(dendyforge::Controller::Button::Start, true);
    controller.SetButton(dendyforge::Controller::Button::Left, true);

    bus.CpuWrite(0x4016, 0x01);
    bus.CpuWrite(0x4016, 0x00);

    CHECK(bus.CpuRead(0x4016) == 1);
    CHECK(bus.CpuRead(0x4016) == 0);
    CHECK(bus.CpuRead(0x4016) == 0);
    CHECK(bus.CpuRead(0x4016) == 1);
    CHECK(bus.CpuRead(0x4016) == 0);
    CHECK(bus.CpuRead(0x4016) == 0);
    CHECK(bus.CpuRead(0x4016) == 1);
    CHECK(bus.CpuRead(0x4016) == 0);
    CHECK(bus.CpuRead(0x4016) == 1);
}
