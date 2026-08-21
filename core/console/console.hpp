#pragma once

#include <memory>
#include <string>

#include "bus/bus.hpp"
#include "cpu/cpu6502.hpp"

namespace dendyforge
{

class Cartridge;

class Console
{
public:
    Console();
    ~Console();

    bool LoadRom(const std::string& path);
    void Reset();
    void Clock();

    CPU6502& Cpu();
    PPU& VideoProcessor();

private:
    Bus m_bus;
    CPU6502 m_cpu;
    std::unique_ptr<Cartridge> m_cartridge;
};

} // namespace dendyforge
