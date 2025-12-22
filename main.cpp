/**
 * Quasar Protocol - Comprehensive Mission Edition (v2.0)
 * 
 * Author: Deevinandu
 * Purpose: Multipurpose telemetry/archive tool for ISRO IRoC-U and Thesis.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip>
#include <random>
#include <cstring>
#include <ctime>
#include <thread>
#include <string>
#include <cmath>
#include <algorithm>

// Core Quasar Libraries
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
    std::string safe_hex = hex;
    while (safe_hex.length() < 64) safe_hex += "0";
    for (size_t i = 0; i < 32; ++i) {
        key[i] = (hex_char_to_byte(safe_hex[2*i]) << 4) | hex_char_to_byte(safe_hex[2*i+1]);
    }
}

// --- MAIN ---

int main(int argc, char* argv[]) {
    // 1. Argument Parsing & UI
    if (argc < 2) {
        std::cerr << "Quasar Protocol v2.0 | Systems Engineer: Deevinandu\n"
                  << "Usage: " << argv[0] << " <input/port> [options...]\n\n"
                  << "Modes:\n"
                  << "  --tx <ip> <port>      Stream mission data to GCS via UDP\n"
                  << "  --rx <port>           Listen as GCS (Base Station)\n"
                  << "  --unpack              Restore a local .qsr file to disk\n\n"
                  << "Multi-ROI Logic (ISRO IRoC-U):\n"
                  << "  --roi <x> <y> <r>     Define high-detail target (Max 8)\n"
                  << "  --est_x, --est_y, --est_z   Drone pose telemetry\n"
                  << "  --id <uint>           Target feature identification ID\n\n"
                  << "Security & Precision:\n"
                  << "  --encrypt             Enable ChaCha20 encryption\n"
                  << "  --key <hex>           Use 256-bit Pre-Shared Key\n"
                  << "  --scale <float>       Quantization precision (default 10.0)\n";
        return 1;
    }

    std::string arg1 = argv[1];
    
    // Core States
    bool mode_unpack = false, mode_tx = false, mode_rx = false, do_encrypt = false;
    std::string tx_ip = "127.0.0.1", manual_key = "";
    int tx_port = 0, rx_port = 0;
    float scale = 10.0f;

    // ISRO Data States
    std::vector<ROI> mission_targets;
    float est_x = 0.0f, est_y = 0.0f, est_z = 0.0f;
    uint32_t target_id = 0;

    // --- ARGUMENT PARSER LOOP ---
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--unpack") mode_unpack = true;
        else if (arg == "--tx" && i + 2 < argc) { mode_tx = true; tx_ip = argv[++i]; tx_port = std::stoi(argv[++i]); }
        else if (arg == "--rx" && i + 1 < argc) { mode_rx = true; rx_port = std::stoi(argv[++i]); }
        else if (arg == "--encrypt") do_encrypt = true;
        else if (arg == "--scale" && i + 1 < argc) scale = std::stof(argv[++i]);
        else if (arg == "--key" && i + 1 < argc) manual_key = argv[++i];
        // Multi-ROI Handler
        else if (arg == "--roi" && i + 3 < argc) {
            ROI r;
            r.x = (uint16_t)std::stoi(argv[++i]);
            r.y = (uint16_t)std::stoi(argv[++i]);
            r.r = (uint16_t)std::stoi(argv[++i]);
            mission_targets.push_back(r);
        }
        else if (arg == "--est_x" && i + 1 < argc) est_x = std::stof(argv[++i]);
        else if (arg == "--est_y" && i + 1 < argc) est_y = std::stof(argv[++i]);
        else if (arg == "--est_z" && i + 1 < argc) est_z = std::stof(argv[++i]);
        else if (arg == "--id" && i + 1 < argc) target_id = (uint32_t)std::stoul(argv[++i]);
    }

    // =========================================================================
    //                            RECEIVER MODE (GCS)
    // =========================================================================
    if (mode_rx) {
        std::cout << "[GCS] Listening on UDP Port " << rx_port << "..." << std::endl;
        QuasarRx rx;
        std::vector<uint8_t> frame;
        
        while (rx.listen(rx_port, frame)) {
            if (frame.size() < sizeof(QuasarHeader)) continue;

            QuasarHeader header;
            std::memcpy(&header, frame.data(), sizeof(header));
            if (std::strncmp(header.magic, "QSR1", 4) != 0) continue;

            // --- DISPLAY MISSION TELEMETRY ---
            std::cout << "\n----------------------------------------" << std::endl;
            std::cout << "[!] INCOMING MISSION DATA | Frame ID: " << header.target_id << std::endl;
            std::cout << " -> Drone Pose: (" << header.est_x << ", " << header.est_y << ", " << header.est_z << ")" << std::endl;
            
            int active_rois = static_cast<int>(header.roi_count);
            std::cout << " -> Saliency Bubbles Active: " << active_rois << std::endl;
            for(int k = 0; k < active_rois && k < 8; k++) {
                std::cout << "    [" << k << "] Focus Point: (" << header.targets[k].x << ", " << header.targets[k].y << ") | Radius: " << header.targets[k].r << "px" << std::endl;
            }
            std::cout << "----------------------------------------" << std::endl;

            std::vector<uint8_t> payload(frame.begin() + sizeof(header), frame.end());

            // --- Decryption Layer ---
            if (header.compression_flags & 0x80) {
                uint8_t key[32];
                if (!manual_key.empty()) parse_hex_key(manual_key, key);
                else { 
                    std::cout << "[Rx] Encrypted Frame. Paste PSK: ";
                    std::string k; std::cin >> k; parse_hex_key(k, key); 
                }
                ChaCha20::process(payload, key, header.nonce);
            }

            // --- Decompression & Recovery ---
            HuffmanCodec codec;
            std::vector<uint8_t> decompressed = codec.decompress(payload);
            std::string t_stamp = std::to_string(std::time(nullptr));

            if (header.compression_flags & 0x02) {
                GrayImage img(header.width, header.height);
                dequantize(decompressed, img, header.scale);
                inverseTransform2D(img);
                std::string outName = "rx_" + t_stamp + ".pgm";
                savePGM(outName, img);
                std::cout << "[Rx] Visual Data Reconstructed: " << outName << std::endl;
            } else {
                std::string outName = "rx_" + t_stamp + ".bin";
                std::ofstream out(outName, std::ios::binary);
                out.write((const char*)decompressed.data(), decompressed.size());
                std::cout << "[Rx] Binary Data Recovered: " << outName << std::endl;
            }
        }
        return 0;
    }

    // =========================================================================
    //                         TRANSMITTER / PACK MODE
    // =========================================================================
    if (!mode_unpack) {
        std::vector<uint8_t> finalData;
        uint8_t compressionFlags = 0;
        uint64_t originalSize = 0;
        uint16_t width = 0, height = 0;

        // 1. Pipeline Selection
        if (fs::path(arg1).extension() == ".pgm") {
            std::cout << "[Vision] Processing PGM with Multi-ROI Support..." << std::endl;
            GrayImage img(0, 0);
            if (!loadPGM(arg1, img)) return 1;
            originalSize = img.width * img.height;
            width = img.width; height = img.height;
            
            // Fallback to center if no ROI provided
            if (mission_targets.empty()) {
                mission_targets.push_back({(uint16_t)(width/2), (uint16_t)(height/2), 150});
                std::cout << " -> No ROI specified. Using center fallback." << std::endl;
            }

            applySaliency(img, mission_targets); // Mask the pixels first
            transform2D(img); 
            std::vector<uint8_t> quantized = quantize(img, scale);
            HuffmanCodec codec;
            finalData = codec.compress(quantized);
            compressionFlags |= 0x02; 
        } else {
            std::cout << "[Binary] Processing generic archive..." << std::endl;
            std::ifstream inputFile(arg1, std::ios::binary | std::ios::ate);
            if (!inputFile) { std::cerr << "File not found: " << arg1 << std::endl; return 1; }
            originalSize = inputFile.tellg();
            inputFile.seekg(0, std::ios::beg);
            std::vector<uint8_t> fileData(originalSize);
            inputFile.read((char*)fileData.data(), originalSize);
            HuffmanCodec codec;
            finalData = codec.compress(fileData);
            compressionFlags |= 0x01; 
        }

        // 2. Mission Header Construction
        QuasarHeader header;
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, "QSR1", 4);
        header.original_size = originalSize;
        header.compression_flags = compressionFlags;
        header.scale = scale;
        header.width = width; header.height = height;
        header.est_x = est_x; header.est_y = est_y; header.est_z = est_z;
        header.target_id = target_id;

        // Populate Header Targets (ISRO SPEC)
        header.roi_count = (uint8_t)std::min((int)mission_targets.size(), 8);
        for (int k = 0; k < header.roi_count; ++k) {
            header.targets[k] = mission_targets[k];
        }

        // 3. Security Layer (ChaCha20)
        if (do_encrypt) {
            uint8_t key[32], nonce[12];
            std::random_device rd;
            if (!manual_key.empty()) parse_hex_key(manual_key, key);
            else { 
                for (auto& k : key) k = rd() & 0xFF; 
                std::cout << "[Security] Encrypting stream..." << std::endl;
                print_hex("Generated PSK", key, 32);
            }
            for (auto& n : nonce) n = rd() & 0xFF;
            std::memcpy(header.nonce, nonce, 12);
            header.compression_flags |= 0x80; 
            ChaCha20::process(finalData, key, nonce);
        }

        // 4. Packet Combination
        std::vector<uint8_t> fullArchive(sizeof(header) + finalData.size());
        std::memcpy(fullArchive.data(), &header, sizeof(header));
        std::memcpy(fullArchive.data() + sizeof(header), finalData.data(), finalData.size());

        // 5. TX vs Disk Output
        if (mode_tx) {
            std::cout << "[Tx] Blasting " << fullArchive.size() << " bytes to " << tx_ip << ":" << tx_port << std::endl;
            QuasarTx tx;
            tx.send_frame(fullArchive, tx_ip, tx_port);
        } else {
            std::string outputPath = arg1 + ".qsr";
            std::ofstream out(outputPath, std::ios::binary);
            out.write((const char*)fullArchive.data(), fullArchive.size());
            std::cout << "[Disk] Saved archive to: " << outputPath << std::endl;
        }
    } 

    // =========================================================================
    //                          UNPACK MODE (Disk Utility)
    // =========================================================================
    else {
        std::cout << "[Unpack] Reading local archive " << arg1 << "..." << std::endl;
        std::ifstream in(arg1, std::ios::binary);
        if (!in) { std::cerr << "File Error." << std::endl; return 1; }

        QuasarHeader header;
        in.read((char*)&header, sizeof(header));
        if (std::strncmp(header.magic, "QSR1", 4) != 0) { std::cerr << "Magic mismatch." << std::endl; return 1; }

        std::vector<uint8_t> payload((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        if (header.compression_flags & 0x80) {
            uint8_t key[32];
            if (!manual_key.empty()) parse_hex_key(manual_key, key);
            else { std::cout << "Encrypted. Enter PSK: "; std::string s; std::cin >> s; parse_hex_key(s, key); }
            ChaCha20::process(payload, key, header.nonce);
        }

        HuffmanCodec codec;
        std::vector<uint8_t> decompressed = codec.decompress(payload);

        if (header.compression_flags & 0x02) {
            GrayImage img(header.width, header.height);
            dequantize(decompressed, img, header.scale);
            inverseTransform2D(img);
            savePGM(arg1 + ".recovered.pgm", img);
            std::cout << "[Unpack] Reconstructed image: " << arg1 << ".recovered.pgm" << std::endl;
        } else {
            std::ofstream out(arg1 + ".recovered", std::ios::binary);
            out.write((const char*)decompressed.data(), decompressed.size());
            std::cout << "[Unpack] Reconstructed binary: " << arg1 << ".recovered" << std::endl;
        }
    }

    return 0;
}