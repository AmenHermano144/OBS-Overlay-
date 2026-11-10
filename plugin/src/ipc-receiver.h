#pragma once

#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

namespace hco {

class OverlayRenderer;

struct PluginInfo {
    uint32_t pid;
    uint64_t hwnd;
    uint32_t width;
    uint32_t height;
};

// listens on the named pipe, authenticates clients, decodes draw commands and
// forwards them to the renderer. one client at a time.
class IpcReceiver {
public:
    using WakeCallback       = std::function<void()>;
    using DisconnectCallback = std::function<void()>;
    using InfoProvider       = std::function<PluginInfo()>;

    IpcReceiver();
    ~IpcReceiver();

    bool start(OverlayRenderer*   renderer,
               WakeCallback       onFrameReady,
               DisconnectCallback onClientDisconnect,
               InfoProvider       infoProvider);
    void stop();

private:
    void serverLoop();
    bool handleClient(HANDLE pipe);
    bool authenticate(HANDLE pipe);

    OverlayRenderer*   mRenderer = nullptr;
    WakeCallback       mOnFrameReady;
    DisconnectCallback mOnClientDisconnect;
    InfoProvider       mInfoProvider;

    std::thread       mThread;
    std::atomic<bool> mRunning{false};
    HANDLE            mCancelEvent = nullptr;
};

}  // namespace hco
