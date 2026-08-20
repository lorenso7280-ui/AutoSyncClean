#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace {
constexpr wchar_t kClassName[] = L"AutoSyncClean.Main";
constexpr wchar_t kTitle[] = L"AutoSync Clean 1.0 - Đồng Bộ Thao Tác Phím & Chuột";

enum : int {
    IDC_REFRESH = 1001, IDC_SYNC, IDC_SET_MAIN, IDC_TILE, IDC_RECORD,
    IDC_PLAY, IDC_LIST, IDC_STATUS,
    IDM_SET_MAIN = 2001, IDM_TOGGLE_ITEM, IDM_REFRESH, IDM_CLOSE_ONE,
    IDM_REMOVE_ONE, IDM_SELECT_ALL, IDM_CLEAR_ALL, IDM_SHOW_ALL,
    IDM_CLOSE_ALL, IDM_REMOVE_ALL
};

struct WindowItem {
    HWND hwnd{};
    std::wstring title;
    bool selected{true};
};

enum class MacroType { KeyDown, KeyUp, MouseMove, MouseDown, MouseUp, Wheel };
struct MacroEvent {
    MacroType type{};
    DWORD data{};
    double nx{};
    double ny{};
    int wheel{};
    uint32_t delayMs{};
};

HINSTANCE g_instance{};
HWND g_main{}, g_list{}, g_btnSync{}, g_status{};
HHOOK g_keyboardHook{}, g_mouseHook{};
std::vector<WindowItem> g_windows;
std::unordered_set<HWND> g_ignored;
std::vector<MacroEvent> g_macro;
HWND g_source{};
bool g_sync{}, g_recording{}, g_playing{}, g_blockMove{};
std::chrono::steady_clock::time_point g_lastMacro;

void SyncChecksFromList();

std::wstring WindowTitle(HWND hwnd) {
    int n = GetWindowTextLengthW(hwnd);
    std::wstring s(static_cast<size_t>(n) + 1, L'\0');
    GetWindowTextW(hwnd, s.data(), n + 1);
    s.resize(static_cast<size_t>(n));
    return s;
}

bool IsCandidate(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || hwnd == g_main) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) return false;
    RECT r{};
    if (!GetWindowRect(hwnd, &r) || r.right - r.left < 120 || r.bottom - r.top < 80) return false;
    return !WindowTitle(hwnd).empty();
}

std::wstring HexHandle(HWND hwnd) {
    wchar_t b[32]{};
    swprintf_s(b, L"%08llX", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
    return b;
}

std::wstring WindowSize(HWND hwnd) {
    RECT r{};
    if (!GetClientRect(hwnd, &r)) return L"-";
    return std::to_wstring(r.right) + L"x" + std::to_wstring(r.bottom);
}

void SetStatus(const std::wstring& text) {
    SetWindowTextW(g_status, text.c_str());
}

void InsertColumn(int index, int width, const wchar_t* text) {
    LVCOLUMNW c{sizeof(c)};
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = const_cast<wchar_t*>(text);
    c.cx = width;
    c.iSubItem = index;
    ListView_InsertColumn(g_list, index, &c);
}

void RebuildList() {
    ListView_DeleteAllItems(g_list);
    for (size_t i = 0; i < g_windows.size(); ++i) {
        auto& w = g_windows[i];
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(i);
        auto id = HexHandle(w.hwnd);
        item.pszText = id.data();
        item.lParam = reinterpret_cast<LPARAM>(w.hwnd);
        int row = ListView_InsertItem(g_list, &item);
        ListView_SetItemText(g_list, row, 1, w.title.data());
        std::wstring state = IsWindow(w.hwnd) ? (g_sync && w.selected ? L"Đang hoạt động" : L"Online") : L"Đã đóng";
        ListView_SetItemText(g_list, row, 2, state.data());
        auto size = WindowSize(w.hwnd);
        ListView_SetItemText(g_list, row, 3, size.data());
        ListView_SetCheckState(g_list, row, w.selected ? TRUE : FALSE);
    }
    std::wstring s = L"Cửa sổ: " + std::to_wstring(g_windows.size());
    if (g_source) s += L" | Cửa sổ chính: " + WindowTitle(g_source);
    SetStatus(s);
}

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
    if (!IsCandidate(hwnd) || g_ignored.contains(hwnd)) return TRUE;
    auto it = std::find_if(g_windows.begin(), g_windows.end(), [hwnd](const auto& w) { return w.hwnd == hwnd; });
    if (it == g_windows.end()) g_windows.push_back({hwnd, WindowTitle(hwnd), true});
    else it->title = WindowTitle(hwnd);
    return TRUE;
}

void RefreshWindows(bool clearIgnored = false) {
    if (g_list) SyncChecksFromList();
    if (clearIgnored) g_ignored.clear();
    g_windows.erase(std::remove_if(g_windows.begin(), g_windows.end(), [](const auto& w) {
        return !IsWindow(w.hwnd) || !IsCandidate(w.hwnd);
    }), g_windows.end());
    EnumWindows(EnumProc, 0);
    std::sort(g_windows.begin(), g_windows.end(), [](const auto& a, const auto& b) { return a.title < b.title; });
    if (g_source && !IsWindow(g_source)) g_source = nullptr;
    RebuildList();
}

void SyncChecksFromList() {
    for (int i = 0; i < static_cast<int>(g_windows.size()); ++i)
        g_windows[static_cast<size_t>(i)].selected = ListView_GetCheckState(g_list, i) != FALSE;
}

bool PointInSource(POINT screen, POINT& client, RECT& rc) {
    HWND source = g_source ? g_source : GetForegroundWindow();
    if (!source || source == g_main) return false;
    client = screen;
    ScreenToClient(source, &client);
    GetClientRect(source, &rc);
    return client.x >= 0 && client.y >= 0 && client.x < rc.right && client.y < rc.bottom;
}

LPARAM ScaledPoint(HWND target, double nx, double ny) {
    RECT r{};
    GetClientRect(target, &r);
    int x = std::clamp(static_cast<int>(nx * r.right), 0, std::max(0L, r.right - 1));
    int y = std::clamp(static_cast<int>(ny * r.bottom), 0, std::max(0L, r.bottom - 1));
    return MAKELPARAM(x, y);
}

void ForTargets(const auto& fn) {
    SyncChecksFromList();
    for (auto& w : g_windows) {
        if (w.selected && w.hwnd != g_source && IsWindow(w.hwnd)) fn(w.hwnd);
    }
}

void AddMacro(MacroEvent e) {
    if (!g_recording || g_playing) return;
    auto now = std::chrono::steady_clock::now();
    e.delayMs = static_cast<uint32_t>(std::min<int64_t>(5000,
        std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastMacro).count()));
    g_lastMacro = now;
    if (e.type == MacroType::MouseMove && !g_macro.empty() &&
        g_macro.back().type == MacroType::MouseMove && e.delayMs < 25) {
        g_macro.back() = e;
    } else {
        g_macro.push_back(e);
    }
}

void SendMouse(UINT msg, const MSLLHOOKSTRUCT* m) {
    POINT p{}; RECT src{};
    if (!PointInSource(m->pt, p, src) || src.right <= 0 || src.bottom <= 0) return;
    double nx = static_cast<double>(p.x) / src.right;
    double ny = static_cast<double>(p.y) / src.bottom;
    WPARAM wp = 0;
    MacroType mt = MacroType::MouseMove;
    switch (msg) {
        case WM_LBUTTONDOWN: wp = MK_LBUTTON; mt = MacroType::MouseDown; break;
        case WM_RBUTTONDOWN: wp = MK_RBUTTON; mt = MacroType::MouseDown; break;
        case WM_MBUTTONDOWN: wp = MK_MBUTTON; mt = MacroType::MouseDown; break;
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: mt = MacroType::MouseUp; break;
        case WM_MOUSEWHEEL: wp = MAKEWPARAM(0, HIWORD(m->mouseData)); mt = MacroType::Wheel; break;
        default: break;
    }
    if (msg == WM_MOUSEMOVE && g_blockMove) return;
    ForTargets([&](HWND h) { PostMessageW(h, msg, wp, ScaledPoint(h, nx, ny)); });
    MacroEvent e{mt, msg, nx, ny, GET_WHEEL_DELTA_WPARAM(wp), 0};
    AddMacro(e);
}

LRESULT CALLBACK MouseHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_sync && !g_playing) {
        auto* m = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        if (!(m->flags & LLMHF_INJECTED)) SendMouse(static_cast<UINT>(wp), m);
    }
    return CallNextHookEx(g_mouseHook, code, wp, lp);
}

LRESULT CALLBACK KeyboardHook(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && g_sync && !g_playing) {
        auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
        if (!(k->flags & LLKHF_INJECTED)) {
            HWND fg = GetForegroundWindow();
            if (!g_source || fg == g_source) {
                UINT msg = static_cast<UINT>(wp);
                LPARAM keyData = 1 | (MapVirtualKeyW(k->scanCode, MAPVK_VSC_TO_VK_EX) << 16);
                if (k->flags & LLKHF_EXTENDED) keyData |= 1LL << 24;
                if (msg == WM_KEYUP || msg == WM_SYSKEYUP) keyData |= (1LL << 30) | (1LL << 31);
                ForTargets([&](HWND h) { PostMessageW(h, msg, k->vkCode, keyData); });
                AddMacro({(msg == WM_KEYUP || msg == WM_SYSKEYUP) ? MacroType::KeyUp : MacroType::KeyDown,
                          k->vkCode, 0, 0, 0, 0});
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wp, lp);
}

void SetSync(bool on) {
    g_sync = on;
    SetWindowTextW(g_btnSync, on ? L"■ Tắt đồng bộ" : L"⟳ Bật đồng bộ");
    if (on) {
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, g_instance, 0);
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, g_instance, 0);
        if (!g_keyboardHook || !g_mouseHook) {
            MessageBoxW(g_main, L"Không thể cài hook. Hãy thử chạy bằng quyền Administrator nếu cửa sổ đích đang chạy quyền cao.", kTitle, MB_ICONERROR);
            g_sync = false;
        }
    } else {
        if (g_keyboardHook) UnhookWindowsHookEx(g_keyboardHook);
        if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
        g_keyboardHook = g_mouseHook = nullptr;
    }
    RebuildList();
}

HWND SelectedHwnd() {
    int row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    if (row < 0 || row >= static_cast<int>(g_windows.size())) return nullptr;
    return g_windows[static_cast<size_t>(row)].hwnd;
}

void SetMainWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    g_source = hwnd;
    RebuildList();
    SetStatus(L"Đã chọn cửa sổ chính: " + WindowTitle(hwnd));
}

void TileSelected() {
    SyncChecksFromList();
    std::vector<HWND> items;
    for (auto& w : g_windows) if (w.selected && IsWindow(w.hwnd)) items.push_back(w.hwnd);
    if (items.empty()) return;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int cols = static_cast<int>(ceil(sqrt(static_cast<double>(items.size()))));
    int rows = static_cast<int>((items.size() + cols - 1) / cols);
    int width = (work.right - work.left) / cols;
    int height = (work.bottom - work.top) / rows;
    for (size_t i = 0; i < items.size(); ++i) {
        int x = work.left + static_cast<int>(i % cols) * width;
        int y = work.top + static_cast<int>(i / cols) * height;
        ShowWindow(items[i], SW_RESTORE);
        SetWindowPos(items[i], nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

DWORD WINAPI PlayThread(void*) {
    g_playing = true;
    for (const auto& e : g_macro) {
        if (!g_playing) break;
        Sleep(e.delayMs);
        ForTargets([&](HWND h) {
            switch (e.type) {
                case MacroType::KeyDown: PostMessageW(h, WM_KEYDOWN, e.data, 1); break;
                case MacroType::KeyUp: PostMessageW(h, WM_KEYUP, e.data, (1LL << 30) | (1LL << 31)); break;
                case MacroType::MouseMove: PostMessageW(h, WM_MOUSEMOVE, 0, ScaledPoint(h, e.nx, e.ny)); break;
                case MacroType::MouseDown: PostMessageW(h, e.data, e.data == WM_LBUTTONDOWN ? MK_LBUTTON : MK_RBUTTON, ScaledPoint(h, e.nx, e.ny)); break;
                case MacroType::MouseUp: PostMessageW(h, e.data, 0, ScaledPoint(h, e.nx, e.ny)); break;
                case MacroType::Wheel: PostMessageW(h, WM_MOUSEWHEEL, MAKEWPARAM(0, e.wheel), ScaledPoint(h, e.nx, e.ny)); break;
            }
        });
    }
    g_playing = false;
    SetStatus(L"Đã phát xong chuỗi thao tác.");
    return 0;
}

void ToggleRecord() {
    g_recording = !g_recording;
    HWND b = GetDlgItem(g_main, IDC_RECORD);
    if (g_recording) {
        g_macro.clear();
        g_lastMacro = std::chrono::steady_clock::now();
        SetWindowTextW(b, L"■ Dừng ghi");
        if (!g_sync) SetSync(true);
        SetStatus(L"Đang ghi chuỗi thao tác...");
    } else {
        SetWindowTextW(b, L"● Ghi thao tác");
        SetStatus(L"Đã ghi " + std::to_wstring(g_macro.size()) + L" sự kiện.");
    }
}

void ShowContextMenu(POINT p) {
    HMENU menu = CreatePopupMenu();
    HMENU sync = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_SET_MAIN, L"Làm cửa sổ chính");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sync), L"Đồng bộ");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_REFRESH, L"Thêm mới cửa sổ");
    AppendMenuW(menu, MF_STRING, IDM_CLOSE_ONE, L"Đóng cửa sổ này");
    AppendMenuW(menu, MF_STRING, IDM_REMOVE_ONE, L"Xóa khỏi danh sách");
    AppendMenuW(sync, MF_STRING, IDM_SELECT_ALL, L"Chọn tất cả");
    AppendMenuW(sync, MF_STRING, IDM_CLEAR_ALL, L"Bỏ chọn tất cả");
    AppendMenuW(sync, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sync, MF_STRING, IDM_SHOW_ALL, L"Hiện tất cả");
    AppendMenuW(sync, MF_STRING, IDM_CLOSE_ALL, L"Đóng tất cả");
    AppendMenuW(sync, MF_STRING, IDM_REMOVE_ALL, L"Xóa tất cả");
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, g_main, nullptr);
    DestroyMenu(menu);
}

void HandleMenu(int id) {
    HWND selected = SelectedHwnd();
    switch (id) {
        case IDM_SET_MAIN: SetMainWindow(selected); break;
        case IDM_REFRESH: RefreshWindows(true); break;
        case IDM_CLOSE_ONE: if (selected) PostMessageW(selected, WM_CLOSE, 0, 0); break;
        case IDM_REMOVE_ONE:
            if (selected) { g_ignored.insert(selected); RefreshWindows(); }
            break;
        case IDM_SELECT_ALL: case IDM_CLEAR_ALL:
            for (int i = 0; i < ListView_GetItemCount(g_list); ++i) ListView_SetCheckState(g_list, i, id == IDM_SELECT_ALL);
            SyncChecksFromList(); RebuildList(); break;
        case IDM_SHOW_ALL:
            ForTargets([](HWND h) { ShowWindow(h, SW_RESTORE); }); break;
        case IDM_CLOSE_ALL:
            ForTargets([](HWND h) { PostMessageW(h, WM_CLOSE, 0, 0); }); break;
        case IDM_REMOVE_ALL:
            for (auto& w : g_windows) g_ignored.insert(w.hwnd);
            g_windows.clear(); RebuildList(); break;
    }
}

void Layout(HWND hwnd) {
    RECT r{}; GetClientRect(hwnd, &r);
    int y = 8, h = 30;
    MoveWindow(GetDlgItem(hwnd, IDC_REFRESH), 8, y, 86, h, TRUE);
    MoveWindow(g_btnSync, 100, y, 130, h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SET_MAIN), 236, y, 120, h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_TILE), 362, y, 112, h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_RECORD), 480, y, 122, h, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_PLAY), 608, y, 110, h, TRUE);
    MoveWindow(g_list, 8, 46, std::max(100L, r.right - 16), std::max(80L, r.bottom - 82), TRUE);
    MoveWindow(g_status, 8, r.bottom - 30, std::max(100L, r.right - 16), 22, TRUE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_main = hwnd;
            auto button = [&](int id, const wchar_t* text) {
                return CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                                     hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
            };
            button(IDC_REFRESH, L"↻ Làm mới");
            g_btnSync = button(IDC_SYNC, L"⟳ Bật đồng bộ");
            button(IDC_SET_MAIN, L"♙ Cửa sổ chính");
            button(IDC_TILE, L"▦ Sắp xếp");
            button(IDC_RECORD, L"● Ghi thao tác");
            button(IDC_PLAY, L"▶ Phát lại");
            g_list = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
                                  0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_LIST), g_instance, nullptr);
            ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
            InsertColumn(0, 105, L"Mã cửa sổ"); InsertColumn(1, 300, L"Tiêu đề");
            InsertColumn(2, 130, L"Trạng thái"); InsertColumn(3, 100, L"Kích thước");
            g_status = CreateWindowW(L"STATIC", L"Sẵn sàng", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_STATUS), g_instance, nullptr);
            SetTimer(hwnd, 1, 3000, nullptr);
            RefreshWindows();
            return 0;
        }
        case WM_SIZE: Layout(hwnd); return 0;
        case WM_TIMER: RefreshWindows(); return 0;
        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(wp) == g_list) ShowContextMenu({GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_NOTIFY: {
            auto* n = reinterpret_cast<NMHDR*>(lp);
            if (n->idFrom == IDC_LIST && n->code == NM_DBLCLK) SetMainWindow(SelectedHwnd());
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id >= IDM_SET_MAIN && id <= IDM_REMOVE_ALL) { HandleMenu(id); return 0; }
            switch (id) {
                case IDC_REFRESH: RefreshWindows(true); break;
                case IDC_SYNC: SetSync(!g_sync); break;
                case IDC_SET_MAIN: SetMainWindow(SelectedHwnd()); break;
                case IDC_TILE: TileSelected(); break;
                case IDC_RECORD: ToggleRecord(); break;
                case IDC_PLAY:
                    if (g_macro.empty()) MessageBoxW(hwnd, L"Chưa có chuỗi thao tác nào được ghi.", kTitle, MB_ICONINFORMATION);
                    else if (!g_playing) CloseHandle(CreateThread(nullptr, 0, PlayThread, nullptr, 0, nullptr));
                    break;
            }
            return 0;
        }
        case WM_DESTROY:
            SetSync(false); g_playing = false; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, kClassName, kTitle, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 820, 580,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
