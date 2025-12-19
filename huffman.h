#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <memory>

class HuffmanCodec {
public:
    // Compresses input data using Static Huffman Coding.
    // The output includes 1024 bytes of frequency table (256 * 4 bytes) followed by bitstream.
    std::vector<uint8_t> compress(const std::vector<uint8_t>& input);

    // Decompresses data compressed by the compress function.
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& input);

private:
    struct Node {
        uint8_t ch;
        uint32_t freq;
        std::shared_ptr<Node> left, right;

        Node(uint8_t c, uint32_t f) : ch(c), freq(f), left(nullptr), right(nullptr) {}
        Node(uint32_t f, std::shared_ptr<Node> l, std::shared_ptr<Node> r) 
            : ch(0), freq(f), left(l), right(r) {}

        bool isLeaf() const { return !left && !right; }
    };

    struct Compare {
        bool operator()(const std::shared_ptr<Node>& l, const std::shared_ptr<Node>& r) {
            return l->freq > r->freq;
        }
    };

    std::shared_ptr<Node> buildTree(const std::vector<uint32_t>& frequencies);
    void generateCodes(const std::shared_ptr<Node>& root, const std::string& str, std::map<uint8_t, std::string>& huffmanCode);
};

#endif // HUFFMAN_H
