#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include "../../generated/include/irq.h"

class IrqController : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(IrqController);
    tlm_utils::simple_target_socket<IrqController> socket;

    sc_core::sc_in<bool> timer_irq;
    sc_core::sc_in<bool> spi_irq;
    sc_core::sc_in<bool> gpio_irq;
    sc_core::sc_in<bool> uart_irq;

    sc_core::sc_out<bool> cpu_irq;

    enum Register : uint32_t {
        PENDING  = generated::irq::PENDING_OFFSET,
        ENABLE   = generated::irq::ENABLE_OFFSET,
        CLAIM    = generated::irq::CLAIM_OFFSET,
        COMPLETE = generated::irq::COMPLETE_OFFSET
    };

    IrqController(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket"),
          timer_irq("timer_irq"),
          spi_irq("spi_irq"),
          gpio_irq("gpio_irq"),
          uart_irq("uart_irq"),
          cpu_irq("cpu_irq") {

        socket.register_b_transport(
            this,
            &IrqController::b_transport
        );

        SC_METHOD(update_irq);

        sensitive << timer_irq;
        sensitive << spi_irq;
        sensitive << gpio_irq;
        sensitive << uart_irq;
        sensitive << update_event;

        cpu_irq.initialize(false);
    }

    void reset() {
        reset_requested = true;

        update_event.notify(
            sc_core::SC_ZERO_TIME
        );
    }

private:
    uint32_t pending = 0;
    uint32_t enable = 0;
    uint32_t claimed = 0;

    bool reset_requested = false;

    sc_core::sc_event update_event;

    static constexpr uint32_t TIMER_IRQ = 1u;
    static constexpr uint32_t SPI_IRQ   = 2u;
    static constexpr uint32_t GPIO_IRQ  = 4u;
    static constexpr uint32_t UART_IRQ  = 8u;

    void update_irq() {

        if (reset_requested) {
            reset_requested = false;

            pending = 0;
            enable = 0;
            claimed = 0;

            cpu_irq.write(false);
            return;
        }

        if (timer_irq.read() &&
            (claimed & TIMER_IRQ) == 0) {
            pending |= TIMER_IRQ;
        }

        if (spi_irq.read() &&
            (claimed & SPI_IRQ) == 0) {
            pending |= SPI_IRQ;
        }

        if (gpio_irq.read() &&
            (claimed & GPIO_IRQ) == 0) {
            pending |= GPIO_IRQ;
        }

        if (uart_irq.read() &&
            (claimed & UART_IRQ) == 0) {
            pending |= UART_IRQ;
        }

        uint32_t active =
            pending & enable & ~claimed;

        cpu_irq.write(active != 0);
    }

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        uint64_t address =
            trans.get_address();

        unsigned char* data =
            trans.get_data_ptr();

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

        if (address >= 0x10000) {
            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        if (trans.is_write()) {

            write_register(
                address,
                get_value(data)
            );

        }
        else if (trans.is_read()) {

            put_value(
                data,
                read_register(address)
            );

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

    uint32_t get_value(
        unsigned char* data) {

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
        uint32_t value) {

        switch (address) {

            case ENABLE:
                enable = value;
                break;

            case PENDING:
                pending &= ~value;
                claimed &= ~value;
                break;

            case COMPLETE:
                claimed &= ~value;
                break;

            default:
                break;
        }

        update_event.notify(
            sc_core::SC_ZERO_TIME
        );
    }

    uint32_t read_register(
        uint64_t address) {

        switch (address) {

            case PENDING:
                return pending;

            case ENABLE:
                return enable;

            case CLAIM:
                return claim_interrupt();

            case COMPLETE:
                return 0;

            default:
                return 0;
        }
    }

    uint32_t claim_interrupt() {

        uint32_t active =
            pending & enable & ~claimed;

        if (active & TIMER_IRQ) {
            claimed |= TIMER_IRQ;
            pending &= ~TIMER_IRQ;

            update_event.notify(
                sc_core::SC_ZERO_TIME
            );

            return TIMER_IRQ;
        }

        if (active & SPI_IRQ) {
            claimed |= SPI_IRQ;
            pending &= ~SPI_IRQ;

            update_event.notify(
                sc_core::SC_ZERO_TIME
            );

            return SPI_IRQ;
        }

        if (active & GPIO_IRQ) {
            claimed |= GPIO_IRQ;
            pending &= ~GPIO_IRQ;

            update_event.notify(
                sc_core::SC_ZERO_TIME
            );

            return GPIO_IRQ;
        }

        if (active & UART_IRQ) {
            claimed |= UART_IRQ;
            pending &= ~UART_IRQ;

            update_event.notify(
                sc_core::SC_ZERO_TIME
            );

            return UART_IRQ;
        }

        return 0;
    }
};