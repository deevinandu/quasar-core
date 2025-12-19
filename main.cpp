#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include "quasar_format.h"
#include "huffman.h"
#include "wavelet.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file> [saliency_radius]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = inputPath + ".qsr";
    float radius = (argc > 2) ? std::stof(argv[2]) : 1000.0f; // Default large radius

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

        // Vision Pipeline
        transform2D(img);
        applySaliency(img, radius);
        std::vector<uint8_t> quantized = quantize(img);
        
        // SCAN STRATEGY EXPLANATION:
        // We use a linear scan of the matrix here. A ZigZag scan (common in JPEG/DCT) 
        // is used because it groups low-frequency coefficients (which are usually larger) 
        // together and high-frequency coefficients (which are usually zero/small) together. 
        // This maximizes "runs" of zeros, making Huffman or RLE compression much more effective.
        // For Haar Wavelets, a simple linear scan still works well because the Detail 
        // coefficients are spatially grouped.

        HuffmanCodec codec;
        finalData = codec.compress(quantized);
        compressionFlags = 2; // 2 = Wavelet + Huffman
    } else {
        // Default Binary Pipeline
        std::ifstream inputFile(inputPath, std::ios::binary | std::ios::ate);
        if (!inputFile) {
            std::cerr << "Error: Could not open input file " << inputPath << std::endl;
            return 1;
        }
        originalSize = inputFile.tellg();
        inputFile.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileData(originalSize);
        inputFile.read(reinterpret_cast<char*>(fileData.data()), originalSize);
        inputFile.close();

        HuffmanCodec codec;
        finalData = codec.compress(fileData);
        compressionFlags = 1; // 1 = Huffman
    }

    // Prepare Header
    QuasarHeader header;
    header.magic[0] = 'Q'; header.magic[1] = 'S'; header.magic[2] = 'R'; header.magic[3] = '1';
    header.file_type = 0;
    header.original_size = originalSize;
    header.compression_flags = compressionFlags;

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
    std::cout << "Original size: " << originalSize << " bytes" << std::endl;
    std::cout << "Compressed size: " << finalData.size() << " bytes" << std::endl;

    return 0;
}

