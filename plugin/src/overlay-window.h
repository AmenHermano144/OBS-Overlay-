#pragma once

#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

namespace hco {

// top-most, click-through, WDA_EXCLUDEFROMCAPTURE window covering the primary
// monitor. owns its own message thread; callbacks fire from that thread.
class OverlayWindow {
public:
    using CreatedCallback = std::function<void(HWND, uint32_t, uint32_t)>;
    using ResizeCallback  = std::function<void(uint32_t, uint32_t)>;
    using TickCallback    = std::function<void()>;
    using DestroyCallback = std::function<void()>;

    OverlayWindow();
    ~OverlayWindow();

    void setCallbacks(CreatedCallback onCreated,
                      ResizeCallback  onResize,
                      TickCallback    onTick,
                      DestroyCallback onDestroy);

    bool start(HINSTANCE hInst);
    void stop();

    // nudge the window thread to run a tick now.
    void wake();

    HWND     hwnd()   const { return mHwnd.load(); }
    uint32_t width()  const { return mWidth.load(); }
    uint32_t height() const { return mHeight.load(); }

private:
    void threadMain();
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);

    std::thread           mThread;
    std::atomic<bool>     mRunning{false};
    std::atomic<HWND>     mHwnd{nullptr};
    std::atomic<uint32_t> mWidth{0};
    std::atomic<uint32_t> mHeight{0};
    std::atomic<DWORD>    mThreadId{0};
    HINSTANCE             mHInst = nullptr;

    CreatedCallback mOnCreated;
    ResizeCallback  mOnResize;
    TickCallback    mOnTick;
    DestroyCallback mOnDestroy;

    static OverlayWindow* sInstance;
};

}  // namespace hco
