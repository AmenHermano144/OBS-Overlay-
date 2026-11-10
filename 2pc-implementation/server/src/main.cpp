// Runs on the PC that has OBS installed. Listens on TCP, authenticates each
// client with a Hello handshake, and forwards every draw command into the
// local OBS plugin pipe via obs_overlay::Client.

#include <obs_overlay/client.h>
#include "../../shared/net_protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

using namespace obs_overlay;
using namespace obs_overlay::net;

namespace {

std::atomic<bool> g_running{true};

void onCtrlC(int) { g_running.store(false); }

bool recvExact(SOCKET s, void* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        int got = recv(s, static_cast<char*>(buf) + off, static_cast<int>(n - off), 0);
        if (got <= 0) return false;
        off += static_cast<size_t>(got);
    }
    return true;
}

void handleSession(SOCKET s) {
    printf("[server] session start\n");

    Client plugin;
    if (!plugin.connect()) {
        printf("[server] couldn't connect to plugin, closing session\n");
        closesocket(s);
        return;
    }

    // handshake
    NetFrameHeader hdr{};
    if (!recvExact(s, &hdr, sizeof(hdr)) ||
        hdr.magic != kMagic || hdr.version != kVersion ||
        hdr.command != static_cast<uint8_t>(NetCmd::Hello)) {
        printf("[server] bad hello\n");
        closesocket(s);
        return;
    }
    std::vector<uint8_t> payload(hdr.payloadSize);
    if (hdr.payloadSize > 0 && !recvExact(s, payload.data(), hdr.payloadSize)) {
        closesocket(s);
        return;
    }
    NetFrameHeader ack{ kMagic, kVersion, static_cast<uint8_t>(NetCmd::Hello), 0 };
    send(s, reinterpret_cast<const char*>(&ack), sizeof(ack), 0);

    while (g_running.load()) {
        if (!recvExact(s, &hdr, sizeof(hdr))) break;
        if (hdr.magic != kMagic || hdr.version != kVersion) {
            printf("[server] bad frame header\n");
            break;
        }
        payload.resize(hdr.payloadSize);
        if (hdr.payloadSize > 0 && !recvExact(s, payload.data(), hdr.payloadSize)) break;

        switch (static_cast<NetCmd>(hdr.command)) {
            case NetCmd::Clear:
                plugin.clear();
                break;
            case NetCmd::Rect: {
                if (payload.size() < sizeof(RectPayload)) break;
                RectPayload p{};
                std::memcpy(&p, payload.data(), sizeof(p));
                plugin.rect(p.x, p.y, p.w, p.h, p.thickness, p.rgba);
                break;
            }
            case NetCmd::FilledRect: {
                if (payload.size() < sizeof(FilledRectPayload)) break;
                FilledRectPayload p{};
                std::memcpy(&p, payload.data(), sizeof(p));
                plugin.filledRect(p.x, p.y, p.w, p.h, p.rgba);
                break;
            }
            case NetCmd::Line: {
                if (payload.size() < sizeof(LinePayload)) break;
                LinePayload p{};
                std::memcpy(&p, payload.data(), sizeof(p));
                plugin.line(p.x1, p.y1, p.x2, p.y2, p.thickness, p.rgba);
                break;
            }
            case NetCmd::Text: {
                if (payload.size() < sizeof(TextPayloadHeader)) break;
                TextPayloadHeader th{};
                std::memcpy(&th, payload.data(), sizeof(th));
                if (payload.size() < sizeof(th) + th.byteLen) break;
                std::string_view utf8(reinterpret_cast<const char*>(payload.data() + sizeof(th)),
                                      th.byteLen);
                plugin.text(th.x, th.y, th.fontSize, th.rgba, utf8);
                break;
            }
            case NetCmd::Present:
                if (!plugin.present()) {
                    printf("[server] plugin pipe broke, ending session\n");
                    closesocket(s);
                    return;
                }
                break;
            default:
                printf("[server] unknown cmd %u\n", hdr.command);
                break;
        }
    }

    plugin.clear();
    plugin.present();
    plugin.disconnect();
    closesocket(s);
    printf("[server] session end\n");
}

uint16_t parsePort(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--port") {
            return static_cast<uint16_t>(std::atoi(argv[i + 1]));
        }
    }
    return kDefaultPort;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::signal(SIGINT, onCtrlC);

    const uint16_t port = parsePort(argc, argv);
    printf("[server] starting on port %u\n", port);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[server] WSAStartup failed\n");
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        printf("[server] socket() failed\n");
        WSACleanup();
        return 1;
    }

    int yes = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(port);

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        printf("[server] bind failed %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    if (listen(listener, 1) == SOCKET_ERROR) {
        printf("[server] listen failed %d\n", WSAGetLastError());
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    printf("[server] waiting for a client on 0.0.0.0:%u\n", port);

    while (g_running.load()) {
        sockaddr_in peer{};
        int peerLen = sizeof(peer);
        SOCKET s = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (s == INVALID_SOCKET) {
            if (!g_running.load()) break;
            printf("[server] accept failed %d\n", WSAGetLastError());
            continue;
        }
        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
        printf("[server] client %s:%u connected\n", ip, ntohs(peer.sin_port));

        handleSession(s);
    }

    closesocket(listener);
    WSACleanup();
    return 0;
}
