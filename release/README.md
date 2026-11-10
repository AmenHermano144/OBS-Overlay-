# release

Ready-to-install build of the OBS plugin. Drop this folder under `%ProgramData%\obs-studio\plugins\` and OBS will pick it up on next launch.

## Layout

```
release/
├── install.ps1
├── uninstall.ps1
└── obs-hidden-chat-overlay/
    ├── bin/
    │   └── 64bit/
    │       └── obs-hidden-chat-overlay.dll
    └── data/
        └── locale/
            └── en-US.ini
```

## Install (automated)

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

This copies `obs-hidden-chat-overlay/` into `%ProgramData%\obs-studio\plugins\`, replacing any previous install.

## Install (manual)

Copy the `obs-hidden-chat-overlay/` folder as-is into:

```
%ProgramData%\obs-studio\plugins\
```

Final paths:

```
%ProgramData%\obs-studio\plugins\obs-hidden-chat-overlay\bin\64bit\obs-hidden-chat-overlay.dll
%ProgramData%\obs-studio\plugins\obs-hidden-chat-overlay\data\locale\en-US.ini
```

## Verify

Launch OBS Studio and open `Help > Log Files > View Current Log`. You should see a line like:

```
[obs-hidden-chat-overlay] loaded
```

Then run the example (`example/build/Release/obs_overlay_example.exe`) and confirm the animated panel appears on your primary monitor.

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall.ps1
```

Or just delete `%ProgramData%\obs-studio\plugins\obs-hidden-chat-overlay\`.

## Rebuilding

Source is in `../plugin/`. See `../plugin/README.md` for build steps. Drop the resulting DLL into `obs-hidden-chat-overlay/bin/64bit/` to repackage this folder.
