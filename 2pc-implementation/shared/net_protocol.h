#pragma once
#include <cstdint>

// Network wire format used between the two PCs.
//
// Layout on the wire:
//   NetFrameHeader { magic, version, command, payloadSize }
//   payload bytes (payloadSize)
//
// Commands mirror a subset of the plugin's draw API so the receiver just
// translates from TCP into its local overlay client. Multi-byte integers are
// sent in native (little-endian) order; both ends are Windows x64.
//
// Not encrypted. Use over a trusted LAN or tunnel over SSH/VPN.

namespace obs_overlay::net {

constexpr uint16_t kDefaultPort = 51423;

constexpr uint32_t kMagic   = 0x324F424F;  // 'OBO2'
constexpr uint8_t  kVersion = 1;

enum class NetCmd : uint8_t {
    Hello      = 0x01,  // handshake: client -> server, server acks with Hello back
    Clear      = 0x02,
    Rect       = 0x03,
    FilledRect = 0x04,
    Line       = 0x05,
    Text       = 0x06,
    Present    = 0x07,
};

#pragma pack(push, 1)
struct NetFrameHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  command;
    uint16_t payloadSize;
};

struct HelloPayload {
    uint32_t clientVersion;
};

struct RectPayload {
    float    x, y, w, h;
    float    thickness;
    uint32_t rgba;
};

struct FilledRectPayload {
    float    x, y, w, h;
    uint32_t rgba;
};

struct LinePayload {
    float    x1, y1, x2, y2;
    float    thickness;
    uint32_t rgba;
};

struct TextPayloadHeader {
    float    x, y;
    float    fontSize;
    uint32_t rgba;
    uint16_t byteLen;    // followed by that many UTF-8 bytes
};
#pragma pack(pop)

}  // namespace obs_overlay::net
