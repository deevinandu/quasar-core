#include "chacha.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>

void print_bytes(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << ": ";
    for (uint8_t b : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    std::string plaintext = "ChaCha20 is a stream cipher developed by Daniel J. Bernstein.";
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());

    uint8_t key[32] = {0}; // Zero key for deterministic test
    uint8_t nonce[12] = {0}; // Zero nonce for deterministic test
    for (int i = 0; i < 32; ++i) key[i] = i;
    for (int i = 0; i < 12; ++i) nonce[i] = i + 100;

    std::cout << "Original: " << plaintext << std::endl;

    // Encryption
    ChaCha20::process(data, key, nonce);
    print_bytes("Encrypted", data);

    // Decryption (same as encryption for stream ciphers)
    ChaCha20::process(data, key, nonce);
    
    std::string result(data.begin(), data.end());
    std::cout << "Decrypted: " << result << std::endl;

    assert(plaintext == result);
    std::cout << "Encryption Verification SUCCESSFUL!" << std::endl;

    return 0;
}
