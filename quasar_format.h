#ifndef QUASAR_FORMAT_H
#define QUASAR_FORMAT_H

#include <cstdint>

// Ensure byte packing to avoid padding
// using __attribute__((packed)) as requested for GCC/Clang.
// #pragma pack(push, 1) is used for MSVC compatibility if needed, 
// but we will stick to the user's specific constraint first.

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

struct 
#ifndef _MSC_VER
__attribute__((packed)) 
#endif
QuasarHeader {
    char magic[4];          // 'Q', 'S', 'R', '1'
    uint8_t file_type;      // 0=Binary, 1=Text, etc.
    uint64_t original_size; // Original file size in bytes
    uint8_t compression_flags; // 0=None, 1=Zlib, etc.
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif // QUASAR_FORMAT_H
