#include "memorycardaccess.hpp"

#include <cstdint>
#include <cstring>

#include "savedata.hpp"

namespace {

// Retail's ((int)p >> 6) + 1) << 6 on the whole pointer: the next 64-byte boundary strictly after p.
char *PastNext64(char *pointer) {
    return pointer + (64 - reinterpret_cast<std::uintptr_t>(pointer) % 64);
}

} // namespace

PC_OVERRIDE void CMemoryCardAccess::SetBuff(char *buffer) {
    char *data;
    char *sum;
    u32   i;
    char  total;

    buffer = PastNext64(buffer);
    this->save_buffer = (CSaveData *) buffer;
    memcpy(this->save_buffer, SaveData, 0x131C0);
    this->save_buffer->ConvertConfig(&sys_config);
    char *version = (char *) this->save_buffer + 0x131C0;
    strcpy(version, this->version);
    this->check_sum = version + 0x20;
    data = (char *) this->save_buffer;
    sum = this->check_sum;
    memset(sum, 0, 0x4C7);
    total = 0;

    for (i = 0; i < 0x131C0; i++) {
        total += *data++;

        if ((int) i % 64 == 63) {
            *sum++ = total;
            total = 0;
        }
    }

    this->read_buffer = PastNext64(sum);
    this->load_buffer = this->read_buffer;
}
