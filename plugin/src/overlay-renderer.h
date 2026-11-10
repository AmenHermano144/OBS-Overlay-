#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dcomp.h>
#include <wrl/client.h>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "common.h"

namespace hco {

// composites submitted draw commands + an optional cross-process shared texture
// via D2D on top of a DirectComposition swap chain.
class OverlayRenderer {
public:
    OverlayRenderer();
    ~OverlayRenderer();

    bool initialize(HWND hwnd, uint32_t width, uint32_t height);
    void shutdown();
    void resize(uint32_t width, uint32_t height);

    bool isInitialized() const { return mSwapChain != nullptr; }

    void submitFrame(CommandList commands);
    void presentIfDirty();

    // called from the ipc thread. the actual OpenSharedResource1 + bitmap
    // creation happens on the window thread inside the next presentIfDirty.
    void queueOpenSharedTexture(HANDLE ntHandle, uint32_t width, uint32_t height,
                                uint32_t keyProducer, uint32_t keyConsumer);
    void queueCloseSharedTexture();

private:
    template <typename T> using CP = Microsoft::WRL::ComPtr<T>;

    bool createDevice();
    bool createComposition(HWND hwnd);
    bool createSwapChain(uint32_t width, uint32_t height);
    bool createD2D();
    bool bindBackbuffer();

    void renderCommands(const CommandList& cmds);
    CP<ID2D1SolidColorBrush> brushFor(uint32_t rgba);
    CP<IDWriteTextFormat>    formatFor(float size);

    bool openSharedTexture(HANDLE ntHandle, uint32_t width, uint32_t height,
                           uint32_t keyProducer, uint32_t keyConsumer);
    void closeSharedTexture();
    void drainSharedTexRequests();
    void compositeSharedTexture();

    CP<ID3D11Device>        mD3dDevice;
    CP<ID3D11DeviceContext> mD3dContext;
    CP<IDXGIFactory2>       mDxgiFactory;
    CP<IDXGISwapChain1>     mSwapChain;

    CP<IDCompositionDevice> mDCompDevice;
    CP<IDCompositionTarget> mDCompTarget;
    CP<IDCompositionVisual> mDCompVisual;

    CP<ID2D1Factory1>       mD2dFactory;
    CP<ID2D1Device>         mD2dDevice;
    CP<ID2D1DeviceContext>  mD2dContext;
    CP<ID2D1Bitmap1>        mD2dTarget;

    CP<IDWriteFactory>      mDWriteFactory;

    std::unordered_map<int, CP<IDWriteTextFormat>> mFormatCache;

    uint32_t mWidth  = 0;
    uint32_t mHeight = 0;
    HWND     mHwnd   = nullptr;

    std::mutex   mFrameMutex;
    CommandList  mPendingFrame;
    bool         mDirty = false;

    CP<ID3D11Texture2D> mSharedTex;
    CP<IDXGIKeyedMutex> mSharedKeyedMutex;
    CP<ID2D1Bitmap1>    mSharedBitmap;
    uint32_t            mSharedWidth       = 0;
    uint32_t            mSharedHeight      = 0;
    uint32_t            mSharedKeyProducer = 1;
    uint32_t            mSharedKeyConsumer = 0;

    struct PendingOpen {
        HANDLE   handle;
        uint32_t width;
        uint32_t height;
        uint32_t keyProducer;
        uint32_t keyConsumer;
    };
    std::mutex        mSharedRequestMutex;
    bool              mHasPendingOpen = false;
    PendingOpen       mPendingOpen{};
    std::atomic<bool> mSharedCloseRequested{false};
};

}  // namespace hco
