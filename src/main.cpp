#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <memory>
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
    IDC_PLAY, IDC_THUMBNAILS, IDC_LIST, IDC_STATUS, IDC_PLAN, IDC_SUPPORT, IDC_GROUP,
    IDM_SET_MAIN = 2001, IDM_TOGGLE_ITEM, IDM_REFRESH, IDM_CLOSE_ONE,
    IDM_REMOVE_ONE, IDM_SELECT_ALL, IDM_CLEAR_ALL, IDM_SHOW_ALL,
    IDM_CLOSE_ALL, IDM_REMOVE_ALL
};

enum : int {
    IDC_LAUNCH_PATH = 3001, IDC_LAUNCH_BROWSE, IDC_LAUNCH_ARGS,
    IDC_LAUNCH_COUNT, IDC_LAUNCH_DELAY, IDC_LAUNCH_OK
};

enum : int {
    IDC_ARRANGE_WIDTH = 3101, IDC_ARRANGE_HEIGHT, IDC_ARRANGE_X,
    IDC_ARRANGE_Y, IDC_ARRANGE_OFFSET_X, IDC_ARRANGE_OFFSET_Y,
    IDC_ARRANGE_KEEP_SIZE, IDC_ARRANGE_OK
};

struct WindowItem {
    HWND hwnd{};
    std::wstring title;
    bool selected{};
    std::wstring groupTitle;
};

struct ThumbnailItem {
    HWND source{};
    HTHUMBNAIL handle{};
    RECT destination{};
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
HWND g_launcher{};
HWND g_arranger{};
HWND g_thumbnailViewer{};
HHOOK g_keyboardHook{}, g_mouseHook{};
HFONT g_uiFont{}, g_smallFont{};
std::vector<WindowItem> g_windows;
std::vector<ThumbnailItem> g_thumbnails;
std::unordered_set<HWND> g_ignored;
std::vector<MacroEvent> g_macro;
HWND g_source{};
bool g_sync{}, g_recording{}, g_playing{}, g_blockMove{};
ULONGLONG g_lastMouseMoveBroadcast{};
std::chrono::steady_clock::time_point g_lastMacro;
constexpr wchar_t kSettingsKey[] = L"Software\\AutoSyncClean";

void SyncChecksFromList();
void RefreshWindows(bool clearIgnored);
void RefreshThumbnailViewer(bool force);

std::wstring LoadSettingString(const wchar_t* name, const wchar_t* fallback = L"") {
    wchar_t value[32768]{};
    DWORD bytes = sizeof(value), type{};
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_SZ,
                     &type, value, &bytes) == ERROR_SUCCESS) return value;
    return fallback;
}

DWORD LoadSettingDword(const wchar_t* name, DWORD fallback) {
    DWORD value{}, bytes = sizeof(value), type{};
    if (RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_DWORD,
                     &type, &value, &bytes) == ERROR_SUCCESS) return value;
    return fallback;
}

void SaveLaunchSettings(const std::wstring& path, const std::wstring& args, DWORD count, DWORD delay) {
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(key, L"GamePath", 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()),
                   static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(key, L"Arguments", 0, REG_SZ, reinterpret_cast<const BYTE*>(args.c_str()),
                   static_cast<DWORD>((args.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(key, L"WindowCount", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&count), sizeof(count));
    RegSetValueExW(key, L"LaunchDelay", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&delay), sizeof(delay));
    RegCloseKey(key);
}

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
    std::wstring title = WindowTitle(hwnd);
    std::transform(title.begin(), title.end(), title.begin(), towlower);
    const bool tracked = std::any_of(g_windows.begin(), g_windows.end(), [hwnd](const auto& window) {
        return window.hwnd == hwnd;
    });
    return tracked || title.find(L"doomsday") != std::wstring::npos;
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
        const bool isSource = g_source && w.hwnd == g_source;
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(i);
        auto number = isSource ? L"★" : std::to_wstring(i + 1);
        item.pszText = number.data();
        item.lParam = reinterpret_cast<LPARAM>(w.hwnd);
        int row = ListView_InsertItem(g_list, &item);
        auto id = HexHandle(w.hwnd);
        ListView_SetItemText(g_list, row, 1, id.data());
        std::wstring displayTitle = isSource ? L"★ [CỬA SỔ CHÍNH] " + w.title : w.title;
        ListView_SetItemText(g_list, row, 2, displayTitle.data());
        std::wstring state = isSource ? L"CỬA SỔ CHÍNH" :
            (IsWindow(w.hwnd) ? (g_sync && w.selected ? L"ĐANG ĐỒNG BỘ" : L"ONLINE") : L"OFFLINE");
        ListView_SetItemText(g_list, row, 3, state.data());
        auto size = WindowSize(w.hwnd);
        ListView_SetItemText(g_list, row, 4, size.data());
        ListView_SetCheckState(g_list, row, w.selected ? TRUE : FALSE);
    }
    std::wstring s = L"Cửa sổ: " + std::to_wstring(g_windows.size());
    if (g_source) s += L" | Cửa sổ chính: " + WindowTitle(g_source);
    SetStatus(s);
}

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM) {
    if (!IsCandidate(hwnd) || g_ignored.contains(hwnd)) return TRUE;
    auto it = std::find_if(g_windows.begin(), g_windows.end(), [hwnd](const auto& w) { return w.hwnd == hwnd; });
    const std::wstring title = WindowTitle(hwnd);
    if (it == g_windows.end()) {
        it = std::find_if(g_windows.begin(), g_windows.end(), [&](const auto& w) {
            return !IsWindow(w.hwnd) && w.groupTitle == title;
        });
        if (it != g_windows.end()) {
            it->hwnd = hwnd;
            if (it->title.rfind(L"Cửa sổ ", 0) == 0) SetWindowTextW(hwnd, it->title.c_str());
            else it->title = title;
        } else {
            const bool trackedGroup = std::any_of(g_windows.begin(), g_windows.end(), [&](const auto& w) {
                return w.groupTitle == title;
            });
            if (trackedGroup) g_windows.push_back({hwnd, title, false, title});
        }
    } else if (it->title.rfind(L"Cửa sổ ", 0) == 0) {
        if (title != it->title) SetWindowTextW(hwnd, it->title.c_str());
    } else {
        it->title = title;
        it->groupTitle = title;
    }
    return TRUE;
}

void RefreshWindows(bool clearIgnored = false) {
    if (g_list) SyncChecksFromList();
    if (clearIgnored) g_ignored.clear();
    for (auto& window : g_windows) {
        if (!IsWindow(window.hwnd) || !IsCandidate(window.hwnd)) {
            if (window.hwnd == g_source) g_source = nullptr;
            window.hwnd = nullptr;
        }
    }
    EnumWindows(EnumProc, 0);
    RebuildList();
    RefreshThumbnailViewer(false);
}

void SyncChecksFromList() {
    for (int i = 0; i < static_cast<int>(g_windows.size()); ++i)
        g_windows[static_cast<size_t>(i)].selected = ListView_GetCheckState(g_list, i) != FALSE;
}

bool PointInSource(POINT screen, POINT& client, RECT& rc) {
    HWND source = g_source ? g_source : GetForegroundWindow();
    if (!source || source == g_main) return false;
    HWND foreground = GetForegroundWindow();
    if (g_source && GetAncestor(foreground, GA_ROOT) != GetAncestor(g_source, GA_ROOT)) return false;
    client = screen;
    ScreenToClient(source, &client);
    GetClientRect(source, &rc);
    return client.x >= 0 && client.y >= 0 && client.x < rc.right && client.y < rc.bottom;
}

HWND InputTarget(HWND topLevel) {
    if (!IsWindow(topLevel)) return nullptr;
    DWORD threadId = GetWindowThreadProcessId(topLevel, nullptr);
    GUITHREADINFO info{sizeof(info)};
    if (threadId && GetGUIThreadInfo(threadId, &info) && info.hwndFocus &&
        (info.hwndFocus == topLevel || IsChild(topLevel, info.hwndFocus))) return info.hwndFocus;
    return topLevel;
}

bool DeliverInput(HWND topLevel, UINT message, WPARAM wp, LPARAM lp, bool critical) {
    HWND target = InputTarget(topLevel);
    if (!target) return false;
    if (!critical) return PostMessageW(target, message, wp, lp) != FALSE;
    DWORD_PTR result{};
    return SendMessageTimeoutW(target, message, wp, lp,
                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 20, &result) != 0;
}

void ActivateSourceWindow() {
    if (!g_source || !IsWindow(g_source)) return;
    ShowWindowAsync(g_source, SW_RESTORE);
    SetWindowPos(g_source, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    DWORD appThread = GetCurrentThreadId();
    DWORD sourceThread = GetWindowThreadProcessId(g_source, nullptr);
    const bool attached = sourceThread && sourceThread != appThread &&
                          AttachThreadInput(appThread, sourceThread, TRUE);
    BringWindowToTop(g_source);
    SetForegroundWindow(g_source);
    SetActiveWindow(g_source);
    if (HWND focus = InputTarget(g_source)) SetFocus(focus);
    if (attached) AttachThreadInput(appThread, sourceThread, FALSE);
}

LPARAM ScaledPoint(HWND target, double nx, double ny) {
    RECT r{};
    GetClientRect(target, &r);
    int maxX = std::max(0, static_cast<int>(r.right) - 1);
    int maxY = std::max(0, static_cast<int>(r.bottom) - 1);
    int x = std::clamp(static_cast<int>(nx * r.right), 0, maxX);
    int y = std::clamp(static_cast<int>(ny * r.bottom), 0, maxY);
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
    if (msg == WM_MOUSEMOVE) {
        ULONGLONG now = GetTickCount64();
        if (now - g_lastMouseMoveBroadcast < 16) return;
        g_lastMouseMoveBroadcast = now;
    }
    const bool critical = msg != WM_MOUSEMOVE;
    ForTargets([&](HWND h) {
        HWND target = InputTarget(h);
        if (target) DeliverInput(h, msg, wp, ScaledPoint(target, nx, ny), critical);
    });
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
            if (!g_source || GetAncestor(fg, GA_ROOT) == GetAncestor(g_source, GA_ROOT)) {
                UINT msg = static_cast<UINT>(wp);
                LPARAM keyData = 1 | (static_cast<LPARAM>(k->scanCode) << 16);
                if (k->flags & LLKHF_EXTENDED) keyData |= 1LL << 24;
                if (msg == WM_KEYUP || msg == WM_SYSKEYUP) keyData |= (1LL << 30) | (1LL << 31);
                ForTargets([&](HWND h) {
                    DeliverInput(h, msg, k->vkCode, keyData, true);
                });
                AddMacro({(msg == WM_KEYUP || msg == WM_SYSKEYUP) ? MacroType::KeyUp : MacroType::KeyDown,
                          k->vkCode, 0, 0, 0, 0});
            }
        }
    }
    return CallNextHookEx(g_keyboardHook, code, wp, lp);
}

void SetSync(bool on) {
    if (on && (!g_source || !IsWindow(g_source))) {
        MessageBoxW(g_main, L"Hãy chọn một cửa sổ ONLINE làm Cửa sổ chính trước khi bật đồng bộ.",
                    kTitle, MB_ICONINFORMATION);
        return;
    }
    g_sync = on;
    SetWindowTextW(g_btnSync, on ? L"■ Tắt đồng bộ" : L"⟳ Bật đồng bộ");
    if (on) {
        g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, g_instance, 0);
        g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, g_instance, 0);
        if (!g_keyboardHook || !g_mouseHook) {
            MessageBoxW(g_main, L"Không thể cài hook. Hãy thử chạy bằng quyền Administrator nếu cửa sổ đích đang chạy quyền cao.", kTitle, MB_ICONERROR);
            g_sync = false;
            SetWindowTextW(g_btnSync, L"⟳ Bật đồng bộ");
        } else {
            g_lastMouseMoveBroadcast = 0;
            ActivateSourceWindow();
        }
    } else {
        if (g_keyboardHook) UnhookWindowsHookEx(g_keyboardHook);
        if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
        g_keyboardHook = g_mouseHook = nullptr;
    }
    RebuildList();
    if (g_sync && g_source)
        SetStatus(L"Đang đồng bộ từ " + WindowTitle(g_source) + L". Cửa sổ chính đã được kích hoạt.");
}

HWND SelectedHwnd() {
    int row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    if (row < 0 || row >= static_cast<int>(g_windows.size())) return nullptr;
    return g_windows[static_cast<size_t>(row)].hwnd;
}

int SelectedIndex() {
    int row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    return row >= 0 && row < static_cast<int>(g_windows.size()) ? row : -1;
}

void SetMainWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    g_source = hwnd;
    RebuildList();
    SetStatus(L"Đã chọn cửa sổ chính: " + WindowTitle(hwnd));
}

void RenameWindowsSequentially() {
    for (size_t i = 0; i < g_windows.size(); ++i) {
        auto& window = g_windows[i];
        window.title = L"Cửa sổ " + std::to_wstring(i + 1);
        if (IsWindow(window.hwnd)) SetWindowTextW(window.hwnd, window.title.c_str());
    }
    RebuildList();
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
    POINT clientPoint = p;
    ScreenToClient(g_list, &clientPoint);
    LVHITTESTINFO hit{};
    hit.pt = clientPoint;
    int row = ListView_HitTest(g_list, &hit);
    if (row < 0 || row >= static_cast<int>(g_windows.size())) return;
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_list, row, FALSE);
    SetFocus(g_list);
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
            if (int row = SelectedIndex(); row >= 0) {
                if (selected) g_ignored.insert(selected);
                g_windows.erase(g_windows.begin() + row);
                RebuildList(); RefreshThumbnailViewer(true);
            }
            break;
        case IDM_SELECT_ALL: case IDM_CLEAR_ALL:
            for (int i = 0; i < ListView_GetItemCount(g_list); ++i) ListView_SetCheckState(g_list, i, id == IDM_SELECT_ALL);
            SyncChecksFromList();
            if (id == IDM_SELECT_ALL) RenameWindowsSequentially();
            else RebuildList();
            break;
        case IDM_SHOW_ALL:
            SyncChecksFromList();
            for (const auto& window : g_windows)
                if (window.selected && IsWindow(window.hwnd)) ShowWindow(window.hwnd, SW_RESTORE);
            break;
        case IDM_CLOSE_ALL: {
            SyncChecksFromList();
            if (g_sync) SetSync(false);
            for (const auto& window : g_windows)
                if (window.selected && IsWindow(window.hwnd)) PostMessageW(window.hwnd, WM_CLOSE, 0, 0);
            SetStatus(L"Đã gửi lệnh đóng tới tất cả cửa sổ được chọn, bao gồm cửa sổ chính.");
            break;
        }
        case IDM_REMOVE_ALL:
            for (auto& w : g_windows) if (w.hwnd) g_ignored.insert(w.hwnd);
            g_windows.clear(); RebuildList(); break;
    }
}

std::wstring ExecutablePath(HWND hwnd) {
    if (!hwnd) return {};
    DWORD pid{}; GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &size)) size = 0;
    CloseHandle(process);
    path.resize(size);
    return path;
}

struct LaunchRequest {
    std::wstring path;
    std::wstring args;
    int count{1};
    DWORD delay{2000};
};

DWORD WINAPI LaunchThread(void* raw) {
    std::unique_ptr<LaunchRequest> request(static_cast<LaunchRequest*>(raw));
    std::wstring directory = request->path;
    size_t slash = directory.find_last_of(L"\\/");
    if (slash != std::wstring::npos) directory.resize(slash);
    int launched = 0;
    for (int i = 0; i < request->count; ++i) {
        std::wstring command = L"\"" + request->path + L"\"";
        if (!request->args.empty()) command += L" " + request->args;
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        if (CreateProcessW(request->path.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
                           CREATE_NEW_PROCESS_GROUP, nullptr,
                           directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
            ++launched;
            CloseHandle(process.hThread); CloseHandle(process.hProcess);
        }
        if (i + 1 < request->count) Sleep(request->delay);
    }
    PostMessageW(g_main, WM_APP + 1, static_cast<WPARAM>(launched), static_cast<LPARAM>(request->count));
    return 0;
}

std::wstring ControlText(HWND hwnd, int id) {
    HWND control = GetDlgItem(hwnd, id);
    int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            auto label = [&](const wchar_t* text, int x, int y, int w) {
                HWND c = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 20, hwnd, nullptr, g_instance, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            };
            auto edit = [&](int id, const wchar_t* text, int x, int y, int w) {
                HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                         x, y, w, 23, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE); return c;
            };
            label(L"Đường dẫn:", 14, 18, 72);
            std::wstring path = ExecutablePath(SelectedHwnd());
            if (path.empty()) path = LoadSettingString(L"GamePath");
            edit(IDC_LAUNCH_PATH, path.c_str(), 86, 15, 445);
            HWND browse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 538, 15, 43, 23,
                                        hwnd, reinterpret_cast<HMENU>(IDC_LAUNCH_BROWSE), g_instance, nullptr);
            SendMessageW(browse, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            const std::wstring args = LoadSettingString(L"Arguments", L"-la");
            const std::wstring count = std::to_wstring(LoadSettingDword(L"WindowCount", 1));
            const std::wstring delay = std::to_wstring(LoadSettingDword(L"LaunchDelay", 2000));
            label(L"Tham số:", 14, 55, 72); edit(IDC_LAUNCH_ARGS, args.c_str(), 86, 52, 160);
            label(L"Số cửa sổ:", 14, 91, 72); edit(IDC_LAUNCH_COUNT, count.c_str(), 86, 88, 60);
            label(L"Thời gian giãn cách:", 182, 91, 140); edit(IDC_LAUNCH_DELAY, delay.c_str(), 322, 88, 80);
            label(L"(Mili giây)", 410, 91, 90);
            HWND ok = CreateWindowW(L"BUTTON", L"Xác nhận", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    260, 132, 88, 30, hwnd, reinterpret_cast<HMENU>(IDC_LAUNCH_OK), g_instance, nullptr);
            SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_LAUNCH_BROWSE) {
                wchar_t path[MAX_PATH]{};
                auto current = ControlText(hwnd, IDC_LAUNCH_PATH);
                wcsncpy_s(path, current.c_str(), _TRUNCATE);
                OPENFILENAMEW dialog{sizeof(dialog)};
                dialog.hwndOwner = hwnd; dialog.lpstrFile = path; dialog.nMaxFile = MAX_PATH;
                dialog.lpstrFilter = L"Chương trình Windows (*.exe)\0*.exe\0Tất cả tệp\0*.*\0";
                dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&dialog)) SetWindowTextW(GetDlgItem(hwnd, IDC_LAUNCH_PATH), path);
                return 0;
            }
            if (LOWORD(wp) == IDC_LAUNCH_OK) {
                auto path = ControlText(hwnd, IDC_LAUNCH_PATH);
                if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
                    MessageBoxW(hwnd, L"Đường dẫn chương trình không hợp lệ.", kTitle, MB_ICONWARNING); return 0;
                }
                auto request = std::make_unique<LaunchRequest>();
                request->path = path; request->args = ControlText(hwnd, IDC_LAUNCH_ARGS);
                request->count = std::clamp(_wtoi(ControlText(hwnd, IDC_LAUNCH_COUNT).c_str()), 1, 50);
                request->delay = static_cast<DWORD>(std::clamp(_wtoi(ControlText(hwnd, IDC_LAUNCH_DELAY).c_str()), 0, 60000));
                SaveLaunchSettings(request->path, request->args,
                                   static_cast<DWORD>(request->count), request->delay);
                HANDLE thread = CreateThread(nullptr, 0, LaunchThread, request.release(), 0, nullptr);
                if (thread) CloseHandle(thread);
                DestroyWindow(hwnd); return 0;
            }
            break;
        case WM_DESTROY: g_launcher = nullptr; return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowLauncher() {
    if (g_launcher) { ShowWindow(g_launcher, SW_RESTORE); SetForegroundWindow(g_launcher); return; }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = LauncherProc; wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"AutoSyncClean.Launcher"; wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc); registered = true;
    }
    RECT owner{}; GetWindowRect(g_main, &owner);
    g_launcher = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoSyncClean.Launcher", L"Mở cửa sổ",
                                 WS_CAPTION | WS_SYSMENU, owner.left + 20, owner.top + 80, 610, 215,
                                 g_main, nullptr, g_instance, nullptr);
    ShowWindow(g_launcher, SW_SHOW); UpdateWindow(g_launcher);
}

struct ArrangeRequest {
    int width{960}, height{540}, x{0}, y{20}, offsetX{0}, offsetY{0};
    bool keepSize{};
};

void ArrangeOverlapped(const ArrangeRequest& request) {
    SyncChecksFromList();
    std::vector<HWND> targets;
    for (auto& item : g_windows) if (item.selected && IsWindow(item.hwnd)) targets.push_back(item.hwnd);
    if (targets.empty()) {
        MessageBoxW(g_main, L"Chưa chọn cửa sổ game nào.", kTitle, MB_ICONINFORMATION); return;
    }
    struct DesiredWindow { HWND hwnd; int x, y, width, height; };
    std::vector<DesiredWindow> desiredWindows;
    for (size_t i = 0; i < targets.size(); ++i) {
        HWND target = targets[i];
        WINDOWPLACEMENT placement{sizeof(placement)};
        if (GetWindowPlacement(target, &placement) && placement.showCmd != SW_SHOWNORMAL) {
            placement.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement(target, &placement);
        }
        ShowWindowAsync(target, SW_RESTORE);
        RECT current{}; GetWindowRect(target, &current);
        int outerWidth = current.right - current.left;
        int outerHeight = current.bottom - current.top;
        if (!request.keepSize) {
            RECT desired{0, 0, request.width, request.height};
            DWORD style = static_cast<DWORD>(GetWindowLongPtrW(target, GWL_STYLE));
            DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(target, GWL_EXSTYLE));
            AdjustWindowRectEx(&desired, style, GetMenu(target) != nullptr, exStyle);
            outerWidth = desired.right - desired.left;
            outerHeight = desired.bottom - desired.top;
        }
        int x = request.x + static_cast<int>(i) * request.offsetX;
        int y = request.y + static_cast<int>(i) * request.offsetY;
        desiredWindows.push_back({target, x, y, outerWidth, outerHeight});
    }
    auto applyPositions = [&]() {
        for (const auto& desired : desiredWindows) {
            SetWindowPos(desired.hwnd, nullptr, desired.x, desired.y, desired.width, desired.height,
                         SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
            MoveWindow(desired.hwnd, desired.x, desired.y, desired.width, desired.height, TRUE);
        }
    };
    applyPositions();
    Sleep(120);
    applyPositions();
    int succeeded = 0;
    for (const auto& desired : desiredWindows) {
        RECT actual{};
        if (GetWindowRect(desired.hwnd, &actual) &&
            abs(actual.left - desired.x) <= 3 && abs(actual.top - desired.y) <= 3 &&
            (request.keepSize || (abs((actual.right - actual.left) - desired.width) <= 8 &&
                                  abs((actual.bottom - actual.top) - desired.height) <= 8))) {
            ++succeeded;
        }
    }
    if (g_source && IsWindow(g_source)) SetWindowPos(g_source, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetStatus(L"Đã xếp chồng " + std::to_wstring(succeeded) + L"/" + std::to_wstring(targets.size()) + L" cửa sổ game.");
    if (succeeded != static_cast<int>(targets.size())) {
        MessageBoxW(g_main,
            L"Một số cửa sổ game đã từ chối thay đổi vị trí hoặc kích thước. Hãy đóng bản này, sau đó nhấp phải AutoSyncClean.exe và chọn Run as administrator rồi thử lại.",
            kTitle, MB_ICONWARNING);
    }
}

LRESULT CALLBACK ArrangerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            auto label = [&](const wchar_t* text, int x, int y, int w) {
                HWND c = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 20, hwnd, nullptr, g_instance, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            };
            auto edit = [&](int id, const wchar_t* text, int x, int y, int w) {
                HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text, WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                         x, y, w, 23, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE); return c;
            };
            label(L"Xếp chồng lên nhau", 18, 16, 220);
            HWND keep = CreateWindowW(L"BUTTON", L"Không thay đổi kích thước hiện tại", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      18, 47, 250, 22, hwnd, reinterpret_cast<HMENU>(IDC_ARRANGE_KEEP_SIZE), g_instance, nullptr);
            SendMessageW(keep, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            label(L"Kích thước vùng game:", 18, 83, 150); edit(IDC_ARRANGE_WIDTH, L"960", 174, 80, 68);
            label(L"×", 249, 83, 18); edit(IDC_ARRANGE_HEIGHT, L"540", 270, 80, 68);
            label(L"Vị trí cách trái (X):", 18, 119, 150); edit(IDC_ARRANGE_X, L"0", 174, 116, 68); label(L"Pixel", 250, 119, 45);
            label(L"Vị trí cách trên (Y):", 18, 155, 150); edit(IDC_ARRANGE_Y, L"20", 174, 152, 68); label(L"Pixel", 250, 155, 45);
            label(L"Độ lệch mỗi cửa sổ X:", 18, 191, 160); edit(IDC_ARRANGE_OFFSET_X, L"0", 174, 188, 68);
            label(L"Y:", 250, 191, 24); edit(IDC_ARRANGE_OFFSET_Y, L"0", 274, 188, 64);
            HWND ok = CreateWindowW(L"BUTTON", L"Xác nhận", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    143, 230, 94, 30, hwnd, reinterpret_cast<HMENU>(IDC_ARRANGE_OK), g_instance, nullptr);
            SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_ARRANGE_OK) {
                ArrangeRequest request;
                request.width = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_WIDTH).c_str()), 320, 7680);
                request.height = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_HEIGHT).c_str()), 240, 4320);
                request.x = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_X).c_str()), 0, 30000);
                request.y = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_Y).c_str()), 0, 30000);
                request.offsetX = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_OFFSET_X).c_str()), 0, 2000);
                request.offsetY = std::clamp(_wtoi(ControlText(hwnd, IDC_ARRANGE_OFFSET_Y).c_str()), 0, 2000);
                request.keepSize = Button_GetCheck(GetDlgItem(hwnd, IDC_ARRANGE_KEEP_SIZE)) == BST_CHECKED;
                ArrangeOverlapped(request); DestroyWindow(hwnd); return 0;
            }
            break;
        case WM_DESTROY: g_arranger = nullptr; return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowArranger() {
    if (g_arranger) { ShowWindow(g_arranger, SW_RESTORE); SetForegroundWindow(g_arranger); return; }
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)}; wc.lpfnWndProc = ArrangerProc; wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = L"AutoSyncClean.Arranger"; wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        RegisterClassExW(&wc); registered = true;
    }
    RECT owner{}; GetWindowRect(g_main, &owner);
    g_arranger = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoSyncClean.Arranger", L"Sắp xếp cửa sổ",
                                 WS_CAPTION | WS_SYSMENU, owner.left + 90, owner.top + 70, 380, 310,
                                 g_main, nullptr, g_instance, nullptr);
    ShowWindow(g_arranger, SW_SHOW); UpdateWindow(g_arranger);
}

void ClearThumbnails() {
    for (auto& item : g_thumbnails)
        if (item.handle) DwmUnregisterThumbnail(item.handle);
    g_thumbnails.clear();
}

void LayoutThumbnails(HWND hwnd) {
    RECT client{}; GetClientRect(hwnd, &client);
    const int count = static_cast<int>(g_thumbnails.size());
    if (!count || client.right <= 0 || client.bottom <= 0) return;
    constexpr int gap = 4;
    const int available = std::max(1, static_cast<int>(client.right) - gap * (count + 1));
    const int cellWidth = std::max(90, std::min(230, available / count));
    int left = gap;
    for (auto& item : g_thumbnails) {
        SIZE sourceSize{};
        DwmQueryThumbnailSourceSize(item.handle, &sourceSize);
        const int maxWidth = std::max(1, cellWidth);
        const int maxHeight = std::max(1, static_cast<int>(client.bottom) - gap * 2);
        double scale = 1.0;
        if (sourceSize.cx > 0 && sourceSize.cy > 0)
            scale = std::min(static_cast<double>(maxWidth) / sourceSize.cx,
                             static_cast<double>(maxHeight) / sourceSize.cy);
        const int width = std::max(1, static_cast<int>(sourceSize.cx * scale));
        const int height = std::max(1, static_cast<int>(sourceSize.cy * scale));
        const int top = gap + (maxHeight - height) / 2;
        item.destination = {left + (cellWidth - width) / 2, top,
                            left + (cellWidth - width) / 2 + width, top + height};
        DWM_THUMBNAIL_PROPERTIES properties{};
        properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE |
                             DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
        properties.rcDestination = item.destination;
        properties.fVisible = TRUE;
        properties.opacity = 255;
        properties.fSourceClientAreaOnly = FALSE;
        DwmUpdateThumbnailProperties(item.handle, &properties);
        left += cellWidth + gap;
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

bool ThumbnailSourcesChanged() {
    std::vector<HWND> online;
    for (const auto& window : g_windows) if (IsWindow(window.hwnd)) online.push_back(window.hwnd);
    if (g_thumbnails.size() != online.size()) return true;
    for (size_t i = 0; i < online.size(); ++i)
        if (g_thumbnails[i].source != online[i] || !IsWindow(g_thumbnails[i].source)) return true;
    return false;
}

void RefreshThumbnailViewer(bool force = false) {
    if (!g_thumbnailViewer || (!force && !ThumbnailSourcesChanged())) return;
    ClearThumbnails();
    for (const auto& window : g_windows) {
        if (!IsWindow(window.hwnd)) continue;
        HTHUMBNAIL thumbnail{};
        if (SUCCEEDED(DwmRegisterThumbnail(g_thumbnailViewer, window.hwnd, &thumbnail)))
            g_thumbnails.push_back({window.hwnd, thumbnail, {}});
    }
    LayoutThumbnails(g_thumbnailViewer);
}

LRESULT CALLBACK ThumbnailViewerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_thumbnailViewer = hwnd;
            RefreshThumbnailViewer(true);
            return 0;
        case WM_SIZE:
            LayoutThumbnails(hwnd);
            return 0;
        case WM_ERASEBKGND: {
            RECT client{}; GetClientRect(hwnd, &client);
            FillRect(reinterpret_cast<HDC>(wp), &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            return TRUE;
        }
        case WM_LBUTTONDOWN: {
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            for (const auto& item : g_thumbnails) {
                if (PtInRect(&item.destination, point) && IsWindow(item.source)) {
                    ShowWindow(item.source, SW_RESTORE);
                    SetWindowPos(item.source, HWND_TOP, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    SetForegroundWindow(item.source);
                    break;
                }
            }
            return 0;
        }
        case WM_DESTROY:
            ClearThumbnails();
            g_thumbnailViewer = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ToggleThumbnailViewer() {
    if (g_thumbnailViewer) {
        DestroyWindow(g_thumbnailViewer);
        return;
    }
    RenameWindowsSequentially();
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = ThumbnailViewerProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.lpszClassName = L"AutoSyncClean.ThumbnailViewer";
        RegisterClassExW(&wc);
        registered = true;
    }
    RECT work{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    constexpr int height = 180;
    HWND viewer = CreateWindowExW(WS_EX_TOOLWINDOW, L"AutoSyncClean.ThumbnailViewer", L"Xem cửa sổ thu nhỏ",
                                  WS_OVERLAPPEDWINDOW, work.left, std::max(work.top, work.bottom - height),
                                  work.right - work.left, height, nullptr, nullptr, g_instance, nullptr);
    if (viewer) {
        ShowWindow(viewer, SW_SHOW);
        UpdateWindow(viewer);
    }
}

LRESULT CALLBACK WindowPickerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                  UINT_PTR, DWORD_PTR) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            SetStatus(L"Giữ chuột và kéo biểu tượng tròn vào cửa sổ game...");
            return 0;
        case WM_MOUSEMOVE:
            if (GetCapture() == hwnd) SetCursor(LoadCursorW(nullptr, IDC_CROSS));
            return 0;
        case WM_LBUTTONUP:
            if (GetCapture() == hwnd) {
                ReleaseCapture();
                POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ClientToScreen(hwnd, &point);
                HWND target = GetAncestor(WindowFromPoint(point), GA_ROOT);
                if (target && IsCandidate(target))
                    SendMessageW(g_main, WM_APP + 2, reinterpret_cast<WPARAM>(target), 0);
                else {
                    RefreshWindows(true);
                    SetStatus(L"Không nhận được cửa sổ. Hãy kéo và thả đúng vào cửa sổ game Doomsday.");
                }
            }
            return 0;
        case WM_SETCURSOR:
            if (GetCapture() == hwnd) {
                SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            }
            break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void AcceptPickedWindow(HWND target) {
    if (!IsCandidate(target)) return;
    const std::wstring title = WindowTitle(target);
    auto item = std::find_if(g_windows.begin(), g_windows.end(), [target](const auto& window) {
        return window.hwnd == target;
    });
    if (item == g_windows.end()) {
        item = std::find_if(g_windows.begin(), g_windows.end(), [&](const auto& window) {
            return !IsWindow(window.hwnd) && window.groupTitle == title;
        });
        if (item == g_windows.end()) g_windows.push_back({target, title, false, title});
        else item->hwnd = target;
    }
    g_ignored.erase(target);
    RebuildList();
    RefreshThumbnailViewer(true);
    SetStatus(L"Đã nhận cửa sổ game: " + title);
}

void Layout(HWND hwnd) {
    RECT r{}; GetClientRect(hwnd, &r);
    constexpr int gap = 4, top = 4, buttonH = 27;
    MoveWindow(GetDlgItem(hwnd, IDC_REFRESH), gap, top, 28, buttonH, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SET_MAIN), 35, top, 28, buttonH, TRUE);
    MoveWindow(g_btnSync, 67, top, 104, buttonH, TRUE);
    int right = r.right - gap;
    MoveWindow(GetDlgItem(hwnd, IDC_PLAY), right - 28, top, 28, buttonH, TRUE); right -= 32;
    MoveWindow(GetDlgItem(hwnd, IDC_RECORD), right - 28, top, 28, buttonH, TRUE); right -= 32;
    MoveWindow(GetDlgItem(hwnd, IDC_THUMBNAILS), right - 28, top, 28, buttonH, TRUE); right -= 32;
    MoveWindow(GetDlgItem(hwnd, IDC_TILE), right - 28, top, 28, buttonH, TRUE);
    MoveWindow(g_list, gap, 35, std::max(100L, r.right - gap * 2), std::max(80L, r.bottom - 67), TRUE);
    int bottom = r.bottom - 28;
    MoveWindow(GetDlgItem(hwnd, IDC_PLAN), gap, bottom, 70, 23, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_SUPPORT), 78, bottom, 72, 23, TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_GROUP), 154, bottom, 86, 23, TRUE);
    MoveWindow(g_status, 248, bottom + 3, std::max(60L, r.right - 254), 18, TRUE);
}

COLORREF ButtonColor(int id) {
    switch (id) {
        case IDC_SYNC: return g_sync ? RGB(238, 82, 83) : RGB(76, 210, 91);
        case IDC_PLAN: return RGB(243, 137, 57);
        case IDC_SUPPORT: return RGB(76, 200, 115);
        case IDC_GROUP: return RGB(54, 153, 219);
        default: return RGB(247, 249, 251);
    }
}

void DrawButton(const DRAWITEMSTRUCT* d) {
    RECT rc = d->rcItem;
    const int id = static_cast<int>(d->CtlID);
    COLORREF bg = ButtonColor(id);
    if (d->itemState & ODS_SELECTED) {
        bg = RGB(std::max(0, static_cast<int>(GetRValue(bg)) - 24),
                 std::max(0, static_cast<int>(GetGValue(bg)) - 24),
                 std::max(0, static_cast<int>(GetBValue(bg)) - 24));
    }
    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(d->hDC, &rc, brush); DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, 1, id == IDC_SYNC || id >= IDC_PLAN ? RGB(255,255,255) : RGB(173,181,189));
    auto oldPen = SelectObject(d->hDC, pen);
    auto oldBrush = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
    Rectangle(d->hDC, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(d->hDC, oldBrush); SelectObject(d->hDC, oldPen); DeleteObject(pen);
    wchar_t text[80]{}; GetWindowTextW(d->hwndItem, text, 80);
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, id == IDC_SYNC || id >= IDC_PLAN ? RGB(255,255,255) : RGB(45,52,59));
    auto oldFont = SelectObject(d->hDC, g_uiFont);
    DrawTextW(d->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(d->hDC, oldFont);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_main = hwnd;
            g_uiFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, VIETNAMESE_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_smallFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, VIETNAMESE_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            auto button = [&](int id, const wchar_t* text) {
                HWND control = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0,
                                             hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance, nullptr);
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
                return control;
            };
            HWND picker = button(IDC_REFRESH, L"◎");
            SetWindowSubclass(picker, WindowPickerProc, 1, 0);
            button(IDC_SET_MAIN, L"▣");
            g_btnSync = button(IDC_SYNC, L"⟳  Bật đồng bộ");
            button(IDC_TILE, L"▦");
            button(IDC_THUMBNAILS, L"▤");
            button(IDC_RECORD, L"●");
            button(IDC_PLAY, L"▶");
            button(IDC_PLAN, L"Miễn phí");
            button(IDC_SUPPORT, L"☎  Hỗ trợ");
            button(IDC_GROUP, L"Cộng đồng");
            g_list = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SHOWSELALWAYS,
                                  0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_LIST), g_instance, nullptr);
            SendMessageW(g_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_smallFont), TRUE);
            ListView_SetExtendedListViewStyle(g_list, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
            ListView_SetBkColor(g_list, RGB(255,255,255));
            ListView_SetTextBkColor(g_list, RGB(255,255,255));
            InsertColumn(0, 34, L"#"); InsertColumn(1, 92, L"Mã cửa sổ");
            InsertColumn(2, 245, L"Tiêu đề"); InsertColumn(3, 128, L"Trạng thái");
            InsertColumn(4, 90, L"Kích thước");
            g_status = CreateWindowW(L"STATIC", L"Sẵn sàng", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                    0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_STATUS), g_instance, nullptr);
            SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(g_smallFont), TRUE);
            const COLORREF caption = RGB(43, 139, 226);
            DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
            SetTimer(hwnd, 1, 3000, nullptr);
            RefreshWindows();
            return 0;
        }
        case WM_DRAWITEM:
            DrawButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp)); return TRUE;
        case WM_CTLCOLORSTATIC:
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize = {560, 380};
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
                case IDC_SET_MAIN: ShowLauncher(); break;
                case IDC_TILE: ShowArranger(); break;
                case IDC_THUMBNAILS: ToggleThumbnailViewer(); break;
                case IDC_RECORD: ToggleRecord(); break;
                case IDC_PLAY:
                    if (g_macro.empty()) MessageBoxW(hwnd, L"Chưa có chuỗi thao tác nào được ghi.", kTitle, MB_ICONINFORMATION);
                    else if (!g_playing) CloseHandle(CreateThread(nullptr, 0, PlayThread, nullptr, 0, nullptr));
                    break;
            }
            return 0;
        }
        case WM_APP + 1: {
            RefreshWindows(true);
            std::wstring text = L"Đã mở " + std::to_wstring(wp) + L"/" + std::to_wstring(lp) + L" cửa sổ.";
            SetStatus(text);
            if (wp != static_cast<WPARAM>(lp))
                MessageBoxW(hwnd, L"Một số tiến trình không mở được. Game có thể đang chặn đa phiên hoặc cần chạy quyền Administrator.", kTitle, MB_ICONWARNING);
            return 0;
        }
        case WM_APP + 2:
            AcceptPickedWindow(reinterpret_cast<HWND>(wp));
            return 0;
        case WM_DESTROY:
            SetSync(false); g_playing = false;
            if (g_thumbnailViewer) DestroyWindow(g_thumbnailViewer);
            if (g_uiFont) DeleteObject(g_uiFont);
            if (g_smallFont) DeleteObject(g_smallFont);
            PostQuitMessage(0); return 0;
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
                                CW_USEDEFAULT, CW_USEDEFAULT, 610, 480,
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
