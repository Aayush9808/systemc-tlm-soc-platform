#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "../model/interconnect/simple_interconnect.h"
#include "../model/memory/simple_memory.h"
#include "../model/memory/simple_rom.h"
#include "../model/peripherals/uart.h"
#include "../model/peripherals/gpio.h"
#include "../model/peripherals/rv_timer.h"
#include "../model/peripherals/spi_device.h"
#include "../model/irq/irq_controller.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

class SocBench : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<SocBench> socket;

    explicit SocBench(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket") {
        SC_THREAD(run);
    }

    void set_transaction_count(uint64_t count) {
        transaction_count = count;
    }

    void set_trace_enabled(bool enabled) {
        trace_enabled = enabled;
    }

private:
    uint64_t transaction_count = 1000000;
    bool trace_enabled = false;

    SimpleMemory memory{"memory"};
    SimpleRom rom{"rom"};
    Uart uart{"uart"};
    Gpio gpio{"gpio"};
    RvTimer timer{"timer"};
    SpiDevice spi{"spi"};
    IrqController irq{"irq"};
    SimpleInterconnect bus{"bus"};

    sc_core::sc_signal<bool> timer_irq{"timer_irq"};
    sc_core::sc_signal<bool> spi_irq{"spi_irq"};
    sc_core::sc_signal<bool> gpio_irq{"gpio_irq"};
    sc_core::sc_signal<bool> uart_irq{"uart_irq"};
    sc_core::sc_signal<bool> cpu_irq{"cpu_irq"};

    void write32(uint64_t address, uint32_t value) {
        unsigned char data[4] = {
            static_cast<unsigned char>(value & 0xff),
            static_cast<unsigned char>((value >> 8) & 0xff),
            static_cast<unsigned char>((value >> 16) & 0xff),
            static_cast<unsigned char>((value >> 24) & 0xff)
        };

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(4);
        trans.set_streaming_width(4);

        socket->b_transport(trans, delay);

        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "Write failed at 0x"
                      << std::hex << address
                      << std::dec << "\n";
            sc_core::sc_stop();
            return;
        }

        wait(delay);
    }

    uint32_t read32(uint64_t address) {
        unsigned char data[4] = {};

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(4);
        trans.set_streaming_width(4);

        socket->b_transport(trans, delay);

        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
            std::cerr << "Read failed at 0x"
                      << std::hex << address
                      << std::dec << "\n";
            sc_core::sc_stop();
            return 0;
        }

        wait(delay);

        return static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8) |
               (static_cast<uint32_t>(data[2]) << 16) |
               (static_cast<uint32_t>(data[3]) << 24);
    }

    void run() {
        wait(sc_core::SC_ZERO_TIME);

        bus.set_trace_enabled(trace_enabled);

        std::cout << "\n=== SOC PERFORMANCE / TRACE BENCHMARK ===\n";
        std::cout << "Transactions: "
                  << transaction_count << "\n";

        std::cout << "Trace: "
                  << (trace_enabled ? "ON" : "OFF")
                  << "\n";

        auto start = std::chrono::steady_clock::now();

        uint32_t checksum = 0;

        for (uint64_t i = 0; i < transaction_count; ++i) {

            uint64_t memory_address =
                0x10000000ULL +
                ((i % 1024) * 4);

            write32(
                memory_address,
                static_cast<uint32_t>(i)
            );

            checksum ^=
                read32(memory_address);

            if ((i % 1000) == 0) {

                write32(
                    0x40010004,
                    static_cast<uint32_t>(i)
                );

                checksum ^=
                    read32(0x40010004);

                write32(
                    0x40000000,
                    0x1
                );

                checksum ^=
                    read32(0x40000004);

                write32(
                    0x40030004,
                    0x1
                );

                checksum ^=
                    read32(0x40030008);
            }
        }

        auto end = std::chrono::steady_clock::now();

        double seconds =
            std::chrono::duration<double>(
                end - start
            ).count();

        uint64_t total =
            bus.total_transactions();

        double throughput =
            static_cast<double>(total) /
            seconds;

        std::cout << std::fixed
                  << std::setprecision(6);

        std::cout
            << "Host runtime: "
            << seconds
            << " s\n";

        std::cout
            << "Bus transactions: "
            << total
            << "\n";

        std::cout
            << "Throughput: "
            << throughput
            << " transactions/sec\n";

        std::cout
            << "SRAM transactions: "
            << bus.target_transactions(
                   generated::TargetId::SRAM)
            << "\n";

        std::cout
            << "UART transactions: "
            << bus.target_transactions(
                   generated::TargetId::UART0)
            << "\n";

        std::cout
            << "GPIO transactions: "
            << bus.target_transactions(
                   generated::TargetId::GPIO)
            << "\n";

        std::cout
            << "SPI transactions: "
            << bus.target_transactions(
                   generated::TargetId::SPI_DEVICE)
            << "\n";

        std::cout
            << "Simulated time: "
            << sc_core::sc_time_stamp()
            << "\n";

        std::cout
            << "Checksum: 0x"
            << std::hex
            << checksum
            << std::dec
            << "\n";

        std::cout
            << "\nBenchmark: PASS\n";

        sc_core::sc_stop();
    }

public:
    void bind_soc() {
        bus.rom_socket.bind(rom.socket);
        bus.memory_socket.bind(memory.socket);
        bus.uart_socket.bind(uart.socket);
        bus.gpio_socket.bind(gpio.socket);
        bus.timer_socket.bind(timer.socket);
        bus.spi_socket.bind(spi.socket);
        bus.irq_socket.bind(irq.socket);

        socket.bind(bus.target_socket);

        timer.irq.bind(timer_irq);
        spi.irq.bind(spi_irq);
        gpio.irq.bind(gpio_irq);
        uart.irq.bind(uart_irq);

        irq.timer_irq(timer_irq);
        irq.spi_irq(spi_irq);
        irq.gpio_irq(gpio_irq);
        irq.uart_irq(uart_irq);
        irq.cpu_irq(cpu_irq);
    }
};

int sc_main(int argc, char* argv[]) {

    uint64_t transactions = 1000000;
    bool trace = false;

    if (argc > 1) {
        transactions =
            std::strtoull(argv[1], nullptr, 10);
    }

    if (argc > 2) {
        std::string mode = argv[2];

        if (mode == "--trace") {
            trace = true;
        }
    }

    SocBench bench("soc_bench");

    bench.set_transaction_count(transactions);
    bench.set_trace_enabled(trace);
    bench.bind_soc();

    sc_core::sc_start();

    return 0;
}