#include <windows.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "protocol.h"

namespace {
HWND g_main{};
std::atomic_bool g_stop{};
std::atomic_int g_x{240}, g_y{160};
std::atomic_bool g_leftDown{};
std::atomic_bool g_downInside{};
std::atomic_uint64_t g_clickCount{};
std::mutex g_eventMutex;
std::wstring g_lastEvent{L"Chưa nhận lệnh"};

std::wstring PipeName() {
    return std::wstring(vilab::kPipePrefix) + std::to_wstring(GetCurrentProcessId());
}

RECT HelpButtonRect(const RECT& rc) {
const int clientWidth = static_cast<int>(rc.right - rc.left);
const int clientHeight = static_cast<int>(rc.bottom - rc.top);
const int width = std::clamp(clientWidth * 36 / 100, 150, 280);
const int height = 54;
const int left = (clientWidth - width) / 2;
const int top = std::max(105, clientHeight * 62 / 100);
    return RECT{left, top, left + width, top + height};
}

POINT ResolvePoint(const vilab::Command& c, const RECT& rc) {
    POINT p{c.x, c.y};
    if ((c.flags & vilab::NormalizedCoordinates) != 0) {
        p.x = MulDiv(c.x, std::max(1L, rc.right), vilab::kCoordinateScale);
        p.y = MulDiv(c.y, std::max(1L, rc.bottom), vilab::kCoordinateScale);
    }
    p.x = std::clamp<LONG>(p.x, 0, std::max(0L, rc.right - 1));
    p.y = std::clamp<LONG>(p.y, 0, std::max(0L, rc.bottom - 1));
    return p;
}

bool Inside(const RECT& rect, POINT point) {
    return PtInRect(&rect, point) != FALSE;
}

void SetLastEvent(const wchar_t* kind, const vilab::Command& c, POINT p, bool hit) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::lock_guard lock(g_eventMutex);
    g_lastEvent = std::wstring(kind) + L"  X=" + std::to_wstring(p.x) +
        L", Y=" + std::to_wstring(p.y) + L"  |  Seq=" + std::to_wstring(c.sequence) +
        L"  |  " + (hit ? L"TRÚNG NÚT" : L"ngoài nút") + L"  |  " +
        std::to_wstring(time.wHour) + L":" + std::to_wstring(time.wMinute) + L":" +
        std::to_wstring(time.wSecond) + L"." + std::to_wstring(time.wMilliseconds);
}

void Apply(const vilab::Command& c) {
    if (c.magic != vilab::kMagic || !IsWindow(g_main)) return;
    RECT rc{};
    GetClientRect(g_main, &rc);
    const POINT p = ResolvePoint(c, rc);
    const RECT button = HelpButtonRect(rc);
    g_x = p.x;
    g_y = p.y;

    switch (c.type) {
    case vilab::CommandType::Move:
        SetLastEvent(L"MOVE", c, p, Inside(button, p));
        break;
    case vilab::CommandType::LeftDown:
        g_leftDown = true;
        g_downInside = Inside(button, p);
        SetLastEvent(L"LEFT DOWN", c, p, g_downInside);
        break;
    case vilab::CommandType::LeftUp: {
        const bool hit = g_downInside && Inside(button, p);
        g_leftDown = false;
        g_downInside = false;
        if (hit) ++g_clickCount;
        SetLastEvent(L"LEFT UP", c, p, hit);
        break;
    }
    case vilab::CommandType::Click: {
        const bool hit = Inside(button, p);
        g_leftDown = true;
        g_downInside = hit;
        SetLastEvent(L"LEFT DOWN", c, p, hit);
        InvalidateRect(g_main, nullptr, FALSE);
        Sleep(std::min<DWORD>(c.holdMs, 500));
        g_leftDown = false;
        g_downInside = false;
        if (hit) ++g_clickCount;
        SetLastEvent(L"LEFT UP", c, p, hit);
        break;
    }
    case vilab::CommandType::Reset:
        g_clickCount = 0;
        g_leftDown = false;
        g_downInside = false;
        SetLastEvent(L"RESET", c, p, false);
        break;
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
    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(25, 38, 55));
        const std::wstring title = L"Virtual Input Target V2 – PID " +
            std::to_wstring(GetCurrentProcessId()) + L" – " +
            std::to_wstring(rc.right) + L"x" + std::to_wstring(rc.bottom);
        TextOutW(dc, 20, 16, title.c_str(), static_cast<int>(title.size()));
        const int x = g_x.load(), y = g_y.load();
        const std::wstring info = L"Tọa độ nhận: X=" + std::to_wstring(x) + L", Y=" +
            std::to_wstring(y) + L"   |   Click trúng nút: " +
            std::to_wstring(g_clickCount.load());
        TextOutW(dc, 20, 43, info.c_str(), static_cast<int>(info.size()));

        RECT button = HelpButtonRect(rc);
        const bool pressed = g_leftDown && g_downInside;
        HBRUSH buttonBrush = CreateSolidBrush(pressed ? RGB(220, 156, 30) : RGB(252, 195, 45));
        FillRect(dc, &button, buttonBrush);
        DeleteObject(buttonBrush);
        FrameRect(dc, &button, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        SetTextColor(dc, RGB(35, 31, 20));
        RECT textRect = button;
        const std::wstring label = pressed ? L"TRỢ GIÚP (ĐANG NHẤN)" : L"TRỢ GIÚP";
        DrawTextW(dc, label.c_str(), -1, &textRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        HBRUSH cursorBrush = CreateSolidBrush(g_leftDown ? RGB(255, 90, 70) : RGB(35, 190, 105));
        HBRUSH old = static_cast<HBRUSH>(SelectObject(dc, cursorBrush));
        Ellipse(dc, x - 10, y - 10, x + 10, y + 10);
        SelectObject(dc, old);
        DeleteObject(cursorBrush);

        std::wstring eventText;
        { std::lock_guard lock(g_eventMutex); eventText = g_lastEvent; }
        SetTextColor(dc, RGB(55, 70, 90));
        TextOutW(dc, 20, rc.bottom - 58, eventText.c_str(), static_cast<int>(eventText.size()));
        const std::wstring note = L"Chỉ tăng bộ đếm khi DOWN và UP cùng nằm trong nút. Con trỏ Windows không bị di chuyển.";
        TextOutW(dc, 20, rc.bottom - 32, note.c_str(), static_cast<int>(note.size()));
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
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = vilab::kTargetClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, wc.lpszClassName, L"Virtual Input Target V2",
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
