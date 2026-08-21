#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace dendyforge
{

class APU
{
public:
    static constexpr int SampleRate = 44'100;

    void Clock();
    std::uint8_t CpuRead(std::uint16_t address) const;
    void CpuWrite(std::uint16_t address, std::uint8_t data);
    std::vector<float> TakeSamples();

private:
    struct PulseChannel
    {
        void WriteControl(std::uint8_t data);
        void WriteTimerLow(std::uint8_t data);
        void WriteTimerHigh(std::uint8_t data);
        void SetEnabled(bool enabled);
        void ClockTimer();
        void ClockLength();
        std::uint8_t Output() const;

        std::uint8_t m_control{0};
        std::uint16_t m_timerPeriod{0};
        std::uint16_t m_timerCounter{0};
        std::uint8_t m_sequenceStep{0};
        std::uint8_t m_lengthCounter{0};
        bool m_enabled{false};
    };

    struct TriangleChannel
    {
        void WriteControl(std::uint8_t data);
        void WriteTimerLow(std::uint8_t data);
        void WriteTimerHigh(std::uint8_t data);
        void SetEnabled(bool enabled);
        void ClockTimer();
        void ClockLinearCounter();
        void ClockLength();
        std::uint8_t Output() const;

        std::uint8_t m_control{0};
        std::uint16_t m_timerPeriod{0};
        std::uint16_t m_timerCounter{0};
        std::uint8_t m_sequenceStep{0};
        std::uint8_t m_lengthCounter{0};
        std::uint8_t m_linearCounter{0};
        bool m_linearReloadFlag{false};
        bool m_enabled{false};
    };

    void ClockFrameCounter();
    void QueueSample();

    PulseChannel m_pulse1;
    PulseChannel m_pulse2;
    TriangleChannel m_triangle;
    std::uint32_t m_frameCounter{0};
    bool m_clockLengthCounters{false};
    std::uint32_t m_samplePhase{0};
    std::vector<float> m_samples;
};

} // namespace dendyforge
