#ifndef CHACHA_H
#define CHACHA_H

#include <vector>
#include <cstdint>
#include <bit>

class ChaCha20 {
public:
    // Encrypts/Decrypts data in-place using the given key and nonce.
    // key: 32 bytes (256-bit)
    // nonce: 12 bytes (96-bit RFC 7539 format)
    // counter: Initial block counter (usually 0 or 1)
    static void process(std::vector<uint8_t>& data, const uint8_t key[32], const uint8_t nonce[12], uint32_t counter = 1);

private:
    static void quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d);
    static void generate_block(uint32_t block[16], const uint32_t state[16]);
};

#endif // CHACHA_H
