#pragma once
#include <cstdint>

// wire protocol shared with the plugin. byte-identical to plugin/src/common.h —
// if you rotate either side, rotate both.

namespace obs_overlay::protocol {

constexpr const wchar_t* kPipeName       = L"\\\\.\\pipe\\obs-hidden-chat-overlay";
constexpr uint32_t       kMagic          = 0x4843484F;   // 'HCHO'
constexpr uint8_t        kVersion        = 1;

enum class Cmd : uint8_t {
    Clear              = 0x01,
    Rect               = 0x02,
    FilledRect         = 0x03,
    Line               = 0x04,
    Text               = 0x05,
    Present            = 0x06,
    Reset              = 0x07,
    SharedTexture      = 0x08,
    ClearSharedTexture = 0x09,
    GetPluginInfo      = 0x0A,
    PluginInfoResponse = 0x8A,
};

#pragma pack(push, 1)
struct FrameHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  command;
    uint16_t payloadSize;
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
    uint16_t byteLen;
};

struct SharedTexturePayload {
    uint64_t sharedHandle;
    uint32_t width;
    uint32_t height;
    uint32_t keyProducer;
    uint32_t keyConsumer;
};

struct PluginInfoReply {
    uint32_t pid;
    uint64_t hwnd;
    uint32_t width;
    uint32_t height;
};
#pragma pack(pop)

}  // namespace obs_overlay::protocol
