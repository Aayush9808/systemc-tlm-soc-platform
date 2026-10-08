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