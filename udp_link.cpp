#include "udp_link.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

// Portable Sleep
void tiny_sleep(int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

// Global Networking Init (for Windows)
struct NetworkEnv {
    NetworkEnv() {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }
    ~NetworkEnv() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};
static NetworkEnv _env;

// --- Transmitter ---
QuasarTx::QuasarTx() : frame_counter(0) {
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

QuasarTx::~QuasarTx() {
    if (sock != INVALID_SOCKET) closesocket(sock);
}

void QuasarTx::send_frame(const std::vector<uint8_t>& full_data, const std::string& ip, int port) {
    sockaddr_in target;
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &target.sin_addr);

    uint16_t total_chunks = (full_data.size() + 1399) / 1400;
    frame_counter++;

    for (uint16_t i = 0; i < total_chunks; ++i) {
        QuasarPacket pkt;
        pkt.frame_id = frame_counter;
        pkt.chunk_id = i;
        pkt.total_chunks = total_chunks;
        
        uint32_t offset = i * 1400;
        uint32_t remaining = full_data.size() - offset;
        pkt.data_size = (remaining > 1400) ? 1400 : (uint16_t)remaining;
        
        std::memcpy(pkt.payload, full_data.data() + offset, pkt.data_size);

        sendto(sock, (const char*)&pkt, sizeof(pkt) - (1400 - pkt.data_size), 0, (sockaddr*)&target, sizeof(target));
        
        // Rate limiting to prevent buffer overflow
        tiny_sleep(100);
    }
    std::cout << "[Tx] Sent Frame " << frame_counter << " (" << total_chunks << " chunks)" << std::endl;
}

// --- Receiver ---
QuasarRx::QuasarRx() {
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

QuasarRx::~QuasarRx() {
    if (sock != INVALID_SOCKET) closesocket(sock);
}

bool QuasarRx::listen(int port, std::vector<uint8_t>& out_data) {
    sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    static bool bound = false;
    if (!bound) {
        if (bind(sock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
            std::cerr << "[Rx] Bind failed" << std::endl;
            return false;
        }
        bound = true;
    }

    while (true) {
        QuasarPacket pkt;
        sockaddr_in sender;
        socklen_t addrLen = sizeof(sender);
        
        int n = recvfrom(sock, (char*)&pkt, sizeof(pkt), 0, (sockaddr*)&sender, &addrLen);
        if (n <= 0) continue;

        auto& reasm = frame_buffer[pkt.frame_id];
        reasm.total_chunks = pkt.total_chunks;
        
        std::vector<uint8_t> chunk(pkt.payload, pkt.payload + pkt.data_size);
        reasm.chunks[pkt.chunk_id] = std::move(chunk);

        if (reasm.chunks.size() == reasm.total_chunks) {
            std::cout << "[Rx] Completed Frame " << pkt.frame_id << std::endl;
            out_data.clear();
            for (uint16_t i = 0; i < reasm.total_chunks; ++i) {
                out_data.insert(out_data.end(), reasm.chunks[i].begin(), reasm.chunks[i].end());
            }
            frame_buffer.erase(pkt.frame_id);
            return true;
        }
    }
}
