#pragma once
#include <windows.h>

// Opens the optional VirtualInputLab V3-compatible IPC controller.
// The module is dormant until this window is opened and the user explicitly
// enables it. It never changes AutoSyncClean's existing Win32 synchronizer.
void ShowIpcController(HWND owner, HINSTANCE instance);
void ShutdownIpcController();
