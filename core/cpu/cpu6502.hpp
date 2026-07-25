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


    std::uint8_t Accumulator() const;
    std::uint16_t ProgramCounter() const;
    std::uint8_t StackPointer() const;
    std::uint8_t Status() const;
    std::uint8_t Opcode() const;
    std::uint8_t Cycles() const;
    const char* CurrentInstruction() const;
    std::uint8_t X() const;
    std::uint8_t Y() const;
    void UpdateZN(std::uint8_t value);

private:

    // Bus interface
    std::uint8_t Read(std::uint16_t address);
    void Write(std::uint16_t address, std::uint8_t data);

    // Режимы адрессации
    std::uint8_t IMP(); // Implied
    std::uint8_t IMM(); // Immediate
    std::uint8_t ZP0(); // Zero Page
    std::uint8_t ZPX(); // Zero Page, X
    std::uint8_t ZPY(); // Zero Page, Y
    std::uint8_t REL(); // Relative
    std::uint8_t ABS(); // Absolute
    std::uint8_t ABX(); // Absolute, X
    std::uint8_t ABY(); // Absolute, Y
    std::uint8_t IND(); // Indirect
    std::uint8_t IZX(); // Indexed Indirect (Indirect, X)
    std::uint8_t IZY(); // Indirect Indexed (Indirect), Y

    // Операции (Инструкции)
    std::uint8_t XXX(); // Illegal/Template

    // Загрузка и Сохранение (Load/Store)
    std::uint8_t LDA(); // Load Accumulator
    std::uint8_t LDX(); // Load X
    std::uint8_t LDY(); // Load Y
    std::uint8_t STA(); // Store Accumulator
    std::uint8_t STX(); // Store X
    std::uint8_t STY(); // Store Y

    // Арифметика и логика (ALU)
    std::uint8_t AND(); // Logical AND
    std::uint8_t ORA(); // Logical Inclusive OR
    std::uint8_t EOR(); // Logical Exclusive OR / XOR
    std::uint8_t BIT(); // Bit Test

    // Инкремент / Декремент
    std::uint8_t INX(); // Increment X
    std::uint8_t INY(); // Increment Y
    std::uint8_t DEX(); // Decrement X
    std::uint8_t DEY(); // Decrement Y

    // Изменение флагов (Status Flags)
    std::uint8_t CLC(); // Clear Carry Flag
    std::uint8_t SEC(); // Set Carry Flag
    std::uint8_t CLI(); // Clear Interrupt Disable
    std::uint8_t SEI(); // Set Interrupt Disable
    std::uint8_t CLD(); // Clear Decimal Mode
    std::uint8_t SED(); // Set Decimal Mode
    std::uint8_t CLV(); // Clear Overflow Flag

    // Переносы между регистрами (Register Transfers)
    std::uint8_t TAX(); // Transfer A to X
    std::uint8_t TAY(); // Transfer A to Y
    std::uint8_t TXA(); // Transfer X to A
    std::uint8_t TYA(); // Transfer Y to A
    std::uint8_t TSX(); // Transfer Stack Pointer to X
    std::uint8_t TXS(); // Transfer X to Stack Pointer

    // Переходы и ветвления (Jumps & Branches)
    std::uint8_t JMP(); // Jump

    static const Instruction& GetInstructionConfig(std::uint8_t opcode);

    // Instruction fetch
    std::uint8_t Fetch();
    std::uint8_t FetchData();

    // Stack operations
    void Push(std::uint8_t data);
    std::uint8_t Pop();

    Bus* m_bus{nullptr};

    // Registers
    std::uint8_t m_a{0};
    std::uint8_t m_x{0};
    std::uint8_t m_y{0};
    std::uint8_t m_sp{0};
    std::uint16_t m_pc{0};
    std::uint8_t m_status{0};

    std::uint8_t m_opcode{0};
    std::uint16_t m_addrAbs{0};
    std::uint16_t m_addrRel{0};
    std::uint8_t m_fetched{0};

    std::uint8_t m_cycles{0};
};

} // namespace dendyforge
