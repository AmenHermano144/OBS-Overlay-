# example

Smallest possible program: connect, draw a rectangle + text, hold for 5s, disconnect. Replace the drawing block with whatever you actually want to render.

Depends on the static library from `../implementation`.

## Build

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

Binary: `build\Release\obs_overlay_example.exe`.

## Run

OBS Studio must be installed. If the plugin is loaded you'll see the rectangle on the primary monitor for ~5 seconds — visible on screen but excluded from any OBS capture source. If OBS isn't running, the example launches it minimized to tray.
