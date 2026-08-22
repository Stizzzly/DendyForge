#include "apu.hpp"

#include <algorithm>
#include <utility>

namespace dendyforge
{

namespace
{

constexpr std::uint32_t CpuClockHz = 1'789'773;
constexpr std::array<std::uint8_t, 32> LengthTable{
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};
constexpr std::array<std::array<bool, 8>, 4> DutySequences{{
    {{false, true, false, false, false, false, false, false}},
    {{false, true, true, false, false, false, false, false}},
    {{false, true, true, true, true, false, false, false}},
    {{true, false, false, true, true, true, true, true}},
}};
constexpr std::array<std::uint8_t, 32> TriangleSequence{
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};
constexpr std::array<std::uint16_t, 16> NoisePeriodTable{
    4, 8, 16, 32, 64, 96, 128, 160,
    202, 254, 380, 508, 762, 1016, 2034, 4068,
};
constexpr std::array<std::uint16_t, 16> DmcRateTable{
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106, 85, 72, 54,
};

}

void APU::Reset()
{
    auto memoryReader = std::move(m_dmcMemoryReader);
    *this = APU{};
    m_dmcMemoryReader = std::move(memoryReader);
}

void APU::Clock()
{
    m_pulse1.ClockTimer();
    m_pulse2.ClockTimer();
    m_triangle.ClockTimer();
    m_noise.ClockTimer();
    m_dmc.ClockTimer(m_dmcMemoryReader);
    ClockFrameCounter();
    if (m_frameIrqLineCountdown != 0)
    {
        --m_frameIrqLineCountdown;
    }

    m_samplePhase += SampleRate;
    if (m_samplePhase >= CpuClockHz)
    {
        m_samplePhase -= CpuClockHz;
        QueueSample();
    }

    m_cpuCycleOdd = !m_cpuCycleOdd;
}

std::uint8_t APU::CpuRead(std::uint16_t address)
{
    if (address != 0x4015)
    {
        return 0;
    }

    const std::uint8_t status =
        (m_pulse1.m_lengthCounter != 0 ? 0x01 : 0x00) |
        (m_pulse2.m_lengthCounter != 0 ? 0x02 : 0x00) |
        (m_triangle.m_lengthCounter != 0 ? 0x04 : 0x00) |
        (m_noise.m_lengthCounter != 0 ? 0x08 : 0x00) |
        (m_dmc.Active() ? 0x10 : 0x00) |
        (m_frameIrqFlag ? 0x40 : 0x00) |
        (m_dmc.IrqPending() ? 0x80 : 0x00);
    m_frameIrqFlag = false;
    return status;
}

void APU::CpuWrite(std::uint16_t address, std::uint8_t data)
{
    // Writes touching length-counter state on the same CPU cycle as a half
    // frame clock are ordered after that clock: the clock samples the halt
    // flags from before the write (blargg 10.len_halt_timing), and a reload
    // is ignored when the counter is then non-zero (blargg
    // 11.len_reload_timing). The tick is applied before the write and this
    // cycle's regular tick is suppressed.
    const bool lengthClockThisCycle =
        (address == 0x4000 || address == 0x4003 || address == 0x4004 ||
         address == 0x4007 || address == 0x4008 || address == 0x400B ||
         address == 0x400C || address == 0x400F || address == 0x4015) &&
        LengthCounterClockPending();
    if (lengthClockThisCycle)
    {
        ClockHalfFrame();
        m_suppressHalfFrame = true;
    }
    const bool blockLengthReload =
        lengthClockThisCycle &&
        (address == 0x4003 || address == 0x4007 || address == 0x400B ||
         address == 0x400F);

    switch (address)
    {
    case 0x4000: m_pulse1.WriteControl(data); break;
    case 0x4001: m_pulse1.WriteSweep(data); break;
    case 0x4002: m_pulse1.WriteTimerLow(data); break;
    case 0x4003:
        m_pulse1.WriteTimerHigh(data, blockLengthReload &&
                                          m_pulse1.m_lengthCounter != 0);
        break;
    case 0x4004: m_pulse2.WriteControl(data); break;
    case 0x4005: m_pulse2.WriteSweep(data); break;
    case 0x4006: m_pulse2.WriteTimerLow(data); break;
    case 0x4007:
        m_pulse2.WriteTimerHigh(data, blockLengthReload &&
                                          m_pulse2.m_lengthCounter != 0);
        break;
    case 0x4008: m_triangle.WriteControl(data); break;
    case 0x400A: m_triangle.WriteTimerLow(data); break;
    case 0x400B:
        m_triangle.WriteTimerHigh(data, blockLengthReload &&
                                            m_triangle.m_lengthCounter != 0);
        break;
    case 0x400C: m_noise.WriteControl(data); break;
    case 0x400E: m_noise.WritePeriod(data); break;
    case 0x400F:
        m_noise.WriteLength(data, blockLengthReload &&
                                      m_noise.m_lengthCounter != 0);
        break;
    case 0x4010: m_dmc.WriteControl(data); break;
    case 0x4011: m_dmc.WriteDirectLoad(data); break;
    case 0x4012: m_dmc.WriteSampleAddress(data); break;
    case 0x4013: m_dmc.WriteSampleLength(data); break;
    case 0x4015:
        m_pulse1.SetEnabled((data & 0x01) != 0);
        m_pulse2.SetEnabled((data & 0x02) != 0);
        m_triangle.SetEnabled((data & 0x04) != 0);
        m_noise.SetEnabled((data & 0x08) != 0);
        m_dmc.SetEnabled((data & 0x10) != 0);
        break;
    case 0x4017:
        m_pendingFiveStepFrameCounter = (data & 0x80) != 0;
        m_pendingFrameIrqInhibit = (data & 0x40) != 0;
        if (m_pendingFrameIrqInhibit)
        {
            m_frameIrqFlag = false;
        }
        m_frameResetDelay = m_cpuCycleOdd ? 3 : 4;
        // A $4017 write on an odd CPU cycle applies the reset one cycle
        // earlier but leaves the divider on the opposite half-cycle, so all
        // sequencer events for this frame land two CPU cycles later
        // (blargg 04.clock_jitter, 07.irq_flag_timing).
        m_frameJitterOffset = m_cpuCycleOdd ? 2 : 0;
        break;
    default:
        break;
    }
}

std::vector<float> APU::TakeSamples()
{
    return std::exchange(m_samples, {});
}

void APU::SetDmcMemoryReader(DmcMemoryReader reader)
{
    m_dmcMemoryReader = std::move(reader);
}

bool APU::ConsumeDmcDmaStallCycle()
{
    return m_dmc.ConsumeDmaStallCycle();
}

bool APU::IrqPending() const
{
    return (m_frameIrqFlag && m_frameIrqLineCountdown == 0) ||
           m_dmc.IrqPending();
}

void APU::PulseChannel::WriteControl(std::uint8_t data)
{
    m_control = data;
}

void APU::PulseChannel::WriteSweep(std::uint8_t data)
{
    m_sweep = data;
    m_sweepReload = true;
}

void APU::PulseChannel::WriteTimerLow(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x0700) | data;
}

void APU::PulseChannel::WriteTimerHigh(std::uint8_t data, bool lengthReloadBlocked)
{
    m_timerPeriod = (m_timerPeriod & 0x00FF) |
                    ((static_cast<std::uint16_t>(data) & 0x07) << 8);
    m_timerCounter = m_timerPeriod * 2 + 1;
    m_sequenceStep = 0;
    m_envelopeStart = true;
    if (m_enabled && !lengthReloadBlocked)
    {
        m_lengthCounter = LengthTable[(data >> 3) & 0x1F];
    }
}

void APU::PulseChannel::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
    {
        m_lengthCounter = 0;
    }
}

void APU::PulseChannel::ClockTimer()
{
    if (m_timerCounter == 0)
    {
        m_timerCounter = m_timerPeriod * 2 + 1;
        m_sequenceStep = (m_sequenceStep + 1) & 0x07;
        return;
    }

    --m_timerCounter;
}

void APU::PulseChannel::ClockLength()
{
    if ((m_control & 0x20) == 0 && m_lengthCounter != 0)
    {
        --m_lengthCounter;
    }
}

void APU::PulseChannel::ClockEnvelope()
{
    if (m_envelopeStart)
    {
        m_envelopeStart = false;
        m_envelopeDecayLevel = 15;
        m_envelopeDivider = m_control & 0x0F;
        return;
    }

    if (m_envelopeDivider != 0)
    {
        --m_envelopeDivider;
        return;
    }

    m_envelopeDivider = m_control & 0x0F;
    if (m_envelopeDecayLevel != 0)
    {
        --m_envelopeDecayLevel;
    }
    else if ((m_control & 0x20) != 0)
    {
        m_envelopeDecayLevel = 15;
    }
}

std::uint16_t APU::PulseChannel::SweepTarget(bool onesComplementNegate) const
{
    const std::uint8_t shift = m_sweep & 0x07;
    const std::uint16_t change = m_timerPeriod >> shift;
    if ((m_sweep & 0x08) == 0)
    {
        return m_timerPeriod + change;
    }

    return m_timerPeriod - change - (onesComplementNegate ? 1 : 0);
}

void APU::PulseChannel::ClockSweep(bool onesComplementNegate)
{
    const std::uint8_t shift = m_sweep & 0x07;
    if (m_sweepDivider == 0)
    {
        if ((m_sweep & 0x80) != 0 && shift != 0 &&
            m_timerPeriod >= 8 && SweepTarget(onesComplementNegate) <= 0x07FF)
        {
            m_timerPeriod = SweepTarget(onesComplementNegate);
        }
        m_sweepDivider = (m_sweep >> 4) & 0x07;
    }
    else
    {
        --m_sweepDivider;
    }

    if (m_sweepReload)
    {
        m_sweepDivider = (m_sweep >> 4) & 0x07;
        m_sweepReload = false;
    }
}

std::uint8_t APU::PulseChannel::Output(bool onesComplementNegate) const
{
    if (!m_enabled || m_lengthCounter == 0 || m_timerPeriod < 8 ||
        ((m_sweep & 0x07) != 0 && SweepTarget(onesComplementNegate) > 0x07FF))
    {
        return 0;
    }

    const std::uint8_t duty = m_control >> 6;
    if (!DutySequences[duty][m_sequenceStep])
    {
        return 0;
    }

    return (m_control & 0x10) != 0 ? (m_control & 0x0F) : m_envelopeDecayLevel;
}

void APU::TriangleChannel::WriteControl(std::uint8_t data)
{
    m_control = data;
}

void APU::TriangleChannel::WriteTimerLow(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x0700) | data;
}

void APU::TriangleChannel::WriteTimerHigh(std::uint8_t data, bool lengthReloadBlocked)
{
    m_timerPeriod = (m_timerPeriod & 0x00FF) |
                    ((static_cast<std::uint16_t>(data) & 0x07) << 8);
    m_timerCounter = m_timerPeriod;
    if (m_enabled && !lengthReloadBlocked)
    {
        m_lengthCounter = LengthTable[(data >> 3) & 0x1F];
    }
    m_linearReloadFlag = true;
}

void APU::TriangleChannel::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
    {
        m_lengthCounter = 0;
    }
}

void APU::TriangleChannel::ClockTimer()
{
    if (m_timerCounter == 0)
    {
        m_timerCounter = m_timerPeriod;
        if (m_lengthCounter != 0 && m_linearCounter != 0 && m_timerPeriod > 1)
        {
            m_sequenceStep = (m_sequenceStep + 1) & 0x1F;
        }
        return;
    }

    --m_timerCounter;
}

void APU::TriangleChannel::ClockLinearCounter()
{
    if (m_linearReloadFlag)
    {
        m_linearCounter = m_control & 0x7F;
    }
    else if (m_linearCounter != 0)
    {
        --m_linearCounter;
    }

    if ((m_control & 0x80) == 0)
    {
        m_linearReloadFlag = false;
    }
}

void APU::TriangleChannel::ClockLength()
{
    if ((m_control & 0x80) == 0 && m_lengthCounter != 0)
    {
        --m_lengthCounter;
    }
}

std::uint8_t APU::TriangleChannel::Output() const
{
    if (!m_enabled || m_lengthCounter == 0 || m_linearCounter == 0 ||
        m_timerPeriod < 2)
    {
        return 0;
    }

    return TriangleSequence[m_sequenceStep];
}

void APU::NoiseChannel::WriteControl(std::uint8_t data)
{
    m_control = data;
}

void APU::NoiseChannel::WritePeriod(std::uint8_t data)
{
    m_mode = (data & 0x80) != 0;
    m_periodIndex = data & 0x0F;
}

void APU::NoiseChannel::WriteLength(std::uint8_t data, bool lengthReloadBlocked)
{
    if (m_enabled && !lengthReloadBlocked)
    {
        m_lengthCounter = LengthTable[(data >> 3) & 0x1F];
    }
    m_envelopeStart = true;
}

void APU::NoiseChannel::SetEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled)
    {
        m_lengthCounter = 0;
    }
}

void APU::NoiseChannel::ClockTimer()
{
    if (m_timerCounter != 0)
    {
        --m_timerCounter;
        return;
    }

    m_timerCounter = NoisePeriodTable[m_periodIndex];
    const std::uint8_t tap = m_mode ? 6 : 1;
    const std::uint16_t feedback = (m_shiftRegister & 0x01) ^
                                   ((m_shiftRegister >> tap) & 0x01);
    m_shiftRegister = (m_shiftRegister >> 1) | (feedback << 14);
}

void APU::NoiseChannel::ClockEnvelope()
{
    if (m_envelopeStart)
    {
        m_envelopeStart = false;
        m_envelopeDecayLevel = 15;
        m_envelopeDivider = m_control & 0x0F;
        return;
    }

    if (m_envelopeDivider != 0)
    {
        --m_envelopeDivider;
        return;
    }

    m_envelopeDivider = m_control & 0x0F;
    if (m_envelopeDecayLevel != 0)
    {
        --m_envelopeDecayLevel;
    }
    else if ((m_control & 0x20) != 0)
    {
        m_envelopeDecayLevel = 15;
    }
}

void APU::NoiseChannel::ClockLength()
{
    if ((m_control & 0x20) == 0 && m_lengthCounter != 0)
    {
        --m_lengthCounter;
    }
}

std::uint8_t APU::NoiseChannel::Output() const
{
    if (!m_enabled || m_lengthCounter == 0 || (m_shiftRegister & 0x01) != 0)
    {
        return 0;
    }

    return (m_control & 0x10) != 0 ? (m_control & 0x0F) : m_envelopeDecayLevel;
}

void APU::DmcChannel::WriteControl(std::uint8_t data)
{
    m_irqEnabled = (data & 0x80) != 0;
    if (!m_irqEnabled)
    {
        m_irqFlag = false;
    }
    m_loop = (data & 0x40) != 0;
    m_rateIndex = data & 0x0F;
}

void APU::DmcChannel::WriteDirectLoad(std::uint8_t data)
{
    m_outputLevel = data & 0x7F;
}

void APU::DmcChannel::WriteSampleAddress(std::uint8_t data)
{
    m_sampleAddress = 0xC000 | (static_cast<std::uint16_t>(data) << 6);
}

void APU::DmcChannel::WriteSampleLength(std::uint8_t data)
{
    m_sampleLength = (static_cast<std::uint16_t>(data) << 4) | 0x0001;
}

void APU::DmcChannel::RestartSample()
{
    m_currentAddress = m_sampleAddress;
    m_bytesRemaining = m_sampleLength;
}

void APU::DmcChannel::SetEnabled(bool enabled)
{
    m_irqFlag = false;
    if (!enabled)
    {
        m_bytesRemaining = 0;
        return;
    }

    if (m_bytesRemaining == 0)
    {
        RestartSample();
    }
}

void APU::DmcChannel::RefillSampleBuffer(const DmcMemoryReader& memoryReader)
{
    if (!m_sampleBufferEmpty || m_bytesRemaining == 0 || !memoryReader)
    {
        return;
    }

    m_sampleBuffer = memoryReader(m_currentAddress);
    m_sampleBufferEmpty = false;
    m_dmaStallCycles = 4;
    m_currentAddress = m_currentAddress == 0xFFFF ? 0x8000 : m_currentAddress + 1;
    --m_bytesRemaining;
    if (m_bytesRemaining == 0)
    {
        if (m_loop)
        {
            RestartSample();
        }
        else if (m_irqEnabled)
        {
            m_irqFlag = true;
        }
    }
}

void APU::DmcChannel::ClockTimer(const DmcMemoryReader& memoryReader)
{
    if (m_timerCounter == 0)
    {
        m_timerCounter = DmcRateTable[m_rateIndex];
        if (!m_silence)
        {
            if ((m_shiftRegister & 0x01) != 0)
            {
                if (m_outputLevel <= 125)
                {
                    m_outputLevel += 2;
                }
            }
            else if (m_outputLevel >= 2)
            {
                m_outputLevel -= 2;
            }
        }

        m_shiftRegister >>= 1;
        if (--m_bitsRemaining == 0)
        {
            m_bitsRemaining = 8;
            if (m_sampleBufferEmpty)
            {
                m_silence = true;
            }
            else
            {
                m_silence = false;
                m_shiftRegister = m_sampleBuffer;
                m_sampleBufferEmpty = true;
            }
        }
    }
    else
    {
        --m_timerCounter;
    }

    RefillSampleBuffer(memoryReader);
}

std::uint8_t APU::DmcChannel::Output() const
{
    return m_outputLevel;
}

bool APU::DmcChannel::ConsumeDmaStallCycle()
{
    if (m_dmaStallCycles == 0)
    {
        return false;
    }

    --m_dmaStallCycles;
    return true;
}

bool APU::DmcChannel::IrqPending() const
{
    return m_irqFlag;
}

bool APU::DmcChannel::Active() const
{
    return m_bytesRemaining != 0;
}

bool APU::LengthCounterClockPending() const
{
    if (m_frameResetDelay != 0)
    {
        return false;
    }

    const std::int32_t next = static_cast<std::int32_t>(m_frameCounter) + 1;
    const std::int32_t offset = m_frameJitterOffset;
    if (m_fiveStepFrameCounter)
    {
        return next == 14912 + offset || next == 37280 + offset;
    }

    return next == 14912 + offset || next == 29828 + offset;
}

void APU::ClockFrameCounter()
{
    if (m_frameResetDelay != 0)
    {
        --m_frameResetDelay;
        if (m_frameResetDelay == 0)
        {
            m_fiveStepFrameCounter = m_pendingFiveStepFrameCounter;
            m_frameIrqInhibit = m_pendingFrameIrqInhibit;
            m_frameCounter = 0;
            if (m_fiveStepFrameCounter)
            {
                ClockQuarterFrame();
                ClockHalfFrame();
            }
        }
        return;
    }

    ++m_frameCounter;
    const std::int32_t offset = m_frameJitterOffset;

    // Event offsets are in CPU cycles from the cycle that applied the
    // sequencer reset, derived from blargg's 2005 APU timing suite: the
    // half/quarter frame boundary is a three-CPU-cycle signal whose middle
    // cycle clocks the length counters, and the frame IRQ flag is set on
    // three consecutive cycles at the end of the frame.
    if (m_fiveStepFrameCounter)
    {
        if (m_frameCounter == 7456 + offset || m_frameCounter == 22370 + offset)
        {
            ClockQuarterFrame();
        }
        else if (m_frameCounter == 14912 + offset || m_frameCounter == 37280 + offset)
        {
            if (m_suppressHalfFrame)
            {
                m_suppressHalfFrame = false;
            }
            else
            {
                ClockHalfFrame();
            }
            ClockQuarterFrame();
        }

        if (m_frameCounter == 37282 + offset)
        {
            m_frameCounter = 0;
        }
        return;
    }

    if (m_frameCounter == 7456 + offset || m_frameCounter == 22370 + offset)
    {
        ClockQuarterFrame();
    }
    else if (m_frameCounter == 14912 + offset || m_frameCounter == 29828 + offset)
    {
        if (m_suppressHalfFrame)
        {
            m_suppressHalfFrame = false;
        }
        else
        {
            ClockHalfFrame();
        }
        ClockQuarterFrame();
    }

    if (m_frameCounter >= 29827 + offset && m_frameCounter <= 29829 + offset &&
        !m_frameIrqInhibit)
    {
        m_frameIrqFlag = true;
       // The frame IRQ line reaches the CPU after the flag becomes readable
        // through $4015; this delay models that latency against the
        // instruction-boundary interrupt poll (blargg 07.irq_flag_timing).
        // Sequenced per-cycle reads shifted the effective poll point; 08
        // now needs the in-CPU poll timing planned for the cycle-accurate
        // CPU Phase 5.
        m_frameIrqLineCountdown = FrameIrqLineLatencyCycles;
    }

    if (m_frameCounter == 29830 + offset)
    {
        m_frameCounter = 0;
    }
}

void APU::ClockQuarterFrame()
{
    m_pulse1.ClockEnvelope();
    m_pulse2.ClockEnvelope();
    m_triangle.ClockLinearCounter();
    m_noise.ClockEnvelope();
}

void APU::ClockHalfFrame()
{
    m_pulse1.ClockLength();
    m_pulse2.ClockLength();
    m_triangle.ClockLength();
    m_noise.ClockLength();
    m_pulse1.ClockSweep(true);
    m_pulse2.ClockSweep(false);
}

float APU::ApplyOutputFilters(float sample)
{
    constexpr float HighPass90Coefficient = 0.98734F;
    constexpr float HighPass440Coefficient = 0.94100F;
    constexpr float LowPass14kCoefficient = 0.66600F;

    const float afterHighPass90 = HighPass90Coefficient *
        (m_highPass90Output + sample - m_highPass90PreviousInput);
    m_highPass90PreviousInput = sample;
    m_highPass90Output = afterHighPass90;

    const float afterHighPass440 = HighPass440Coefficient *
        (m_highPass440Output + afterHighPass90 - m_highPass440PreviousInput);
    m_highPass440PreviousInput = afterHighPass90;
    m_highPass440Output = afterHighPass440;

    m_lowPass14kOutput += LowPass14kCoefficient *
        (afterHighPass440 - m_lowPass14kOutput);
    return m_lowPass14kOutput;
}

void APU::QueueSample()
{
    constexpr std::size_t MaximumQueuedSamples = SampleRate / 5;
    if (m_samples.size() == MaximumQueuedSamples)
    {
        m_samples.erase(m_samples.begin());
    }

    const float pulse1 = static_cast<float>(m_pulse1.Output(true));
    const float pulse2 = static_cast<float>(m_pulse2.Output(false));
    const float triangle = static_cast<float>(m_triangle.Output());
    const float noise = static_cast<float>(m_noise.Output());
    const float dmc = static_cast<float>(m_dmc.Output());

    const float pulseSum = pulse1 + pulse2;
    const float pulseMix = pulseSum == 0.0F
                               ? 0.0F
                               : 95.88F / ((8128.0F / pulseSum) + 100.0F);
    const float tndInput = triangle / 8227.0F + noise / 12241.0F +
                           dmc / 22638.0F;
    const float tndMix = tndInput == 0.0F
                             ? 0.0F
                             : 159.79F / ((1.0F / tndInput) + 100.0F);
    m_samples.push_back(ApplyOutputFilters(pulseMix + tndMix));
}

} // namespace dendyforge
