#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include "cpu/cpu6502.hpp"

namespace
{

class FlatRamCpuBus final : public dendyforge::CpuBus
{
public:
    std::uint8_t CpuRead(std::uint16_t address) override
    {
        return memory[address];
    }

    void CpuWrite(std::uint16_t address, std::uint8_t value) override
    {
        memory[address] = value;
    }

    std::array<std::uint8_t, 0x10000> memory{};
};

void CompleteInstruction(dendyforge::CPU6502& cpu)
{
    cpu.Clock();
    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
}

std::filesystem::path KlausRomPath()
{
    return std::filesystem::path(DENDYFORGE_SOURCE_DIR) /
           "tests" / "cpu" / "roms" / "klaus" / "6502_functional_test.bin";
}

} // namespace

TEST_CASE("CPU6502 passes Klaus Dormann NMOS 6502 functional test")
{
    // Upstream's default image has decimal tests enabled, uses writable code
    // for self-modifying cases, and reports success by executing JMP $3469.
    // It is a standalone NMOS 6502 test, not an iNES cartridge image.
    constexpr std::uint16_t kEntryPoint = 0x0400;
    constexpr std::uint16_t kSuccessLoop = 0x3469;
    constexpr std::size_t kInstructionLimit = 40'000'000;

    FlatRamCpuBus bus;
    std::ifstream image(KlausRomPath(), std::ios::binary);
    REQUIRE(image.is_open());

    image.read(reinterpret_cast<char*>(bus.memory.data()),
               static_cast<std::streamsize>(bus.memory.size()));
    REQUIRE(image.gcount() == static_cast<std::streamsize>(bus.memory.size()));
    REQUIRE(image.peek() == std::char_traits<char>::eof());

    dendyforge::CPU6502 cpu({.decimalModeEnabled = true});
    cpu.ConnectBus(&bus);
    cpu.Reset();
    while (cpu.Cycles() > 0)
    {
        cpu.Clock();
    }
    cpu.SetProgramCounter(kEntryPoint);

    for (std::size_t instruction = 0; instruction < kInstructionLimit; ++instruction)
    {
        const auto instructionAddress = cpu.ProgramCounter();
        CompleteInstruction(cpu);

        if (cpu.ProgramCounter() == instructionAddress)
        {
            CHECK_MESSAGE(instructionAddress == kSuccessLoop,
                          "Klaus functional test stopped in an error trap at $",
                          std::hex,
                          instructionAddress,
                          " after ",
                          std::dec,
                          instruction + 1,
                          " instructions");
            return;
        }
    }

    FAIL_CHECK("Klaus functional test exceeded its instruction limit");
}
