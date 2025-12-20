#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <random>
#include <cstring>
#include <ctime>
#include <thread>

#include "quasar_format.h"
#include "huffman.h"
#include "wavelet.h"
#include "chacha.h"
#include "udp_link.h" 

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
        std::cerr << "Usage: " << argv[0] << " <input_file/port> [options...]\n"
                  << "Options:\n"
                  << "  --unpack              Restore a file from disk\n"
                  << "  --tx <ip> <port>      Stream file to network\n"
                  << "  --rx <port>           Listen for network stream\n"
                  << "  --encrypt             Enable ChaCha20 security\n"
                  << "  --scale <float>       Precision scale (default 10.0)\n";
        return 1;
    }

    std::string arg1 = argv[1];
    
    // Modes
    bool mode_unpack = false;
    bool mode_tx = false;
    bool mode_rx = false;
    std::string tx_ip = "127.0.0.1";
    int tx_port = 0;
    int rx_port = 0;

    // Settings
    float radius = 1000.0f;
    float scale = 10.0f; 
    bool do_encrypt = false;

    // Argument Parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--unpack") mode_unpack = true;
        else if (arg == "--tx" && i + 2 < argc) {
            mode_tx = true;
            tx_ip = argv[++i];
            tx_port = std::stoi(argv[++i]);
        }
        else if (arg == "--rx" && i + 1 < argc) {
            mode_rx = true;
            rx_port = std::stoi(argv[++i]);
        }
        else if (arg == "--encrypt") do_encrypt = true;
        else if (arg == "--scale" && i + 1 < argc) scale = std::stof(argv[++i]);
        else if (i > 1 && !mode_tx && !mode_rx) { 
            // Only try parsing radius if it's not part of IP/Port args
            try { radius = std::stof(arg); } catch (...) {} 
        }
    }

    // ==========================================
    //           RECEIVER MODE (RX)
    // ==========================================
    if (mode_rx) {
        std::cout << "[Network] Listening on UDP Port " << rx_port << "..." << std::endl;
        QuasarRx rx;
        std::vector<uint8_t> frame;
        
        // Loop forever (Server Mode)
        while (rx.listen(rx_port, frame)) {
            if (frame.size() < sizeof(QuasarHeader)) {
                std::cerr << "[Rx] Error: Frame too small." << std::endl;
                continue;
            }

            // Extract Header
            QuasarHeader header;
            std::memcpy(&header, frame.data(), sizeof(header));

            if (std::strncmp(header.magic, "QSR1", 4) != 0) {
                std::cerr << "[Rx] Error: Invalid Magic Bytes." << std::endl;
                continue;
            }

            // Extract Payload
            std::vector<uint8_t> payload(frame.begin() + sizeof(header), frame.end());

            std::cout << "[Rx] Received Frame. Scale: " << header.scale 
                      << " | Orig Size: " << header.original_size << std::endl;

            // Decrypt
            if (header.compression_flags & 0x80) {
                std::cout << "[SECURITY] Frame is Encrypted. Enter Key (Hex): ";
                std::string hexKey;
                std::cin >> hexKey;
                uint8_t key[32];
                parse_hex_key(hexKey, key);
                ChaCha20::process(payload, key, header.nonce);
            }

            // Decompress
            HuffmanCodec codec;
            std::vector<uint8_t> decompressed = codec.decompress(payload);

            // Reconstruct
            std::string timestamp = std::to_string(std::time(nullptr));
            if (header.compression_flags & 0x02) {
                GrayImage img(header.width, header.height);
                dequantize(decompressed, img, header.scale);
                inverseTransform2D(img);
                
                std::string outName = "rx_" + timestamp + ".pgm";
                savePGM(outName, img);
                std::cout << "[Rx] Saved Image: " << outName << std::endl;
            } else {
                std::string outName = "rx_" + timestamp + ".bin";
                std::ofstream out(outName, std::ios::binary);
                out.write((const char*)decompressed.data(), decompressed.size());
                std::cout << "[Rx] Saved File: " << outName << std::endl;
            }
            std::cout << "[Network] Waiting for next frame..." << std::endl;
        }
        return 0;
    }

    // ==========================================
    //       COMPRESS / ENCRYPT / TX MODE
    // ==========================================
    if (!mode_unpack) {
        // 1. Vision Pipeline
        std::vector<uint8_t> finalData;
        uint8_t compressionFlags = 0;
        uint64_t originalSize = 0;
        uint16_t width = 0, height = 0;

        if (fs::path(arg1).extension() == ".pgm") {
            std::cout << "Processing PGM... Scale: " << scale << std::endl;
            GrayImage img(0, 0);
            if (!loadPGM(arg1, img)) return 1;
            originalSize = img.width * img.height;
            width = img.width; height = img.height;
            
            transform2D(img);
            applySaliency(img, radius);
            std::vector<uint8_t> quantized = quantize(img, scale);
            HuffmanCodec codec;
            finalData = codec.compress(quantized);
            compressionFlags |= 0x02; 
        } else {
            std::ifstream inputFile(arg1, std::ios::binary | std::ios::ate);
            if (!inputFile) { std::cerr << "File not found." << std::endl; return 1; }
            originalSize = inputFile.tellg();
            inputFile.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(originalSize);
            inputFile.read((char*)fileData.data(), originalSize);
            HuffmanCodec codec;
            finalData = codec.compress(fileData);
            compressionFlags |= 0x01; 
        }

        // 2. Prepare Header
        QuasarHeader header;
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, "QSR1", 4);
        header.original_size = originalSize;
        header.compression_flags = compressionFlags;
        header.scale = scale;
        header.width = width; header.height = height;

        // 3. Encrypt
        if (do_encrypt) {
            std::cout << "Encrypting..." << std::endl;
            uint8_t key[32], nonce[12];
            std::random_device rd;
            for (auto& k : key) k = rd() & 0xFF;
            for (auto& n : nonce) n = rd() & 0xFF;
            print_hex("Generated Key", key, 32);
            std::memcpy(header.nonce, nonce, 12);
            header.compression_flags |= 0x80; 
            ChaCha20::process(finalData, key, nonce);
        }

        // 4. Combine Header + Payload
        std::vector<uint8_t> fullPacket(sizeof(header) + finalData.size());
        std::memcpy(fullPacket.data(), &header, sizeof(header));
        std::memcpy(fullPacket.data() + sizeof(header), finalData.data(), finalData.size());

        // 5. Output (File or Network)
        if (mode_tx) {
            std::cout << "[Tx] Streaming to " << tx_ip << ":" << tx_port << "..." << std::endl;
            QuasarTx tx;
            tx.send_frame(fullPacket, tx_ip, tx_port);
        } else {
            std::string outputPath = arg1 + ".qsr";
            std::ofstream out(outputPath, std::ios::binary);
            out.write((const char*)fullPacket.data(), fullPacket.size());
            std::cout << "Saved: " << outputPath << std::endl;
        }
    }
    // ==========================================
    //               UNPACK MODE
    // ==========================================
    else {
        // (Keep your existing Unpack logic here, or paste it back if it was lost)
        // I will reproduce the Unpack logic here for completeness
        std::ifstream in(arg1, std::ios::binary);
        if (!in) return 1;

        QuasarHeader header;
        in.read((char*)&header, sizeof(header));
        std::vector<uint8_t> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        if (header.compression_flags & 0x80) {
            std::cout << "Enter Key (Hex): ";
            std::string hexKey; std::cin >> hexKey;
            uint8_t key[32]; parse_hex_key(hexKey, key);
            ChaCha20::process(payload, key, header.nonce);
        }

        HuffmanCodec codec;
        std::vector<uint8_t> decompressed = codec.decompress(payload);

        if (header.compression_flags & 0x02) {
            GrayImage img(header.width, header.height);
            dequantize(decompressed, img, header.scale);
            inverseTransform2D(img);
            savePGM(arg1 + ".recovered.pgm", img);
        } else {
            std::ofstream out(arg1 + ".recovered", std::ios::binary);
            out.write((const char*)decompressed.data(), decompressed.size());
        }
        std::cout << "Unpacked successfully." << std::endl;
    }

    return 0;
}