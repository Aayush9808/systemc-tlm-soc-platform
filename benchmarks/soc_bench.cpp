#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

#include "../model/interconnect/simple_interconnect.h"
#include "../model/memory/simple_memory.h"
#include "../model/memory/simple_rom.h"
#include "../model/peripherals/uart.h"
#include "../model/peripherals/gpio.h"
#include "../model/peripherals/rv_timer.h"
#include "../model/peripherals/spi_device.h"
#include "../model/irq/irq_controller.h"
#include "../generated/include/soc_memory_map.h"
#include "../generated/include/uart0.h"
#include "../generated/include/gpio.h"
#include "../generated/include/spi_device.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

class SocBench : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(SocBench);
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

    bool set_quantum_ns(uint64_t nanoseconds) {
        if (nanoseconds == 0) return false;
        quantum_enabled = true;
        quantum_ns = nanoseconds;
        tlm_utils::tlm_quantumkeeper::set_global_quantum(
            sc_core::sc_time(static_cast<double>(nanoseconds), sc_core::SC_NS)
        );
        quantum_keeper.reset();
        return true;
    }

private:
    uint64_t transaction_count = 1000000;
    bool trace_enabled = false;
    bool quantum_enabled = false;
    uint64_t quantum_ns = 0;
    uint64_t synchronization_count = 0;
    tlm_utils::tlm_quantumkeeper quantum_keeper;

    SimpleMemory memory{"memory", generated::SRAM_SIZE};
    SimpleRom rom{"rom", generated::ROM_SIZE};
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
        sc_core::sc_time delay = quantum_enabled
            ? quantum_keeper.get_local_time()
            : sc_core::SC_ZERO_TIME;

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

        if (quantum_enabled) {
            quantum_keeper.set(delay);
            if (quantum_keeper.need_sync()) {
                quantum_keeper.sync();
                ++synchronization_count;
            }
        } else {
            wait(delay);
            ++synchronization_count;
        }
    }

    uint32_t read32(uint64_t address) {
        unsigned char data[4] = {};

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = quantum_enabled
            ? quantum_keeper.get_local_time()
            : sc_core::SC_ZERO_TIME;

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

        if (quantum_enabled) {
            quantum_keeper.set(delay);
            if (quantum_keeper.need_sync()) {
                quantum_keeper.sync();
                ++synchronization_count;
            }
        } else {
            wait(delay);
            ++synchronization_count;
        }

        return static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8) |
               (static_cast<uint32_t>(data[2]) << 16) |
               (static_cast<uint32_t>(data[3]) << 24);
    }

    void run() {
        wait(sc_core::SC_ZERO_TIME);

        if (quantum_enabled) quantum_keeper.reset();
        bus.set_trace_enabled(trace_enabled);

        std::cout << "\n=== SOC PERFORMANCE / TRACE BENCHMARK ===\n";
        std::cout << "Transactions: "
                  << transaction_count << "\n";

        std::cout << "Trace: "
                  << (trace_enabled ? "ON" : "OFF")
                  << "\n";
        std::cout << "Quantum keeper: "
                  << (quantum_enabled ? "ON" : "OFF");
        if (quantum_enabled) std::cout << " (" << quantum_ns << " ns)";
        std::cout << "\n";

        auto start = std::chrono::steady_clock::now();

        uint32_t checksum = 0;

        for (uint64_t i = 0; i < transaction_count; ++i) {

            uint64_t memory_address =
                generated::SRAM_BASE +
                ((i % 1024) * 4);

            write32(
                memory_address,
                static_cast<uint32_t>(i)
            );

            checksum ^=
                read32(memory_address);

            if ((i % 1000) == 0) {

                write32(
                    (generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET),
                    static_cast<uint32_t>(i)
                );

                checksum ^=
                    read32(generated::GPIO_BASE + generated::gpio::DIRECT_OUT_OFFSET);

                write32(
                    (generated::UART0_BASE + generated::uart0::CTRL_OFFSET),
                    0x1
                );

                checksum ^=
                    read32(generated::UART0_BASE + generated::uart0::STATUS_OFFSET);

                write32(
                    (generated::SPI_DEVICE_BASE + generated::spi_device::CFG_OFFSET),
                    0x1
                );

                checksum ^=
                    read32(generated::SPI_DEVICE_BASE + generated::spi_device::STATUS_OFFSET);
            }
        }

        if (quantum_enabled &&
            quantum_keeper.get_local_time() != sc_core::SC_ZERO_TIME) {
            quantum_keeper.sync();
            ++synchronization_count;
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
            << "Synchronization count: "
            << synchronization_count
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
    uint64_t quantum_ns = 0;

    if (argc > 1) {
        transactions = std::strtoull(argv[1], nullptr, 10);
    }

    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--trace") {
            trace = true;
        } else if (option == "--quantum-ns" && i + 1 < argc) {
            quantum_ns = std::strtoull(argv[++i], nullptr, 10);
        } else {
            std::cerr << "Unknown benchmark option: " << option << "\n";
            return 2;
        }
    }

    SocBench bench("soc_bench");

    bench.set_transaction_count(transactions);
    bench.set_trace_enabled(trace);
    if (quantum_ns != 0 && !bench.set_quantum_ns(quantum_ns)) {
        std::cerr << "Invalid quantum value\n";
        return 2;
    }
    bench.bind_soc();

    sc_core::sc_start();

    return 0;
}