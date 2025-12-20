#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <random>
#include "quasar_format.h"
#include "huffman.h"
#include "wavelet.h"
#include "chacha.h"
#include <cstring>

namespace fs = std::filesystem;

// --- Helper Functions ---
void print_hex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::cout << std::dec << std::endl;
}

uint8_t hex_char_to_byte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

void parse_hex_key(const std::string& hex, uint8_t* key) {
    for (size_t i = 0; i < 32; ++i) {
        key[i] = (hex_char_to_byte(hex[2*i]) << 4) | hex_char_to_byte(hex[2*i+1]);
    }
}

// --- MAIN ---
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [--unpack] [options...]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    bool mode_unpack = false;
    float radius = 1000.0f;
    float scale = 10.0f; // Default low precision
    bool do_encrypt = false;

    // Argument Parsing
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--unpack") mode_unpack = true;
        else if (arg == "--encrypt") do_encrypt = true;
        else if (arg == "--scale" && i + 1 < argc) scale = std::stof(argv[++i]);
        else { try { radius = std::stof(arg); } catch (...) {} }
    }

    // ==========================================
    //               PACK MODE
    // ==========================================
    if (!mode_unpack) {
        std::string outputPath = inputPath + ".qsr";
        std::vector<uint8_t> finalData;
        uint8_t compressionFlags = 0;
        uint64_t originalSize = 0;
        uint16_t width = 0, height = 0;

        if (fs::path(inputPath).extension() == ".pgm") {
            std::cout << "Packing PGM (Vision Pipeline)... Scale: " << scale << std::endl;
            GrayImage img(0, 0);
            if (!loadPGM(inputPath, img)) {
                std::cerr << "Error loading PGM." << std::endl; return 1;
            }
            
            originalSize = img.width * img.height;
            width = img.width;
            height = img.height;
            
            transform2D(img);
            applySaliency(img, radius);
            std::vector<uint8_t> quantized = quantize(img, scale); // Uses 32-bit
            HuffmanCodec codec;
            finalData = codec.compress(quantized);
            compressionFlags |= 0x02; // Wavelet Flag
        } else {
            std::ifstream inputFile(inputPath, std::ios::binary | std::ios::ate);
            if (!inputFile) { std::cerr << "File not found." << std::endl; return 1; }
            originalSize = inputFile.tellg();
            inputFile.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(originalSize);
            inputFile.read(reinterpret_cast<char*>(fileData.data()), originalSize);
            HuffmanCodec codec;
            finalData = codec.compress(fileData);
            compressionFlags |= 0x01; // Huffman Flag
        }

        // Prepare Header
        QuasarHeader header;
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, "QSR1", 4);
        header.original_size = originalSize;
        header.compression_flags = compressionFlags;
        header.scale = scale;
        header.width = width;
        header.height = height;

        // Encryption
        if (do_encrypt) {
            std::cout << "Encrypting with ChaCha20..." << std::endl;
            uint8_t key[32], nonce[12];
            std::random_device rd;
            for (auto& k : key) k = rd() & 0xFF;
            for (auto& n : nonce) n = rd() & 0xFF;
            
            print_hex("Generated Key (COPY THIS)", key, 32);
            std::memcpy(header.nonce, nonce, 12);
            header.compression_flags |= 0x80; // Encrypted Flag
            ChaCha20::process(finalData, key, nonce);
        }

        std::ofstream out(outputPath, std::ios::binary);
        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(finalData.data()), finalData.size());
        std::cout << "Saved: " << outputPath << " (" << finalData.size() << " bytes)" << std::endl;
    } 
    
    // ==========================================
    //             UNPACK MODE
    // ==========================================
    else {
        std::cout << "Unpacking " << inputPath << "..." << std::endl;
        std::ifstream in(inputPath, std::ios::binary);
        if (!in) { std::cerr << "File not found." << std::endl; return 1; }

        QuasarHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (std::strncmp(header.magic, "QSR1", 4) != 0) {
            std::cerr << "Invalid Magic Bytes!" << std::endl; return 1;
        }

        std::cout << "Detected Scale: " << header.scale << std::endl;
        std::cout << "Original Size: " << header.original_size << " bytes" << std::endl;

        // Read Payload
        std::vector<uint8_t> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        // Decrypt
        if (header.compression_flags & 0x80) {
            std::cout << "[SECURITY] File is Encrypted." << std::endl;
            std::cout << "Enter 32-byte Key (Hex): ";
            std::string hexKey;
            std::cin >> hexKey;
            uint8_t key[32];
            parse_hex_key(hexKey, key);
            ChaCha20::process(payload, key, header.nonce);
        }

        // Decompress Huffman
        HuffmanCodec codec;
        std::vector<uint8_t> decompressed = codec.decompress(payload);

        // Vision Reconstruct
        if (header.compression_flags & 0x02) {
            // Use width/height from header to rebuild image buffer
            GrayImage img(header.width, header.height);
            
            // 32-bit Dequantization
            dequantize(decompressed, img, header.scale); 
            
            inverseTransform2D(img);
            
            std::string outName = inputPath + ".recovered.pgm";
            savePGM(outName, img);
            std::cout << "Recovered Image: " << outName << std::endl;
        } else {
            std::string outName = inputPath + ".recovered";
            std::ofstream out(outName, std::ios::binary);
            out.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
            std::cout << "Recovered File: " << outName << std::endl;
        }
    }

    return 0;
}