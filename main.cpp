#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include "quasar_format.h"
#include "huffman.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = inputPath + ".qsr";

    // 1. Open Input File
    std::ifstream inputFile(inputPath, std::ios::binary | std::ios::ate);
    if (!inputFile) {
        std::cerr << "Error: Could not open input file " << inputPath << std::endl;
        return 1;
    }

    std::streamsize fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);

    std::vector<uint8_t> fileData(fileSize);
    if (!inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
        std::cerr << "Error reading file data" << std::endl;
        return 1;
    }
    inputFile.close();

    std::cout << "Original file size: " << fileSize << " bytes" << std::endl;

    // 2. Compress Data using Librarian (Huffman)
    HuffmanCodec codec;
    std::vector<uint8_t> compressedData = codec.compress(fileData);
    
    std::cout << "Compressed size (with freq table): " << compressedData.size() << " bytes" << std::endl;

    // 3. Prepare Header
    QuasarHeader header;
    header.magic[0] = 'Q';
    header.magic[1] = 'S';
    header.magic[2] = 'R';
    header.magic[3] = '1';
    header.file_type = 0; 
    header.original_size = static_cast<uint64_t>(fileSize);
    header.compression_flags = 1; // 1 = Huffman

    // 4. Write Output File
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Could not open output file " << outputPath << std::endl;
        return 1;
    }

    outputFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
    outputFile.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
    outputFile.close();

    std::cout << "Successfully wrote to " << outputPath << std::endl;
    std::cout << "Header size: " << sizeof(header) << " bytes" << std::endl;
    std::cout << "Total output size: " << fs::file_size(outputPath) << " bytes" << std::endl;

    return 0;
}

