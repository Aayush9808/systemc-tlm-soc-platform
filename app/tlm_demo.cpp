#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <iomanip>
#include <iostream>

#include "../model/memory/simple_memory.h"
#include "../model/memory/simple_rom.h"
#include "../model/interconnect/simple_interconnect.h"
#include "../model/peripherals/uart.h"
#include "../model/peripherals/gpio.h"
#include "../model/peripherals/rv_timer.h"
#include "../model/peripherals/spi_device.h"
#include "../model/irq/irq_controller.h"

class TlmDemo : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<TlmDemo> socket;

    sc_core::sc_signal<bool> timer_irq;
    sc_core::sc_signal<bool> spi_irq;
    sc_core::sc_signal<bool> cpu_irq;

    SimpleMemory memory;
    SimpleRom rom;
    Uart uart;
    Gpio gpio;
    RvTimer timer;
    SpiDevice spi;
    IrqController irq;
    SimpleInterconnect bus;

    TlmDemo(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket"),
          memory("memory"),
          rom("rom"),
          uart("uart"),
          gpio("gpio"),
          timer("timer"),
          spi("spi"),
          irq("irq"),
          bus("bus") {

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

        irq.timer_irq(timer_irq);
        irq.spi_irq(spi_irq);
        irq.cpu_irq(cpu_irq);

        SC_THREAD(run);
    }

private:
    void write32(uint64_t address, uint32_t value) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);

        socket->b_transport(trans, delay);
        wait(delay);

        std::cout << "WRITE 0x"
                  << std::hex << address
                  << " -> "
                  << trans.get_response_string()
                  << std::dec << "\n";
    }

    uint32_t read32(uint64_t address) {
        uint32_t value = 0;

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(true);

        socket->b_transport(trans, delay);
        wait(delay);

        std::cout << "READ  0x"
                  << std::hex << address
                  << " -> "
                  << trans.get_response_string()
                  << " value=0x"
                  << value
                  << std::dec << "\n";

        return value;
    }

    void test_dmi() {
        std::cout << "\n--- DMI TEST ---\n";

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        uint32_t value = 0xAABBCCDD;

        trans.set_command(tlm::TLM_READ_COMMAND);

        // Request DMI for the SRAM address
        trans.set_address(0x10000000);

        trans.set_data_ptr(
            reinterpret_cast<unsigned char*>(&value)
        );
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(true);

        tlm::tlm_dmi dmi_data;

        bool dmi_available =
            socket->get_direct_mem_ptr(trans, dmi_data);

        if (!dmi_available) {
            std::cout << "DMI: NOT AVAILABLE\n";
            return;
        }

        std::cout << "DMI: AVAILABLE\n";
        std::cout << "DMI range: 0x"
                  << std::hex
                  << dmi_data.get_start_address()
                  << " - 0x"
                  << dmi_data.get_end_address()
                  << std::dec << "\n";

        unsigned char* dmi_ptr =
            dmi_data.get_dmi_ptr();

        unsigned char* sram_ptr =
            dmi_ptr + (0x10000000 -
                       dmi_data.get_start_address());

        uint32_t direct_value = 0xDEADBEEF;

        *reinterpret_cast<uint32_t*>(sram_ptr) =
            direct_value;

        std::cout << "DMI direct write: 0x"
                  << std::hex
                  << direct_value
                  << std::dec << "\n";

        uint32_t read_back =
            read32(0x10000000);

        if (read_back == direct_value) {
            std::cout << "DMI TEST: PASS\n";
        }
        else {
            std::cout << "DMI TEST: FAIL\n";
        }
    }

    void run() {
        write32(0x10000000, 0x12345678);
        read32(0x10000000);

        read32(0x00000000);

        write32(0x00000000, 0x11111111);

        write32(0x40000000, 0x3);
        write32(0x4000000C, 'H');
        write32(0x4000000C, 'i');
        write32(0x4000000C, '\n');
        write32(0x4000000C, '\n');
        read32(0x40000004);

        write32(0x40010004, 0xF);
        read32(0x40010004);

        write32(0x40010010, 0xF);
        read32(0x40010010);

        write32(0x40010008, 0x2);
        read32(0x40010004);

        write32(0x40020004, 1);
        write32(0x40020014, 0);
        write32(0x40020010, 10);
        write32(0x4002001C, 1);
        write32(0x40020000, 1);

        read32(0x40020008);
        read32(0x40020018);

        wait(50, sc_core::SC_NS);

        write32(0x40040004, 1);
        read32(0x40040000);
        read32(0x40040008);
        write32(0x4004000C, 1);

        std::cout << "\n--- SPI TEST ---\n";

        write32(0x40030000, 1);
        write32(0x40030004, 1);
        write32(0x4003000C, 0x55);

        read32(0x40030008);
        read32(0x40030010);

        test_dmi();

        std::cout << "\nSimulation time: "
                  << sc_core::sc_time_stamp()
                  << "\n";

        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    TlmDemo demo("demo");

    sc_core::sc_start();

    return 0;
}