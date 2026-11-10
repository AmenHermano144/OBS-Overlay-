# implementation

C++ client library for the OBS plugin. Statically linkable; depends only on `bcrypt`, `advapi32` and `shell32`. No D3D11 or ImGui required for the shape API.

## Public surface

```
obs_overlay::launcher::ensureRunning()         // auto-launch OBS if needed
obs_overlay::Client                            // the overlay client
    connect() / disconnect() / isConnected()
    getPluginInfo(out)                         // pid, hwnd, width, height
    clear() / rect() / filledRect() / line() / text()
    present()
    attachSharedTexture() / detachSharedTexture()
```

Everything lives under `include/obs_overlay/`. Add the `include/` directory to your project and compile `src/client.cpp` + `src/obs_launcher.cpp`, or build the static library via CMake:

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

## How it works

1. `connect()` launches OBS (if `autoLaunchObs == true`), opens `\\.\pipe\obs-hidden-chat-overlay`, and performs the HMAC-SHA256 challenge/response handshake.
2. Each shape call writes an 8-byte frame header + payload to the pipe. Commands are buffered server-side until you call `present()`.
3. `getPluginInfo()` returns the plugin's pid, hwnd and overlay dimensions. Use the pid to `DuplicateHandle` a shared D3D11 texture into the plugin's process, then call `attachSharedTexture()`.
4. The plugin composites the shared texture as the background of each frame; shapes submitted via `clear/rect/filledRect/line/text/present` draw on top.

## Shared secret

`kSharedSecret` in `src/client.cpp` must byte-for-byte match the one in `plugin/src/ipc-receiver.cpp`. Rotate both sides together.

## Minimal usage

```cpp
#include <obs_overlay/client.h>

int main() {
    obs_overlay::Client c;
    if (!c.connect()) return 1;

    obs_overlay::PluginInfo info{};
    c.getPluginInfo(info);

    for (int i = 0; i < 300; ++i) {
        c.clear();
        c.filledRect(100.f, 100.f, 400.f, 80.f, 0xC0202020u);
        c.text(120.f, 120.f, 22.f, 0xFFFFFFFFu, "overlay works");
        if (!c.present()) break;
        Sleep(16);
    }
    return 0;
}
```

See `example/` for a runnable version.
