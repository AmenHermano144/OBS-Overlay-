# plugin

OBS plugin that creates a transparent, click-through, top-most window covering the primary monitor. The window is marked `WDA_EXCLUDEFROMCAPTURE`, so anything drawn on it is visible on your desktop but excluded from any OBS capture source.

The plugin exposes a named pipe (`\\.\pipe\obs-hidden-chat-overlay`). External processes connect, authenticate via HMAC-SHA256 challenge/response, and send draw commands (D2D shapes or a cross-process D3D11 shared texture).

## Build

Requires OBS Studio source or SDK checked out somewhere. Point `LIBOBS_PATH` at it.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DLIBOBS_PATH="C:/dev/obs-studio"
cmake --build build --config RelWithDebInfo
```

## Install

Copy the DLL and `data/` folder under the OBS plugin root:

```
%ProgramData%\obs-studio\plugins\obs-hidden-chat-overlay\bin\64bit\obs-hidden-chat-overlay.dll
%ProgramData%\obs-studio\plugins\obs-hidden-chat-overlay\data\locale\en-US.ini
```

Launch OBS and check `Help > Log Files > View Current Log` for a `[obs-hidden-chat-overlay] loaded` line.

## Files

- `common.h` — wire protocol constants, enums and payload structs. Kept in sync with the implementation library.
- `plugin-main.cpp` — `obs_module_load` / `obs_module_unload`, wires the window, renderer and ipc together.
- `overlay-window.h/.cpp` — the top-most, click-through, streamproof HWND. Owns its own message thread.
- `overlay-renderer.h/.cpp` — D2D/DirectComposition compositor. Draws shapes and a shared texture onto a swap chain.
- `ipc-receiver.h/.cpp` — named pipe server, HMAC handshake, command decoder.
- `source-registrar.cpp` — registers a no-op dummy source so the plugin appears in OBS's source menu.

## Shared secret

`kSharedSecret` in `src/ipc-receiver.cpp` must byte-for-byte match the one the client uses. Rotate both sides together for a release.
