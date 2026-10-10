#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <iostream>
#include <string>

#include "../generated/include/soc_memory_map.h"
#include "../generated/include/uart0.h"
#include "../generated/include/gpio.h"
#include "../generated/include/rv_timer.h"
#include "../generated/include/spi_device.h"
#include "../generated/include/irq.h"

#include "../model/memory/simple_memory.h"
#include "../model/memory/simple_rom.h"
#include "../model/interconnect/simple_interconnect.h"
#include "../model/peripherals/uart.h"
#include "../model/peripherals/gpio.h"
#include "../model/peripherals/rv_timer.h"
#include "../model/peripherals/spi_device.h"
#include "../model/irq/irq_controller.h"

class SocSim : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(SocSim);
    tlm_utils::simple_initiator_socket<SocSim> socket;

    sc_core::sc_signal<bool> timer_irq;
    sc_core::sc_signal<bool> spi_irq;
    sc_core::sc_signal<bool> gpio_irq;
    sc_core::sc_signal<bool> uart_irq;
    sc_core::sc_signal<bool> cpu_irq;

    SimpleMemory memory;
    SimpleRom rom;
    Uart uart;
    Gpio gpio;
    RvTimer timer;
    SpiDevice spi;
    IrqController irq;
    SimpleInterconnect bus;

    bool failed = false;
    uint32_t observed_gpio_output = 0;
    uint64_t gpio_output_notifications = 0;

    SocSim(
        sc_core::sc_module_name name,
        bool functional_fast)
        : sc_core::sc_module(name),
          socket("socket"),
          timer_irq("timer_irq"),
          spi_irq("spi_irq"),
          gpio_irq("gpio_irq"),
          uart_irq("uart_irq"),
          cpu_irq("cpu_irq"),
          memory("memory"),
          rom("rom"),
          uart("uart"),
          gpio("gpio"),
          timer("timer"),
          spi("spi"),
          irq("irq"),
          bus("bus"),
          functional_fast_(functional_fast) {

        socket.bind(bus.target_socket);

        bus.rom_socket.bind(rom.socket);
        bus.memory_socket.bind(memory.socket);
        bus.uart_socket.bind(uart.socket);
        bus.gpio_socket.bind(gpio.socket);
        bus.timer_socket.bind(timer.socket);
        bus.spi_socket.bind(spi.socket);
        bus.irq_socket.bind(irq.socket);

        timer.irq(timer_irq);
        spi.irq(spi_irq);
        gpio.irq(gpio_irq);
        uart.irq(uart_irq);

        irq.timer_irq(timer_irq);
        irq.spi_irq(spi_irq);
        irq.gpio_irq(gpio_irq);
        irq.uart_irq(uart_irq);
        irq.cpu_irq(cpu_irq);

        gpio.set_output_callback([this](uint32_t value) {
            observed_gpio_output = value;
            ++gpio_output_notifications;
        });

        SC_THREAD(run);
    }

private:
    bool functional_fast_;

    bool write32(
        uint64_t address,
        uint32_t value) {

        tlm::tlm_generic_payload trans;

        sc_core::sc_time delay =
            sc_core::SC_ZERO_TIME;

        trans.set_command(
            tlm::TLM_WRITE_COMMAND
        );

        trans.set_address(address);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );

        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);

        socket->b_transport(
            trans,
            delay
        );

        if (functional_fast_) {
            wait(sc_core::SC_ZERO_TIME);
            wait(sc_core::SC_ZERO_TIME);
        }
        else {
            wait(delay);
        }

        return trans.get_response_status() ==
               tlm::TLM_OK_RESPONSE;
    }

    bool read32(
        uint64_t address,
        uint32_t& value) {

        value = 0;

        tlm::tlm_generic_payload trans;

        sc_core::sc_time delay =
            sc_core::SC_ZERO_TIME;

        trans.set_command(
            tlm::TLM_READ_COMMAND
        );

        trans.set_address(address);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );

        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);

        socket->b_transport(
            trans,
            delay
        );

        if (functional_fast_) {
            wait(sc_core::SC_ZERO_TIME);
            wait(sc_core::SC_ZERO_TIME);
        }
        else {
            wait(delay);
        }

        return trans.get_response_status() ==
               tlm::TLM_OK_RESPONSE;
    }

    void uart_print(
        const std::string& text) {

        for (char value : text) {

            write32(
                (generated::UART0_BASE + generated::uart0::WDATA_OFFSET),
                static_cast<uint32_t>(
                    static_cast<unsigned char>(value)
                )
            );
        }
    }

    void fail(
        const std::string& message) {

        failed = true;

        std::cout
            << "FAIL: "
            << message
            << "\n";
    }

    bool boot_scenario() {

        std::cout
            << "\n=== SOC BOOT SCENARIO ===\n";

        uart_print(
            "Virtual RISC-V SoC boot\n"
        );

        uint32_t rom_value0 = 0;
        uint32_t rom_value1 = 0;

        if (!read32(
                generated::ROM_BASE,
                rom_value0)) {

            fail("ROM table entry 0 read failed");
            return false;
        }

        if (!read32(
                (generated::ROM_BASE + 0x4ULL),
                rom_value1)) {

            fail("ROM table entry 1 read failed");
            return false;
        }

        if (!write32(
                generated::SRAM_BASE,
                rom_value0)) {

            fail("SRAM table entry 0 write failed");
            return false;
        }

        if (!write32(
                (generated::SRAM_BASE + 0x4ULL),
                rom_value1)) {

            fail("SRAM table entry 1 write failed");
            return false;
        }

        uint32_t ram_value0 = 0;
        uint32_t ram_value1 = 0;

        if (!read32(
                generated::SRAM_BASE,
                ram_value0)) {

            fail("SRAM table entry 0 read failed");
            return false;
        }

        if (!read32(
                (generated::SRAM_BASE + 0x4ULL),
                ram_value1)) {

            fail("SRAM table entry 1 read failed");
            return false;
        }

        std::cout
            << "ROM[0] -> SRAM[0]: 0x"
            << std::hex
            << rom_value0
            << " -> 0x"
            << ram_value0
            << "\n";

        std::cout
            << "ROM[4] -> SRAM[4]: 0x"
            << rom_value1
            << " -> 0x"
            << ram_value1
            << std::dec
            << "\n";

        if (ram_value0 != rom_value0 ||
            ram_value1 != rom_value1) {

            fail("ROM to SRAM table copy mismatch");
            return false;
        }

        uart_print(
            "ROM -> SRAM copy OK\n"
        );

        std::cout
            << "\nBOOT SCENARIO: PASS\n";

        return true;
    }

    bool gpio_output_scenario() {

        std::cout
            << "\n=== GPIO OUTPUT SCENARIO ===\n";

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OE_OFFSET),
                0x0000000F)) {

            fail("GPIO output-enable configuration failed");
            return false;
        }

        uint32_t oe = 0;

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OE_OFFSET),
                oe)) {

            fail("GPIO output-enable read failed");
            return false;
        }

        if (oe != 0x0000000F) {

            fail("GPIO output-enable value mismatch");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                0x00000001)) {

            fail("GPIO direct output write failed");
            return false;
        }

        uint32_t output = 0;

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                output)) {

            fail("GPIO direct output read failed");
            return false;
        }

        if (output != 0x00000001) {

            fail("GPIO direct output mismatch");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::MASKED_OUT_LOWER_OFFSET),
                0x00060004)) {

            fail("GPIO masked lower write failed");
            return false;
        }

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                output)) {

            fail("GPIO output read after masked write failed");
            return false;
        }

        if (output != 0x00000005) {

            fail("GPIO masked lower write mismatch");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::MASKED_OUT_UPPER_OFFSET),
                0x00010001)) {

            fail("GPIO masked upper write failed");
            return false;
        }

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                output)) {

            fail("GPIO output read after upper masked write failed");
            return false;
        }

        if (output != 0x00010005) {

            fail("GPIO masked upper write mismatch");
            return false;
        }

        std::cout
            << "GPIO direct output: 0x"
            << std::hex
            << 0x00000001
            << "\n";

        std::cout
            << "GPIO masked output: 0x"
            << output
            << std::dec
            << "\n";

        std::cout
            << "GPIO output-enable + direct/masked writes: PASS\n";

        return true;
    }

    bool invalid_transaction_scenario() {

        std::cout
            << "\n=== INVALID TRANSACTION SCENARIO ===\n";

        uint32_t value = 0;

        bool result = read32(
            0x50000000,
            value
        );

        if (result) {

            fail("unmapped transaction unexpectedly succeeded");
            return false;
        }

        std::cout
            << "Unmapped address rejected: PASS\n";

        return true;
    }

    bool uart_irq_scenario() {

        std::cout
            << "\n=== UART IRQ SCENARIO ===\n";

        // STATUS is schema-declared RO: a write must not alter
        // the value observed by a subsequent read.
        uint32_t uart_status_before = 0;
        uint32_t uart_status_after = 0;
        if (!read32(
                (generated::UART0_BASE + generated::uart0::STATUS_OFFSET),
                uart_status_before) ||
            write32(
                (generated::UART0_BASE + generated::uart0::STATUS_OFFSET),
                0) ||
            !read32(
                (generated::UART0_BASE + generated::uart0::STATUS_OFFSET),
                uart_status_after) ||
            uart_status_before != uart_status_after) {
            fail("UART read-only STATUS register changed after write");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                0xFFFFFFFFu)) {
            fail("UART control mask write failed");
            return false;
        }

        uint32_t uart_control = 0;
        if (!read32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                uart_control) ||
            uart_control != 0x3u) {
            fail("UART control register mask was not applied");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                0)) {
            fail("UART control reset before IRQ scenario failed");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                1)) {

            fail("UART TX enable failed");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::INTR_ENABLE_OFFSET),
                1)) {

            fail("UART interrupt enable failed");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                8)) {

            fail("UART IRQ controller enable failed");
            return false;
        }

        uint32_t uart_write_only_value = 0;
        if (read32(
                (generated::UART0_BASE + generated::uart0::WDATA_OFFSET),
                uart_write_only_value)) {
            fail("UART write-only WDATA register unexpectedly allowed reads");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::WDATA_OFFSET),
                'A')) {

            fail("UART TX failed");
            return false;
        }

        if (!cpu_irq.read()) {

            fail("UART did not assert CPU IRQ");
            return false;
        }

        uint32_t claim = 0;

        if (!read32(
                (generated::IRQ_BASE + generated::irq::CLAIM_OFFSET),
                claim)) {

            fail("UART IRQ claim failed");
            return false;
        }

        if (claim != 8) {

            fail("wrong UART IRQ claim value");
            return false;
        }

        if (!write32(
                (generated::UART0_BASE + generated::uart0::INTR_STATE_OFFSET),
                1)) {

            fail("UART interrupt clear failed");
            return false;
        }

        uint32_t uart_intr_state = 0;
        if (!read32(
                (generated::UART0_BASE + generated::uart0::INTR_STATE_OFFSET),
                uart_intr_state) ||
            (uart_intr_state & 1u) != 0) {
            fail("UART W1C interrupt state did not clear");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::COMPLETE_OFFSET),
                claim)) {

            fail("UART IRQ complete failed");
            return false;
        }

        if (cpu_irq.read()) {

            fail("UART CPU IRQ remained asserted");
            return false;
        }

        std::cout
            << "UART TX -> IRQ -> claim -> clear -> complete: PASS\n";

        return true;
    }

    bool gpio_irq_scenario() {

        std::cout
            << "\n=== GPIO IRQ SCENARIO ===\n";

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::INTR_RISE_OFFSET),
                1)) {

            fail("GPIO rise configuration failed");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::INTR_ENABLE_OFFSET),
                1)) {

            fail("GPIO interrupt enable failed");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                4)) {

            fail("GPIO IRQ controller enable failed");
            return false;
        }

        gpio.set_input(0);
        wait(sc_core::SC_ZERO_TIME);

        gpio.set_input(1);
        wait(sc_core::SC_ZERO_TIME);

        uint32_t state = 0;

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::INTR_STATE_OFFSET),
                state)) {

            fail("GPIO interrupt state read failed");
            return false;
        }

        if ((state & 1) == 0) {

            fail("GPIO rising edge did not set interrupt");
            return false;
        }

        if (!cpu_irq.read()) {

            fail("GPIO did not assert CPU IRQ");
            return false;
        }

        uint32_t claim = 0;

        if (!read32(
                (generated::IRQ_BASE + generated::irq::CLAIM_OFFSET),
                claim)) {

            fail("GPIO IRQ claim failed");
            return false;
        }

        if (claim != 4) {

            fail("wrong GPIO IRQ claim value");
            return false;
        }

        if (cpu_irq.read()) {

            fail("GPIO CPU IRQ remained asserted");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::COMPLETE_OFFSET),
                claim)) {

            fail("GPIO IRQ complete failed");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::INTR_STATE_OFFSET),
                1)) {

            fail("GPIO interrupt clear failed");
            return false;
        }

        std::cout
            << "GPIO rising edge -> IRQ -> claim -> complete: PASS\n";

        return true;
    }

    bool timer_scenario() {

        std::cout
            << "\n=== TIMER IRQ SCENARIO ===\n";

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CFG0_OFFSET),
                1)) {

            fail("timer configuration failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_LOWER_OFFSET),
                0)) {

            fail("timer lower reset failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_UPPER_OFFSET),
                0)) {

            fail("timer upper reset failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_LOWER_OFFSET),
                5)) {

            fail("timer compare lower failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_UPPER_OFFSET),
                0)) {

            fail("timer compare upper failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_ENABLE_OFFSET),
                1)) {

            fail("timer interrupt enable failed");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CTRL_OFFSET),
                1)) {

            fail("timer start failed");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                1)) {

            fail("timer IRQ controller enable failed");
            return false;
        }

        wait(
            60,
            sc_core::SC_NS
        );

        uint32_t timer_low = 0;

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_LOWER_OFFSET),
                timer_low)) {

            fail("timer value read failed");
            return false;
        }

        std::cout
            << "Timer value after simulated time: "
            << timer_low
            << "\n";

        uint32_t intr_state = 0;

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_STATE_OFFSET),
                intr_state)) {

            fail("timer interrupt state read failed");
            return false;
        }

        if ((intr_state & 1) == 0) {

            fail("timer compare did not set interrupt");
            return false;
        }

        if (!cpu_irq.read()) {

            fail("timer did not assert CPU IRQ");
            return false;
        }

        uint32_t claim = 0;

        if (!read32(
                (generated::IRQ_BASE + generated::irq::CLAIM_OFFSET),
                claim)) {

            fail("timer IRQ claim failed");
            return false;
        }

        if (claim != 1) {

            fail("wrong timer IRQ claim value");
            return false;
        }

        if (cpu_irq.read()) {

            fail("timer CPU IRQ remained asserted");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_STATE_OFFSET),
                1)) {

            fail("timer interrupt clear failed");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::COMPLETE_OFFSET),
                claim)) {

            fail("timer IRQ complete failed");
            return false;
        }

        std::cout
            << "Timer compare -> IRQ -> claim -> clear -> complete: PASS\n";

        // Exercise the documented split-counter read protocol across a
        // low-word rollover. The upper word paired with TIMER_V_LOWER must
        // come from the same sampled 64-bit value, not a later live read.
        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_UPPER_OFFSET),
                0xFFFFFFFFu) ||
            !write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_LOWER_OFFSET),
                0xFFFFFFFFu) ||
            !write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_UPPER_OFFSET),
                0) ||
            !write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_LOWER_OFFSET),
                0xFFFFFFFDu)) {
            fail("timer rollover test setup failed");
            return false;
        }

        uint32_t sampled_low = 0;
        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_LOWER_OFFSET),
                sampled_low) ||
            sampled_low != 0xFFFFFFFEu) {
            fail("timer rollover test did not capture expected low word");
            return false;
        }

        // Four 10 ns timer ticks cross from 0x00000000_FFFFFFFE into
        // 0x00000001_00000002 while the captured upper half stays coherent.
        wait(30, sc_core::SC_NS);

        uint32_t coherent_upper = 0;
        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_UPPER_OFFSET),
                coherent_upper) ||
            coherent_upper != 0) {
            fail("timer split read returned incoherent upper word");
            return false;
        }

        uint32_t live_upper = 0;
        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_UPPER_OFFSET),
                live_upper) ||
            live_upper != 1) {
            fail("timer counter did not roll over to upper word 1");
            return false;
        }

        std::cout
            << "Timer split-read coherency across 32-bit rollover: PASS\n";

        return true;
    }

    bool spi_scenario() {

        std::cout
            << "\n=== SPI SCENARIO ===\n";

        if (!write32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CONTROL_OFFSET),
                1)) {

            fail("SPI enable failed");
            return false;
        }

        if (!write32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CFG_OFFSET),
                0)) {

            fail("SPI configuration failed");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                2)) {

            fail("SPI IRQ controller enable failed");
            return false;
        }

        if (!write32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::TX_OFFSET),
                0x55)) {

            fail("SPI TX failed");
            return false;
        }

        if (!cpu_irq.read()) {

            fail("SPI did not assert CPU IRQ");
            return false;
        }

        uint32_t status = 0;

        if (!read32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::STATUS_OFFSET),
                status)) {

            fail("SPI status read failed");
            return false;
        }

        if ((status & (1u << 1)) == 0) {

            fail("SPI RX FIFO is empty");
            return false;
        }

        uint32_t rx = 0;

        if (!read32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::RX_OFFSET),
                rx)) {

            fail("SPI RX failed");
            return false;
        }

        if ((rx & 0xff) != 0xAA) {

            fail("unexpected SPI response");
            return false;
        }

        uint32_t claim = 0;

        if (!read32(
                (generated::IRQ_BASE + generated::irq::CLAIM_OFFSET),
                claim)) {

            fail("SPI IRQ claim failed");
            return false;
        }

        if (claim != 2) {

            fail("wrong SPI IRQ claim value");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::COMPLETE_OFFSET),
                claim)) {

            fail("SPI IRQ complete failed");
            return false;
        }

        std::cout
            << "SPI TX 0x55 -> RX 0xAA -> IRQ -> claim -> complete: PASS\n";

        return true;
    }

    void reset_soc() {

        memory.reset();
        uart.reset();
        gpio.reset();
        timer.reset();
        spi.reset();
        irq.reset();

        wait(sc_core::SC_ZERO_TIME);
        wait(sc_core::SC_ZERO_TIME);
    }

    bool reset_scenario() {

        std::cout
            << "\n=== RESET SCENARIO ===\n";

        if (!write32(
                generated::SRAM_BASE,
                0xDEADBEEF)) {

            fail("failed to create SRAM reset state");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OE_OFFSET),
                0x0000000F)) {

            fail("failed to create GPIO reset state");
            return false;
        }

        if (!write32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                0x00000055)) {

            fail("failed to create GPIO output state");
            return false;
        }

        if (observed_gpio_output != 0x00000055) {
            fail("GPIO output observer did not receive output update");
            return false;
        }

        const uint64_t notifications_before_reset =
            gpio_output_notifications;

        if (!write32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                1)) {

            fail("failed to create UART reset state");
            return false;
        }

        if (!write32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CONTROL_OFFSET),
                1)) {

            fail("failed to create SPI reset state");
            return false;
        }

        if (!write32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CFG_OFFSET),
                1)) {

            fail("failed to create SPI config state");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CFG0_OFFSET),
                1)) {

            fail("failed to create timer reset state");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_LOWER_OFFSET),
                100)) {

            fail("failed to create timer compare state");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_ENABLE_OFFSET),
                1)) {

            fail("failed to create timer interrupt state");
            return false;
        }

        if (!write32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CTRL_OFFSET),
                1)) {

            fail("failed to start timer");
            return false;
        }

        if (!write32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                0x0F)) {

            fail("failed to create IRQ controller state");
            return false;
        }

        wait(
            100,
            sc_core::SC_NS
        );

        sc_core::sc_time reset_time =
            sc_core::sc_time_stamp();

        reset_soc();

        uint32_t value = 0;

        if (!read32(
                generated::SRAM_BASE,
                value) ||
            value != 0) {

            fail("SRAM was not reset to zero");
            return false;
        }

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                value) ||
            value != 0) {

            fail("GPIO output was not reset");
            return false;
        }

        if (observed_gpio_output != 0 ||
            gpio_output_notifications <= notifications_before_reset) {
            fail("GPIO reset did not notify output observer");
            return false;
        }

        if (!read32(
                (generated::GPIO_BASE + generated::gpio::DIRECT_OE_OFFSET),
                value) ||
            value != 0) {

            fail("GPIO output-enable was not reset");
            return false;
        }

        if (!read32(
                (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                value) ||
            value != 0) {

            fail("UART control was not reset");
            return false;
        }

        if (!read32(
                (generated::UART0_BASE + generated::uart0::INTR_STATE_OFFSET),
                value) ||
            value != 0) {

            fail("UART interrupt state was not reset");
            return false;
        }

        if (!read32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CONTROL_OFFSET),
                value) ||
            value != 0) {

            fail("SPI control was not reset");
            return false;
        }

        if (!read32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::CFG_OFFSET),
                value) ||
            value != 0) {

            fail("SPI configuration was not reset");
            return false;
        }

        if (!read32(
                (generated::SPI_DEVICE_BASE + generated::spi_device::STATUS_OFFSET),
                value) ||
            value != 0x5) {

            fail("SPI FIFO status was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CTRL_OFFSET),
                value) ||
            value != 0) {

            fail("timer control was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::CFG0_OFFSET),
                value) ||
            value != 0) {

            fail("timer configuration was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_LOWER_OFFSET),
                value) ||
            value != 0) {
            fail("timer lower counter was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::TIMER_V_UPPER_OFFSET),
                value) ||
            value != 0) {
            fail("timer upper counter was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_LOWER_OFFSET),
                value) ||
            value != 0) {
            fail("timer compare lower was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::COMPARE_UPPER_OFFSET),
                value) ||
            value != 0) {
            fail("timer compare upper was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_ENABLE_OFFSET),
                value) ||
            value != 0) {
            fail("timer interrupt enable was not reset");
            return false;
        }

        if (!read32(
                (generated::RV_TIMER_BASE + generated::rv_timer::INTR_STATE_OFFSET),
                value) ||
            value != 0) {

            fail("timer interrupt state was not reset");
            return false;
        }

        if (!read32(
                (generated::IRQ_BASE + generated::irq::PENDING_OFFSET),
                value) ||
            value != 0) {

            fail("IRQ pending state was not reset");
            return false;
        }

        if (!read32(
                (generated::IRQ_BASE + generated::irq::ENABLE_OFFSET),
                value) ||
            value != 0) {

            fail("IRQ enable state was not reset");
            return false;
        }

        if (timer_irq.read() ||
            spi_irq.read() ||
            gpio_irq.read() ||
            uart_irq.read() ||
            cpu_irq.read()) {

            fail("IRQ signal remained asserted after reset");
            return false;
        }

        // Reset was performed after simulation time was already non-zero.
        if (reset_time <=
            sc_core::SC_ZERO_TIME) {

            fail("reset was attempted before simulated time advanced");
            return false;
        }

        std::cout
            << "Reset after simulated time: PASS\n";

        std::cout
            << "SRAM, GPIO, UART, Timer, SPI and IRQ state cleared\n";
        std::cout
            << "GPIO output observer stayed synchronized through reset: PASS\n";

        std::cout
            << "RESET SCENARIO: PASS\n";

        return true;
    }

    void run() {

        bool ok = boot_scenario();

        if (ok) {
            ok = gpio_output_scenario();
        }

        if (ok) {
            ok = invalid_transaction_scenario();
        }

        if (ok) {
            ok = uart_irq_scenario();
        }

        if (ok) {
            ok = gpio_irq_scenario();
        }

        if (ok) {
            ok = timer_scenario();
        }

        if (ok) {
            ok = spi_scenario();
        }

        if (ok) {
            ok = reset_scenario();
        }

        if (ok) {
            std::cout
                << "\n=== FIRMWARE SCENARIO ===\n"
                << "All required firmware checks: PASS\n";
        }
        else {
            std::cout
                << "\n=== FIRMWARE SCENARIO ===\n"
                << "FAIL\n";
        }

        std::cout
            << "\nSimulation time: "
            << sc_core::sc_time_stamp()
            << "\n";

        sc_core::sc_stop();
    }
};

int sc_main(
    int argc,
    char* argv[]) {

    std::string scenario = "boot";
    std::string timing = "timed-lt";

    for (int i = 1; i < argc; ++i) {

        std::string arg = argv[i];

        if (arg == "--scenario" &&
            i + 1 < argc) {

            scenario = argv[++i];

        }
        else if (arg == "--timing" &&
                 i + 1 < argc) {

            timing = argv[++i];
        }
    }

    if (scenario != "boot") {

        std::cout
            << "Unsupported scenario: "
            << scenario
            << "\n";

        return 1;
    }

    bool functional_fast = false;

    if (timing == "functional-fast") {

        functional_fast = true;

    }
    else if (timing == "timed-lt") {

        functional_fast = false;

    }
    else {

        std::cout
            << "Unsupported timing mode: "
            << timing
            << "\n";

        return 1;
    }

    std::cout
        << "Scenario: "
        << scenario
        << "\n";

    std::cout
        << "Timing: "
        << timing
        << "\n";

    SocSim soc(
        "soc",
        functional_fast
    );

    sc_core::sc_start();

    return soc.failed ? 1 : 0;
}