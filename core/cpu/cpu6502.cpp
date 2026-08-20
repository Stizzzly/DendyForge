#include "cpu6502.hpp"

namespace dendyforge
{

const CPU6502::Instruction& CPU6502::GetInstructionConfig(std::uint8_t opcode)
{
    static constexpr auto LookupTable = []() consteval {
        std::array<Instruction, 256> table{};

        table.fill({
            .name = "???",
            .operate = &CPU6502::XXX,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        });

        table[0x06] = {
            .name = "ASL",
            .operate = &CPU6502::ASL,
            .addressMode = &CPU6502::ZP0,
            .cycles = 5
        };

        table[0x09] = {
            .name = "ORA",
            .operate = &CPU6502::ORA,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0x0A] = {
            .name = "ASL",
            .operate = &CPU6502::ASL,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x0E] = {
            .name = "ASL",
            .operate = &CPU6502::ASL,
            .addressMode = &CPU6502::ABS,
            .cycles = 6
        };

        table[0x1E] = {
            .name = "ASL",
            .operate = &CPU6502::ASL,
            .addressMode = &CPU6502::ABX,
            .cycles = 7
        };

        table[0x16] = {
            .name = "ASL",
            .operate = &CPU6502::ASL,
            .addressMode = &CPU6502::ZPX,
            .cycles = 6
        };

        table[0x18] = {
            .name = "CLC",
            .operate = &CPU6502::CLC,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x24] = {
            .name = "BIT",
            .operate = &CPU6502::BIT,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0x29] = {
            .name = "AND",
            .operate = &CPU6502::AND,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0x38] = {
            .name = "SEC",
            .operate = &CPU6502::SEC,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x49] = {
            .name = "EOR",
            .operate = &CPU6502::EOR,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0x58] = {
            .name = "CLI",
            .operate = &CPU6502::CLI,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x69] = {
            .name = "ADC",
            .operate = &CPU6502::ADC,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0x65] = {"ADC", &CPU6502::ADC, &CPU6502::ZP0, 3};
        table[0x75] = {"ADC", &CPU6502::ADC, &CPU6502::ZPX, 4};
        table[0x6D] = {"ADC", &CPU6502::ADC, &CPU6502::ABS, 4};
        table[0x7D] = {"ADC", &CPU6502::ADC, &CPU6502::ABX, 4};
        table[0x79] = {"ADC", &CPU6502::ADC, &CPU6502::ABY, 4};
        table[0x61] = {"ADC", &CPU6502::ADC, &CPU6502::IZX, 6};
        table[0x71] = {"ADC", &CPU6502::ADC, &CPU6502::IZY, 5};

        table[0x78] = {
            .name = "SEI",
            .operate = &CPU6502::SEI,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x98] = {
            .name = "TYA",
            .operate = &CPU6502::TYA,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x9A] = {
            .name = "TXS",
            .operate = &CPU6502::TXS,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xD8] = {
            .name = "CLD",
            .operate = &CPU6502::CLD,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xF8] = {
            .name = "SED",
            .operate = &CPU6502::SED,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xB8] = {
            .name = "CLV",
            .operate = &CPU6502::CLV,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xBA] = {
            .name = "TSX",
            .operate = &CPU6502::TSX,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };
        table[0x85] = {
            .name = "STA",
            .operate = &CPU6502::STA,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0x86] = {
             .name = "STX",
             .operate = &CPU6502::STX,
             .addressMode = &CPU6502::ZP0,
             .cycles = 3
        };

        table[0x84] = {
            .name = "STY",
            .operate = &CPU6502::STY,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };
        table[0x8D] = {
            .name = "STA",
            .operate = &CPU6502::STA,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0x8E] = {
            .name = "STX",
            .operate = &CPU6502::STX,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0x8C] = {
            .name = "STY",
            .operate = &CPU6502::STY,
            .addressMode = &CPU6502::ABS,
            .cycles = 4
        };

        table[0x8A] = {
            .name = "TXA",
            .operate = &CPU6502::TXA,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x94] = {
            .name = "STY",
            .operate = &CPU6502::STY,
            .addressMode = &CPU6502::ZPX,
            .cycles = 4
        };

        table[0x95] = {
            .name = "STA",
            .operate = &CPU6502::STA,
            .addressMode = &CPU6502::ZPX,
            .cycles = 4
        };

        table[0x96] = {
            .name = "STX",
            .operate = &CPU6502::STX,
            .addressMode = &CPU6502::ZPY,
            .cycles = 4
        };

        table[0x81] = {"STA", &CPU6502::STA, &CPU6502::IZX, 6};
        table[0x91] = {"STA", &CPU6502::STA, &CPU6502::IZY, 6};
        table[0x99] = {"STA", &CPU6502::STA, &CPU6502::ABY, 5};
        table[0x9D] = {"STA", &CPU6502::STA, &CPU6502::ABX, 5};

        table[0xA0] = {
            .name = "LDY",
            .operate = &CPU6502::LDY,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xA2] = {
            .name = "LDX",
            .operate = &CPU6502::LDX,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xA5] = {
            .name = "LDA",
            .operate = &CPU6502::LDA,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA6] = {
            .name = "LDX",
            .operate = &CPU6502::LDX,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA4] = {
            .name = "LDY",
            .operate = &CPU6502::LDY,
            .addressMode = &CPU6502::ZP0,
            .cycles = 3
        };

        table[0xA8] = {
            .name = "TAY",
            .operate = &CPU6502::TAY,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xA9] = {
            .name = "LDA",
            .operate = &CPU6502::LDA,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xA1] = {"LDA", &CPU6502::LDA, &CPU6502::IZX, 6};
        table[0xB1] = {"LDA", &CPU6502::LDA, &CPU6502::IZY, 5};
        table[0xB5] = {"LDA", &CPU6502::LDA, &CPU6502::ZPX, 4};
        table[0xAD] = {"LDA", &CPU6502::LDA, &CPU6502::ABS, 4};
        table[0xBD] = {"LDA", &CPU6502::LDA, &CPU6502::ABX, 4};
        table[0xB9] = {"LDA", &CPU6502::LDA, &CPU6502::ABY, 4};

        table[0xB6] = {"LDX", &CPU6502::LDX, &CPU6502::ZPY, 4};
        table[0xAE] = {"LDX", &CPU6502::LDX, &CPU6502::ABS, 4};
        table[0xBE] = {"LDX", &CPU6502::LDX, &CPU6502::ABY, 4};

        table[0xB4] = {"LDY", &CPU6502::LDY, &CPU6502::ZPX, 4};
        table[0xAC] = {"LDY", &CPU6502::LDY, &CPU6502::ABS, 4};
        table[0xBC] = {"LDY", &CPU6502::LDY, &CPU6502::ABX, 4};

        table[0xAA] = {
            .name = "TAX",
            .operate = &CPU6502::TAX,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };
        table[0x4C] = {
            .name = "JMP",
            .operate = &CPU6502::JMP,
            .addressMode = &CPU6502::ABS,
            .cycles = 3
        };
        table[0x6C] = {"JMP", &CPU6502::JMP, &CPU6502::IND, 5};

        table[0x10] = {"BPL", &CPU6502::BPL, &CPU6502::REL, 2};
        table[0x30] = {"BMI", &CPU6502::BMI, &CPU6502::REL, 2};
        table[0x50] = {"BVC", &CPU6502::BVC, &CPU6502::REL, 2};
        table[0x70] = {"BVS", &CPU6502::BVS, &CPU6502::REL, 2};
        table[0x90] = {"BCC", &CPU6502::BCC, &CPU6502::REL, 2};
        table[0xB0] = {"BCS", &CPU6502::BCS, &CPU6502::REL, 2};
        table[0xD0] = {"BNE", &CPU6502::BNE, &CPU6502::REL, 2};
        table[0xF0] = {"BEQ", &CPU6502::BEQ, &CPU6502::REL, 2};
        table[0xE8] = {
            .name = "INX",
            .operate = &CPU6502::INX,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xC0] = {
            .name = "CPY",
            .operate = &CPU6502::CPY,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xC8] = {
            .name = "INY",
            .operate = &CPU6502::INY,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xC9] = {
             .name = "CMP",
             .operate = &CPU6502::CMP,
             .addressMode = &CPU6502::IMM,
             .cycles = 2
        };

        table[0xCA] = {
            .name = "DEX",
            .operate = &CPU6502::DEX,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0x88] = {
            .name = "DEY",
            .operate = &CPU6502::DEY,
            .addressMode = &CPU6502::IMP,
            .cycles = 2
        };

        table[0xE0] = {
            .name = "CPX",
            .operate = &CPU6502::CPX,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };

        table[0xE9] = {
            .name = "SBC",
            .operate = &CPU6502::SBC,
            .addressMode = &CPU6502::IMM,
            .cycles = 2
        };
        table[0xE5] = {"SBC", &CPU6502::SBC, &CPU6502::ZP0, 3};
        table[0xF5] = {"SBC", &CPU6502::SBC, &CPU6502::ZPX, 4};
        table[0xED] = {"SBC", &CPU6502::SBC, &CPU6502::ABS, 4};
        table[0xFD] = {"SBC", &CPU6502::SBC, &CPU6502::ABX, 4};
        table[0xF9] = {"SBC", &CPU6502::SBC, &CPU6502::ABY, 4};
        table[0xE1] = {"SBC", &CPU6502::SBC, &CPU6502::IZX, 6};
        table[0xF1] = {"SBC", &CPU6502::SBC, &CPU6502::IZY, 5};

        table[0x00] = {"BRK", &CPU6502::BRK, &CPU6502::IMP, 7};
        table[0x40] = {"RTI", &CPU6502::RTI, &CPU6502::IMP, 6};

        table[0x08] = {"PHP", &CPU6502::PHP, &CPU6502::IMP, 3};
        table[0x28] = {"PLP", &CPU6502::PLP, &CPU6502::IMP, 4};
        table[0x48] = {"PHA", &CPU6502::PHA, &CPU6502::IMP, 3};
        table[0x68] = {"PLA", &CPU6502::PLA, &CPU6502::IMP, 4};
        table[0x20] = {"JSR", &CPU6502::JSR, &CPU6502::ABS, 6};
        table[0x60] = {"RTS", &CPU6502::RTS, &CPU6502::IMP, 6};
        return table;
    }();

    return LookupTable[opcode];
}
CPU6502::CPU6502()
    : CPU6502(Configuration{})
{
}

CPU6502::CPU6502(Configuration configuration)
    : m_decimalModeEnabled(configuration.decimalModeEnabled)
{
}

void CPU6502::ConnectBus(CpuBus* bus)
{
    m_bus = bus;
}

bool CPU6502::IsDecimalModeEnabled() const
{
    return m_decimalModeEnabled;
}

bool CPU6502::GetFlag(Flags flag) const
{
    return (m_status & static_cast<std::uint8_t>(flag)) != 0;
}

void CPU6502::SetFlag(Flags flag, bool value)
{
    if (value)
    {
        m_status |= static_cast<std::uint8_t>(flag);
    }
    else
    {
        m_status &= ~static_cast<std::uint8_t>(flag);
    }
}

void CPU6502::Reset()
{
    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0xFD;
    m_status = static_cast<std::uint8_t>(Flags::U);

    const std::uint16_t lo = Read(0xFFFC);
    const std::uint16_t hi = Read(0xFFFD);

    m_pc = (hi << 8) | lo;
    m_cycles = 8;
}

void CPU6502::Clock()
{
    if (m_cycles == 0)
    {
        Fetch();

        const auto& instruction = GetInstructionConfig(m_opcode);

        m_cycles = instruction.cycles;

        const std::uint8_t additionalAddressCycles =
            (this->*instruction.addressMode)();
        const std::uint8_t additionalOperationCycles =
            (this->*instruction.operate)();

        m_cycles += additionalAddressCycles & additionalOperationCycles;
    }

    --m_cycles;
}

void CPU6502::IRQ()
{
    if (GetFlag(Flags::I))
    {
        return;
    }

    EnterInterrupt(0xFFFE, false);
    m_cycles = 7;
}

void CPU6502::NMI()
{
    EnterInterrupt(0xFFFA, false);
    m_cycles = 7;
}

std::uint8_t CPU6502::Accumulator() const
{
    return m_a;
}

std::uint8_t CPU6502::Read(std::uint16_t address)
{
    return m_bus->CpuRead(address);
}

void CPU6502::Write(std::uint16_t address, std::uint8_t data)
{
    m_bus->CpuWrite(address, data);
}

void CPU6502::Push(std::uint8_t data)
{
    Write(0x0100 + m_sp, data);
    --m_sp;
}

std::uint8_t CPU6502::Pop()
{
    ++m_sp;
    return Read(0x0100 + m_sp);
}

std::uint16_t CPU6502::ProgramCounter() const
{
    return m_pc;
}

std::uint8_t CPU6502::StackPointer() const
{
    return m_sp;
}

std::uint8_t CPU6502::Status() const
{
    return m_status;
}

std::uint8_t CPU6502::Fetch()
{
   m_opcode = Read(m_pc);
   ++m_pc;
   return m_opcode;
}

std::uint8_t CPU6502::Opcode() const
{
    return m_opcode;
}

const char* CPU6502::CurrentInstruction() const
{
    return GetInstructionConfig(m_opcode).name;
}

std::uint8_t CPU6502::ABX()
{
    std::uint16_t lo = Read(m_pc);
    ++m_pc;

    std::uint16_t hi = Read(m_pc);
    ++m_pc;

    m_addrAbs = (hi << 8) | lo;

    std::uint16_t oldPage = m_addrAbs & 0xFF00;

    m_addrAbs += m_x;

    return ((m_addrAbs & 0xFF00) != oldPage);
}

std::uint8_t CPU6502::ABY()
{
    std::uint16_t lo = Read(m_pc);
    ++m_pc;

    std::uint16_t hi = Read(m_pc);
    ++m_pc;

    m_addrAbs = (hi << 8) | lo;

    std::uint16_t oldPage = m_addrAbs & 0xFF00;

    m_addrAbs += m_y;

    return ((m_addrAbs & 0xFF00) != oldPage);
}

std::uint8_t CPU6502::ASL()
{
    FetchData();

    std::uint16_t temp =
        static_cast<std::uint16_t>(m_fetched) << 1;

    SetFlag(Flags::C, (temp & 0xFF00) != 0);

    temp &= 0x00FF;

    SetFlag(Flags::Z, temp == 0x00);
    SetFlag(Flags::N, (temp & 0x80) != 0);

    if (GetInstructionConfig(m_opcode).addressMode == &CPU6502::IMP)
    {
        m_a = static_cast<std::uint8_t>(temp);
    }
    else
    {
        Write(m_addrAbs, static_cast<std::uint8_t>(temp));
    }

    return 0;
}

std::uint8_t CPU6502::ADC()
{
    FetchData();

    const std::uint8_t accumulator = m_a;
    const std::uint8_t carryIn = GetFlag(Flags::C) ? 1 : 0;
    std::uint16_t temp =
        static_cast<std::uint16_t>(accumulator)
        + static_cast<std::uint16_t>(m_fetched)
        + carryIn;

    if (m_decimalModeEnabled && GetFlag(Flags::D))
    {
        const std::uint8_t binaryResult = static_cast<std::uint8_t>(temp);
        std::uint8_t lowNibble =
            (accumulator & 0x0F) + (m_fetched & 0x0F) + carryIn;

        bool decimalCarry = lowNibble > 9;
        if (decimalCarry)
        {
            lowNibble = (lowNibble - 10) & 0x0F;
        }

        std::uint8_t highNibble =
            (accumulator >> 4) + (m_fetched >> 4) + decimalCarry;
        const bool negative = (highNibble & 0x08) != 0;

        decimalCarry = highNibble > 9;
        if (decimalCarry)
        {
            highNibble = (highNibble - 10) & 0x0F;
        }

        m_a = (highNibble << 4) | lowNibble;
        SetFlag(Flags::N, negative);
        SetFlag(
            Flags::V,
            ((accumulator >= 0x80) ^ negative) &&
            ((m_fetched >= 0x80) ^ negative)
        );
        SetFlag(Flags::Z, binaryResult == 0);
        SetFlag(Flags::C, decimalCarry);
        return 1;
    }

    SetFlag(Flags::C, temp > 0xFF);

    SetFlag(
        Flags::V,
        (~(m_a ^ m_fetched) & (m_a ^ temp) & 0x80)
    );

    m_a = temp & 0xFF;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 1;
}

std::uint8_t CPU6502::SBC()
{
    FetchData();

    const std::uint8_t accumulator = m_a;
    const std::uint8_t carryIn = GetFlag(Flags::C) ? 1 : 0;
    std::uint16_t value =
        static_cast<std::uint16_t>(m_fetched) ^ 0x00FF;

    std::uint16_t temp =
        static_cast<std::uint16_t>(accumulator)
        + value
        + carryIn;

    if (m_decimalModeEnabled && GetFlag(Flags::D))
    {
        const std::uint8_t binaryResult = static_cast<std::uint8_t>(temp);
        const bool negative = (binaryResult & 0x80) != 0;
        const std::uint8_t borrow = carryIn == 0 ? 1 : 0;
        std::uint8_t lowNibble =
            (accumulator & 0x0F) - (m_fetched & 0x0F) - borrow;

        bool decimalBorrow = lowNibble >= 0x80;
        if (decimalBorrow)
        {
            lowNibble = (lowNibble + 10) & 0x0F;
        }

        std::uint8_t highNibble =
            (accumulator >> 4) - (m_fetched >> 4) - decimalBorrow;
        decimalBorrow = highNibble >= 0x80;
        if (decimalBorrow)
        {
            highNibble = (highNibble + 10) & 0x0F;
        }

        m_a = (highNibble << 4) | lowNibble;
        SetFlag(Flags::N, negative);
        SetFlag(
            Flags::V,
            ((accumulator >= 0x80) ^ negative) &&
            ((m_fetched < 0x80) ^ negative)
        );
        SetFlag(Flags::Z, binaryResult == 0);
        SetFlag(Flags::C, !decimalBorrow);
        return 1;
    }

    SetFlag(Flags::C, temp & 0xFF00);

    SetFlag(
        Flags::V,
        ((temp ^ m_a) & (temp ^ value) & 0x0080)
    );

    m_a = temp & 0x00FF;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 1;
}

std::uint8_t CPU6502::IMP()
{
    m_fetched = m_a;
    return 0;
}

std::uint8_t CPU6502::IMM()
{
    m_addrAbs = m_pc++;
    return 0;
}

std::uint8_t CPU6502::XXX()
{
    return 0;
}

std::uint8_t CPU6502::SEI()
{
    SetFlag(Flags::I, true);
    return 0;
}

std::uint8_t CPU6502::CLC()
{
    SetFlag(Flags::C, false);
    return 0;
}

std::uint8_t CPU6502::SEC()
{
    SetFlag(Flags::C, true);
    return 0;
}

std::uint8_t CPU6502::CLI()
{
    SetFlag(Flags::I, false);
    return 0;
}

std::uint8_t CPU6502::CLD()
{
    SetFlag(Flags::D, false);
    return 0;
}

std::uint8_t CPU6502::SED()
{
    SetFlag(Flags::D, true);
    return 0;
}

std::uint8_t CPU6502::CLV()
{
    SetFlag(Flags::V, false);
    return 0;
}

std::uint8_t CPU6502::LDA()
{
    FetchData();

    m_a = m_fetched;

    UpdateZN(m_a);
    return 1;
}

void CPU6502::UpdateZN(std::uint8_t value)
{
    SetFlag(Flags::Z, value == 0x00);
    SetFlag(Flags::N, value & 0x80);
}

std::uint8_t CPU6502::CMP()
{
    FetchData();

    std::uint16_t temp =
        static_cast<std::uint16_t>(m_a)
        - static_cast<std::uint16_t>(m_fetched);

    SetFlag(Flags::C, m_a >= m_fetched);
    SetFlag(Flags::Z, (temp & 0x00FF) == 0x00);
    SetFlag(Flags::N, temp & 0x80);

    return 1;
}

std::uint8_t CPU6502::CPX()
{
    FetchData();

    std::uint16_t temp =
        static_cast<std::uint16_t>(m_x)
        - static_cast<std::uint16_t>(m_fetched);

    SetFlag(Flags::C, m_x >= m_fetched);
    SetFlag(Flags::Z, (temp & 0x00FF) == 0x00);
    SetFlag(Flags::N, temp & 0x80);

    return 1;
}

std::uint8_t CPU6502::CPY()
{
    FetchData();

    std::uint16_t temp =
        static_cast<std::uint16_t>(m_y)
        - static_cast<std::uint16_t>(m_fetched);

    SetFlag(Flags::C, m_y >= m_fetched);
    SetFlag(Flags::Z, (temp & 0x00FF) == 0x00);
    SetFlag(Flags::N, temp & 0x80);

    return 1;
}

std::uint8_t CPU6502::AND()
{
    FetchData();

    m_a &= m_fetched;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 1;
}

std::uint8_t CPU6502::ORA()
{
    FetchData();

    m_a |= m_fetched;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 1;
}

std::uint8_t CPU6502::EOR()
{
    FetchData();

    m_a ^= m_fetched;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 1;
}

std::uint8_t CPU6502::BIT()
{
    FetchData();

    SetFlag(Flags::Z, (m_a & m_fetched) == 0x00);
    SetFlag(Flags::V, m_fetched & 0x40);
    SetFlag(Flags::N, m_fetched & 0x80);

    return 1;
}

std::uint8_t CPU6502::TAX()
{
    m_x = m_a;

    SetFlag(Flags::Z, m_x == 0x00);
    SetFlag(Flags::N, m_x & 0x80);

    return 0;
}

std::uint8_t CPU6502::TAY()
{
    m_y = m_a;

    SetFlag(Flags::Z, m_y == 0x00);
    SetFlag(Flags::N, m_y & 0x80);

    return 0;
}

std::uint8_t CPU6502::TXA()
{
    m_a = m_x;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 0;
}

std::uint8_t CPU6502::TYA()
{
    m_a = m_y;

    SetFlag(Flags::Z, m_a == 0x00);
    SetFlag(Flags::N, m_a & 0x80);

    return 0;
}

std::uint8_t CPU6502::TSX()
{
    m_x = m_sp;

    SetFlag(Flags::Z, m_x == 0x00);
    SetFlag(Flags::N, m_x & 0x80);

    return 0;
}

std::uint8_t CPU6502::TXS()
{
    m_sp = m_x;
    return 0;
}

std::uint8_t CPU6502::PHA()
{
    Push(m_a);
    return 0;
}

std::uint8_t CPU6502::PHP()
{
    Push(m_status | static_cast<std::uint8_t>(Flags::B) |
         static_cast<std::uint8_t>(Flags::U));
    return 0;
}

std::uint8_t CPU6502::PLA()
{
    m_a = Pop();
    UpdateZN(m_a);
    return 0;
}

std::uint8_t CPU6502::PLP()
{
    m_status = Pop();
    SetFlag(Flags::B, false);
    SetFlag(Flags::U, true);
    return 0;
}

std::uint8_t CPU6502::LDX()
{
    FetchData();

    m_x = m_fetched;

    UpdateZN(m_x);
    return 1;
}

std::uint8_t CPU6502::LDY()
{
    FetchData();

    m_y = m_fetched;

    UpdateZN(m_y);
    return 1;
}

std::uint8_t CPU6502::STA()
{
    Write(m_addrAbs, m_a);
    return 0;
}

std::uint8_t CPU6502::STX()
{
    Write(m_addrAbs, m_x);
    return 0;
}

std::uint8_t CPU6502::STY()
{
    Write(m_addrAbs, m_y);
    return 0;
}

std::uint8_t CPU6502::ZPX()
{
    // Читаем адрес из следующего байта
    m_addrAbs = Read(m_pc);
    ++m_pc;

    // Добавляем X и оставляем только младший байт
    m_addrAbs = (m_addrAbs + m_x) & 0x00FF;

    return 0;
}

std::uint8_t CPU6502::ZPY()
{
    // Читаем адрес из следующего байта
    m_addrAbs = Read(m_pc);
    ++m_pc;

    // Добавляем Y и оставляем только младший байт
    m_addrAbs = (m_addrAbs + m_y) & 0x00FF;

    return 0;
}

std::uint8_t CPU6502::REL()
{
    m_addrRel = Read(m_pc);
    ++m_pc;

    if (m_addrRel & 0x0080)
    {
        m_addrRel |= 0xFF00;
    }

    return 0;
}

std::uint8_t CPU6502::IND()
{
    const std::uint16_t pointerLo = Read(m_pc);
    ++m_pc;
    const std::uint16_t pointerHi = Read(m_pc);
    ++m_pc;

    const std::uint16_t pointer = (pointerHi << 8) | pointerLo;
    const std::uint16_t lo = Read(pointer);

    const std::uint16_t hiAddress =
        pointerLo == 0x00FF ? pointer & 0xFF00 : pointer + 1;

    const std::uint16_t hi = Read(hiAddress);
    m_addrAbs = (hi << 8) | lo;

    return 0;
}

std::uint8_t CPU6502::IZX()
{
    const std::uint16_t pointer = Read(m_pc);
    ++m_pc;

    const std::uint16_t lo = Read((pointer + m_x) & 0x00FF);
    const std::uint16_t hi = Read((pointer + m_x + 1) & 0x00FF);
    m_addrAbs = (hi << 8) | lo;

    return 0;
}

std::uint8_t CPU6502::IZY()
{
    const std::uint16_t pointer = Read(m_pc);
    ++m_pc;

    const std::uint16_t lo = Read(pointer & 0x00FF);
    const std::uint16_t hi = Read((pointer + 1) & 0x00FF);
    const std::uint16_t baseAddress = (hi << 8) | lo;

    m_addrAbs = baseAddress + m_y;

    return (m_addrAbs & 0xFF00) != (baseAddress & 0xFF00);
}

std::uint8_t CPU6502::INX()
{
    ++m_x;

    UpdateZN(m_x);
    return 0;
}

std::uint8_t CPU6502::INY()
{
    ++m_y;

    UpdateZN(m_y);
    return 0;
}

std::uint8_t CPU6502::DEX()
{
    --m_x;

    SetFlag(Flags::Z, m_x == 0x00);
    SetFlag(Flags::N, m_x & 0x80);

    return 0;
}

std::uint8_t CPU6502::DEY()
{
    --m_y;

    SetFlag(Flags::Z, m_y == 0x00);
    SetFlag(Flags::N, m_y & 0x80);

    return 0;
}

std::uint8_t CPU6502::ABS()
{
    // Читаем младший байт адреса
    const std::uint16_t lo = Read(m_pc);
    ++m_pc;

    // Читаем старший байт адреса
    const std::uint16_t hi = Read(m_pc);
    ++m_pc;

    // Собираем 16-битный адрес
    m_addrAbs = (hi << 8) | lo;

    return 0;
}

std::uint8_t CPU6502::ZP0()
{
    m_addrAbs = Read(m_pc);
    m_pc++;

    m_addrAbs &= 0x00FF;

    return 0;
}

std::uint8_t CPU6502::JMP()
{
    m_pc = m_addrAbs;
    return 0;
}

std::uint8_t CPU6502::JSR()
{
    const std::uint16_t returnAddress = m_pc - 1;
    Push((returnAddress >> 8) & 0x00FF);
    Push(returnAddress & 0x00FF);
    m_pc = m_addrAbs;
    return 0;
}

std::uint8_t CPU6502::RTS()
{
    const std::uint16_t lo = Pop();
    const std::uint16_t hi = Pop();
    m_pc = (hi << 8) | lo;
    ++m_pc;
    return 0;
}

std::uint8_t CPU6502::BRK()
{
    ++m_pc;
    EnterInterrupt(0xFFFE, true);
    return 0;
}

std::uint8_t CPU6502::RTI()
{
    m_status = Pop();
    SetFlag(Flags::B, false);
    SetFlag(Flags::U, true);

    const std::uint16_t lo = Pop();
    const std::uint16_t hi = Pop();
    m_pc = (hi << 8) | lo;
    return 0;
}

void CPU6502::BranchIf(bool condition)
{
    if (!condition)
    {
        return;
    }

    ++m_cycles;

    const std::uint16_t targetAddress = m_pc + m_addrRel;
    if ((targetAddress & 0xFF00) != (m_pc & 0xFF00))
    {
        ++m_cycles;
    }

    m_pc = targetAddress;
}

void CPU6502::EnterInterrupt(std::uint16_t vector, bool breakInstruction)
{
    Push((m_pc >> 8) & 0x00FF);
    Push(m_pc & 0x00FF);

    std::uint8_t status = m_status | static_cast<std::uint8_t>(Flags::U);
    if (breakInstruction)
    {
        status |= static_cast<std::uint8_t>(Flags::B);
    }
    else
    {
        status &= ~static_cast<std::uint8_t>(Flags::B);
    }

    Push(status);
    SetFlag(Flags::B, false);
    SetFlag(Flags::U, true);
    SetFlag(Flags::I, true);

    const std::uint16_t lo = Read(vector);
    const std::uint16_t hi = Read(vector + 1);
    m_pc = (hi << 8) | lo;
}

std::uint8_t CPU6502::BCC()
{
    BranchIf(!GetFlag(Flags::C));
    return 0;
}

std::uint8_t CPU6502::BCS()
{
    BranchIf(GetFlag(Flags::C));
    return 0;
}

std::uint8_t CPU6502::BEQ()
{
    BranchIf(GetFlag(Flags::Z));
    return 0;
}

std::uint8_t CPU6502::BMI()
{
    BranchIf(GetFlag(Flags::N));
    return 0;
}

std::uint8_t CPU6502::BNE()
{
    BranchIf(!GetFlag(Flags::Z));
    return 0;
}

std::uint8_t CPU6502::BPL()
{
    BranchIf(!GetFlag(Flags::N));
    return 0;
}

std::uint8_t CPU6502::BVC()
{
    BranchIf(!GetFlag(Flags::V));
    return 0;
}

std::uint8_t CPU6502::BVS()
{
    BranchIf(GetFlag(Flags::V));
    return 0;
}

std::uint8_t CPU6502::Cycles() const
{
    return m_cycles;
}

std::uint8_t CPU6502::FetchData()
{
    m_fetched = Read(m_addrAbs);
    return m_fetched;
}

std::uint8_t CPU6502::X() const
{
    return m_x;
}

std::uint8_t CPU6502::Y() const
{
    return m_y;
}
} // namespace dendyforge
