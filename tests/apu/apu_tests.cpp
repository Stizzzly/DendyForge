#include <doctest/doctest.h>

#include <algorithm>

#include "apu/apu.hpp"

TEST_CASE("APU pulse channel produces samples after its registers are enabled")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4015, 0x01);
    apu.CpuWrite(0x4000, 0xDF);
    apu.CpuWrite(0x4002, 0x20);
    apu.CpuWrite(0x4003, 0x00);

    for (int cycle = 0; cycle < 10'000; ++cycle)
    {
        apu.Clock();
    }

    const auto samples = apu.TakeSamples();
    CHECK_FALSE(samples.empty());
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
    CHECK((apu.CpuRead(0x4015) & 0x01) != 0);
}

TEST_CASE("APU status disables a pulse channel and clears its length counter")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4015, 0x01);
    apu.CpuWrite(0x4003, 0x00);
    CHECK((apu.CpuRead(0x4015) & 0x01) != 0);

    apu.CpuWrite(0x4015, 0x00);
    CHECK((apu.CpuRead(0x4015) & 0x01) == 0);
}
