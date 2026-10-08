#include "../include/uart0.h"

namespace generated {

namespace uart0 {

const RegisterMetadata* find_register(
    uint32_t offset) {

    for (unsigned int i = 0;
         i < REGISTER_COUNT;
         ++i) {

        if (REGISTERS[i].offset == offset)
            return &REGISTERS[i];
    }

    return nullptr;
}

bool allows_read(const RegisterMetadata& reg) {
    return reg.access == RegisterAccess::RO ||
           reg.access == RegisterAccess::RW ||
           reg.access == RegisterAccess::W1C;
}

bool allows_write(const RegisterMetadata& reg) {
    return reg.access == RegisterAccess::RW ||
           reg.access == RegisterAccess::WO ||
           reg.access == RegisterAccess::W1C;
}

}
}
