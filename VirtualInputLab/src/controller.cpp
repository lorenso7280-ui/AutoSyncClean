#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "protocol.h"

namespace {
enum {
    IDC_LIST = 1001, IDC_REFRESH, IDC_X, IDC_Y, IDC_CLICK, IDC_REPEAT, IDC_GAP,
    IDC_START, IDC_RESET, IDC_NORMALIZED, IDC_HOLD
};
HINSTANCE g_instance{};
HWND g_main{}, g_list{}, g_x{}, g_y{}, g_repeat{}, g_gap{}, g_start{}, g_normalized{}, g_hold{};
std::vector<DWORD> g_pids;
enum class RunState { Stopped, Running, Paused };
std::atomic<RunState> g_runState{RunState::Stopped};
std::atomic_uint64_t g_sequence{};

BOOL CALLBACK EnumTarget(HWND hwnd, LPARAM) {
    wchar_t cls[128]{};
    GetClassNameW(hwnd, cls, 128);
    if (lstrcmpW(cls, vilab::kTargetClass) != 0) return TRUE;
    DWORD pid{};
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid) g_pids.push_back(pid);
    return TRUE;
}

void Refresh() {
    g_pids.clear();
    EnumWindows(EnumTarget, 0);
    ListView_DeleteAllItems(g_list);
    for (size_t i = 0; i < g_pids.size(); ++i) {
        std::wstring number = std::to_wstring(i + 1);
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = number.data();
        const int row = ListView_InsertItem(g_list, &item);
        std::wstring pid = std::to_wstring(g_pids[i]);
        ListView_SetItemText(g_list, row, 1, pid.data());
        ListView_SetCheckState(g_list, row, TRUE);
    }
}

int ReadInt(HWND edit, int fallback) {
    wchar_t b[32]{};
    GetWindowTextW(edit, b, 32);
    wchar_t* end{};
    const long v = wcstol(b, &end, 10);
    return end == b ? fallback : static_cast<int>(v);
}

bool SendTo(DWORD pid, const vilab::Command& command) {
    const std::wstring pipeName = std::wstring(vilab::kPipePrefix) + std::to_wstring(pid);
    if (!WaitNamedPipeW(pipeName.c_str(), 150)) return false;
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

vilab::Command MakeCommand(vilab::CommandType type, int x, int y,
                           std::uint64_t sequence, int hold, bool normalized) {
    vilab::Command command{};
    command.type = type;
    command.x = x;
    command.y = y;
    command.holdMs = static_cast<std::uint32_t>(hold);
    command.sequence = sequence;
    if (normalized)
        command.flags = vilab::NormalizedCoordinates;
    return command;
}

void SendParallel(const std::vector<DWORD>& targets, const vilab::Command& command) {
    std::vector<std::thread> workers;
    workers.reserve(targets.size());
    for (const DWORD pid : targets)
        workers.emplace_back([pid, command] { SendTo(pid, command); });
    for (auto& worker : workers) worker.join();
}

void SendClickSequence(const std::vector<DWORD>& targets, int x, int y,
                       int hold, bool normalized) {
    const auto sequence = ++g_sequence;
    const auto down = MakeCommand(vilab::CommandType::LeftDown, x, y, sequence, hold, normalized);
    SendParallel(targets, down);
    Sleep(static_cast<DWORD>(hold));
    const auto up = MakeCommand(vilab::CommandType::LeftUp, x, y, sequence, hold, normalized);
    SendParallel(targets, up);
}

void BroadcastClick() {
    const int hold = std::clamp(ReadInt(g_hold, 80), 1, 500);
    const bool normalized = Button_GetCheck(g_normalized) == BST_CHECKED;
    SendClickSequence(SelectedPids(), ReadInt(g_x, 5000), ReadInt(g_y, 6900), hold, normalized);
}

void ResetCounters() {
    const auto targets = SelectedPids();
    auto command = MakeCommand(vilab::CommandType::Reset, 0, 0, ++g_sequence, 1, false);
    SendParallel(targets, command);
}

void StartStop() {
    const RunState current = g_runState.load();
    if (current == RunState::Running) {
        g_runState = RunState::Paused;
        SetWindowTextW(g_start, L"Tiếp tục");
        return;
    }
    if (current == RunState::Paused) {
        g_runState = RunState::Running;
        SetWindowTextW(g_start, L"Tạm dừng");
        return;
    }
    SetWindowTextW(g_start, L"Tạm dừng");
    const int repeat = std::clamp(ReadInt(g_repeat, 1), 1, 99999);
    const int gap = std::clamp(ReadInt(g_gap, 1000), 20, 600000);
    const int x = ReadInt(g_x, 5000), y = ReadInt(g_y, 6900);
    const int hold = std::clamp(ReadInt(g_hold, 80), 1, 500);
    const bool normalized = Button_GetCheck(g_normalized) == BST_CHECKED;
    const auto targets = SelectedPids();
    if (targets.empty()) return;
    g_runState = RunState::Running;
    std::thread([repeat, gap, x, y, hold, normalized, targets] {
        int completed = 0;
        while (g_runState != RunState::Stopped && completed < repeat) {
            if (g_runState == RunState::Paused) {
                Sleep(20);
                continue;
            }
            SendClickSequence(targets, x, y, hold, normalized);
            ++completed;
            for (int waited = 0; g_runState != RunState::Stopped && waited < gap; waited += 20) {
                while (g_runState == RunState::Paused) Sleep(20);
                if (g_runState == RunState::Stopped) break;
                Sleep(20);
            }
        }
        g_runState = RunState::Stopped;
        PostMessageW(g_main, WM_APP + 1, 0, 0);
    }).detach();
}

void AddColumn(int index, int width, const wchar_t* name) {
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = width;
    col.pszText = const_cast<wchar_t*>(name);
    ListView_InsertColumn(g_list, index, &col);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT, 14, 48, 315, 345,
            hwnd, reinterpret_cast<HMENU>(IDC_LIST), g_instance, nullptr);
        ListView_SetExtendedListViewStyle(g_list,
            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        AddColumn(0, 80, L"#");
        AddColumn(1, 190, L"PID Target V2");
        CreateWindowW(L"BUTTON", L"Làm mới danh sách", WS_CHILD | WS_VISIBLE,
            14, 14, 150, 28, hwnd, reinterpret_cast<HMENU>(IDC_REFRESH), g_instance, nullptr);

        g_normalized = CreateWindowW(L"BUTTON", L"Tọa độ chuẩn hóa 0–10000",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 350, 18, 245, 24,
            hwnd, reinterpret_cast<HMENU>(IDC_NORMALIZED), g_instance, nullptr);
        Button_SetCheck(g_normalized, BST_CHECKED);
        CreateWindowW(L"STATIC", L"X:", WS_CHILD | WS_VISIBLE, 350, 58, 25, 24,
            hwnd, nullptr, g_instance, nullptr);
        g_x = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"5000",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 380, 54, 90, 26,
            hwnd, reinterpret_cast<HMENU>(IDC_X), g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Y:", WS_CHILD | WS_VISIBLE, 485, 58, 25, 24,
            hwnd, nullptr, g_instance, nullptr);
        g_y = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"6900",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 515, 54, 90, 26,
            hwnd, reinterpret_cast<HMENU>(IDC_Y), g_instance, nullptr);

        CreateWindowW(L"STATIC", L"Giữ nút (ms):", WS_CHILD | WS_VISIBLE,
            350, 94, 105, 24, hwnd, nullptr, g_instance, nullptr);
        g_hold = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"80",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 465, 90, 140, 26,
            hwnd, reinterpret_cast<HMENU>(IDC_HOLD), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Click đồng thời (DOWN + UP)", WS_CHILD | WS_VISIBLE,
            350, 128, 255, 34, hwnd, reinterpret_cast<HMENU>(IDC_CLICK), g_instance, nullptr);
        CreateWindowW(L"BUTTON", L"Đặt lại bộ đếm", WS_CHILD | WS_VISIBLE,
            350, 168, 255, 30, hwnd, reinterpret_cast<HMENU>(IDC_RESET), g_instance, nullptr);

        CreateWindowW(L"STATIC", L"Lặp lại:", WS_CHILD | WS_VISIBLE,
            350, 218, 70, 24, hwnd, nullptr, g_instance, nullptr);
        g_repeat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"10",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 430, 214, 175, 26,
            hwnd, reinterpret_cast<HMENU>(IDC_REPEAT), g_instance, nullptr);
        CreateWindowW(L"STATIC", L"Giãn cách (ms):", WS_CHILD | WS_VISIBLE,
            350, 254, 110, 24, hwnd, nullptr, g_instance, nullptr);
        g_gap = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"1000",
            WS_CHILD | WS_VISIBLE | ES_NUMBER, 470, 250, 135, 26,
            hwnd, reinterpret_cast<HMENU>(IDC_GAP), g_instance, nullptr);
        g_start = CreateWindowW(L"BUTTON", L"Bắt đầu lặp", WS_CHILD | WS_VISIBLE,
            350, 290, 255, 34, hwnd, reinterpret_cast<HMENU>(IDC_START), g_instance, nullptr);
        CreateWindowW(L"STATIC",
            L"Mặc định X=5000, Y=6900 là tâm nút TRỢ GIÚP theo tỷ lệ. Hãy đổi kích thước các Target để kiểm tra quy đổi.",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 350, 342, 270, 65,
            hwnd, nullptr, g_instance, nullptr);
        Refresh();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_REFRESH: Refresh(); return 0;
        case IDC_CLICK: BroadcastClick(); return 0;
        case IDC_RESET: ResetCounters(); return 0;
        case IDC_START: StartStop(); return 0;
        }
        break;
    case WM_APP + 1:
        SetWindowTextW(g_start, L"Bắt đầu lặp");
        return 0;
    case WM_DESTROY:
        g_runState = RunState::Stopped;
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
    wc.hInstance = instance;
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = L"VirtualInputLab.Controller.V2";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
    g_main = CreateWindowExW(0, wc.lpszClassName, L"Virtual Input Lab V2 – Bộ điều khiển",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 455, nullptr, nullptr, instance, nullptr);
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
