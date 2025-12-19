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

void print_hex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << ": ";
    for (size_t i = 0; i < len; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::cout << std::dec << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [saliency_radius] [--encrypt]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = inputPath + ".qsr";
    
    // Simple arg parsing
    float radius = 1000.0f;
    bool do_encrypt = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--encrypt") do_encrypt = true;
        else {
            try { radius = std::stof(arg); } catch (...) {}
        }
    }

    std::vector<uint8_t> finalData;
    uint8_t compressionFlags = 0;
    uint64_t originalSize = 0;

    if (fs::path(inputPath).extension() == ".pgm") {
        std::cout << "Detected PGM image. Using Vision Module pipeline..." << std::endl;
        GrayImage img(0, 0);
        if (!loadPGM(inputPath, img)) {
            std::cerr << "Error loading PGM file" << std::endl;
            return 1;
        }
        originalSize = img.width * img.height;
        transform2D(img);
        applySaliency(img, radius);
        std::vector<uint8_t> quantized = quantize(img);
        HuffmanCodec codec;
        finalData = codec.compress(quantized);
        compressionFlags |= 0x02; // Bit 1: Wavelet
    } else {
        std::ifstream inputFile(inputPath, std::ios::binary | std::ios::ate);
        if (!inputFile) {
            std::cerr << "Error: Could not open input file" << std::endl;
            return 1;
        }
        originalSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileData(originalSize);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), originalSize);
        inputFile.close();

        HuffmanCodec codec;
        finalData = codec.compress(fileData);
        compressionFlags |= 0x01; // Bit 0: Huffman
    }

    // Prepare Header
    QuasarHeader header;
    std::memset(&header, 0, sizeof(header));
    header.magic[0] = 'Q'; header.magic[1] = 'S'; header.magic[2] = 'R'; header.magic[3] = '1';
    header.file_type = 0;
    header.original_size = originalSize;
    header.compression_flags = compressionFlags;

    // Encryption Step
    if (do_encrypt) {
        std::cout << "Encrypting archive with ChaCha20..." << std::endl;
        
        uint8_t key[32];
        uint8_t nonce[12];
        
        std::random_device rd;
        for (int i = 0; i < 32; ++i) key[i] = rd() & 0xFF;
        for (int i = 0; i < 12; ++i) nonce[i] = rd() & 0xFF;

        print_hex("Generated Key (KEEP THIS SECURE)", key, 32);
        std::memcpy(header.nonce, nonce, 12);
        header.compression_flags |= 0x80; // Bit 7: Encrypted

        ChaCha20::process(finalData, key, nonce);
    }

    // Write Output
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Could not open output file" << std::endl;
        return 1;
    }
    outputFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    outputFile.write(reinterpret_cast<const char*>(finalData.data()), finalData.size());
    outputFile.close();

    std::cout << "Successfully wrote to " << outputPath << std::endl;
    std::cout << "Final Archive Size: " << fs::file_size(outputPath) << " bytes" << std::endl;

    return 0;
}

