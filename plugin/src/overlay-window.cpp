#include "overlay-window.h"
#include "common.h"

#include <util/base.h>

namespace hco {

OverlayWindow* OverlayWindow::sInstance = nullptr;

static constexpr UINT WM_HCO_QUIT = WM_APP + 1;
static constexpr UINT WM_HCO_WAKE = WM_APP + 2;

OverlayWindow::OverlayWindow() {
    sInstance = this;
}

OverlayWindow::~OverlayWindow() {
    stop();
    sInstance = nullptr;
}

void OverlayWindow::setCallbacks(CreatedCallback onCreated,
                                 ResizeCallback  onResize,
                                 TickCallback    onTick,
                                 DestroyCallback onDestroy) {
    mOnCreated = std::move(onCreated);
    mOnResize  = std::move(onResize);
    mOnTick    = std::move(onTick);
    mOnDestroy = std::move(onDestroy);
}

bool OverlayWindow::start(HINSTANCE hInst) {
    if (mRunning.exchange(true)) return false;
    mHInst = hInst;
    mThread = std::thread(&OverlayWindow::threadMain, this);
    return true;
}

void OverlayWindow::stop() {
    if (!mRunning.exchange(false)) return;
    const DWORD tid = mThreadId.load();
    if (tid) PostThreadMessageW(tid, WM_HCO_QUIT, 0, 0);
    if (mThread.joinable()) mThread.join();
}

void OverlayWindow::wake() {
    const DWORD tid = mThreadId.load();
    if (tid) PostThreadMessageW(tid, WM_HCO_WAKE, 0, 0);
}

void OverlayWindow::threadMain() {
    mThreadId = GetCurrentThreadId();

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &OverlayWindow::wndProc;
    wc.hInstance     = mHInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    // primary monitor only. extend here for multi-monitor support.
    const int sx = 0;
    const int sy = 0;
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    mWidth  = static_cast<uint32_t>(sw);
    mHeight = static_cast<uint32_t>(sh);

    const HWND hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT |
        WS_EX_TOPMOST |
        WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW |
        WS_EX_NOREDIRECTIONBITMAP,
        kWindowClass,
        kWindowTitle,
        WS_POPUP,
        sx, sy, sw, sh,
        nullptr, nullptr, mHInst, nullptr);

    if (!hwnd) {
        blog(LOG_ERROR, "[%s] CreateWindowExW failed %lu", kPluginName, GetLastError());
        UnregisterClassW(kWindowClass, mHInst);
        mRunning = false;
        return;
    }

    mHwnd = hwnd;

    // LAYERED post-creation: avoids the conflict with NOREDIRECTIONBITMAP at
    // CreateWindow time while still giving per-pixel click-through.
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    if (mOnCreated) mOnCreated(hwnd, mWidth.load(), mHeight.load());

    MSG msg;
    while (mRunning.load()) {
        BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0 || got == -1) break;

        if (msg.hwnd == nullptr) {
            if (msg.message == WM_HCO_QUIT) break;
            if (msg.message == WM_HCO_WAKE) {
                if (mOnTick) mOnTick();
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (mOnDestroy) mOnDestroy();

    DestroyWindow(hwnd);
    mHwnd = nullptr;
    UnregisterClassW(kWindowClass, mHInst);
    mThreadId = 0;
}

LRESULT CALLBACK OverlayWindow::wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            return 0;

        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_CLOSE) return 0;
            break;

        case WM_NCHITTEST:
            return HTTRANSPARENT;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_ACTIVATE:
            return 0;

        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE: {
            if (sInstance) {
                const int sw = GetSystemMetrics(SM_CXSCREEN);
                const int sh = GetSystemMetrics(SM_CYSCREEN);
                sInstance->mWidth  = static_cast<uint32_t>(sw);
                sInstance->mHeight = static_cast<uint32_t>(sh);
                SetWindowPos(h, HWND_TOPMOST, 0, 0, sw, sh,
                             SWP_NOACTIVATE | SWP_SHOWWINDOW);
                if (sInstance->mOnResize) {
                    sInstance->mOnResize(static_cast<uint32_t>(sw),
                                         static_cast<uint32_t>(sh));
                }
            }
            return 0;
        }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace hco
