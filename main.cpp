#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include "quasar_format.h"

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

    std::cout << "Input file: " << inputPath << " (" << fileSize << " bytes)" << std::endl;

    // 2. Prepare Header
    QuasarHeader header;
    header.magic[0] = 'Q';
    header.magic[1] = 'S';
    header.magic[2] = 'R';
    header.magic[3] = '1';
    header.file_type = 0; // Default to binary
    header.original_size = static_cast<uint64_t>(fileSize);
    header.compression_flags = 0; // No compression

    // 3. Write Output File
    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Error: Could not open output file " << outputPath << std::endl;
        return 1;
    }

    // EXPLANATION OF REINTERPRET_CAST:
    // reinterpret_cast<const char*>(&header) treats the memory address of the struct 
    // as a raw char pointer. This allows us to write the exact bytes of the struct 
    // directly to the file stream. Because the struct is packed, there is no padding between 
    // fields, ensuring the file format is compact and predictable.
    outputFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // 4. Stream Data
    // For large files, we should stream in chunks. For simplicity here with typical small files,
    // we can use a buffer or stream buffer iterators. Let's use a 4KB buffer.
    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    while (inputFile.read(buffer, BUFFER_SIZE)) {
        outputFile.write(buffer, inputFile.gcount());
    }
    // Write remaining bytes
    if (inputFile.gcount() > 0) {
        outputFile.write(buffer, inputFile.gcount());
    }

    std::cout << "Successfully wrote to " << outputPath << std::endl;
    std::cout << "Header size: " << sizeof(header) << " bytes" << std::endl;
    std::cout << "Total output size: " << fs::file_size(outputPath) << " bytes" << std::endl;

    return 0;
}
