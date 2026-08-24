#pragma once

#include <cstdint>
#include <array>

#include "cpu_bus.hpp"

namespace dendyforge
{

class CPU6502
{
public:
    struct Configuration
    {
        // CPU2A03 exposes the D flag, but its ALU always uses binary
        // arithmetic. A standalone CPU6502 keeps decimal arithmetic.
        bool decimalModeEnabled{true};

    };

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

    CPU6502();
    explicit CPU6502(Configuration configuration);

    void ConnectBus(CpuBus* bus);

    bool IsDecimalModeEnabled() const;

    void Reset();
    void Clock();

    // IRQ is level-triggered: the caller reports the line state every
    // cycle, and a low line clears a previously latched IRQ so a stale
    // latch (for example one taken during an entry sequence before I is
    // set) cannot fire after the line has dropped.
    void IRQ(bool line = true);
    void NMI();

    // The /NMI line input, sampled once per CPU cycle by the console. A
    // rising edge latches the internal NMI; the latch survives the line
    // dropping again and is cleared only by servicing.
    void NmiLine(bool asserted);

    bool GetFlag(Flags flag) const;
    void SetFlag(Flags flag, bool value);


    std::uint8_t Accumulator() const;
    std::uint16_t ProgramCounter() const;
    void SetProgramCounter(std::uint16_t value);
    std::uint8_t StackPointer() const;
    std::uint8_t Status() const;
    std::uint8_t Opcode() const;
    std::uint8_t Cycles() const;
    const char* CurrentInstruction() const;
    std::uint8_t X() const;
    std::uint8_t Y() const;
    bool IsJammed() const;
private:

    struct Instruction
    {
        const char* name;

        std::uint8_t (CPU6502::*operate)();
        std::uint8_t (CPU6502::*addressMode)();

        std::uint8_t cycles;
    };

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
    std::uint8_t NOP();

    // Загрузка и Сохранение (Load/Store)
    std::uint8_t LDA(); // Load Accumulator
    std::uint8_t LDX(); // Load X
    std::uint8_t LDY(); // Load Y
    std::uint8_t STA(); // Store Accumulator
    std::uint8_t STX(); // Store X
    std::uint8_t STY(); // Store Y

    void UpdateZN(std::uint8_t value);

    // Арифметика и логика (ALU)
    std::uint8_t AND(); // Logical AND
    std::uint8_t ORA(); // Logical Inclusive OR
    std::uint8_t EOR(); // Logical Exclusive OR / XOR
    std::uint8_t BIT(); // Bit Test
    std::uint8_t CMP();
    std::uint8_t CPX();
    std::uint8_t CPY();
    std::uint8_t ADC();
    std::uint8_t SBC();
    std::uint8_t LAX();
    std::uint8_t SAX();
    std::uint8_t SLO();
    std::uint8_t RLA();
    std::uint8_t SRE();
    std::uint8_t RRA();
    std::uint8_t DCP();
    std::uint8_t ISB();
    std::uint8_t ANC();
    std::uint8_t ALR();
    std::uint8_t ARR();
    std::uint8_t AXS();
    std::uint8_t XAA();
    std::uint8_t LAS();
    std::uint8_t AHX();
    std::uint8_t TAS();
    std::uint8_t SHY();
    std::uint8_t SHX();
    std::uint8_t KIL();

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

    // Стек (Stack)
    std::uint8_t PHA(); // Push Accumulator
    std::uint8_t PHP(); // Push Processor Status
    std::uint8_t PLA(); // Pull Accumulator
    std::uint8_t PLP(); // Pull Processor Status

    std::uint8_t ASL();
    std::uint8_t LSR();
    std::uint8_t ROL();
    std::uint8_t ROR();
    std::uint8_t INC();
    std::uint8_t DEC();

    // Переходы и ветвления (Jumps & Branches)
    std::uint8_t JMP(); // Jump
    std::uint8_t JSR(); // Jump to Subroutine
    std::uint8_t RTS(); // Return from Subroutine
    std::uint8_t BRK(); // Break
    std::uint8_t RTI(); // Return from Interrupt
    std::uint8_t BCC();
    std::uint8_t BCS();
    std::uint8_t BEQ();
    std::uint8_t BMI();
    std::uint8_t BNE();
    std::uint8_t BPL();
    std::uint8_t BVC();
    std::uint8_t BVS();

    void BranchIf(bool condition);

    static const Instruction& GetInstructionConfig(std::uint8_t opcode);

    // Instruction fetch
    std::uint8_t Fetch();
    std::uint8_t FetchData();

    // Per-cycle execution (cycle-accurate CPU work, Phases 2-5 of
    // CPU_CYCLE_ACCURACY_PLAN.md). Instruction classes perform one bus
    // transaction per cycle, including the hardware dummy reads; the
    // legacy atomic execution remains only as the "no sequencer" sentinel.
    enum class ExecutionKind
    {
        Legacy,          // executed atomically on the opcode-fetch cycle
        Implied,         // dummy read at PC, then operate
        Read,            // read-type instruction with sequenced addressing
        Write,           // store-type instruction with sequenced addressing
        ReadModifyWrite, // sequenced addressing, then read, write old, write new
        JumpAbsolute,    // JMP $nnnn
        JumpIndirect,    // JMP ($nnnn) with page wrap
        Push,            // PHA/PHP: dummy read at PC, then write the stack
        Pull,            // PLA/PLP: dummy reads at PC and the stack, then pop
        Jsr,             // JSR with the stack and operand fetches on their cycles
        Rts,             // RTS with the stack pulls on their cycles
        Rti,             // RTI with the status and address pulls on their cycles
        Brk,             // BRK software interrupt entry across seven cycles
        Branch,          // conditional branch: dummy fetch when taken, internal fix-up on page cross
        Jam              // KIL: CPU remains halted until Reset()
    };

    ExecutionKind ClassifyExecution(const Instruction& instruction) const;
    void BeginInstruction();
    void StepInstruction();
    void StepMemoryInstruction(const Instruction& instruction, int cycle);
    void StepDataPhase(const Instruction& instruction, int cycle, int dataCycle);
    void RunOperate(const Instruction& instruction);
    void StoreHighIndexed(std::uint8_t value);

    // Cached addressing-mode classification so the per-cycle sequencer
    // dispatches on a dense enum instead of comparing member-function
    // pointers every cycle (Phase 6 performance work).
    enum class AddressModeKind
    {
        IMM,
        ZP0,
        ZPX,
        ZPY,
        ABS,
        ABX,
        ABY,
        IZX,
        IZY
    };

    static AddressModeKind ClassifyAddressMode(
        const Instruction& instruction);

    // Seven-cycle hardware sequences (Phase 5): interrupt entry and reset
    // run their bus transactions on their hardware cycles instead of
    // atomically at the boundary.
    enum class SpecialSequence
    {
        None,
        InterruptEntry, // dummy fetches, stack pushes, vector fetch
        Reset           // dummy reads only, SP -= 3, vector fetch
    };

    void BeginInterruptEntry();
    void StepSpecialSequence();

    // Stack operations
    void Push(std::uint8_t data);
    std::uint8_t Pop();

    CpuBus* m_bus{nullptr};
    bool m_decimalModeEnabled{true};

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

    // Per-cycle execution state
    ExecutionKind m_executionKind{ExecutionKind::Legacy};
    const Instruction* m_currentInstruction{nullptr};
    int m_stepCycle{0};
    bool m_operandReady{false};
    std::uint16_t m_addrBase{0};
    AddressModeKind m_addressModeKind{AddressModeKind::ZP0};

    // The branch operate stage records its condition here on the operand
    // cycle; the sequencer performs the taken branch's dummy fetch and PC
    // update on the next cycle.
    bool m_branchTaken{false};
    bool m_jammed{false};

    // Hardware polls interrupt lines at an instruction boundary, so a
    // signalled interrupt must not cut into the cycle burn of the current
    // instruction (blargg 08.irq_timing).
    enum class PendingInterrupt
    {
        None,
        Irq,
        Nmi
    };
    bool m_nmiLinePrevious{false};
    PendingInterrupt m_pendingInterrupt{PendingInterrupt::None};

    // The lines are sampled during the penultimate cycle of an
    // instruction (phi2); the sampled state decides at the following
    // boundary whether the seven-cycle entry begins.
    PendingInterrupt m_recognizedInterrupt{PendingInterrupt::None};

    SpecialSequence m_specialSequence{SpecialSequence::None};
    std::uint16_t m_interruptVector{0};
};

} // namespace dendyforge
