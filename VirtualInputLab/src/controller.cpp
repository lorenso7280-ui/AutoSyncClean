#include <windows.h>
#include <commctrl.h>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "protocol.h"

namespace {
enum { IDC_LIST = 1001, IDC_REFRESH, IDC_X, IDC_Y, IDC_CLICK, IDC_REPEAT, IDC_GAP, IDC_START };
HINSTANCE g_instance{};
HWND g_main{}, g_list{}, g_x{}, g_y{}, g_repeat{}, g_gap{}, g_start{};
std::vector<DWORD> g_pids;
std::atomic_bool g_running{};

BOOL CALLBACK EnumTarget(HWND hwnd, LPARAM) {
    wchar_t cls[128]{};
    GetClassNameW(hwnd, cls, 128);
    if (lstrcmpW(cls, vilab::kTargetClass) != 0) return TRUE;
    DWORD pid{}; GetWindowThreadProcessId(hwnd, &pid);
    if (pid) g_pids.push_back(pid);
    return TRUE;
}

void Refresh() {
    g_pids.clear();
    EnumWindows(EnumTarget, 0);
    ListView_DeleteAllItems(g_list);
    for (size_t i = 0; i < g_pids.size(); ++i) {
        std::wstring number = std::to_wstring(i + 1);
        LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = static_cast<int>(i);
        item.pszText = number.data();
        int row = ListView_InsertItem(g_list, &item);
        std::wstring pid = std::to_wstring(g_pids[i]);
        ListView_SetItemText(g_list, row, 1, pid.data());
        ListView_SetCheckState(g_list, row, TRUE);
    }
}

int ReadInt(HWND edit, int fallback) {
    wchar_t b[32]{}; GetWindowTextW(edit, b, 32);
    wchar_t* end{}; long v = wcstol(b, &end, 10);
    return end == b ? fallback : static_cast<int>(v);
}

bool SendTo(DWORD pid, const vilab::Command& command) {
    std::wstring pipeName = std::wstring(vilab::kPipePrefix) + std::to_wstring(pid);
    if (!WaitNamedPipeW(pipeName.c_str(), 100)) return false;
    HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;
    DWORD written{};
    const bool ok = WriteFile(pipe, &command, sizeof(command), &written, nullptr) &&
                    written == sizeof(command);
    CloseHandle(pipe);
    return ok;
}

std::vector<DWORD> SelectedPids() {
    std::vector<DWORD> selected;
    for (int i = 0; i < ListView_GetItemCount(g_list); ++i) {
        if (ListView_GetCheckState(g_list, i) && i < static_cast<int>(g_pids.size()))
            selected.push_back(g_pids[static_cast<size_t>(i)]);
    }
    return selected;
}

void SendBatch(const std::vector<DWORD>& targets, int x, int y) {
    vilab::Command c{};
    c.type = vilab::CommandType::Click;
    c.x = x;
    c.y = y;
    c.holdMs = 30;
    c.sequence = GetTickCount64();
    std::vector<std::thread> workers;
    for (const DWORD pid : targets) {
        workers.emplace_back([pid, c] { SendTo(pid, c); });
    }
    for (auto& worker : workers) worker.join();
}

void BroadcastClick() {
    SendBatch(SelectedPids(), ReadInt(g_x, 240), ReadInt(g_y, 160));
}

void StartStop() {
    if (g_running.exchange(!g_running)) {
        SetWindowTextW(g_start, L"Bắt đầu lặp");
        return;
    }
    SetWindowTextW(g_start, L"Tạm dừng");
    const int repeat = ReadInt(g_repeat, 1);
    const int gap = ReadInt(g_gap, 1000);
    const int x = ReadInt(g_x, 240), y = ReadInt(g_y, 160);
    const auto targets = SelectedPids();
    std::thread([repeat, gap, x, y, targets] {
        for (int i = 0; g_running && i < repeat; ++i) {
            SendBatch(targets, x, y);
            for (int waited = 0; g_running && waited < gap; waited += 20) Sleep(20);
        }
        g_running = false;
        PostMessageW(g_main, WM_APP + 1, 0, 0);
    }).detach();
}

void AddColumn(int index, int width, const wchar_t* name) {
    LVCOLUMNW col{}; col.mask = LVCF_TEXT | LVCF_WIDTH; col.cx = width;
    col.pszText = const_cast<wchar_t*>(name);
    ListView_InsertColumn(g_list, index, &col);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT, 14, 48, 300, 300,
            hwnd, reinterpret_cast<HMENU>(IDC_LIST), g_instance, nullptr);
        ListView_SetExtendedListViewStyle(g_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        AddColumn(0, 80, L"#"); AddColumn(1, 180, L"PID ứng dụng thử nghiệm");
        CreateWindowW(L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, 335, 58, 25, 24, hwnd, nullptr, g_instance, nullptr);
        g_x = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"240", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            365, 54, 80, 26, hwnd, reinterpret_cast<HMENU>(IDC_X), g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, 455, 58, 25, 24, hwnd, nullptr, g_instance, nullptr);
        g_y = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"160", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            485, 54, 80, 26, hwnd, reinterpret_cast<HMENU>(IDC_Y), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Làm mới danh sách", WS_CHILD | WS_VISIBLE, 14, 14, 145, 28,
            hwnd, reinterpret_cast<HMENU>(IDC_REFRESH), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Click đồng thời", WS_CHILD | WS_VISIBLE, 335, 94, 230, 34,
            hwnd, reinterpret_cast<HMENU>(IDC_CLICK), g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Lặp lại:", WS_CHILD | WS_VISIBLE, 335, 152, 70, 24, hwnd, nullptr, g_instance, nullptr);
        g_repeat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"99999", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            415, 148, 150, 26, hwnd, reinterpret_cast<HMENU>(IDC_REPEAT), g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Giãn cách (ms):", WS_CHILD | WS_VISIBLE, 335, 188, 110, 24, hwnd, nullptr, g_instance, nullptr);
        g_gap = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1000", WS_CHILD | WS_VISIBLE | ES_NUMBER,
            455, 184, 110, 26, hwnd, reinterpret_cast<HMENU>(IDC_GAP), g_instance, nullptr);
        g_start = CreateWindowW(L"BUTTON", L"Bắt đầu lặp", WS_CHILD | WS_VISIBLE, 335, 228, 230, 34,
            hwnd, reinterpret_cast<HMENU>(IDC_START), g_instance, nullptr);
        CreateWindowW(L"STATIC",
            L"Mỗi target có tọa độ ảo riêng. Lệnh được gửi song song qua Named Pipe; con trỏ Windows không bị di chuyển.",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 335, 282, 260, 70, hwnd, nullptr, g_instance, nullptr);
        Refresh();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_REFRESH: Refresh(); return 0;
        case IDC_CLICK: BroadcastClick(); return 0;
        case IDC_START: StartStop(); return 0;
        }
        break;
    case WM_APP + 1:
        SetWindowTextW(g_start, L"Bắt đầu lặp");
        return 0;
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance; wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"VirtualInputLab.Controller";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, wc.lpszClassName, L"Virtual Input Lab – Bộ điều khiển",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 410, nullptr, nullptr, instance, nullptr);
    if (!g_main) return 1;
    ShowWindow(g_main, show); UpdateWindow(g_main);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
