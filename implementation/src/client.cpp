#include "obs_overlay/client.h"
#include "obs_overlay/obs_launcher.h"
#include "obs_overlay/protocol.h"

#include <bcrypt.h>
#include <cstring>
#include <thread>
#include <chrono>

#pragma comment(lib, "bcrypt.lib")

namespace obs_overlay {

using namespace protocol;

// keep in sync with plugin/src/ipc-receiver.cpp
static const uint8_t kSharedSecret[32] = {
    0x1E, 0x9C, 0x77, 0xA2, 0x3B, 0x84, 0xD1, 0x0F,
    0x5A, 0xC6, 0x22, 0x91, 0xFD, 0x48, 0x67, 0xB5,
    0x0E, 0x33, 0xAA, 0x71, 0x8C, 0x59, 0x24, 0xE7,
    0xB1, 0x46, 0x99, 0x02, 0x7D, 0xC8, 0x35, 0x50,
};

static bool hmacSha256(const uint8_t* key, size_t klen,
                       const uint8_t* data, size_t dlen,
                       uint8_t out[32]) {
    BCRYPT_ALG_HANDLE  alg  = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) == 0) {
        if (BCryptCreateHash(alg, &hash, nullptr, 0,
                             const_cast<uint8_t*>(key), static_cast<ULONG>(klen), 0) == 0) {
            if (BCryptHashData(hash, const_cast<uint8_t*>(data),
                               static_cast<ULONG>(dlen), 0) == 0) {
                ok = (BCryptFinishHash(hash, out, 32, 0) == 0);
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return ok;
}

Client::Client() = default;

Client::~Client() {
    disconnect();
}

void Client::disconnect() {
    if (mPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(mPipe);
        mPipe = INVALID_HANDLE_VALUE;
    }
}

bool Client::openPipe(unsigned timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeoutMs);
    for (;;) {
        HANDLE p = CreateFileW(kPipeName,
            GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (p != INVALID_HANDLE_VALUE) {
            mPipe = p;
            return true;
        }
        const DWORD err = GetLastError();
        if (std::chrono::steady_clock::now() >= deadline) return false;
        if (err == ERROR_PIPE_BUSY) WaitNamedPipeW(kPipeName, 2000);
        else                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

bool Client::doHandshake() {
    uint8_t nonce[32];
    if (!readExact(nonce, sizeof(nonce))) return false;
    uint8_t mac[32];
    if (!hmacSha256(kSharedSecret, sizeof(kSharedSecret), nonce, sizeof(nonce), mac))
        return false;
    return writeExact(mac, sizeof(mac));
}

bool Client::connect(const ConnectOptions& opts) {
    disconnect();

    if (opts.autoLaunchObs && !launcher::ensureRunning())
        return false;

    if (!openPipe(opts.connectTimeoutMs)) return false;
    if (!doHandshake()) {
        disconnect();
        return false;
    }
    return true;
}

bool Client::writeExact(const void* buf, DWORD n) {
    if (mPipe == INVALID_HANDLE_VALUE) return false;
    DWORD off = 0;
    while (off < n) {
        DWORD w = 0;
        if (!WriteFile(mPipe, static_cast<const uint8_t*>(buf) + off, n - off, &w, nullptr)) return false;
        if (w == 0) return false;
        off += w;
    }
    return true;
}

bool Client::readExact(void* buf, DWORD n) {
    if (mPipe == INVALID_HANDLE_VALUE) return false;
    DWORD off = 0;
    while (off < n) {
        DWORD got = 0;
        if (!ReadFile(mPipe, static_cast<uint8_t*>(buf) + off, n - off, &got, nullptr)) return false;
        if (got == 0) return false;
        off += got;
    }
    return true;
}

bool Client::sendCmd(uint8_t cmd, const void* payload, uint16_t payloadSize) {
    FrameHeader hdr{ kMagic, kVersion, cmd, payloadSize };
    if (!writeExact(&hdr, sizeof(hdr))) return false;
    if (payloadSize > 0 && !writeExact(payload, payloadSize)) return false;
    return true;
}

bool Client::getPluginInfo(PluginInfo& out) {
    if (!sendCmd(static_cast<uint8_t>(Cmd::GetPluginInfo), nullptr, 0)) return false;

    FrameHeader hdr{};
    if (!readExact(&hdr, sizeof(hdr))) return false;
    if (hdr.magic   != kMagic ||
        hdr.command != static_cast<uint8_t>(Cmd::PluginInfoResponse) ||
        hdr.payloadSize != sizeof(PluginInfoReply))
        return false;

    PluginInfoReply reply{};
    if (!readExact(&reply, sizeof(reply))) return false;

    out.pid    = reply.pid;
    out.hwnd   = reply.hwnd;
    out.width  = reply.width;
    out.height = reply.height;
    return true;
}

void Client::clear() {
    sendCmd(static_cast<uint8_t>(Cmd::Clear), nullptr, 0);
}

void Client::rect(float x, float y, float w, float h, float thickness, uint32_t rgba) {
    RectPayload p{ x, y, w, h, thickness, rgba };
    sendCmd(static_cast<uint8_t>(Cmd::Rect), &p, sizeof(p));
}

void Client::filledRect(float x, float y, float w, float h, uint32_t rgba) {
    FilledRectPayload p{ x, y, w, h, rgba };
    sendCmd(static_cast<uint8_t>(Cmd::FilledRect), &p, sizeof(p));
}

void Client::line(float x1, float y1, float x2, float y2, float thickness, uint32_t rgba) {
    LinePayload p{ x1, y1, x2, y2, thickness, rgba };
    sendCmd(static_cast<uint8_t>(Cmd::Line), &p, sizeof(p));
}

void Client::text(float x, float y, float fontSize, uint32_t rgba, std::string_view utf8) {
    if (utf8.size() > 0xFFFF) utf8 = utf8.substr(0, 0xFFFF);
    const uint16_t len = static_cast<uint16_t>(utf8.size());

    TextPayloadHeader th{ x, y, fontSize, rgba, len };
    FrameHeader hdr{ kMagic, kVersion, static_cast<uint8_t>(Cmd::Text),
                     static_cast<uint16_t>(sizeof(th) + len) };
    if (!writeExact(&hdr, sizeof(hdr))) return;
    if (!writeExact(&th,  sizeof(th)))  return;
    if (len > 0) writeExact(utf8.data(), len);
}

bool Client::present() {
    return sendCmd(static_cast<uint8_t>(Cmd::Present), nullptr, 0);
}

bool Client::attachSharedTexture(uint64_t dupedHandle, uint32_t width, uint32_t height,
                                 uint32_t keyProducer, uint32_t keyConsumer) {
    SharedTexturePayload p{ dupedHandle, width, height, keyProducer, keyConsumer };
    return sendCmd(static_cast<uint8_t>(Cmd::SharedTexture), &p, sizeof(p));
}

bool Client::detachSharedTexture() {
    return sendCmd(static_cast<uint8_t>(Cmd::ClearSharedTexture), nullptr, 0);
}

}  // namespace obs_overlay
