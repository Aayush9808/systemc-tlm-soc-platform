#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <deque>
#include <iostream>

class Uart : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Uart> socket;
    sc_core::sc_out<bool> irq;

    enum Register : uint32_t {
        CTRL        = 0x00,
        STATUS      = 0x04,
        RDATA       = 0x08,
        WDATA       = 0x0C,
        FIFO_CTRL   = 0x10,
        FIFO_STATUS = 0x14,
        INTR_STATE  = 0x18,
        INTR_ENABLE = 0x1C
    };

    enum ControlBits : uint32_t {
        TX_ENABLE = 1 << 0,
        RX_ENABLE = 1 << 1
    };

    enum StatusBits : uint32_t {
        TX_EMPTY = 1 << 0,
        RX_EMPTY = 1 << 1
    };

    enum InterruptBits : uint32_t {
        TX_INTERRUPT = 1 << 0,
        RX_INTERRUPT = 1 << 1
    };

    Uart(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket"),
          irq("irq") {

        socket.register_b_transport(
            this,
            &Uart::b_transport
        );

        irq.initialize(false);
    }

    void reset() {
        ctrl = 0;
        intr_state = 0;
        intr_enable = 0;

        tx_fifo.clear();
        rx_fifo.clear();

        irq.write(false);
    }

    void inject_rx(uint8_t value) {
        if ((ctrl & RX_ENABLE) == 0) {
            return;
        }

        if (rx_fifo.size() >= FIFO_SIZE) {
            return;
        }

        rx_fifo.push_back(value);

        intr_state |= RX_INTERRUPT;
        update_irq();
    }

private:
    uint32_t ctrl = 0;
    uint32_t intr_state = 0;
    uint32_t intr_enable = 0;

    std::deque<uint8_t> tx_fifo;
    std::deque<uint8_t> rx_fifo;

    static constexpr size_t FIFO_SIZE = 16;

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        uint64_t address = trans.get_address();
        unsigned int length = trans.get_data_length();
        unsigned char* data = trans.get_data_ptr();

        trans.set_response_status(
            tlm::TLM_INCOMPLETE_RESPONSE
        );

        if (data == nullptr ||
            length != 4 ||
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

    uint32_t read_value(unsigned char* data) {
        return static_cast<uint32_t>(data[0]) |
               (static_cast<uint32_t>(data[1]) << 8) |
               (static_cast<uint32_t>(data[2]) << 16) |
               (static_cast<uint32_t>(data[3]) << 24);
    }

    void write_value(
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

        uint32_t value = read_value(data);

        switch (address) {

            case CTRL:
                ctrl = value;
                update_irq();
                break;

            case WDATA:
                write_tx(value);
                break;

            case FIFO_CTRL:
                if (value & 0x1) {
                    tx_fifo.clear();
                }

                if (value & 0x2) {
                    rx_fifo.clear();
                }

                update_irq();
                break;

            case INTR_STATE:
                // Write-1-to-clear
                intr_state &= ~value;
                update_irq();
                break;

            case INTR_ENABLE:
                intr_enable = value;
                update_irq();
                break;

            default:
                break;
        }
    }

    void write_tx(uint32_t value) {

        if ((ctrl & TX_ENABLE) == 0) {
            return;
        }

        if (tx_fifo.size() >= FIFO_SIZE) {
            return;
        }

        uint8_t byte =
            static_cast<uint8_t>(value & 0xff);

        tx_fifo.push_back(byte);

        std::cout
            << static_cast<char>(byte)
            << std::flush;

        // Host consumes the byte immediately.
        tx_fifo.pop_front();

        intr_state |= TX_INTERRUPT;

        update_irq();
    }

    void read_register(
        uint64_t address,
        unsigned char* data) {

        uint32_t value = 0;

        switch (address) {

            case CTRL:
                value = ctrl;
                break;

            case STATUS:
                if (tx_fifo.empty()) {
                    value |= TX_EMPTY;
                }

                if (rx_fifo.empty()) {
                    value |= RX_EMPTY;
                }
                break;

            case RDATA:
                if (!rx_fifo.empty()) {
                    value = rx_fifo.front();
                    rx_fifo.pop_front();

                    if (rx_fifo.empty()) {
                        intr_state &= ~RX_INTERRUPT;
                    }

                    update_irq();
                }
                break;

            case FIFO_CTRL:
                value = 0;
                break;

            case FIFO_STATUS:
                value =
                    static_cast<uint32_t>(tx_fifo.size()) |
                    (static_cast<uint32_t>(rx_fifo.size()) << 16);
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

        write_value(data, value);
    }

    void update_irq() {
        bool active =
            (intr_state & intr_enable) != 0;

        irq.write(active);
    }
};