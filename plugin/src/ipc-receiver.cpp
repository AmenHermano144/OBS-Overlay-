#include "ipc-receiver.h"
#include "overlay-renderer.h"

#include <util/base.h>
#include <bcrypt.h>
#include <cstring>
#include <vector>
#include <string>

namespace hco {

// shared secret for the client/plugin handshake. not cryptographic; rotate
// before any release and keep both sides in sync.
static const uint8_t kSharedSecret[32] = {
    0x1E, 0x9C, 0x77, 0xA2, 0x3B, 0x84, 0xD1, 0x0F,
    0x5A, 0xC6, 0x22, 0x91, 0xFD, 0x48, 0x67, 0xB5,
    0x0E, 0x33, 0xAA, 0x71, 0x8C, 0x59, 0x24, 0xE7,
    0xB1, 0x46, 0x99, 0x02, 0x7D, 0xC8, 0x35, 0x50,
};

static bool readExact(HANDLE h, void* buf, DWORD n) {
    DWORD off = 0;
    while (off < n) {
        DWORD got = 0;
        if (!ReadFile(h, static_cast<uint8_t*>(buf) + off, n - off, &got, nullptr)) return false;
        if (got == 0) return false;
        off += got;
    }
    return true;
}

static bool writeExact(HANDLE h, const void* buf, DWORD n) {
    DWORD off = 0;
    while (off < n) {
        DWORD w = 0;
        if (!WriteFile(h, static_cast<const uint8_t*>(buf) + off, n - off, &w, nullptr)) return false;
        if (w == 0) return false;
        off += w;
    }
    return true;
}

static bool hmacSha256(const uint8_t* key, size_t klen,
                       const uint8_t* data, size_t dlen,
                       uint8_t out[32]) {
    BCRYPT_ALG_HANDLE  alg  = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    NTSTATUS s = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                             BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (s == 0) {
        s = BCryptCreateHash(alg, &hash, nullptr, 0,
                             const_cast<uint8_t*>(key), static_cast<ULONG>(klen), 0);
        if (s == 0) {
            s = BCryptHashData(hash, const_cast<uint8_t*>(data),
                               static_cast<ULONG>(dlen), 0);
            if (s == 0) {
                ok = (BCryptFinishHash(hash, out, 32, 0) == 0);
            }
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return ok;
}

IpcReceiver::IpcReceiver() {
    mCancelEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

IpcReceiver::~IpcReceiver() {
    stop();
    if (mCancelEvent) CloseHandle(mCancelEvent);
    mCancelEvent = nullptr;
}

bool IpcReceiver::start(OverlayRenderer*   renderer,
                        WakeCallback       onFrameReady,
                        DisconnectCallback onClientDisconnect,
                        InfoProvider       infoProvider) {
    if (mRunning.exchange(true)) return false;
    mRenderer           = renderer;
    mOnFrameReady       = std::move(onFrameReady);
    mOnClientDisconnect = std::move(onClientDisconnect);
    mInfoProvider       = std::move(infoProvider);
    if (mCancelEvent) ResetEvent(mCancelEvent);
    mThread = std::thread(&IpcReceiver::serverLoop, this);
    return true;
}

void IpcReceiver::stop() {
    if (!mRunning.exchange(false)) return;
    if (mCancelEvent) SetEvent(mCancelEvent);
    if (mThread.joinable()) mThread.join();
}

void IpcReceiver::serverLoop() {
    while (mRunning.load()) {
        HANDLE pipe = CreateNamedPipeW(
            kPipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1,
            64 * 1024, 64 * 1024,
            0, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            blog(LOG_ERROR, "[%s] CreateNamedPipeW failed %lu", kPluginName, GetLastError());
            const HANDLE waits[1] = { mCancelEvent };
            if (WaitForMultipleObjects(1, waits, FALSE, 500) == WAIT_OBJECT_0) break;
            continue;
        }

        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

        BOOL connected = ConnectNamedPipe(pipe, &ov);
        DWORD err = GetLastError();

        if (!connected && err == ERROR_IO_PENDING) {
            const HANDLE waits[2] = { ov.hEvent, mCancelEvent };
            const DWORD w = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (w != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &ov);
                CloseHandle(ov.hEvent);
                CloseHandle(pipe);
                break;
            }
        } else if (!connected && err != ERROR_PIPE_CONNECTED) {
            CloseHandle(ov.hEvent);
            CloseHandle(pipe);
            continue;
        }
        CloseHandle(ov.hEvent);

        if (mRunning.load()) {
            handleClient(pipe);
            if (mOnClientDisconnect) mOnClientDisconnect();
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

bool IpcReceiver::authenticate(HANDLE pipe) {
    uint8_t nonce[32];
    if (BCryptGenRandom(nullptr, nonce, sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        return false;
    if (!writeExact(pipe, nonce, sizeof(nonce))) return false;

    uint8_t clientMac[32];
    if (!readExact(pipe, clientMac, sizeof(clientMac))) return false;

    uint8_t expected[32];
    if (!hmacSha256(kSharedSecret, sizeof(kSharedSecret),
                    nonce, sizeof(nonce), expected)) return false;

    uint8_t diff = 0;
    for (int i = 0; i < 32; ++i) diff |= expected[i] ^ clientMac[i];
    return diff == 0;
}

bool IpcReceiver::handleClient(HANDLE pipe) {
    if (!authenticate(pipe)) {
        blog(LOG_WARNING, "[%s] ipc auth rejected", kPluginName);
        return false;
    }

    CommandList pending;
    pending.reserve(128);
    std::vector<uint8_t> buf;

    while (mRunning.load()) {
        FrameHeader hdr;
        if (!readExact(pipe, &hdr, sizeof(hdr))) return false;
        if (hdr.magic != kProtocolMagic || hdr.version != kProtocolVersion) {
            blog(LOG_WARNING, "[%s] bad frame header", kPluginName);
            return false;
        }

        buf.resize(hdr.payloadSize);
        if (hdr.payloadSize > 0 && !readExact(pipe, buf.data(), hdr.payloadSize)) {
            return false;
        }

        switch (static_cast<Command>(hdr.command)) {
            case Command::Clear:
            case Command::Reset:
                pending.clear();
                break;

            case Command::Rect: {
                if (buf.size() < sizeof(RectPayload)) return false;
                RectPayload p;
                std::memcpy(&p, buf.data(), sizeof(p));
                DrawCommand dc;
                dc.type      = Command::Rect;
                dc.x         = p.x;   dc.y  = p.y;
                dc.x2        = p.w;   dc.y2 = p.h;
                dc.thickness = p.thickness;
                dc.rgba      = p.rgba;
                pending.push_back(std::move(dc));
                break;
            }
            case Command::FilledRect: {
                if (buf.size() < sizeof(FilledRectPayload)) return false;
                FilledRectPayload p;
                std::memcpy(&p, buf.data(), sizeof(p));
                DrawCommand dc;
                dc.type = Command::FilledRect;
                dc.x    = p.x;   dc.y  = p.y;
                dc.x2   = p.w;   dc.y2 = p.h;
                dc.rgba = p.rgba;
                pending.push_back(std::move(dc));
                break;
            }
            case Command::Line: {
                if (buf.size() < sizeof(LinePayload)) return false;
                LinePayload p;
                std::memcpy(&p, buf.data(), sizeof(p));
                DrawCommand dc;
                dc.type      = Command::Line;
                dc.x         = p.x1;  dc.y  = p.y1;
                dc.x2        = p.x2;  dc.y2 = p.y2;
                dc.thickness = p.thickness;
                dc.rgba      = p.rgba;
                pending.push_back(std::move(dc));
                break;
            }
            case Command::Text: {
                if (buf.size() < sizeof(TextPayloadHeader)) return false;
                TextPayloadHeader p;
                std::memcpy(&p, buf.data(), sizeof(p));
                if (buf.size() < sizeof(p) + p.byteLen) return false;

                const char* utf8 = reinterpret_cast<const char*>(buf.data() + sizeof(p));
                std::wstring ws;
                if (p.byteLen > 0) {
                    const int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, p.byteLen, nullptr, 0);
                    ws.resize(static_cast<size_t>(wlen));
                    MultiByteToWideChar(CP_UTF8, 0, utf8, p.byteLen, ws.data(), wlen);
                }
                DrawCommand dc;
                dc.type     = Command::Text;
                dc.x        = p.x;
                dc.y        = p.y;
                dc.fontSize = p.fontSize;
                dc.rgba     = p.rgba;
                dc.text     = std::move(ws);
                pending.push_back(std::move(dc));
                break;
            }
            case Command::Present: {
                if (mRenderer) {
                    mRenderer->submitFrame(std::move(pending));
                    pending.clear();
                    pending.reserve(128);
                    if (mOnFrameReady) mOnFrameReady();
                }
                break;
            }

            case Command::SharedTexture: {
                if (buf.size() < sizeof(SharedTexturePayload)) return false;
                SharedTexturePayload p;
                std::memcpy(&p, buf.data(), sizeof(p));
                if (mRenderer) {
                    mRenderer->queueOpenSharedTexture(
                        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(p.sharedHandle)),
                        p.width, p.height, p.keyProducer, p.keyConsumer);
                    if (mOnFrameReady) mOnFrameReady();
                }
                break;
            }

            case Command::ClearSharedTexture:
                if (mRenderer) mRenderer->queueCloseSharedTexture();
                if (mOnFrameReady) mOnFrameReady();
                break;

            case Command::GetPluginInfo: {
                PluginInfo info{};
                if (mInfoProvider) info = mInfoProvider();
                PluginInfoReply reply{};
                reply.pid    = info.pid;
                reply.hwnd   = info.hwnd;
                reply.width  = info.width;
                reply.height = info.height;
                FrameHeader out{};
                out.magic       = kProtocolMagic;
                out.version     = kProtocolVersion;
                out.command     = static_cast<uint8_t>(Command::PluginInfoResponse);
                out.payloadSize = sizeof(reply);
                if (!writeExact(pipe, &out, sizeof(out))) return false;
                if (!writeExact(pipe, &reply, sizeof(reply))) return false;
                break;
            }

            case Command::PluginInfoResponse:
                blog(LOG_WARNING, "[%s] client sent PluginInfoResponse; dropping", kPluginName);
                return false;

            default:
                blog(LOG_WARNING, "[%s] unknown cmd %u", kPluginName, hdr.command);
                return false;
        }
    }
    return true;
}

}  // namespace hco
