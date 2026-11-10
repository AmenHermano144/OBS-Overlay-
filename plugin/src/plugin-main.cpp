#include <obs-module.h>
#include <util/base.h>
#include <Windows.h>
#include <memory>

#include "common.h"
#include "overlay-window.h"
#include "overlay-renderer.h"
#include "ipc-receiver.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-hidden-chat-overlay", "en-US")

namespace hco { void registerDummySource(); }

namespace {

std::unique_ptr<hco::OverlayWindow>   g_window;
std::unique_ptr<hco::OverlayRenderer> g_renderer;
std::unique_ptr<hco::IpcReceiver>     g_ipc;

HINSTANCE getSelfModule() {
    HMODULE h = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&getSelfModule),
        &h);
    return reinterpret_cast<HINSTANCE>(h);
}

void onWindowCreated(HWND hwnd, uint32_t w, uint32_t h) {
    if (!g_renderer->initialize(hwnd, w, h)) {
        blog(LOG_ERROR, "[%s] renderer init failed", hco::kPluginName);
        return;
    }
    g_ipc->start(
        g_renderer.get(),
        [] { if (g_window) g_window->wake(); },
        [] {
            // client dropped: release any shared texture and paint one blank frame.
            if (g_renderer) g_renderer->queueCloseSharedTexture();
            if (g_window)   g_window->wake();
        },
        [] {
            hco::PluginInfo info{};
            info.pid = GetCurrentProcessId();
            if (g_window) {
                info.hwnd   = reinterpret_cast<uint64_t>(g_window->hwnd());
                info.width  = g_window->width();
                info.height = g_window->height();
            }
            return info;
        });
    blog(LOG_INFO, "[%s] ready (%ux%u)", hco::kPluginName, w, h);
}

void onWindowResize(uint32_t w, uint32_t h) {
    if (g_renderer) g_renderer->resize(w, h);
}

void onWindowTick() {
    if (g_renderer) g_renderer->presentIfDirty();
}

void onWindowDestroy() {
    if (g_ipc)      g_ipc->stop();
    if (g_renderer) g_renderer->shutdown();
}

}  // namespace

bool obs_module_load(void) {
    blog(LOG_INFO, "[%s] loading", hco::kPluginName);

    g_window   = std::make_unique<hco::OverlayWindow>();
    g_renderer = std::make_unique<hco::OverlayRenderer>();
    g_ipc      = std::make_unique<hco::IpcReceiver>();

    g_window->setCallbacks(
        &onWindowCreated,
        &onWindowResize,
        &onWindowTick,
        &onWindowDestroy);

    if (!g_window->start(getSelfModule())) {
        blog(LOG_ERROR, "[%s] window start failed", hco::kPluginName);
        g_ipc.reset();
        g_renderer.reset();
        g_window.reset();
        return false;
    }

    hco::registerDummySource();
    blog(LOG_INFO, "[%s] loaded", hco::kPluginName);
    return true;
}

void obs_module_unload(void) {
    blog(LOG_INFO, "[%s] unloading", hco::kPluginName);
    if (g_window) g_window->stop();
    g_ipc.reset();
    g_renderer.reset();
    g_window.reset();
}

const char* obs_module_name(void)        { return hco::kPluginName; }
const char* obs_module_description(void) { return "Top-most overlay excluded from captured video."; }
