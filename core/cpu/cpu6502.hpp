class CPU6502
{
public:
    CPU6502();

    void ConnectBus(Bus* bus);

    void Reset();
    void Clock();

private:
    Bus* m_bus = nullptr;

    uint8_t m_a = 0;
    uint8_t m_x = 0;
    uint8_t m_y = 0;

    uint8_t m_sp = 0;
    uint16_t m_pc = 0;

    uint8_t m_status = 0;
};
