#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

#include "spi_device.h"
#include <deque>

class SpiDevice : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SpiDevice> socket;
    sc_core::sc_out<bool> irq;

    enum Register : uint32_t {
        CONTROL = generated::spi_device::CONTROL_OFFSET,
        CFG     = generated::spi_device::CFG_OFFSET,
        STATUS  = generated::spi_device::STATUS_OFFSET,
        TX      = generated::spi_device::TX_OFFSET,
        RX      = generated::spi_device::RX_OFFSET
    };

    SpiDevice(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket"),
          irq("irq") {

        socket.register_b_transport(
            this,
            &SpiDevice::b_transport
        );

        irq.initialize(false);
    }

    void reset() {
        control = 0;
        cfg = 0;

        tx_fifo.clear();
        rx_fifo.clear();

        irq.write(false);
    }

private:
    uint32_t control = 0;
    uint32_t cfg = 0;

    std::deque<uint8_t> tx_fifo;
    std::deque<uint8_t> rx_fifo;

    static constexpr size_t FIFO_SIZE = 16;

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

        uint32_t offset =
            static_cast<uint32_t>(address);

        if (trans.is_write()) {

            write_register(
                offset,
                get_value(data)
            );

        }
        else if (trans.is_read()) {

            put_value(
                data,
                read_register(offset)
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

    uint32_t get_value(unsigned char* data) {

        return
            static_cast<uint32_t>(data[0]) |
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
        uint32_t address,
        uint32_t value) {

        switch (address) {

            case CONTROL:
                control = value;

                if ((control & 1) == 0) {
                    irq.write(false);
                }
                break;

            case CFG:
                cfg = value;
                break;

            case TX:
                write_tx(value);
                break;

            default:
                break;
        }
    }

    uint32_t read_register(
        uint32_t address) {

        switch (address) {

            case CONTROL:
                return control;

            case CFG:
                return cfg;

            case STATUS:
                return get_status();

            case RX:
                return read_rx();

            default:
                return 0;
        }
    }

    void write_tx(uint32_t value) {

        if ((control & 1) == 0) {
            return;
        }

        if (tx_fifo.size() >= FIFO_SIZE) {
            return;
        }

        uint8_t value8 =
            static_cast<uint8_t>(value & 0xff);

        tx_fifo.push_back(value8);

        uint8_t received;

        // CFG bit 0 selects simple loopback.
        if (cfg & 0x1) {
            received = value8;
        }
        else {
            received =
                static_cast<uint8_t>(value8 ^ 0xFF);
        }

        if (rx_fifo.size() < FIFO_SIZE) {
            rx_fifo.push_back(received);
        }

        irq.write(true);
    }

    uint32_t read_rx() {

        if (rx_fifo.empty()) {
            return 0;
        }

        uint8_t value =
            rx_fifo.front();

        rx_fifo.pop_front();

        if (rx_fifo.empty()) {
            irq.write(false);
        }

        return value;
    }

    uint32_t get_status() const {

        uint32_t status = 0;

        if (tx_fifo.size() < FIFO_SIZE) {
            status |= 1u << 0;
        }

        if (!rx_fifo.empty()) {
            status |= 1u << 1;
        }

        if (tx_fifo.empty()) {
            status |= 1u << 2;
        }

        if (rx_fifo.size() >= FIFO_SIZE) {
            status |= 1u << 3;
        }

        return status;
    }
};