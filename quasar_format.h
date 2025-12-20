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
    uint8_t file_type;      // 0=Binary, 1=Text, 2=PGM, etc.
    uint64_t original_size; // Original file size in bytes
    uint8_t compression_flags; // Bit 0: Huffman, Bit 1: Wavelet, Bit 7: Encrypted
    uint8_t nonce[12];      // 96-bit Nonce for ChaCha20
    float scale;            // Quantization scale factor
    uint16_t width;   
    uint16_t height; 
};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#endif // QUASAR_FORMAT_H
