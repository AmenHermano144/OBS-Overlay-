#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hco {

constexpr const char*    kPluginName        = "obs-hidden-chat-overlay";
constexpr const wchar_t* kWindowClass       = L"HiddenChatOverlayWindow";
constexpr const wchar_t* kWindowTitle       = L"Hidden Chat Overlay";
constexpr const wchar_t* kPipeName          = L"\\\\.\\pipe\\obs-hidden-chat-overlay";
constexpr const char*    kSourceId          = "hidden_chat_overlay_source";
constexpr const char*    kSourceDisplayName = "Hidden Chat Overlay";

constexpr uint32_t kProtocolMagic   = 0x4843484F;   // 'HCHO'
constexpr uint8_t  kProtocolVersion = 1;

enum class Command : uint8_t {
    Clear              = 0x01,
    Rect               = 0x02,
    FilledRect         = 0x03,
    Line               = 0x04,
    Text               = 0x05,
    Present            = 0x06,
    Reset              = 0x07,

    // cross-process D3D11 shared texture (NT handle + keyed mutex).
    SharedTexture      = 0x08,
    ClearSharedTexture = 0x09,

    // handshake info so an external process can DuplicateHandle into the plugin.
    GetPluginInfo      = 0x0A,
    PluginInfoResponse = 0x8A,   // server -> client only
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

// x2/y2 are repurposed: Rect/FilledRect use them for w/h, Line for the end point.
struct DrawCommand {
    Command      type;
    float        x         = 0.f;
    float        y         = 0.f;
    float        x2        = 0.f;
    float        y2        = 0.f;
    float        thickness = 1.f;
    float        fontSize  = 14.f;
    uint32_t     rgba      = 0xFFFFFFFFu;
    std::wstring text;
};

using CommandList = std::vector<DrawCommand>;

}  // namespace hco
