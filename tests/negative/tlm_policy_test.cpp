#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>
#include <cstdint>

#include "../../model/memory/simple_memory.h"

class TlmPolicyTest : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<TlmPolicyTest> socket;

    SC_HAS_PROCESS(TlmPolicyTest);

    explicit TlmPolicyTest(sc_core::sc_module_name name)
        : sc_core::sc_module(name),
          socket("socket") {

        SC_THREAD(run);
    }

    int failures = 0;

private:
    bool check(
        const char* name,
        uint64_t address,
        unsigned int length,
        tlm::tlm_response_status expected,
        unsigned int streaming_width = 4,
        bool use_byte_enable = false,
        tlm::tlm_command command = tlm::TLM_READ_COMMAND) {

        unsigned char data[8] = {};

        unsigned char byte_enable[4] = {
            0xFF, 0xFF, 0xFF, 0xFF
        };

        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_streaming_width(streaming_width);

        if (use_byte_enable) {
            trans.set_byte_enable_ptr(byte_enable);
            trans.set_byte_enable_length(4);
        }

        socket->b_transport(trans, delay);

        bool passed =
            trans.get_response_status() == expected;

        std::cout
            << (passed ? "[PASS] " : "[FAIL] ")
            << name
            << " -> "
            << trans.get_response_string()
            << "\n";

        return passed;
    }

    void run() {
        std::cout << "\n=== TLM POLICY TEST ===\n";

        if (!check(
                "valid 4-byte access",
                0x00000000,
                4,
                tlm::TLM_OK_RESPONSE))
            failures++;

        if (!check(
                "unaligned address",
                0x00000001,
                4,
                tlm::TLM_ADDRESS_ERROR_RESPONSE))
            failures++;

        if (!check(
                "2-byte access",
                0x00000000,
                2,
                tlm::TLM_BURST_ERROR_RESPONSE))
            failures++;

        if (!check(
                "streaming width 2",
                0x00000000,
                4,
                tlm::TLM_BURST_ERROR_RESPONSE,
                2))
            failures++;

        if (!check(
                "byte enable",
                0x00000000,
                4,
                tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                4,
                true))
            failures++;

        if (!check(
                "unsupported command",
                0x00000000,
                4,
                tlm::TLM_COMMAND_ERROR_RESPONSE,
                4,
                false,
                tlm::TLM_IGNORE_COMMAND))
            failures++;

        if (!check(
                "out of bounds",
                0x00100000,
                4,
                tlm::TLM_ADDRESS_ERROR_RESPONSE))
            failures++;

        // Verify DMI range, permissions, boundary rejection, and that a
        // direct-memory write is visible through an ordinary TLM read.
        {
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_address(0x20);
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(true);

            tlm::tlm_dmi dmi;
            const bool available = socket->get_direct_mem_ptr(trans, dmi);
            const bool range_ok = available &&
                dmi.get_dmi_ptr() != nullptr &&
                dmi.get_start_address() == 0 &&
                dmi.get_end_address() == 1024 * 1024 - 1;
            const bool permissions_ok = available &&
                dmi.is_read_allowed() && dmi.is_write_allowed();

            if (range_ok && permissions_ok) {
                const uint64_t address = 0x20;
                unsigned char* direct = dmi.get_dmi_ptr() +
                    (address - dmi.get_start_address());
                direct[0] = 0x78;
                direct[1] = 0x56;
                direct[2] = 0x34;
                direct[3] = 0x12;

                unsigned char read_data[4] = {};
                tlm::tlm_generic_payload read_trans;
                read_trans.set_command(tlm::TLM_READ_COMMAND);
                read_trans.set_address(address);
                read_trans.set_data_ptr(read_data);
                read_trans.set_data_length(4);
                read_trans.set_streaming_width(4);
                read_trans.set_byte_enable_ptr(nullptr);
                sc_core::sc_time read_delay = sc_core::SC_ZERO_TIME;
                socket->b_transport(read_trans, read_delay);

                const bool consistent =
                    read_trans.get_response_status() == tlm::TLM_OK_RESPONSE &&
                    read_data[0] == 0x78 && read_data[1] == 0x56 &&
                    read_data[2] == 0x34 && read_data[3] == 0x12;
                if (consistent) {
                    std::cout << "[PASS] DMI direct write matches TLM read\n";
                } else {
                    std::cout << "[FAIL] DMI direct write matches TLM read\n";
                    failures++;
                }
            } else {
                std::cout << "[FAIL] DMI range and read/write permissions\n";
                failures++;
            }

            tlm::tlm_generic_payload outside;
            outside.set_command(tlm::TLM_READ_COMMAND);
            outside.set_address(1024 * 1024);
            outside.set_data_length(4);
            outside.set_streaming_width(4);
            tlm::tlm_dmi outside_dmi;
            if (!socket->get_direct_mem_ptr(outside, outside_dmi)) {
                std::cout << "[PASS] DMI rejects address past SRAM end\n";
            } else {
                std::cout << "[FAIL] DMI rejects address past SRAM end\n";
                failures++;
            }
        }

        std::cout << "\n";

        if (failures == 0) {
            std::cout << "TLM POLICY TEST: PASS\n";
        } else {
            std::cout
                << "TLM POLICY TEST: FAIL ("
                << failures
                << " failures)\n";
        }

        sc_core::sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    SimpleMemory memory("memory");
    TlmPolicyTest test("test");

    test.socket.bind(memory.socket);

    sc_core::sc_start();

    return test.failures == 0 ? 0 : 1;
}