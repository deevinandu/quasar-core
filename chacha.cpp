#include "chacha.h"
#include <cstring>

// Rotate Left using std::rotl (C++20)
inline uint32_t rotl(uint32_t x, int n) {
    return std::rotl(x, n);
}

void ChaCha20::quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = rotl(d, 16);
    c += d; b ^= c; b = rotl(b, 12);
    a += b; d ^= a; d = rotl(d, 8);
    c += d; b ^= c; b = rotl(b, 7);
}

void ChaCha20::generate_block(uint32_t block[16], const uint32_t state[16]) {
    uint32_t x[16];
    std::memcpy(x, state, 16 * sizeof(uint32_t));

    // 20 rounds (10 iterations of 2 rounds each)
    for (int i = 0; i < 10; ++i) {
        // Column rounds
        quarter_round(x[0], x[4], x[8], x[12]);
        quarter_round(x[1], x[5], x[9], x[13]);
        quarter_round(x[2], x[6], x[10], x[14]);
        quarter_round(x[3], x[7], x[11], x[15]);
        // Diagonal rounds
        quarter_round(x[0], x[5], x[10], x[15]);
        quarter_round(x[1], x[6], x[11], x[12]);
        quarter_round(x[2], x[7], x[8], x[13]);
        quarter_round(x[3], x[4], x[9], x[14]);
    }

    for (int i = 0; i < 16; ++i) {
        block[i] = x[i] + state[i];
    }
}

void ChaCha20::process(std::vector<uint8_t>& data, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    uint32_t state[16];
    
    // Constants: "expand 32-byte k"
    state[0] = 0x61707865;
    state[1] = 0x3320646e;
    state[2] = 0x79622d32;
    state[3] = 0x6b206574;

    // Key
    std::memcpy(&state[4], key, 32);

    // Counter
    state[12] = counter;

    // Nonce
    std::memcpy(&state[13], nonce, 12);

    uint32_t block[16];
    size_t data_pos = 0;
    size_t data_len = data.size();

    while (data_pos < data_len) {
        generate_block(block, state);
        state[12]++; // Increment block counter

        uint8_t keystream[64];
        std::memcpy(keystream, block, 64);

        for (int i = 0; i < 64 && data_pos < data_len; ++i, ++data_pos) {
            data[data_pos] ^= keystream[i];
        }
    }
}
