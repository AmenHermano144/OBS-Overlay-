#include "overlay-renderer.h"

#include <util/base.h>

using Microsoft::WRL::ComPtr;

namespace hco {

static D2D1_COLOR_F rgbaToColor(uint32_t rgba) {
    return D2D1::ColorF(
        ((rgba >> 24) & 0xFF) / 255.0f,
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >>  8) & 0xFF) / 255.0f,
        ( rgba        & 0xFF) / 255.0f);
}

OverlayRenderer::OverlayRenderer() = default;
OverlayRenderer::~OverlayRenderer() { shutdown(); }

bool OverlayRenderer::initialize(HWND hwnd, uint32_t width, uint32_t height) {
    mHwnd   = hwnd;
    mWidth  = width  ? width  : 1;
    mHeight = height ? height : 1;

    if (!createDevice())                   return false;
    if (!createComposition(hwnd))          return false;
    if (!createSwapChain(mWidth, mHeight)) return false;
    if (!createD2D())                      return false;
    if (!bindBackbuffer())                 return false;
    return true;
}

void OverlayRenderer::shutdown() {
    mFormatCache.clear();
    closeSharedTexture();
    mD2dTarget.Reset();
    mD2dContext.Reset();
    mD2dDevice.Reset();
    mD2dFactory.Reset();
    mDWriteFactory.Reset();
    mDCompVisual.Reset();
    mDCompTarget.Reset();
    mDCompDevice.Reset();
    mSwapChain.Reset();
    mDxgiFactory.Reset();
    mD3dContext.Reset();
    mD3dDevice.Reset();
}

bool OverlayRenderer::createDevice() {
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
    };
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &mD3dDevice, nullptr, &mD3dContext);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] D3D11CreateDevice 0x%08lx", kPluginName, hr);
        return false;
    }

    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(mD3dDevice.As(&dxgi))) return false;
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi->GetAdapter(&adapter))) return false;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&mDxgiFactory)))) return false;
    return true;
}

bool OverlayRenderer::createComposition(HWND hwnd) {
    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(mD3dDevice.As(&dxgi))) return false;

    HRESULT hr = DCompositionCreateDevice(dxgi.Get(), IID_PPV_ARGS(&mDCompDevice));
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] DCompositionCreateDevice 0x%08lx", kPluginName, hr);
        return false;
    }
    hr = mDCompDevice->CreateTargetForHwnd(hwnd, TRUE, &mDCompTarget);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] CreateTargetForHwnd 0x%08lx", kPluginName, hr);
        return false;
    }
    hr = mDCompDevice->CreateVisual(&mDCompVisual);
    if (FAILED(hr)) return false;
    return true;
}

bool OverlayRenderer::createSwapChain(uint32_t width, uint32_t height) {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width            = width;
    desc.Height           = height;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = 2;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;

    HRESULT hr = mDxgiFactory->CreateSwapChainForComposition(
        mD3dDevice.Get(), &desc, nullptr, &mSwapChain);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] CreateSwapChainForComposition 0x%08lx", kPluginName, hr);
        return false;
    }

    mDCompVisual->SetContent(mSwapChain.Get());
    mDCompTarget->SetRoot(mDCompVisual.Get());
    mDCompDevice->Commit();
    return true;
}

bool OverlayRenderer::createD2D() {
    D2D1_FACTORY_OPTIONS opts{};
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),
        &opts,
        reinterpret_cast<void**>(mD2dFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice> dxgi;
    if (FAILED(mD3dDevice.As(&dxgi))) return false;

    hr = mD2dFactory->CreateDevice(dxgi.Get(), &mD2dDevice);
    if (FAILED(hr)) return false;

    hr = mD2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mD2dContext);
    if (FAILED(hr)) return false;

    mD2dContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    mD2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(mDWriteFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    return true;
}

bool OverlayRenderer::bindBackbuffer() {
    ComPtr<IDXGISurface> surface;
    HRESULT hr = mSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) return false;

    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    hr = mD2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &mD2dTarget);
    if (FAILED(hr)) return false;

    mD2dContext->SetTarget(mD2dTarget.Get());
    return true;
}

void OverlayRenderer::resize(uint32_t width, uint32_t height) {
    if (!mSwapChain) return;
    if (width == 0)  width = 1;
    if (height == 0) height = 1;
    if (width == mWidth && height == mHeight) return;

    mWidth  = width;
    mHeight = height;
    mD2dContext->SetTarget(nullptr);
    mD2dTarget.Reset();

    HRESULT hr = mSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] ResizeBuffers 0x%08lx", kPluginName, hr);
        return;
    }
    bindBackbuffer();
}

void OverlayRenderer::submitFrame(CommandList commands) {
    std::lock_guard lk(mFrameMutex);
    mPendingFrame = std::move(commands);
    mDirty = true;
}

void OverlayRenderer::queueOpenSharedTexture(HANDLE ntHandle, uint32_t width, uint32_t height,
                                             uint32_t keyProducer, uint32_t keyConsumer) {
    // if two SharedTexture cmds arrive back-to-back before the window drains,
    // close the pending handle so we don't leak into the plugin's handle table.
    std::lock_guard lk(mSharedRequestMutex);
    if (mHasPendingOpen && mPendingOpen.handle) {
        CloseHandle(mPendingOpen.handle);
    }
    mPendingOpen = { ntHandle, width, height, keyProducer, keyConsumer };
    mHasPendingOpen = true;

    std::lock_guard f(mFrameMutex);
    mDirty = true;
}

void OverlayRenderer::queueCloseSharedTexture() {
    mSharedCloseRequested.store(true);
    std::lock_guard lk(mFrameMutex);
    mDirty = true;
}

bool OverlayRenderer::openSharedTexture(HANDLE ntHandle, uint32_t width, uint32_t height,
                                        uint32_t keyProducer, uint32_t keyConsumer) {
    closeSharedTexture();

    if (!ntHandle || width == 0 || height == 0 || width > 8192 || height > 8192) {
        blog(LOG_WARNING, "[%s] openSharedTexture: bogus params h=%p %ux%u",
             kPluginName, ntHandle, width, height);
        if (ntHandle) CloseHandle(ntHandle);
        return false;
    }

    ComPtr<ID3D11Device1> device1;
    HRESULT hr = mD3dDevice.As(&device1);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] QI ID3D11Device1 0x%08lx", kPluginName, hr);
        CloseHandle(ntHandle);
        return false;
    }

    ComPtr<ID3D11Texture2D> tex;
    hr = device1->OpenSharedResource1(ntHandle, IID_PPV_ARGS(&tex));
    // OpenSharedResource1 doesn't take ownership of the NT handle.
    CloseHandle(ntHandle);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] OpenSharedResource1 0x%08lx (integrity level mismatch?)",
             kPluginName, hr);
        return false;
    }

    ComPtr<IDXGIKeyedMutex> mutex;
    hr = tex.As(&mutex);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] QI IDXGIKeyedMutex 0x%08lx", kPluginName, hr);
        return false;
    }

    ComPtr<IDXGISurface> surf;
    hr = tex.As(&surf);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] QI IDXGISurface 0x%08lx", kPluginName, hr);
        return false;
    }

    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));

    ComPtr<ID2D1Bitmap1> bitmap;
    hr = mD2dContext->CreateBitmapFromDxgiSurface(surf.Get(), &props, &bitmap);
    if (FAILED(hr)) {
        blog(LOG_ERROR, "[%s] CreateBitmapFromDxgiSurface(shared) 0x%08lx", kPluginName, hr);
        return false;
    }

    mSharedTex         = std::move(tex);
    mSharedKeyedMutex  = std::move(mutex);
    mSharedBitmap      = std::move(bitmap);
    mSharedWidth       = width;
    mSharedHeight      = height;
    mSharedKeyProducer = keyProducer;
    mSharedKeyConsumer = keyConsumer;
    blog(LOG_INFO, "[%s] shared texture opened (%ux%u, kProd=%u kCons=%u)",
         kPluginName, width, height, keyProducer, keyConsumer);
    return true;
}

void OverlayRenderer::closeSharedTexture() {
    mSharedBitmap.Reset();
    mSharedKeyedMutex.Reset();
    mSharedTex.Reset();
    mSharedWidth = mSharedHeight = 0;
}

void OverlayRenderer::drainSharedTexRequests() {
    if (mSharedCloseRequested.exchange(false)) {
        blog(LOG_INFO, "[%s] shared tex close requested", kPluginName);
        closeSharedTexture();
    }

    PendingOpen open{};
    bool hasOpen = false;
    {
        std::lock_guard lk(mSharedRequestMutex);
        if (mHasPendingOpen) {
            open = mPendingOpen;
            hasOpen = true;
            mHasPendingOpen = false;
            mPendingOpen = {};
        }
    }
    if (hasOpen) {
        openSharedTexture(open.handle, open.width, open.height,
                          open.keyProducer, open.keyConsumer);
    }
}

void OverlayRenderer::compositeSharedTexture() {
    if (!mSharedBitmap || !mSharedKeyedMutex) return;

    // non-blocking acquire: skip this composite if the producer hasn't handed
    // us a new frame yet. keeps our vsync cadence independent from theirs.
    HRESULT hr = mSharedKeyedMutex->AcquireSync(mSharedKeyProducer, 0);
    if (hr == static_cast<HRESULT>(WAIT_TIMEOUT)) return;
    if (FAILED(hr)) {
        blog(LOG_WARNING, "[%s] shared AcquireSync(%u) 0x%08lx",
             kPluginName, mSharedKeyProducer, hr);
        return;
    }

    const D2D1_RECT_F dst = D2D1::RectF(0.f, 0.f,
        static_cast<float>(mWidth), static_cast<float>(mHeight));
    mD2dContext->DrawBitmap(mSharedBitmap.Get(), dst, 1.0f,
        D2D1_INTERPOLATION_MODE_LINEAR);

    HRESULT rel = mSharedKeyedMutex->ReleaseSync(mSharedKeyConsumer);
    if (FAILED(rel)) {
        blog(LOG_WARNING, "[%s] shared ReleaseSync(%u) 0x%08lx",
             kPluginName, mSharedKeyConsumer, rel);
    }
}

void OverlayRenderer::presentIfDirty() {
    if (!mSwapChain) return;

    drainSharedTexRequests();

    CommandList frame;
    {
        std::lock_guard lk(mFrameMutex);
        if (!mDirty) return;
        frame  = std::move(mPendingFrame);
        mDirty = false;
    }
    renderCommands(frame);
}

Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> OverlayRenderer::brushFor(uint32_t rgba) {
    ComPtr<ID2D1SolidColorBrush> brush;
    mD2dContext->CreateSolidColorBrush(rgbaToColor(rgba), &brush);
    return brush;
}

Microsoft::WRL::ComPtr<IDWriteTextFormat> OverlayRenderer::formatFor(float size) {
    const int key = static_cast<int>(size * 100.0f + 0.5f);
    auto it = mFormatCache.find(key);
    if (it != mFormatCache.end()) return it->second;

    ComPtr<IDWriteTextFormat> fmt;
    HRESULT hr = mDWriteFactory->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size, L"en-us", &fmt);
    if (FAILED(hr)) return {};
    mFormatCache[key] = fmt;
    return fmt;
}

void OverlayRenderer::renderCommands(const CommandList& cmds) {
    mD2dContext->BeginDraw();
    mD2dContext->Clear(D2D1::ColorF(0, 0, 0, 0));

    compositeSharedTexture();

    for (const auto& c : cmds) {
        auto brush = brushFor(c.rgba);
        if (!brush) continue;

        switch (c.type) {
            case Command::Rect: {
                const D2D1_RECT_F r = D2D1::RectF(c.x, c.y, c.x + c.x2, c.y + c.y2);
                mD2dContext->DrawRectangle(r, brush.Get(), c.thickness);
                break;
            }
            case Command::FilledRect: {
                const D2D1_RECT_F r = D2D1::RectF(c.x, c.y, c.x + c.x2, c.y + c.y2);
                mD2dContext->FillRectangle(r, brush.Get());
                break;
            }
            case Command::Line: {
                mD2dContext->DrawLine(
                    D2D1::Point2F(c.x, c.y),
                    D2D1::Point2F(c.x2, c.y2),
                    brush.Get(), c.thickness);
                break;
            }
            case Command::Text: {
                auto fmt = formatFor(c.fontSize);
                if (!fmt) break;
                const D2D1_RECT_F layout = D2D1::RectF(c.x, c.y, c.x + 4096.f, c.y + 256.f);
                mD2dContext->DrawTextW(
                    c.text.c_str(),
                    static_cast<UINT32>(c.text.size()),
                    fmt.Get(), layout, brush.Get());
                break;
            }
            default:
                break;
        }
    }

    HRESULT hr = mD2dContext->EndDraw();
    if (FAILED(hr)) {
        blog(LOG_WARNING, "[%s] EndDraw 0x%08lx", kPluginName, hr);
    }

    hr = mSwapChain->Present(1, 0);
    if (FAILED(hr)) {
        blog(LOG_WARNING, "[%s] Present 0x%08lx", kPluginName, hr);
    }
    mDCompDevice->Commit();
}

}  // namespace hco
