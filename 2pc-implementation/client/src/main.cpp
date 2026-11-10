// Sender side. Connects to the net_server on the OBS PC and sends one frame:
// a filled rectangle + text, then hold, then clear. Replace the body with
// your own draw loop.

#include "../../shared/net_protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

using namespace obs_overlay::net;

namespace {

bool sendAll(SOCKET s, const void* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        int sent = send(s, static_cast<const char*>(buf) + off,
                        static_cast<int>(n - off), 0);
        if (sent <= 0) return false;
        off += static_cast<size_t>(sent);
    }
    return true;
}

bool recvExact(SOCKET s, void* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        int got = recv(s, static_cast<char*>(buf) + off,
                       static_cast<int>(n - off), 0);
        if (got <= 0) return false;
        off += static_cast<size_t>(got);
    }
    return true;
}

bool sendCmd(SOCKET s, NetCmd cmd, const void* payload, uint16_t payloadSize) {
    NetFrameHeader hdr{ kMagic, kVersion, static_cast<uint8_t>(cmd), payloadSize };
    if (!sendAll(s, &hdr, sizeof(hdr))) return false;
    if (payloadSize > 0 && !sendAll(s, payload, payloadSize)) return false;
    return true;
}

bool sendText(SOCKET s, float x, float y, float size, uint32_t rgba, const char* utf8) {
    const uint16_t len = static_cast<uint16_t>(std::strlen(utf8));
    TextPayloadHeader th{ x, y, size, rgba, len };
    NetFrameHeader hdr{ kMagic, kVersion,
                        static_cast<uint8_t>(NetCmd::Text),
                        static_cast<uint16_t>(sizeof(th) + len) };
    if (!sendAll(s, &hdr, sizeof(hdr))) return false;
    if (!sendAll(s, &th,  sizeof(th)))  return false;
    return len == 0 || sendAll(s, utf8, len);
}

bool handshake(SOCKET s) {
    HelloPayload hp{ 1 };
    if (!sendCmd(s, NetCmd::Hello, &hp, sizeof(hp))) return false;

    NetFrameHeader ack{};
    if (!recvExact(s, &ack, sizeof(ack))) return false;
    return ack.magic == kMagic && ack.version == kVersion &&
           ack.command == static_cast<uint8_t>(NetCmd::Hello);
}

struct Args {
    std::string host = "127.0.0.1";
    uint16_t    port = kDefaultPort;
};

Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&](const char* v){ return (i + 1 < argc) ? argv[++i] : v; };
        if      (k == "--host") a.host = next(a.host.c_str());
        else if (k == "--port") a.port = static_cast<uint16_t>(std::atoi(next("0")));
    }
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(args.port);
    if (inet_pton(AF_INET, args.host.c_str(), &addr.sin_addr) != 1) {
        printf("bad host %s\n", args.host.c_str());
        closesocket(s); WSACleanup(); return 1;
    }

    printf("connecting to %s:%u\n", args.host.c_str(), args.port);
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        printf("connect failed %d\n", WSAGetLastError());
        closesocket(s); WSACleanup(); return 1;
    }
    if (!handshake(s)) {
        printf("handshake failed\n");
        closesocket(s); WSACleanup(); return 1;
    }

    sendCmd(s, NetCmd::Clear, nullptr, 0);

    FilledRectPayload bg{ 200.f, 200.f, 400.f, 100.f, 0xC0202020u };
    sendCmd(s, NetCmd::FilledRect, &bg, sizeof(bg));

    RectPayload outline{ 200.f, 200.f, 400.f, 100.f, 2.f, 0xFF4A90E2u };
    sendCmd(s, NetCmd::Rect, &outline, sizeof(outline));

    sendText(s, 220.f, 230.f, 24.f, 0xFFFFFFFFu, "overlay ok (over TCP)");

    sendCmd(s, NetCmd::Present, nullptr, 0);

    Sleep(5000);

    sendCmd(s, NetCmd::Clear,   nullptr, 0);
    sendCmd(s, NetCmd::Present, nullptr, 0);

    closesocket(s);
    WSACleanup();
    return 0;
}
