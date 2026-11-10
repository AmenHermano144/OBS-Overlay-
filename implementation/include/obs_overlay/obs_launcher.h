#pragma once
#include <string>

namespace obs_overlay::launcher {

bool isRunning();

// returns the full path to obs64.exe (or obs32.exe on older installs), empty if
// not found. probes the registry and common Program Files locations.
std::wstring findInstallPath();

// launches OBS with --minimize-to-tray --disable-shutdown-check. the working
// directory is set to the exe's bin folder — OBS derives its data/plugins
// paths from CWD, so launching without it breaks plugin discovery.
bool launch();

// returns true immediately if OBS is already running. otherwise launches it
// and polls for the process to appear (~15s). false if OBS isn't installed or
// never came up.
bool ensureRunning();

}  // namespace obs_overlay::launcher
