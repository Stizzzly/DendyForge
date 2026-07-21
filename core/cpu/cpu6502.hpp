#pragma once

#include <cstdint>
#include <array>

namespace dendyforge
{

class Bus;

class CPU6502
{
public:
    enum class Flags : std::uint8_t
    {
        C = 1 << 0, // Carry
        Z = 1 << 1, // Zero
        I = 1 << 2, // Interrupt Disable
        D = 1 << 3, // Decimal Mode
        B = 1 << 4, // Break
        U = 1 << 5, // Unused (always set)
        V = 1 << 6, // Overflow
        N = 1 << 7  // Negative
    };

    struct Instruction
    {
        const char* name;

        std::uint8_t (CPU6502::*operate)();
        std::uint8_t (CPU6502::*addressMode)();

        std::uint8_t cycles;
    };

    CPU6502();

    void ConnectBus(Bus* bus);

    void Reset();
    void Clock();

    bool GetFlag(Flags flag) const;
    void SetFlag(Flags flag, bool value);
    std::uint16_t ProgramCounter() const;
    std::uint8_t Opcode() const;
    const char* CurrentInstruction() const;

private:
    //Bus interface
    private: std::uint8_t Read(std::uint16_t address);
    void Write(std::uint16_t address, std::uint8_t data);

    // Instruction fetch
    std::uint8_t Fetch();

    // Stack operations
    void Push(std::uint8_t data);
    std::uint8_t Pop();

    Bus* m_bus{nullptr};

    // Registers
    std::uint8_t m_a{0}; //Accumulator
    std::uint8_t m_x{0}; // Index X
    std::uint8_t m_y{0}; //Index Y

    std::uint8_t m_sp{0}; // Stack Pointer
    std::uint16_t m_pc{0}; // Program Counter

    std::uint8_t m_status{0}; //Processor status

    // Current opcode
    std::uint8_t m_opcode{0};

    // Remaining CPU cycles
    std::uint8_t m_cycles{0};

    std::array<Instruction, 256> m_lookup;
};

} // namespace dendyforge
