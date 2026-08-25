#include "ipc_controller.h"
#include "ipc_protocol.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace {
enum : int {
    IDC_ENABLE = 4101, IDC_REFRESH, IDC_LIST, IDC_HOLD, IDC_GAP, IDC_REPEAT,
    IDC_RUN, IDC_PAUSE, IDC_STOP, IDC_RESET, IDC_STATUS,
    IDC_POINT_1, IDC_POINT_2, IDC_POINT_3, IDC_POINT_4
};
constexpr UINT WM_IPC_FINISHED = WM_APP + 74;
constexpr UINT WM_IPC_PROGRESS = WM_APP + 75;
constexpr wchar_t kControllerClass[] = L"AutoSyncClean.IPCController.V1";

HINSTANCE g_instance{};
HWND g_window{}, g_list{}, g_status{}, g_run{}, g_pause{};
std::array<POINT, 4> g_points{};
std::array<bool, 4> g_hasPoint{};
std::atomic_bool g_running{false}, g_paused{false}, g_stop{false};
std::atomic_uint64_t g_sequence{0};

int ReadNumber(HWND window, int id, int fallback, int low, int high) {
    wchar_t value[32]{};
    GetWindowTextW(GetDlgItem(window, id), value, 32);
    wchar_t* end{};
    const long parsed = wcstol(value, &end, 10);
    if (end == value) return fallback;
    return std::clamp(static_cast<int>(parsed), low, high);
}

void SetStatus(const std::wstring& text) {
    if (g_status && IsWindow(g_status)) SetWindowTextW(g_status, text.c_str());
}

BOOL CALLBACK FindTargets(HWND window, LPARAM data) {
    wchar_t className[128]{};
    GetClassNameW(window, className, 128);
    if (lstrcmpW(className, autosync_ipc::kTargetClass) != 0) return TRUE;
    DWORD pid{};
    GetWindowThreadProcessId(window, &pid);
    if (pid) reinterpret_cast<std::vector<DWORD>*>(data)->push_back(pid);
    return TRUE;
}

void InsertColumn(int index, int width, const wchar_t* title) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = width;
    column.pszText = const_cast<wchar_t*>(title);
    ListView_InsertColumn(g_list, index, &column);
}

void RefreshTargets() {
    std::vector<DWORD> pids;
    EnumWindows(FindTargets, reinterpret_cast<LPARAM>(&pids));
    std::sort(pids.begin(), pids.end());
    pids.erase(std::unique(pids.begin(), pids.end()), pids.end());
    ListView_DeleteAllItems(g_list);
    for (size_t i = 0; i < pids.size(); ++i) {
        const std::wstring number = std::to_wstring(i + 1);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<wchar_t*>(number.c_str());
        item.lParam = static_cast<LPARAM>(pids[i]);
        const int row = ListView_InsertItem(g_list, &item);
        const std::wstring pid = std::to_wstring(pids[i]);
        ListView_SetItemText(g_list, row, 1, const_cast<wchar_t*>(pid.c_str()));
        ListView_SetCheckState(g_list, row, TRUE);
    }
    SetStatus(L"Đã tìm thấy " + std::to_wstring(pids.size()) + L" Target tương thích V3.");
}

std::vector<DWORD> SelectedTargets() {
    std::vector<DWORD> result;
    const int count = ListView_GetItemCount(g_list);
    for (int row = 0; row < count; ++row) {
        if (!ListView_GetCheckState(g_list, row)) continue;
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (ListView_GetItem(g_list, &item)) result.push_back(static_cast<DWORD>(item.lParam));
    }
    return result;
}

bool SendTo(DWORD pid, const autosync_ipc::Command& command, DWORD& elapsed) {
    const ULONGLONG start = GetTickCount64();
    const std::wstring pipeName = std::wstring(autosync_ipc::kPipePrefix) + std::to_wstring(pid);
    if (!WaitNamedPipeW(pipeName.c_str(), 150)) {
        elapsed = static_cast<DWORD>(GetTickCount64() - start);
        return false;
    }
    HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
        elapsed = static_cast<DWORD>(GetTickCount64() - start);
        return false;
    }
    DWORD written{};
    const bool ok = WriteFile(pipe, &command, sizeof(command), &written, nullptr) && written == sizeof(command);
    CloseHandle(pipe);
    elapsed = static_cast<DWORD>(GetTickCount64() - start);
    return ok;
}

void AppendLog(DWORD pid, const autosync_ipc::Command& command, bool ok, DWORD elapsed) {
    HANDLE file = CreateFileW(L"AutoSyncClean_IPC_log.csv", FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    const std::string row = std::to_string(GetTickCount64()) + "," + std::to_string(pid) + "," +
        std::to_string(command.sequence) + "," + std::to_string(command.step) + "," +
        std::to_string(static_cast<unsigned>(command.type)) + "," + (ok ? "OK" : "FAIL") + "," +
        std::to_string(elapsed) + "\r\n";
    DWORD written{};
    WriteFile(file, row.data(), static_cast<DWORD>(row.size()), &written, nullptr);
    CloseHandle(file);
}

void SendParallel(const std::vector<DWORD>& targets, const autosync_ipc::Command& command) {
    std::vector<std::thread> workers;
    workers.reserve(targets.size());
    for (DWORD pid : targets) {
        workers.emplace_back([pid, command] {
            DWORD elapsed{};
            const bool ok = SendTo(pid, command, elapsed);
            AppendLog(pid, command, ok, elapsed);
        });
    }
    for (auto& worker : workers) worker.join();
}

autosync_ipc::Command MakeCommand(autosync_ipc::CommandType type, POINT point,
                                  unsigned step, std::uint64_t sequence) {
    autosync_ipc::Command command{};
    command.type = type;
    command.x = point.x;
    command.y = point.y;
    command.step = step;
    command.sequence = sequence;
    return command;
}

bool InterruptibleWait(int milliseconds) {
    for (int elapsed = 0; elapsed < milliseconds && !g_stop; elapsed += 10) {
        while (g_paused && !g_stop) Sleep(20);
        Sleep(static_cast<DWORD>(std::min(10, milliseconds - elapsed)));
    }
    return !g_stop;
}

void RunScript(std::vector<DWORD> targets, int hold, int gap, int repeat) {
    for (int round = 0; round < repeat && !g_stop; ++round) {
        for (int pointIndex = 0; pointIndex < 4 && !g_stop; ++pointIndex) {
            while (g_paused && !g_stop) Sleep(20);
            const auto sequence = ++g_sequence;
            const auto point = g_points[pointIndex];
            SendParallel(targets, MakeCommand(autosync_ipc::CommandType::Move, point, pointIndex + 1, sequence));
            SendParallel(targets, MakeCommand(autosync_ipc::CommandType::LeftDown, point, pointIndex + 1, sequence));
            if (!InterruptibleWait(hold)) break;
            SendParallel(targets, MakeCommand(autosync_ipc::CommandType::LeftUp, point, pointIndex + 1, sequence));
            if (g_window && IsWindow(g_window))
                PostMessageW(g_window, WM_IPC_PROGRESS, static_cast<WPARAM>(round + 1),
                             static_cast<LPARAM>(pointIndex + 1));
            if (!InterruptibleWait(gap)) break;
        }
    }
    g_running = false;
    g_paused = false;
    if (g_window && IsWindow(g_window)) PostMessageW(g_window, WM_IPC_FINISHED, 0, 0);
}

void CapturePoint(int index) {
    if (!Button_GetCheck(GetDlgItem(g_window, IDC_ENABLE))) {
        SetStatus(L"Hãy bật mô-đun IPC trước khi lấy điểm.");
        return;
    }
    POINT screen{};
    GetCursorPos(&screen);
    HWND target = WindowFromPoint(screen);
    wchar_t className[128]{};
    GetClassNameW(target, className, 128);
    if (lstrcmpW(className, autosync_ipc::kTargetClass) != 0) {
        SetStatus(L"Đặt chuột trong Target V3 tương thích rồi nhấn F1–F4.");
        return;
    }
    RECT client{};
    GetClientRect(target, &client);
    POINT local = screen;
    ScreenToClient(target, &local);
    g_points[index].x = std::clamp<LONG>(MulDiv(local.x, autosync_ipc::kCoordinateScale,
                                                std::max(1L, client.right)), 0L,
                                                autosync_ipc::kCoordinateScale);
    g_points[index].y = std::clamp<LONG>(MulDiv(local.y, autosync_ipc::kCoordinateScale,
                                                std::max(1L, client.bottom)), 0L,
                                                autosync_ipc::kCoordinateScale);
    g_hasPoint[index] = true;
    const int labelId = IDC_POINT_1 + index;
    const std::wstring label = L"F" + std::to_wstring(index + 1) + L": X=" +
        std::to_wstring(g_points[index].x) + L"  Y=" + std::to_wstring(g_points[index].y);
    SetWindowTextW(GetDlgItem(g_window, labelId), label.c_str());
    SetStatus(L"Đã lưu điểm " + std::to_wstring(index + 1) + L".");
}

void EnableControls(bool enabled) {
    EnableWindow(GetDlgItem(g_window, IDC_REFRESH), enabled);
    EnableWindow(g_list, enabled);
    EnableWindow(g_run, enabled);
    EnableWindow(GetDlgItem(g_window, IDC_RESET), enabled);
}

LRESULT CALLBACK ControllerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_window = window;
            CreateWindowW(L"BUTTON", L"Bật mô-đun IPC (Target tương thích)",
                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 14, 12, 290, 26, window,
                          reinterpret_cast<HMENU>(IDC_ENABLE), g_instance, nullptr);
            CreateWindowW(L"BUTTON", L"Làm mới Target", WS_CHILD | WS_VISIBLE,
                          14, 46, 140, 28, window, reinterpret_cast<HMENU>(IDC_REFRESH), g_instance, nullptr);
            g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                     WS_CHILD | WS_VISIBLE | LVS_REPORT, 14, 82, 300, 404, window,
                                     reinterpret_cast<HMENU>(IDC_LIST), g_instance, nullptr);
            ListView_SetExtendedListViewStyle(g_list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            InsertColumn(0, 55, L"#");
            InsertColumn(1, 210, L"PID Target V3");
            CreateWindowW(L"STATIC", L"Đặt chuột trên Target mẫu rồi nhấn F1–F4",
                          WS_CHILD | WS_VISIBLE, 335, 18, 385, 24, window, nullptr, g_instance, nullptr);
            for (int i = 0; i < 4; ++i) {
                const std::wstring initial = L"F" + std::to_wstring(i + 1) + L": chưa lưu";
                CreateWindowW(L"STATIC", initial.c_str(), WS_CHILD | WS_VISIBLE,
                              345, 52 + i * 31, 260, 24, window,
                              reinterpret_cast<HMENU>(IDC_POINT_1 + i), g_instance, nullptr);
            }
            CreateWindowW(L"STATIC", L"Giữ nút (ms):", WS_CHILD | WS_VISIBLE,
                          345, 184, 110, 24, window, nullptr, g_instance, nullptr);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"80", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                            470, 180, 110, 26, window, reinterpret_cast<HMENU>(IDC_HOLD), g_instance, nullptr);
            CreateWindowW(L"STATIC", L"Giãn cách điểm (ms):", WS_CHILD | WS_VISIBLE,
                          345, 220, 145, 24, window, nullptr, g_instance, nullptr);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"500", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                            495, 216, 85, 26, window, reinterpret_cast<HMENU>(IDC_GAP), g_instance, nullptr);
            CreateWindowW(L"STATIC", L"Số vòng:", WS_CHILD | WS_VISIBLE,
                          345, 256, 85, 24, window, nullptr, g_instance, nullptr);
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"10", WS_CHILD | WS_VISIBLE | ES_NUMBER,
                            435, 252, 145, 26, window, reinterpret_cast<HMENU>(IDC_REPEAT), g_instance, nullptr);
            g_run = CreateWindowW(L"BUTTON", L"Bắt đầu 1–2–3–4", WS_CHILD | WS_VISIBLE,
                                  345, 300, 235, 34, window, reinterpret_cast<HMENU>(IDC_RUN), g_instance, nullptr);
            g_pause = CreateWindowW(L"BUTTON", L"Tạm dừng", WS_CHILD | WS_VISIBLE,
                                    590, 300, 115, 34, window, reinterpret_cast<HMENU>(IDC_PAUSE), g_instance, nullptr);
            CreateWindowW(L"BUTTON", L"Dừng", WS_CHILD | WS_VISIBLE, 590, 342, 115, 32,
                          window, reinterpret_cast<HMENU>(IDC_STOP), g_instance, nullptr);
            CreateWindowW(L"BUTTON", L"Đặt lại bộ đếm Target", WS_CHILD | WS_VISIBLE,
                          345, 342, 235, 32, window, reinterpret_cast<HMENU>(IDC_RESET), g_instance, nullptr);
            g_status = CreateWindowW(L"STATIC", L"Mô-đun đang tắt. Chức năng AutoSyncClean cũ không bị thay đổi.",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT, 345, 397, 370, 70, window,
                                     reinterpret_cast<HMENU>(IDC_STATUS), g_instance, nullptr);
            for (int i = 0; i < 4; ++i) RegisterHotKey(window, i + 1, 0, VK_F1 + i);
            EnableControls(false);
            return 0;
        }
        case WM_HOTKEY:
            if (wParam >= 1 && wParam <= 4) CapturePoint(static_cast<int>(wParam) - 1);
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (id == IDC_ENABLE) {
                const bool enabled = Button_GetCheck(GetDlgItem(window, IDC_ENABLE)) == BST_CHECKED;
                if (!enabled && g_running) {
                    g_stop = true;
                    g_paused = false;
                }
                EnableControls(enabled && !g_running);
                if (enabled) RefreshTargets();
                else SetStatus(L"Mô-đun đang tắt. Chức năng AutoSyncClean cũ không bị thay đổi.");
                return 0;
            }
            if (id == IDC_REFRESH) { RefreshTargets(); return 0; }
            if (id == IDC_RUN) {
                if (g_running) return 0;
                if (std::any_of(g_hasPoint.begin(), g_hasPoint.end(), [](bool value) { return !value; })) {
                    SetStatus(L"Chưa đủ bốn điểm F1–F4.");
                    return 0;
                }
                auto targets = SelectedTargets();
                if (targets.empty()) { SetStatus(L"Chưa chọn Target tương thích."); return 0; }
                const int hold = ReadNumber(window, IDC_HOLD, 80, 1, 10000);
                const int gap = ReadNumber(window, IDC_GAP, 500, 0, 60000);
                const int repeat = ReadNumber(window, IDC_REPEAT, 10, 1, 99999);
                g_stop = false;
                g_paused = false;
                g_running = true;
                EnableControls(false);
                EnableWindow(g_pause, TRUE);
                SetStatus(L"Đang chạy mô-đun IPC độc lập...");
                std::thread(RunScript, std::move(targets), hold, gap, repeat).detach();
                return 0;
            }
            if (id == IDC_PAUSE && g_running) {
                g_paused = !g_paused;
                SetWindowTextW(g_pause, g_paused ? L"Tiếp tục" : L"Tạm dừng");
                SetStatus(g_paused ? L"Đã tạm dừng mô-đun IPC." : L"Đang tiếp tục mô-đun IPC...");
                return 0;
            }
            if (id == IDC_STOP) { g_stop = true; g_paused = false; return 0; }
            if (id == IDC_RESET) {
                auto targets = SelectedTargets();
                autosync_ipc::Command command{};
                command.type = autosync_ipc::CommandType::Reset;
                command.sequence = ++g_sequence;
                std::thread([targets = std::move(targets), command] { SendParallel(targets, command); }).detach();
                SetStatus(L"Đã gửi lệnh đặt lại tới các Target được chọn.");
                return 0;
            }
            return 0;
        }
        case WM_IPC_PROGRESS:
            SetStatus(L"Đang chạy vòng " + std::to_wstring(wParam) + L", bước " +
                      std::to_wstring(lParam) + L". Seq=" + std::to_wstring(g_sequence.load()));
            return 0;
        case WM_IPC_FINISHED:
            EnableControls(Button_GetCheck(GetDlgItem(window, IDC_ENABLE)) == BST_CHECKED);
            EnableWindow(g_pause, FALSE);
            SetWindowTextW(g_pause, L"Tạm dừng");
            SetStatus(g_stop ? L"Đã dừng mô-đun IPC." : L"Đã chạy xong kịch bản IPC.");
            return 0;
        case WM_CLOSE:
            g_stop = true;
            g_paused = false;
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            g_stop = true;
            for (int i = 0; i < 4; ++i) UnregisterHotKey(window, i + 1);
            g_window = nullptr;
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

void ShowIpcController(HWND owner, HINSTANCE instance) {
    g_instance = instance;
    if (g_window && IsWindow(g_window)) {
        ShowWindow(g_window, SW_SHOWNORMAL);
        SetForegroundWindow(g_window);
        return;
    }
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = ControllerProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = kControllerClass;
    RegisterClassExW(&windowClass);
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, kControllerClass,
                              L"AutoSyncClean – Mô-đun IPC tùy chọn",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 750, 535,
                              owner, nullptr, instance, nullptr);
    if (g_window) {
        ShowWindow(g_window, SW_SHOWNORMAL);
        UpdateWindow(g_window);
    }
}

void ShutdownIpcController() {
    g_stop = true;
    g_paused = false;
    if (g_window && IsWindow(g_window)) DestroyWindow(g_window);
    g_window = nullptr;
}
