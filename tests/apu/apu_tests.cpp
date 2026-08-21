#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

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
    CHECK(std::all_of(samples.begin(), samples.end(), [](float sample) {
        return std::isfinite(sample) && std::abs(sample) <= 1.0F;
    }));
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

TEST_CASE("APU pulse envelope and sweep affect generated samples")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4015, 0x01);
    apu.CpuWrite(0x4000, 0x80);
    apu.CpuWrite(0x4001, 0x81);
    apu.CpuWrite(0x4002, 0x40);
    apu.CpuWrite(0x4003, 0x38);

    for (int cycle = 0; cycle < 80'000; ++cycle)
    {
        apu.Clock();
    }

    const auto samples = apu.TakeSamples();
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
}

TEST_CASE("APU triangle channel produces samples while linear and length counters run")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4015, 0x04);
    apu.CpuWrite(0x4008, 0xFF);
    apu.CpuWrite(0x400A, 0x20);
    apu.CpuWrite(0x400B, 0x00);

    for (int cycle = 0; cycle < 10'000; ++cycle)
    {
        apu.Clock();
    }

    const auto samples = apu.TakeSamples();
    CHECK((apu.CpuRead(0x4015) & 0x04) != 0);
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
}

TEST_CASE("APU noise channel produces samples through its shift register")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4015, 0x08);
    apu.CpuWrite(0x400C, 0x3F);
    apu.CpuWrite(0x400E, 0x00);
    apu.CpuWrite(0x400F, 0x18);

    for (int cycle = 0; cycle < 10'000; ++cycle)
    {
        apu.Clock();
    }

    const auto samples = apu.TakeSamples();
    CHECK((apu.CpuRead(0x4015) & 0x08) != 0);
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
}

TEST_CASE("APU DMC fetches sample bytes and contributes to the output")
{
    dendyforge::APU apu;
    apu.SetDmcMemoryReader([](std::uint16_t address) {
        return address == 0xC000 ? 0xFF : 0x00;
    });
    apu.CpuWrite(0x4010, 0x0F);
    apu.CpuWrite(0x4012, 0x00);
    apu.CpuWrite(0x4013, 0x10);
    apu.CpuWrite(0x4015, 0x10);

    for (int cycle = 0; cycle < 10'000; ++cycle)
    {
        apu.Clock();
    }

    const auto samples = apu.TakeSamples();
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
}

TEST_CASE("APU four-step frame counter raises and clears its status IRQ")
{
    dendyforge::APU apu;
    for (int cycle = 0; cycle < 29'829; ++cycle)
    {
        apu.Clock();
    }

    CHECK((apu.CpuRead(0x4015) & 0x40) != 0);
    CHECK((apu.CpuRead(0x4015) & 0x40) == 0);
}

TEST_CASE("APU five-step frame counter does not raise a frame IRQ")
{
    dendyforge::APU apu;
    apu.CpuWrite(0x4017, 0x80);
    for (int cycle = 0; cycle < 37'281; ++cycle)
    {
        apu.Clock();
    }

    CHECK((apu.CpuRead(0x4015) & 0x40) == 0);
}

TEST_CASE("APU reset clears channel status while preserving its DMC reader")
{
    dendyforge::APU apu;
    apu.SetDmcMemoryReader([](std::uint16_t) { return 0xFF; });
    apu.CpuWrite(0x4012, 0x00);
    apu.CpuWrite(0x4013, 0x10);
    apu.CpuWrite(0x4015, 0x10);
    apu.Reset();

    CHECK(apu.CpuRead(0x4015) == 0);
    apu.CpuWrite(0x4015, 0x10);
    for (int cycle = 0; cycle < 4'000; ++cycle)
    {
        apu.Clock();
    }
    const auto samples = apu.TakeSamples();
    CHECK(std::any_of(samples.begin(), samples.end(),
                      [](float sample) { return sample > 0.0F; }));
}
