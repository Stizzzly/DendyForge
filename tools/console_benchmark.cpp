#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include "console/console.hpp"

namespace
{

// Realtime Dendy/NES CPU rate; one console clock is one CPU cycle.
constexpr std::uint64_t CpuClockHz = 1'789'773;
constexpr std::uint64_t BenchmarkCycles = 20'000'000;

} // namespace

// Clocks a fixed number of console cycles against a Mapper 0 ROM and
// reports the sustained speed in realtime multiples. Measurement protocol
// for CPU_CYCLE_ACCURACY_PLAN.md Phase 6; run it against the Release
// preset only (Debug is not a gameplay target).
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: DendyForgeConsoleBenchmark <mapper0.nes>\n";
        return 2;
    }

    dendyforge::Console console;
    if (!console.LoadRom(argv[1]))
    {
        std::cerr << argv[1] << ": could not load ROM\n";
        return 2;
    }

    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t cycle = 0; cycle < BenchmarkCycles; ++cycle)
    {
        console.Clock();
    }
    // The generated sample queue is only about 2 MiB for this run. Keeping it
    // intact mirrors the core work performed by the frontend without turning
    // the benchmark into a per-cycle vector allocation test.
    console.AudioProcessor().TakeSamples();
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const double seconds =
        std::chrono::duration<double>(elapsed).count();
    const double clocksPerSecond = BenchmarkCycles / seconds;
    const double realtimeMultiple = clocksPerSecond /
                                    static_cast<double>(CpuClockHz);

    std::printf("%.0f console clocks/s (%.2fx realtime, %.3f s for %llu "
                "clocks)\n",
                clocksPerSecond,
                realtimeMultiple,
                seconds,
                static_cast<unsigned long long>(BenchmarkCycles));
    return 0;
}
