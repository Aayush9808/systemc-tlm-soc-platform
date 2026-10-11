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

class SimpleMemory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimpleMemory> socket;

    explicit SimpleMemory(
        sc_core::sc_module_name name,
        std::size_t size = 1024 * 1024,
        sc_core::sc_time latency = sc_core::sc_time(10, sc_core::SC_NS),
        const std::string& image_path = std::string())
        : SimpleMemory(name, size, latency, latency, image_path) {}

    SimpleMemory(
        sc_core::sc_module_name name,
        std::size_t size,
        sc_core::sc_time read_latency,
        sc_core::sc_time write_latency,
        const std::string& image_path = std::string())
        : sc_core::sc_module(name),
          socket("socket"),
          memory(size, 0),
          read_latency_(read_latency),
          write_latency_(write_latency) {

        load_initialization_image(image_path);

        socket.register_b_transport(
            this,
            &SimpleMemory::b_transport
        );

        socket.register_get_direct_mem_ptr(
            this,
            &SimpleMemory::get_direct_mem_ptr
        );
    }

    void reset() {
        std::fill(
            memory.begin(),
            memory.end(),
            static_cast<unsigned char>(0)
        );
    }

private:
    std::vector<unsigned char> memory;
    sc_core::sc_time read_latency_;
    sc_core::sc_time write_latency_;

    void load_initialization_image(const std::string& image_path) {
        if (image_path.empty()) return;

        std::ifstream image(image_path, std::ios::binary | std::ios::ate);
        if (!image) {
            throw std::runtime_error("cannot open SRAM initialization image: " + image_path);
        }
        const std::streamoff image_size = image.tellg();
        if (image_size < 0 || static_cast<uint64_t>(image_size) > memory.size()) {
            throw std::runtime_error("SRAM initialization image exceeds configured SRAM size: " + image_path);
        }
        image.seekg(0, std::ios::beg);
        if (image_size > 0 && !image.read(
                reinterpret_cast<char*>(memory.data()),
                static_cast<std::streamsize>(image_size))) {
            throw std::runtime_error("failed to read SRAM initialization image: " + image_path);
        }
    }

    void b_transport(
        tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay) {

        uint64_t address = trans.get_address();
        unsigned char* data = trans.get_data_ptr();
        unsigned int length = trans.get_data_length();

        if (length == 0 || data == nullptr) {
            trans.set_response_status(
                tlm::TLM_GENERIC_ERROR_RESPONSE
            );
            return;
        }

        if (length != 4) {
            trans.set_response_status(
                tlm::TLM_BURST_ERROR_RESPONSE
            );
            return;
        }

        if (address % 4 != 0) {
            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        if (trans.get_streaming_width() != 4) {
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

        if (address > memory.size() ||
            length > memory.size() - address) {
            trans.set_response_status(
                tlm::TLM_ADDRESS_ERROR_RESPONSE
            );
            return;
        }

        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::copy(
                memory.begin() + address,
                memory.begin() + address + length,
                data
            );
        }
        else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::copy(
                data,
                data + length,
                memory.begin() + address
            );
        }
        else {
            trans.set_response_status(
                tlm::TLM_COMMAND_ERROR_RESPONSE
            );
            return;
        }

        delay += trans.is_read() ? read_latency_ : write_latency_;

        trans.set_response_status(
            tlm::TLM_OK_RESPONSE
        );
    }

    bool get_direct_mem_ptr(
        tlm::tlm_generic_payload& trans,
        tlm::tlm_dmi& dmi_data) {

        uint64_t address = trans.get_address();

        if (address >= memory.size()) {
            dmi_data.set_start_address(0);
            dmi_data.set_end_address(0);
            dmi_data.set_dmi_ptr(nullptr);
            return false;
        }

        dmi_data.set_dmi_ptr(memory.data());
        dmi_data.set_start_address(0);
        dmi_data.set_end_address(memory.size() - 1);

        dmi_data.set_read_latency(read_latency_);
        dmi_data.set_write_latency(write_latency_);

        dmi_data.allow_read_write();

        return true;
    }
};