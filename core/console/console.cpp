#include "console.hpp"

#include <utility>

#include "cartridge/cartridge.hpp"
#include "ines/ines_reader.hpp"

namespace dendyforge
{

Console::Console()
    : m_cpu(CPU6502::Configuration{.decimalModeEnabled = false})
{
    m_cpu.ConnectBus(&m_bus);
}

Console::~Console() = default;

bool Console::LoadRom(const std::string& path)
{
    INesReader reader;
    if (!reader.Load(path))
    {
        return false;
    }

    m_cartridge = std::make_unique<Cartridge>(
        reader.Header(), reader.TakePRGRom(), reader.TakeCHRRom());
    m_bus.InsertCartridge(m_cartridge.get());
    Reset();
    return true;
}

void Console::Reset()
{
    m_cpu.Reset();
    m_bus.AudioProcessor().Reset();
    m_cpuCycleIsOdd = false;
}

void Console::Clock()
{
    if (!m_bus.ConsumeDmaStallCycle())
    {
        m_cpu.Clock();
        m_bus.BeginPendingDma(m_cpuCycleIsOdd);
    }

    m_bus.AudioProcessor().Clock();

    // The IRQ line is level-triggered: report its state every cycle so a
    // dropped line clears any stale latch taken while I was clear (blargg
    // 08.irq_timing). Cartridge IRQs (MMC3 scanline counter) share the
    // line.
    m_cpu.IRQ(m_bus.AudioProcessor().IrqPending() ||
              m_bus.CartridgeIrqPending());

    // One CPU cycle spans three PPU dots and the /NMI line is sampled
    // once per CPU cycle at phi2, between the second and third dot. A
    // pulse shorter than the distance to the next sample point - for
    // example VBlank raised at the last dot and cleared by the CPU's
    // next $2002 read - is missed entirely, which is how the hardware
    // NMI suppression windows arise (blargg vbl_nmi_timing 5-7).
    m_bus.ClockPpu();
    m_cpu.NmiLine(m_bus.PpuNmiLine());
    m_bus.ClockPpu();
    m_bus.ClockPpu();

    m_cpuCycleIsOdd = !m_cpuCycleIsOdd;
}

std::uint8_t Console::ReadCpuRamForDiagnostics(std::uint16_t address)
{
    if (address > 0x1FFF)
    {
        return 0;
    }

    return m_bus.CpuRead(address);
}

std::uint8_t Console::ReadCartridgeRamForDiagnostics(std::uint16_t address)
{
    if (address < 0x6000 || address > 0x7FFF)
    {
        return 0;
    }

    return m_bus.CpuRead(address);
}

CPU6502& Console::Cpu()
{
    return m_cpu;
}

PPU& Console::VideoProcessor()
{
    return m_bus.VideoProcessor();
}

APU& Console::AudioProcessor()
{
    return m_bus.AudioProcessor();
}

Controller& Console::PrimaryController()
{
    return m_bus.PrimaryController();
}

Zapper& Console::SecondaryZapper()
{
    return m_bus.SecondaryZapper();
}

} // namespace dendyforge
