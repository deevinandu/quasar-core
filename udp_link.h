#ifndef UDP_LINK_H
#define UDP_LINK_H

#include <vector>
#include <string>
#include <cstdint>
#include <map>

// MTU-safe packet structure (1400 bytes payload)
#pragma pack(push, 1)
struct QuasarPacket {
    uint32_t frame_id;      // Unique ID for the whole image
    uint16_t chunk_id;      // Slice number (0, 1, 2...)
    uint16_t total_chunks;  // Total slices in this frame
    uint16_t data_size;     // Size of current payload
    uint8_t payload[1400];  // Raw data slice
};
#pragma pack(pop)

class QuasarTx {
public:
    QuasarTx();
    ~QuasarTx();
    void send_frame(const std::vector<uint8_t>& full_data, const std::string& ip, int port);
private:
    uint32_t frame_counter;
#ifdef _WIN32
    uintptr_t sock;
#else
    int sock;
#endif
};

class QuasarRx {
public:
    QuasarRx();
    ~QuasarRx();
    bool listen(int port, std::vector<uint8_t>& out_data);
private:
    struct FrameReassembler {
        uint16_t total_chunks;
        std::map<uint16_t, std::vector<uint8_t>> chunks;
    };
    std::map<uint32_t, FrameReassembler> frame_buffer;
#ifdef _WIN32
    uintptr_t sock;
#else
    int sock;
#endif
};

#endif // UDP_LINK_H
