#include <windows.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include "protocol.h"

namespace {
HINSTANCE g_instance{};
HWND g_main{};
std::atomic_bool g_stop{};
std::atomic_int g_x{240}, g_y{160};
std::atomic_bool g_leftDown{};
std::atomic_uint64_t g_clickCount{};

std::wstring PipeName() {
    return std::wstring(vilab::kPipePrefix) + std::to_wstring(GetCurrentProcessId());
}

void Apply(const vilab::Command& c) {
    if (c.magic != 0x56494C31) return;
    RECT rc{};
    GetClientRect(g_main, &rc);
    g_x = std::clamp<int>(c.x, 0, std::max(0L, rc.right - 1));
    g_y = std::clamp<int>(c.y, 0, std::max(0L, rc.bottom - 1));
    switch (c.type) {
    case vilab::CommandType::LeftDown: g_leftDown = true; break;
    case vilab::CommandType::LeftUp: g_leftDown = false; ++g_clickCount; break;
    case vilab::CommandType::Click:
        g_leftDown = true;
        InvalidateRect(g_main, nullptr, FALSE);
        Sleep(std::min<DWORD>(c.holdMs, 500));
        g_leftDown = false;
        ++g_clickCount;
        break;
    case vilab::CommandType::Reset: g_clickCount = 0; g_leftDown = false; break;
    default: break;
    }
    InvalidateRect(g_main, nullptr, FALSE);
}

void PipeLoop() {
    const auto name = PipeName();
    while (!g_stop) {
        HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, sizeof(vilab::Command), sizeof(vilab::Command), 250, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return;
        const BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE :
            (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            vilab::Command command{};
            DWORD read{};
            while (!g_stop && ReadFile(pipe, &command, sizeof(command), &read, nullptr) &&
                   read == sizeof(command)) {
                Apply(command);
            }
        }
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        std::thread(PipeLoop).detach();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{}; GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(25, 38, 55));
        std::wstring title = L"Ứng dụng thử nghiệm – PID " + std::to_wstring(GetCurrentProcessId());
        TextOutW(dc, 20, 18, title.c_str(), static_cast<int>(title.size()));
        const int x = g_x.load(), y = g_y.load();
        std::wstring info = L"Tọa độ ảo: X=" + std::to_wstring(x) + L", Y=" +
            std::to_wstring(y) + L"   |   Số click: " + std::to_wstring(g_clickCount.load());
        TextOutW(dc, 20, 45, info.c_str(), static_cast<int>(info.size()));
        HBRUSH brush = CreateSolidBrush(g_leftDown ? RGB(255, 110, 80) : RGB(35, 190, 105));
        HBRUSH old = static_cast<HBRUSH>(SelectObject(dc, brush));
        Ellipse(dc, x - 12, y - 12, x + 12, y + 12);
        SelectObject(dc, old); DeleteObject(brush);
        const std::wstring note = L"Con trỏ thật không bị di chuyển. Dấu tròn là con trỏ ảo của riêng tiến trình này.";
        TextOutW(dc, 20, rc.bottom - 34, note.c_str(), static_cast<int>(note.size()));
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        g_stop = true;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = vilab::kTargetClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, wc.lpszClassName, L"Virtual Input Target",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 420,
        nullptr, nullptr, instance, nullptr);
    if (!g_main) return 1;
    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
