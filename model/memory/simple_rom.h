#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <vector>

class SimpleRom : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimpleRom> socket;

    SimpleRom(
        sc_core::sc_module_name name,
        std::size_t size = 64 * 1024
    )
        : sc_core::sc_module(name),
          socket("socket"),
          data(size, 0) {

        load_firmware_image();

        socket.register_b_transport(
            this,
            &SimpleRom::b_transport
        );
    }

private:
    std::vector<std::uint8_t> data;

    void load_firmware_image() {
        // Deterministic firmware/data value at ROM address 0.
        uint32_t value = 0x11223344;

        data[0] = value & 0xff;
        data[1] = (value >> 8) & 0xff;
        data[2] = (value >> 16) & 0xff;
        data[3] = (value >> 24) & 0xff;

        // More test data for future firmware scenarios.
        uint32_t value2 = 0xAABBCCDD;

        data[4] = value2 & 0xff;
        data[5] = (value2 >> 8) & 0xff;
        data[6] = (value2 >> 16) & 0xff;
        data[7] = (value2 >> 24) & 0xff;
    }

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        const auto address = trans.get_address();
        const auto length = trans.get_data_length();

        if (length == 0 ||
            trans.get_data_ptr() == nullptr) {

            trans.set_response_status(
                tlm::TLM_GENERIC_ERROR_RESPONSE
            );
            return;
        }

        if (address % 4 != 0 ||
            length != 4 ||
            trans.get_streaming_width() != 4) {

            trans.set_response_status(
                tlm::TLM_BURST_ERROR_RESPONSE
            );
            return;
        }

        if (trans.get_byte_enable_ptr() != nullptr) {
            trans.set_response_status(
                tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE
            );
            return;
        }

        if (address > data.size() ||
            length > data.size() - address) {

            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        if (!trans.is_read()) {
            trans.set_response_status(
                tlm::TLM_COMMAND_ERROR_RESPONSE
            );
            return;
        }

        auto* bytes = trans.get_data_ptr();

        for (std::size_t i = 0; i < length; ++i) {
            bytes[i] = data[address + i];
        }

        delay += sc_core::sc_time(10, sc_core::SC_NS);

        trans.set_response_status(
            tlm::TLM_OK_RESPONSE
        );
    }
};