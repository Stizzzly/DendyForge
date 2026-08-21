#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace dendyforge
{

class APU
{
public:
    static constexpr int SampleRate = 44'100;
    using DmcMemoryReader = std::function<std::uint8_t(std::uint16_t)>;

    void Reset();
    void Clock();
    std::uint8_t CpuRead(std::uint16_t address);
    void CpuWrite(std::uint16_t address, std::uint8_t data);
    void SetDmcMemoryReader(DmcMemoryReader reader);
    bool ConsumeDmcDmaStallCycle();
    bool IrqPending() const;
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

    struct DmcChannel
    {
        void WriteControl(std::uint8_t data);
        void WriteDirectLoad(std::uint8_t data);
        void WriteSampleAddress(std::uint8_t data);
        void WriteSampleLength(std::uint8_t data);
        void SetEnabled(bool enabled);
        void ClockTimer(const DmcMemoryReader& memoryReader);
        std::uint8_t Output() const;
        bool ConsumeDmaStallCycle();
        bool IrqPending() const;
        bool Active() const;

        std::uint8_t m_rateIndex{0};
        std::uint16_t m_timerCounter{0};
        std::uint8_t m_outputLevel{0};
        std::uint16_t m_sampleAddress{0xC000};
        std::uint16_t m_sampleLength{1};
        std::uint16_t m_currentAddress{0xC000};
        std::uint16_t m_bytesRemaining{0};
        std::uint8_t m_sampleBuffer{0};
        std::uint8_t m_shiftRegister{0};
        std::uint8_t m_bitsRemaining{8};
        std::uint8_t m_dmaStallCycles{0};
        bool m_sampleBufferEmpty{true};
        bool m_silence{true};
        bool m_irqEnabled{false};
        bool m_loop{false};
        bool m_irqFlag{false};

        void RestartSample();
        void RefillSampleBuffer(const DmcMemoryReader& memoryReader);
    };

    void ClockFrameCounter();
    void ClockQuarterFrame();
    void ClockHalfFrame();
    void QueueSample();

    PulseChannel m_pulse1;
    PulseChannel m_pulse2;
    TriangleChannel m_triangle;
    NoiseChannel m_noise;
    DmcChannel m_dmc;
    DmcMemoryReader m_dmcMemoryReader;
    std::uint32_t m_frameCounter{0};
    bool m_fiveStepFrameCounter{false};
    bool m_frameIrqInhibit{false};
    bool m_frameIrqFlag{false};
    bool m_pendingFiveStepFrameCounter{false};
    bool m_pendingFrameIrqInhibit{false};
    bool m_cpuCycleOdd{false};
    std::uint8_t m_frameResetDelay{0};
    std::uint32_t m_samplePhase{0};
    std::vector<float> m_samples;
};

} // namespace dendyforge
