#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstdint>
#include <iostream>

#include "../../generated/include/soc_memory_map.h"

class SimpleInterconnect : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimpleInterconnect> target_socket;

    tlm_utils::simple_initiator_socket<SimpleInterconnect> rom_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> memory_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> uart_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> gpio_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> timer_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> spi_socket;
    tlm_utils::simple_initiator_socket<SimpleInterconnect> irq_socket;

    SimpleInterconnect(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          target_socket("target_socket"),
          rom_socket("rom_socket"),
          memory_socket("memory_socket"),
          uart_socket("uart_socket"),
          gpio_socket("gpio_socket"),
          timer_socket("timer_socket"),
          spi_socket("spi_socket"),
          irq_socket("irq_socket") {

        target_socket.register_b_transport(
            this,
            &SimpleInterconnect::b_transport
        );

        target_socket.register_get_direct_mem_ptr(
            this,
            &SimpleInterconnect::get_direct_mem_ptr
        );
    }

    void set_trace_enabled(bool enabled) {
        trace_enabled_ = enabled;
    }

    bool trace_enabled() const {
        return trace_enabled_;
    }

    void reset_stats() {
        total_transactions_ = 0;
        total_bytes_ = 0;

        for (unsigned int i = 0;
             i < generated::MEMORY_MAP_COUNT;
             ++i) {

            transaction_count_[i] = 0;
            byte_count_[i] = 0;
        }
    }

    uint64_t total_transactions() const {
        return total_transactions_;
    }

    uint64_t total_bytes() const {
        return total_bytes_;
    }

    uint64_t target_transactions(
        generated::TargetId target) const {

        int index = target_index(target);

        if (index < 0) {
            return 0;
        }

        return transaction_count_[index];
    }

    uint64_t target_bytes(
        generated::TargetId target) const {

        int index = target_index(target);

        if (index < 0) {
            return 0;
        }

        return byte_count_[index];
    }

private:
    bool trace_enabled_ = false;

    uint64_t total_transactions_ = 0;
    uint64_t total_bytes_ = 0;

    uint64_t transaction_count_[
        generated::MEMORY_MAP_COUNT
    ] = {};

    uint64_t byte_count_[
        generated::MEMORY_MAP_COUNT
    ] = {};

    int target_index(
        generated::TargetId target) const {

        for (unsigned int i = 0;
             i < generated::MEMORY_MAP_COUNT;
             ++i) {

            if (generated::MEMORY_MAP[i].target == target) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    const char* target_name(
        generated::TargetId target) const {

        for (unsigned int i = 0;
             i < generated::MEMORY_MAP_COUNT;
             ++i) {

            if (generated::MEMORY_MAP[i].target == target) {
                return generated::MEMORY_MAP[i].name;
            }
        }

        return "UNKNOWN";
    }

    bool find_range(
        uint64_t address,
        uint64_t length,
        const generated::MemoryRange*& range) {

        for (unsigned int i = 0;
             i < generated::MEMORY_MAP_COUNT;
             ++i) {

            const auto& current = generated::MEMORY_MAP[i];

            uint64_t end =
                current.base + current.size;

            if (address >= current.base &&
                address < end) {

                if (length > current.size ||
                    address > end - length) {
                    return false;
                }

                range = &current;
                return true;
            }
        }

        return false;
    }

    void record_transaction(
        const generated::MemoryRange& range,
        tlm::tlm_generic_payload& trans,
        const sc_core::sc_time& delay) {

        int index = target_index(range.target);

        if (index >= 0) {
            transaction_count_[index]++;
            byte_count_[index] += trans.get_data_length();
        }

        total_transactions_++;
        total_bytes_ += trans.get_data_length();

        if (!trace_enabled_) {
            return;
        }

        const char* command = "UNKNOWN";

        if (trans.is_read()) {
            command = "READ";
        }
        else if (trans.is_write()) {
            command = "WRITE";
        }

        std::cout
            << "[TRACE] "
            << "time=" << sc_core::sc_time_stamp()
            << " initiator=CPU"
            << " command=" << command
            << " address=0x"
            << std::hex
            << trans.get_address()
            << std::dec
            << " length=" << trans.get_data_length()
            << " target=" << target_name(range.target)
            << " response="
            << trans.get_response_string()
            << " delay=" << delay
            << "\n";
    }

    void route_to_target(
        const generated::MemoryRange& range,
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        uint64_t original_address =
            trans.get_address();

        trans.set_address(
            original_address - range.base
        );

        switch (range.target) {

            case generated::TargetId::ROM:
                rom_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::SRAM:
                memory_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::UART0:
                uart_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::GPIO:
                gpio_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::RV_TIMER:
                timer_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::SPI_DEVICE:
                spi_socket->b_transport(trans, delay);
                break;

            case generated::TargetId::IRQ:
                irq_socket->b_transport(trans, delay);
                break;
        }

        trans.set_address(original_address);
    }

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        const generated::MemoryRange* range = nullptr;

        if (!find_range(
                trans.get_address(),
                trans.get_data_length(),
                range)) {

            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );

            if (trace_enabled_) {
                std::cout
                    << "[TRACE] "
                    << "time=" << sc_core::sc_time_stamp()
                    << " initiator=CPU"
                    << " command=INVALID"
                    << " address=0x"
                    << std::hex
                    << trans.get_address()
                    << std::dec
                    << " length="
                    << trans.get_data_length()
                    << " target=UNMAPPED"
                    << " response="
                    << trans.get_response_string()
                    << "\n";
            }

            return;
        }

        route_to_target(
            *range,
            trans,
            delay
        );

        record_transaction(
            *range,
            trans,
            delay
        );
    }

    bool get_direct_mem_ptr(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_dmi& dmi_data) {

        const generated::MemoryRange* range = nullptr;

        if (!find_range(
                trans.get_address(),
                1,
                range)) {

            return false;
        }

        if (range->target !=
            generated::TargetId::SRAM) {

            return false;
        }

        uint64_t original_address =
            trans.get_address();

        trans.set_address(
            original_address - range->base
        );

        bool result =
            memory_socket->get_direct_mem_ptr(
                trans,
                dmi_data
            );

        trans.set_address(
            original_address
        );

        if (!result) {
            return false;
        }

        dmi_data.set_start_address(
            dmi_data.get_start_address()
            + range->base
        );

        dmi_data.set_end_address(
            dmi_data.get_end_address()
            + range->base
        );

        return true;
    }
};