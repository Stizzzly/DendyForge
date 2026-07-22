#include "test_runner.hpp"

#include "cpu/cpu_tests.hpp"
#include "bus/bus_tests.hpp"
#include "cartridge/cartridge_tests.hpp"

void RunAllTests()
{
    RunBusTests();
    RunCartridgeTests();
    RunCpuTests();
}
