#include "apu.hpp"

#include <algorithm>
#include <utility>

namespace dendyforge
{

namespace
{

constexpr std::uint32_t CpuClockHz = 1'789'773;
constexpr std::uint32_t HalfFramePeriod = CpuClockHz / 240;
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

}

void APU::Clock()
{
    m_pulse1.ClockTimer();
    m_pulse2.ClockTimer();
    m_triangle.ClockTimer();
    ClockFrameCounter();

    m_samplePhase += SampleRate;
    if (m_samplePhase >= CpuClockHz)
    {
        m_samplePhase -= CpuClockHz;
        QueueSample();
    }
}

std::uint8_t APU::CpuRead(std::uint16_t address) const
{
    if (address != 0x4015)
    {
        return 0;
    }

    return (m_pulse1.m_lengthCounter != 0 ? 0x01 : 0x00) |
           (m_pulse2.m_lengthCounter != 0 ? 0x02 : 0x00) |
           (m_triangle.m_lengthCounter != 0 ? 0x04 : 0x00);
}

void APU::CpuWrite(std::uint16_t address, std::uint8_t data)
{
    switch (address)
    {
    case 0x4000: m_pulse1.WriteControl(data); break;
    case 0x4002: m_pulse1.WriteTimerLow(data); break;
    case 0x4003: m_pulse1.WriteTimerHigh(data); break;
    case 0x4004: m_pulse2.WriteControl(data); break;
    case 0x4006: m_pulse2.WriteTimerLow(data); break;
    case 0x4007: m_pulse2.WriteTimerHigh(data); break;
    case 0x4008: m_triangle.WriteControl(data); break;
    case 0x400A: m_triangle.WriteTimerLow(data); break;
    case 0x400B: m_triangle.WriteTimerHigh(data); break;
    case 0x4015:
        m_pulse1.SetEnabled((data & 0x01) != 0);
        m_pulse2.SetEnabled((data & 0x02) != 0);
        m_triangle.SetEnabled((data & 0x04) != 0);
        break;
    default:
        break;
    }
}

std::vector<float> APU::TakeSamples()
{
    return std::exchange(m_samples, {});
}

void APU::PulseChannel::WriteControl(std::uint8_t data)
{
    m_control = data;
}

void APU::PulseChannel::WriteTimerLow(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x0700) | data;
}

void APU::PulseChannel::WriteTimerHigh(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x00FF) |
                    ((static_cast<std::uint16_t>(data) & 0x07) << 8);
    m_timerCounter = m_timerPeriod * 2 + 1;
    m_sequenceStep = 0;
    if (m_enabled)
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

std::uint8_t APU::PulseChannel::Output() const
{
    if (!m_enabled || m_lengthCounter == 0 || m_timerPeriod < 8)
    {
        return 0;
    }

    const std::uint8_t duty = m_control >> 6;
    if (!DutySequences[duty][m_sequenceStep])
    {
        return 0;
    }

    return m_control & 0x0F;
}

void APU::TriangleChannel::WriteControl(std::uint8_t data)
{
    m_control = data;
}

void APU::TriangleChannel::WriteTimerLow(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x0700) | data;
}

void APU::TriangleChannel::WriteTimerHigh(std::uint8_t data)
{
    m_timerPeriod = (m_timerPeriod & 0x00FF) |
                    ((static_cast<std::uint16_t>(data) & 0x07) << 8);
    m_timerCounter = m_timerPeriod;
    if (m_enabled)
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

void APU::ClockFrameCounter()
{
    ++m_frameCounter;
    if (m_frameCounter < HalfFramePeriod)
    {
        return;
    }

    m_frameCounter = 0;
    m_triangle.ClockLinearCounter();
    m_clockLengthCounters = !m_clockLengthCounters;
    if (m_clockLengthCounters)
    {
        m_pulse1.ClockLength();
        m_pulse2.ClockLength();
        m_triangle.ClockLength();
    }
}

void APU::QueueSample()
{
    constexpr std::size_t MaximumQueuedSamples = SampleRate / 5;
    if (m_samples.size() == MaximumQueuedSamples)
    {
        m_samples.erase(m_samples.begin());
    }

    const float pulse = static_cast<float>(m_pulse1.Output() +
                                           m_pulse2.Output());
    const float triangle = static_cast<float>(m_triangle.Output());
    m_samples.push_back(pulse / 60.0F + triangle / 120.0F);
}

} // namespace dendyforge
