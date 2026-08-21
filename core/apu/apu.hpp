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
        void WriteSweep(std::uint8_t data);
        void WriteTimerLow(std::uint8_t data);
        void WriteTimerHigh(std::uint8_t data);
        void SetEnabled(bool enabled);
        void ClockTimer();
        void ClockEnvelope();
        void ClockLength();
        void ClockSweep(bool onesComplementNegate);
        std::uint8_t Output(bool onesComplementNegate) const;

        std::uint8_t m_control{0};
        std::uint8_t m_sweep{0};
        std::uint16_t m_timerPeriod{0};
        std::uint16_t m_timerCounter{0};
        std::uint8_t m_sequenceStep{0};
        std::uint8_t m_lengthCounter{0};
        std::uint8_t m_envelopeDivider{0};
        std::uint8_t m_envelopeDecayLevel{0};
        std::uint8_t m_sweepDivider{0};
        bool m_envelopeStart{false};
        bool m_sweepReload{false};
        bool m_enabled{false};

        std::uint16_t SweepTarget(bool onesComplementNegate) const;
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

    struct NoiseChannel
    {
        void WriteControl(std::uint8_t data);
        void WritePeriod(std::uint8_t data);
        void WriteLength(std::uint8_t data);
        void SetEnabled(bool enabled);
        void ClockTimer();
        void ClockEnvelope();
        void ClockLength();
        std::uint8_t Output() const;

        std::uint8_t m_control{0};
        std::uint8_t m_periodIndex{0};
        std::uint16_t m_timerCounter{0};
        std::uint16_t m_shiftRegister{1};
        std::uint8_t m_lengthCounter{0};
        std::uint8_t m_envelopeDivider{0};
        std::uint8_t m_envelopeDecayLevel{0};
        bool m_envelopeStart{false};
        bool m_mode{false};
        bool m_enabled{false};
    };

    void ClockFrameCounter();
    void QueueSample();

    PulseChannel m_pulse1;
    PulseChannel m_pulse2;
    TriangleChannel m_triangle;
    NoiseChannel m_noise;
    std::uint32_t m_frameCounter{0};
    bool m_clockLengthCounters{false};
    std::uint32_t m_samplePhase{0};
    std::vector<float> m_samples;
};

} // namespace dendyforge
