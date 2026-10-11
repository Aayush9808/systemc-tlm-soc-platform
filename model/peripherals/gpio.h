#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <unordered_map>

#include "../../generated/include/gpio.h"
#include "../../generated/include/soc_memory_map.h"
#include <functional>

class Gpio : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Gpio> socket;

    sc_core::sc_out<bool> irq;

    using OutputCallback =
        std::function<void(uint32_t)>;

    enum Register : uint32_t {
        DATA_IN          = generated::gpio::DATA_IN_OFFSET,
        DIRECT_OUT       = generated::gpio::DIRECT_OUT_OFFSET,
        MASKED_OUT_LOWER = generated::gpio::MASKED_OUT_LOWER_OFFSET,
        MASKED_OUT_UPPER = generated::gpio::MASKED_OUT_UPPER_OFFSET,
        DIRECT_OE        = generated::gpio::DIRECT_OE_OFFSET,
        MASKED_OE_LOWER  = generated::gpio::MASKED_OE_LOWER_OFFSET,
        MASKED_OE_UPPER  = generated::gpio::MASKED_OE_UPPER_OFFSET,
        INTR_STATE       = generated::gpio::INTR_STATE_OFFSET,
        INTR_ENABLE      = generated::gpio::INTR_ENABLE_OFFSET,
        INTR_RISE        = generated::gpio::INTR_RISE_OFFSET,
        INTR_FALL        = generated::gpio::INTR_FALL_OFFSET
    };

    Gpio(
        sc_core::sc_module_name name,
        sc_core::sc_time transaction_latency =
            sc_core::sc_time(10, sc_core::SC_NS)
    )
        : sc_core::sc_module(name),
          socket("socket"),
          irq("irq"),
          transaction_latency_(transaction_latency) {

        socket.register_b_transport(
            this,
            &Gpio::b_transport
        );

        irq.initialize(false);
        reset_generic_registers();
    }

    void set_output_callback(OutputCallback callback) {
        output_callback = std::move(callback);
    }

    void set_input(uint32_t value) {
        uint32_t previous = data_in;

        data_in = value;

        uint32_t rising =
            (~previous) & data_in;

        uint32_t falling =
            previous & (~data_in);

        intr_state |= rising & intr_rise;
        intr_state |= falling & intr_fall;

        update_irq();
    }

    uint32_t get_output() const {
        return direct_out;
    }

    uint32_t get_output_enable() const {
        return output_enable;
    }

    void reset() {
        const uint32_t previous_output = direct_out;

        data_in = generated::gpio::DATA_IN_RESET;
        direct_out = generated::gpio::DIRECT_OUT_RESET;
        output_enable = generated::gpio::DIRECT_OE_RESET;
        intr_state = generated::gpio::INTR_STATE_RESET;
        intr_enable = generated::gpio::INTR_ENABLE_RESET;
        intr_rise = generated::gpio::INTR_RISE_RESET;
        intr_fall = generated::gpio::INTR_FALL_RESET;
        reset_generic_registers();

        // Keep external observers (for example, a GPIO-backed LED) in sync
        // when reset changes the output state.
        notify_output_change(previous_output, direct_out);
        irq.write(false);
    }

private:
    sc_core::sc_time transaction_latency_;

    uint32_t data_in = generated::gpio::DATA_IN_RESET;
    uint32_t direct_out = generated::gpio::DIRECT_OUT_RESET;
    uint32_t output_enable = generated::gpio::DIRECT_OE_RESET;

    uint32_t intr_state = generated::gpio::INTR_STATE_RESET;
    uint32_t intr_enable = generated::gpio::INTR_ENABLE_RESET;
    uint32_t intr_rise = generated::gpio::INTR_RISE_RESET;
    uint32_t intr_fall = generated::gpio::INTR_FALL_RESET;

    OutputCallback output_callback;

    // Preserve YAML-defined registers that do not have bespoke GPIO side
    // effects. This makes adding a register to gpio.yaml affect runtime state,
    // rather than only generated metadata and documentation.
    std::unordered_map<uint32_t, uint32_t> generic_registers_;

    void reset_generic_registers() {
        generic_registers_.clear();
        for (unsigned int i = 0; i < generated::gpio::REGISTER_COUNT; ++i) {
            const auto& reg = generated::gpio::REGISTERS[i];
            generic_registers_[reg.offset] = reg.reset;
        }
    }

    void notify_output_change(
        uint32_t previous,
        uint32_t current) {

        if (previous == current) {
            return;
        }

        if (output_callback) {
            output_callback(current);
        }
    }

    void update_irq() {
        bool active =
            (intr_state & intr_enable) != 0;

        irq.write(active);
    }

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        uint64_t address = trans.get_address();
        unsigned char* data = trans.get_data_ptr();

        trans.set_response_status(
            tlm::TLM_INCOMPLETE_RESPONSE
        );

        if (data == nullptr ||
            trans.get_data_length() != 4 ||
            trans.get_streaming_width() != 4 ||
            address % 4 != 0) {

            trans.set_response_status(
                tlm::TLM_GENERIC_ERROR_RESPONSE
            );
            return;
        }

        if (trans.get_byte_enable_ptr() != nullptr) {
            trans.set_response_status(
                tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE
            );
            return;
        }

        const auto* memory_range = generated::find_memory_range(
            generated::TargetId::GPIO
        );
        if (memory_range == nullptr || address >= memory_range->size) {
            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        const auto* register_metadata = generated::gpio::find_register(
            static_cast<uint32_t>(address)
        );

        if (register_metadata == nullptr) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if ((trans.is_read() &&
             !generated::gpio::allows_read(*register_metadata)) ||
            (trans.is_write() &&
             !generated::gpio::allows_write(*register_metadata))) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        if (trans.is_write()) {
            unsigned char masked_data[4];
            generated::store_register_value(
                masked_data,
                generated::apply_register_mask(
                    *register_metadata,
                    generated::load_register_value(data)
                )
            );
            write_register(address, masked_data);
        }
        else if (trans.is_read()) {
            read_register(address, data);
        }
        else {
            trans.set_response_status(
                tlm::TLM_COMMAND_ERROR_RESPONSE
            );
            return;
        }

        delay += transaction_latency_;

        trans.set_response_status(
            tlm::TLM_OK_RESPONSE
        );
    }

    uint32_t get_value(unsigned char* data) {
        return static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8) |
               (static_cast<uint32_t>(data[2]) << 16) |
               (static_cast<uint32_t>(data[3]) << 24);
    }

    void put_value(
        unsigned char* data,
        uint32_t value) {

        data[0] = value & 0xff;
        data[1] = (value >> 8) & 0xff;
        data[2] = (value >> 16) & 0xff;
        data[3] = (value >> 24) & 0xff;
    }

    void write_register(
        uint64_t address,
        unsigned char* data) {

        uint32_t value = get_value(data);

        switch (address) {

            case DIRECT_OUT: {
                uint32_t previous = direct_out;

                direct_out = value;

                notify_output_change(
                    previous,
                    direct_out
                );
                break;
            }

            case MASKED_OUT_LOWER: {
                uint32_t previous = direct_out;

                uint16_t mask = value >> 16;
                uint16_t bits = value & 0xffff;

                direct_out =
                    (direct_out &
                     ~static_cast<uint32_t>(mask)) |
                    (static_cast<uint32_t>(bits) & mask);

                notify_output_change(
                    previous,
                    direct_out
                );
                break;
            }

            case MASKED_OUT_UPPER: {
                uint32_t previous = direct_out;

                uint16_t mask = value >> 16;
                uint16_t bits = value & 0xffff;

                uint32_t mask32 =
                    static_cast<uint32_t>(mask) << 16;

                uint32_t bits32 =
                    static_cast<uint32_t>(bits) << 16;

                direct_out =
                    (direct_out & ~mask32) |
                    (bits32 & mask32);

                notify_output_change(
                    previous,
                    direct_out
                );

                break;
            }

            case DIRECT_OE:
                output_enable = value;
                break;

            case MASKED_OE_LOWER: {
                uint16_t mask = value >> 16;
                uint16_t bits = value & 0xffff;

                output_enable =
                    (output_enable &
                     ~static_cast<uint32_t>(mask)) |
                    (static_cast<uint32_t>(bits) & mask);

                break;
            }

            case MASKED_OE_UPPER: {
                uint16_t mask = value >> 16;
                uint16_t bits = value & 0xffff;

                uint32_t mask32 =
                    static_cast<uint32_t>(mask) << 16;

                uint32_t bits32 =
                    static_cast<uint32_t>(bits) << 16;

                output_enable =
                    (output_enable & ~mask32) |
                    (bits32 & mask32);

                break;
            }

            case INTR_STATE:
                // Write-1-to-clear
                intr_state &= ~value;
                update_irq();
                break;

            case INTR_ENABLE:
                intr_enable = value;
                update_irq();
                break;

            case INTR_RISE:
                intr_rise = value;
                break;

            case INTR_FALL:
                intr_fall = value;
                break;

            default: {
                auto it = generic_registers_.find(
                    static_cast<uint32_t>(address)
                );
                if (it != generic_registers_.end()) {
                    if (register_is_w1c(address)) {
                        it->second &= ~value;
                    } else {
                        it->second = value;
                    }
                }
                break;
            }
        }
    }

    bool register_is_w1c(uint64_t address) const {
        const auto* metadata = generated::gpio::find_register(
            static_cast<uint32_t>(address)
        );
        return metadata != nullptr &&
               metadata->access == generated::RegisterAccess::W1C;
    }

    void read_register(
        uint64_t address,
        unsigned char* data) {

        uint32_t value = 0;

        switch (address) {

            case DATA_IN:
                value = data_in;
                break;

            case DIRECT_OUT:
                value = direct_out;
                break;

            case DIRECT_OE:
                value = output_enable;
                break;

            case INTR_STATE:
                value = intr_state;
                break;

            case INTR_ENABLE:
                value = intr_enable;
                break;

            case INTR_RISE:
                value = intr_rise;
                break;

            case INTR_FALL:
                value = intr_fall;
                break;

            default: {
                auto it = generic_registers_.find(
                    static_cast<uint32_t>(address)
                );
                value = it == generic_registers_.end() ? 0 : it->second;
                break;
            }
        }

        put_value(data, value);
    }
};