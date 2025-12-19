#include <iostream>
#include <vector>
#include <cassert>
#include "huffman.h"

int main() {
    HuffmanCodec codec;
    
    std::string testStr = "Huffman coding is a lossless data compression algorithm.";
    std::vector<uint8_t> input(testStr.begin(), testStr.end());

    std::cout << "Original: " << testStr << " (" << input.size() << " bytes)" << std::endl;

    auto compressed = codec.compress(input);
    std::cout << "Compressed: " << compressed.size() << " bytes (including 1024B freq table)" << std::endl;

    auto decompressed = codec.decompress(compressed);
    std::string result(decompressed.begin(), decompressed.end());

    std::cout << "Decompressed: " << result << std::endl;

    assert(testStr == result);
    std::cout << "Verification SUCCESSFUL!" << std::endl;

    return 0;
}
