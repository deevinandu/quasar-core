#include "huffman.h"
#include <queue>
#include <iostream>
#include <cstring>

class BitWriter {
public:
    std::vector<uint8_t> data;
    uint8_t buffer = 0;
    int bitCount = 0;

    void writeBit(int bit) {
        buffer <<= 1;
        if (bit) buffer |= 1;
        bitCount++;
        if (bitCount == 8) {
            data.push_back(buffer);
            buffer = 0;
            bitCount = 0;
        }
    }

    void flush() {
        if (bitCount > 0) {
            buffer <<= (8 - bitCount);
            data.push_back(buffer);
        }
    }
};

class BitReader {
public:
    const std::vector<uint8_t>& data;
    size_t byteIdx;
    int bitIdx;

    BitReader(const std::vector<uint8_t>& d, size_t offset) : data(d), byteIdx(offset), bitIdx(7) {}

    int readBit() {
        if (byteIdx >= data.size()) return -1;
        int bit = (data[byteIdx] >> bitIdx) & 1;
        bitIdx--;
        if (bitIdx < 0) {
            bitIdx = 7;
            byteIdx++;
        }
        return bit;
    }
};

std::shared_ptr<HuffmanCodec::Node> HuffmanCodec::buildTree(const std::vector<uint32_t>& frequencies) {
    std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, Compare> pq;

    for (int i = 0; i < 256; ++i) {
        if (frequencies[i] > 0) {
            pq.push(std::make_shared<Node>(static_cast<uint8_t>(i), frequencies[i]));
        }
    }

    // Handle single character case
    if (pq.empty()) return nullptr;
    if (pq.size() == 1) {
        auto lone = pq.top();
        pq.pop();
        return std::make_shared<Node>(lone->freq, lone, nullptr);
    }

    while (pq.size() != 1) {
        auto left = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();
        uint32_t sum = left->freq + right->freq;
        pq.push(std::make_shared<Node>(sum, left, right));
    }

    return pq.top();
}

void HuffmanCodec::generateCodes(const std::shared_ptr<Node>& root, const std::string& str, std::map<uint8_t, std::string>& huffmanCode) {
    if (!root) return;

    if (root->isLeaf()) {
        huffmanCode[root->ch] = str.empty() ? "0" : str;
        return;
    }

    generateCodes(root->left, str + "0", huffmanCode);
    generateCodes(root->right, str + "1", huffmanCode);
}

std::vector<uint8_t> HuffmanCodec::compress(const std::vector<uint8_t>& input) {
    if (input.empty()) return {};

    // 1. Frequency Analysis
    std::vector<uint32_t> frequencies(256, 0);
    for (uint8_t b : input) {
        frequencies[b]++;
    }

    // 2. Build Tree and Codes
    auto root = buildTree(frequencies);
    std::map<uint8_t, std::string> huffmanCode;
    generateCodes(root, "", huffmanCode);

    // 3. Serialize Header (Frequency Table)
    std::vector<uint8_t> output;
    for (int i = 0; i < 256; ++i) {
        uint32_t freq = frequencies[i];
        output.push_back((freq >> 0) & 0xFF);
        output.push_back((freq >> 8) & 0xFF);
        output.push_back((freq >> 16) & 0xFF);
        output.push_back((freq >> 24) & 0xFF);
    }

    // 4. Encode Data
    BitWriter writer;
    for (uint8_t b : input) {
        for (char bit : huffmanCode[b]) {
            writer.writeBit(bit == '1');
        }
    }
    writer.flush();

    output.insert(output.end(), writer.data.begin(), writer.data.end());
    return output;
}

std::vector<uint8_t> HuffmanCodec::decompress(const std::vector<uint8_t>& input) {
    if (input.size() < 1024) return {};

    // 1. Read Frequency Table
    std::vector<uint32_t> frequencies(256);
    for (int i = 0; i < 256; ++i) {
        uint32_t freq = 0;
        freq |= static_cast<uint32_t>(input[i * 4 + 0]);
        freq |= static_cast<uint32_t>(input[i * 4 + 1]) << 8;
        freq |= static_cast<uint32_t>(input[i * 4 + 2]) << 16;
        freq |= static_cast<uint32_t>(input[i * 4 + 3]) << 24;
        frequencies[i] = freq;
    }

    uint64_t totalChars = 0;
    for (uint32_t f : frequencies) totalChars += f;
    if (totalChars == 0) return {};

    // 2. Rebuild Tree
    auto root = buildTree(frequencies);
    if (!root) return {};

    // 3. Decode Bitstream
    std::vector<uint8_t> output;
    BitReader reader(input, 1024);
    
    for (uint64_t i = 0; i < totalChars; ++i) {
        auto curr = root;
        while (!curr->isLeaf()) {
            int bit = reader.readBit();
            if (bit == -1) break;
            curr = (bit == 0) ? curr->left : curr->right;
            // Handle edge case of single character tree
            if (!curr) break; 
        }
        if (curr) output.push_back(curr->ch);
    }

    return output;
}
