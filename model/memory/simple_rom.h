#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>
#include <vector>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

class SimpleRom : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimpleRom> socket;

    SimpleRom(
        sc_core::sc_module_name name,
        std::size_t size = 64 * 1024,
        const std::string& image_path = std::string(),
        sc_core::sc_time latency = sc_core::sc_time(10, sc_core::SC_NS)
    )
        : sc_core::sc_module(name),
          socket("socket"),
          data(size, 0),
          latency_(latency) {

        load_firmware_image(image_path);

        socket.register_b_transport(
            this,
            &SimpleRom::b_transport
        );
    }

private:
    std::vector<std::uint8_t> data;
    sc_core::sc_time latency_;

    void load_firmware_image(const std::string& image_path) {
        std::fill(data.begin(), data.end(), static_cast<std::uint8_t>(0));

        if (image_path.empty()) {
            // Deterministic built-in firmware/data table used by default.
            const std::uint32_t values[] = {0x11223344, 0xAABBCCDD};
            for (std::size_t word = 0; word < 2; ++word) {
                for (std::size_t byte = 0; byte < 4; ++byte) {
                    data[word * 4 + byte] =
                        static_cast<std::uint8_t>((values[word] >> (byte * 8)) & 0xFF);
                }
            }
            return;
        }

        std::ifstream image(image_path, std::ios::binary | std::ios::ate);
        if (!image) {
            throw std::runtime_error("cannot open ROM image: " + image_path);
        }

        const std::streamoff image_size = image.tellg();
        if (image_size < 0 ||
            static_cast<std::uint64_t>(image_size) > data.size()) {
            throw std::runtime_error("ROM image exceeds configured ROM size: " + image_path);
        }

        image.seekg(0, std::ios::beg);
        if (image_size > 0 &&
            !image.read(
                reinterpret_cast<char*>(data.data()),
                static_cast<std::streamsize>(image_size))) {
            throw std::runtime_error("failed to read ROM image: " + image_path);
        }
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

        delay += latency_;

        trans.set_response_status(
            tlm::TLM_OK_RESPONSE
        );
    }
};