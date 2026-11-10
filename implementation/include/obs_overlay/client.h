#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <string_view>

namespace obs_overlay {

struct PluginInfo {
    uint32_t pid    = 0;
    uint64_t hwnd   = 0;
    uint32_t width  = 0;
    uint32_t height = 0;
};

struct ConnectOptions {
    // if true, launches OBS (minimized to tray) when it isn't already running.
    bool autoLaunchObs = true;

    // how long to wait for the pipe to appear, in milliseconds.
    unsigned connectTimeoutMs = 15000;
};

// Client connects to the OBS plugin over its named pipe and streams draw
// commands. Thread-compatible but not thread-safe: call from one thread.
//
// Usage:
//   obs_overlay::Client c;
//   if (!c.connect())                return 1;
//   obs_overlay::PluginInfo info;
//   c.getPluginInfo(info);
//
//   while (running) {
//       c.clear();
//       c.filledRect(100, 100, 200, 60, 0x80FFFFFFu);
//       c.text(120, 120, 20.f, 0xFFFFFFFFu, "hello");
//       if (!c.present()) break;
//   }
class Client {
public:
    Client();
    ~Client();

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&)                 = delete;
    Client& operator=(Client&&)      = delete;

    bool connect(const ConnectOptions& opts = {});
    void disconnect();
    bool isConnected() const { return mPipe != INVALID_HANDLE_VALUE; }

    // asks the plugin for its pid, hwnd and overlay dimensions. useful for
    // shared-texture setup, or to size your own drawing to the overlay.
    bool getPluginInfo(PluginInfo& out);

    // ---- D2D shape commands (buffered until present) ----

    void clear();
    void rect(float x, float y, float w, float h, float thickness, uint32_t rgba);
    void filledRect(float x, float y, float w, float h, uint32_t rgba);
    void line(float x1, float y1, float x2, float y2, float thickness, uint32_t rgba);
    void text(float x, float y, float fontSize, uint32_t rgba, std::string_view utf8);

    // flush the buffered frame to the plugin. returns false if the pipe broke.
    bool present();

    // ---- cross-process D3D11 shared texture ----
    //
    // call once after your app has created a BGRA8 texture with the
    // SHARED_NTHANDLE + SHARED_KEYEDMUTEX flags, created a shared NT handle
    // (IDXGIResource1::CreateSharedHandle) and DuplicateHandle'd it into the
    // plugin's process (use PluginInfo::pid). from then on, the plugin
    // composites that texture every present.
    //
    // keyed-mutex contract: your app releases with keyConsumer (plugin
    // acquires with keyConsumer) and acquires with keyProducer (plugin
    // releases with keyProducer). the plugin starts by waiting on
    // keyProducer, so give it the first slot by priming the mutex with
    // Acquire(0) / Release(keyConsumer) before calling this.
    bool attachSharedTexture(uint64_t dupedHandle, uint32_t width, uint32_t height,
                             uint32_t keyProducer, uint32_t keyConsumer);
    bool detachSharedTexture();

private:
    bool openPipe(unsigned timeoutMs);
    bool doHandshake();

    bool writeExact(const void* buf, DWORD n);
    bool readExact(void* buf, DWORD n);
    bool sendCmd(uint8_t cmd, const void* payload, uint16_t payloadSize);

    HANDLE mPipe = INVALID_HANDLE_VALUE;
};

}  // namespace obs_overlay
