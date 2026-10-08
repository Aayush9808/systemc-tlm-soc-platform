#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

class RvTimer : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<RvTimer> socket;
    sc_core::sc_out<bool> irq;

    enum Register : uint32_t {
        CTRL          = 0x00,
        CFG0          = 0x04,
        TIMER_V_LOWER = 0x08,
        TIMER_V_UPPER = 0x0C,
        COMPARE_LOWER = 0x10,
        COMPARE_UPPER = 0x14,
        INTR_STATE    = 0x18,
        INTR_ENABLE   = 0x1C
    };

    RvTimer(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket"),
          irq("irq") {

        socket.register_b_transport(
            this,
            &RvTimer::b_transport
        );

        irq.initialize(false);

        SC_METHOD(timer_process);
        sensitive << compare_event;
        sensitive << irq_update_event;
    }

    void reset() {
        reset_requested = true;

        compare_event.cancel();
        irq_update_event.notify(
            sc_core::SC_ZERO_TIME
        );
    }

private:
    uint32_t ctrl = 0;
    uint32_t cfg0 = 0;

    uint64_t timer_value = 0;
    uint64_t compare_value = 0;

    uint32_t intr_state = 0;
    uint32_t intr_enable = 0;

    bool timer_running = false;

    sc_core::sc_time timer_base_time =
        sc_core::SC_ZERO_TIME;

    uint64_t timer_base_value = 0;

    uint64_t read_latch = 0;
    bool read_latch_valid = false;

    bool reset_requested = false;

    sc_core::sc_event compare_event;
    sc_core::sc_event irq_update_event;

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

        if (address >= 0x1000) {
            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        if (trans.is_write()) {
            write_register(address, data);
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

        delay += sc_core::sc_time(
            10,
            sc_core::SC_NS
        );

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

    uint64_t get_step() const {
        uint64_t step =
            static_cast<uint64_t>(cfg0 & 0xFFFF);

        return step == 0 ? 1 : step;
    }

    uint64_t get_prescaler() const {
        uint64_t prescaler =
            static_cast<uint64_t>((cfg0 >> 16) & 0xFFFF);

        return prescaler == 0 ? 1 : prescaler;
    }

    uint64_t current_timer_value() const {
        if (!timer_running) {
            return timer_base_value;
        }

        sc_core::sc_time elapsed =
            sc_core::sc_time_stamp() -
            timer_base_time;

        double elapsed_ns =
            elapsed.to_seconds() * 1.0e9;

        double tick_ns =
            10.0 *
            static_cast<double>(get_prescaler());

        uint64_t ticks =
            static_cast<uint64_t>(
                elapsed_ns / tick_ns
            );

        return timer_base_value +
               ticks * get_step();
    }

    void sync_timer_value() {
        timer_value = current_timer_value();
    }

    void write_register(
        uint64_t address,
        unsigned char* data) {

        uint32_t value = get_value(data);

        switch (address) {

            case CTRL: {
                sync_timer_value();

                ctrl = value;

                bool new_running =
                    (ctrl & 0x1) != 0;

                if (new_running != timer_running) {
                    timer_running = new_running;

                    timer_base_value = timer_value;
                    timer_base_time =
                        sc_core::sc_time_stamp();
                }

                schedule_compare_event();

                irq_update_event.notify(
                    sc_core::SC_ZERO_TIME
                );
                break;
            }

            case CFG0:
                sync_timer_value();

                cfg0 = value;

                timer_base_value = timer_value;
                timer_base_time =
                    sc_core::sc_time_stamp();

                schedule_compare_event();
                break;

            case TIMER_V_LOWER:
                sync_timer_value();

                timer_value =
                    (timer_value &
                     0xFFFFFFFF00000000ULL) |
                    static_cast<uint64_t>(value);

                timer_base_value = timer_value;
                timer_base_time =
                    sc_core::sc_time_stamp();

                schedule_compare_event();
                break;

            case TIMER_V_UPPER:
                sync_timer_value();

                timer_value =
                    (timer_value &
                     0x00000000FFFFFFFFULL) |
                    (static_cast<uint64_t>(value) << 32);

                timer_base_value = timer_value;
                timer_base_time =
                    sc_core::sc_time_stamp();

                schedule_compare_event();
                break;

            case COMPARE_LOWER:
                compare_value =
                    (compare_value &
                     0xFFFFFFFF00000000ULL) |
                    static_cast<uint64_t>(value);

                schedule_compare_event();
                break;

            case COMPARE_UPPER:
                compare_value =
                    (compare_value &
                     0x00000000FFFFFFFFULL) |
                    (static_cast<uint64_t>(value) << 32);

                schedule_compare_event();
                break;

            case INTR_STATE:
                // Write-1-to-clear
                intr_state &= ~value;

                irq_update_event.notify(
                    sc_core::SC_ZERO_TIME
                );

                schedule_compare_event();
                break;

            case INTR_ENABLE:
                intr_enable = value;

                irq_update_event.notify(
                    sc_core::SC_ZERO_TIME
                );

                schedule_compare_event();
                break;

            default:
                break;
        }
    }

    void read_register(
        uint64_t address,
        unsigned char* data) {

        uint32_t value = 0;

        switch (address) {

            case CTRL:
                value = ctrl;
                break;

            case CFG0:
                value = cfg0;
                break;

            case TIMER_V_LOWER:
                read_latch =
                    current_timer_value();

                read_latch_valid = true;

                value =
                    static_cast<uint32_t>(
                        read_latch & 0xFFFFFFFFULL
                    );
                break;

            case TIMER_V_UPPER:
                if (read_latch_valid) {
                    value =
                        static_cast<uint32_t>(
                            read_latch >> 32
                        );

                    read_latch_valid = false;
                }
                else {
                    value =
                        static_cast<uint32_t>(
                            current_timer_value() >> 32
                        );
                }
                break;

            case COMPARE_LOWER:
                value =
                    static_cast<uint32_t>(
                        compare_value & 0xFFFFFFFFULL
                    );
                break;

            case COMPARE_UPPER:
                value =
                    static_cast<uint32_t>(
                        compare_value >> 32
                    );
                break;

            case INTR_STATE:
                value = intr_state;
                break;

            case INTR_ENABLE:
                value = intr_enable;
                break;

            default:
                value = 0;
                break;
        }

        put_value(data, value);
    }

    void schedule_compare_event() {
        compare_event.cancel();

        if (!timer_running ||
            (intr_enable & 0x1) == 0) {
            return;
        }

        uint64_t current =
            current_timer_value();

        if (current >= compare_value) {
            compare_event.notify(
                sc_core::SC_ZERO_TIME
            );
            return;
        }

        uint64_t remaining =
            compare_value - current;

        uint64_t step = get_step();

        uint64_t ticks =
            (remaining + step - 1) / step;

        double tick_ns =
            10.0 *
            static_cast<double>(
                get_prescaler()
            );

        double delay_ns =
            static_cast<double>(ticks) *
            tick_ns;

        compare_event.notify(
            sc_core::sc_time(
                delay_ns,
                sc_core::SC_NS
            )
        );
    }

    void timer_process() {

        if (reset_requested) {
            reset_requested = false;

            compare_event.cancel();

            ctrl = 0;
            cfg0 = 0;

            timer_value = 0;
            compare_value = 0;

            intr_state = 0;
            intr_enable = 0;

            timer_running = false;

            timer_base_time =
                sc_core::sc_time_stamp();

            timer_base_value = 0;

            read_latch = 0;
            read_latch_valid = false;

            irq.write(false);
            return;
        }

        if (!timer_running ||
            (intr_enable & 0x1) == 0) {

            irq.write(false);
            return;
        }

        sync_timer_value();

        if (timer_value >= compare_value) {
            timer_value = compare_value;

            timer_base_value = timer_value;
            timer_base_time =
                sc_core::sc_time_stamp();

            intr_state |= 0x1;

            irq.write(true);
            return;
        }

        irq.write(false);
    }
};