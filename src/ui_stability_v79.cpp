#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <string>
#include <vector>

namespace autosync_v79_stability {

constexpr wchar_t kMainClass[] = L"AutoSyncClean.Main";
constexpr wchar_t kV79Title[] = L"AutoSync Clean v.79 Clean - Đồng Bộ Thao Tác Phím & Chuột";
constexpr int kListControlId = 1008;
constexpr UINT_PTR kRefreshTimerId = 1;
constexpr int kStableChangeSamples = 2;

std::atomic<WNDPROC> g_originalMainProc{nullptr};
std::atomic<HWND> g_mainWindow{nullptr};
std::vector<UINT_PTR> g_pendingListed;
std::vector<UINT_PTR> g_pendingActual;
int g_pendingSamples{};

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
    return value;
}

std::wstring ExecutablePath(HWND hwnd) {
    if (!hwnd) return {};
    DWORD processId{};
    GetWindowThreadProcessId(hwnd, &processId);
    if (!processId) return {};

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};

    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length)) length = 0;
    CloseHandle(process);
    path.resize(length);
    return ToLower(std::move(path));
}

std::vector<UINT_PTR> ListedHandles(HWND list, std::wstring& trackedPath) {
    std::vector<UINT_PTR> handles;
    if (!list) return handles;

    const int count = ListView_GetItemCount(list);
    handles.reserve(static_cast<size_t>(std::max(0, count)));
    for (int row = 0; row < count; ++row) {
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (!ListView_GetItem(list, &item)) continue;
        HWND gameWindow = reinterpret_cast<HWND>(item.lParam);
        if (!gameWindow) continue;
        handles.push_back(reinterpret_cast<UINT_PTR>(gameWindow));
        if (trackedPath.empty() && IsWindow(gameWindow)) trackedPath = ExecutablePath(gameWindow);
    }
    std::sort(handles.begin(), handles.end());
    handles.erase(std::unique(handles.begin(), handles.end()), handles.end());
    return handles;
}

struct EnumContext {
    const std::wstring* trackedPath{};
    std::vector<UINT_PTR>* handles{};
};

BOOL CALLBACK EnumSameExecutable(HWND hwnd, LPARAM parameter) {
    auto* context = reinterpret_cast<EnumContext*>(parameter);
    if (!context || !context->trackedPath || !context->handles || !IsWindow(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;
    if (ExecutablePath(hwnd) != *context->trackedPath) return TRUE;
    context->handles->push_back(reinterpret_cast<UINT_PTR>(hwnd));
    return TRUE;
}

std::vector<UINT_PTR> ActualHandles(const std::wstring& trackedPath) {
    std::vector<UINT_PTR> handles;
    if (trackedPath.empty()) return handles;
    EnumContext context{&trackedPath, &handles};
    EnumWindows(EnumSameExecutable, reinterpret_cast<LPARAM>(&context));
    std::sort(handles.begin(), handles.end());
    handles.erase(std::unique(handles.begin(), handles.end()), handles.end());
    return handles;
}

bool HoldTransientWindowChange(HWND mainWindow) {
    HWND list = GetDlgItem(mainWindow, kListControlId);
    if (!list) return false;

    std::wstring trackedPath;
    const auto listed = ListedHandles(list, trackedPath);
    if (listed.empty()) {
        g_pendingListed.clear();
        g_pendingActual.clear();
        g_pendingSamples = 0;
        return false;
    }

    // If every listed HWND is momentarily invalid, do not let one timer tick
    // tear down the whole preview strip. A persistent change will still be
    // accepted on the next identical sample.
    if (trackedPath.empty()) {
        if (listed == g_pendingListed && g_pendingActual.empty()) ++g_pendingSamples;
        else {
            g_pendingListed = listed;
            g_pendingActual.clear();
            g_pendingSamples = 1;
        }
        if (g_pendingSamples < kStableChangeSamples) return true;
        g_pendingListed.clear();
        g_pendingSamples = 0;
        return false;
    }

    const auto actual = ActualHandles(trackedPath);
    if (listed == actual) {
        g_pendingListed.clear();
        g_pendingActual.clear();
        g_pendingSamples = 0;
        return false;
    }

    if (listed == g_pendingListed && actual == g_pendingActual) ++g_pendingSamples;
    else {
        g_pendingListed = listed;
        g_pendingActual = actual;
        g_pendingSamples = 1;
    }

    if (g_pendingSamples < kStableChangeSamples) return true;

    // The change survived two refresh samples, so it is treated as a real
    // window open/close event and the normal v78 refresh logic may reconcile it.
    g_pendingListed.clear();
    g_pendingActual.clear();
    g_pendingSamples = 0;
    return false;
}

struct ScrollAnchor {
    int topIndex{-1};
};

ScrollAnchor CaptureScrollAnchor(HWND list) {
    ScrollAnchor anchor;
    if (list) anchor.topIndex = ListView_GetTopIndex(list);
    return anchor;
}

void RestoreScrollAnchor(HWND list, ScrollAnchor anchor) {
    if (!list || anchor.topIndex < 0) return;
    const int count = ListView_GetItemCount(list);
    if (count <= 0) return;

    const int wantedTop = std::clamp(anchor.topIndex, 0, count - 1);
    ListView_EnsureVisible(list, wantedTop, FALSE);

    for (int attempt = 0; attempt < 3; ++attempt) {
        const int currentTop = ListView_GetTopIndex(list);
        if (currentTop == wantedTop) break;
        if (currentTop < 0 || currentTop >= count) break;

        RECT itemRect{};
        if (!ListView_GetItemRect(list, currentTop, &itemRect, LVIR_BOUNDS)) break;
        const int rowHeight = std::max(1L, itemRect.bottom - itemRect.top);
        ListView_Scroll(list, 0, (wantedTop - currentTop) * rowHeight);
    }
}

LRESULT CALLBACK StableMainProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    WNDPROC original = g_originalMainProc.load(std::memory_order_acquire);
    if (!original) return DefWindowProcW(hwnd, message, wParam, lParam);

    if (message == WM_TIMER && wParam == kRefreshTimerId) {
        // A one-sample HWND disappearance/reappearance is the main trigger for
        // the occasional DWM preview strip churn. Ignore that transient sample.
        if (HoldTransientWindowChange(hwnd)) return 0;

        HWND list = GetDlgItem(hwnd, kListControlId);
        const ScrollAnchor anchor = CaptureScrollAnchor(list);
        const LRESULT result = CallWindowProcW(original, hwnd, message, wParam, lParam);
        RestoreScrollAnchor(list, anchor);
        return result;
    }

    if (message == WM_NCDESTROY) {
        const LRESULT result = CallWindowProcW(original, hwnd, message, wParam, lParam);
        g_mainWindow.store(nullptr, std::memory_order_release);
        g_originalMainProc.store(nullptr, std::memory_order_release);
        return result;
    }

    return CallWindowProcW(original, hwnd, message, wParam, lParam);
}

DWORD WINAPI InstallStabilityGuard(void*) {
    for (;;) {
        HWND hwnd = FindWindowW(kMainClass, nullptr);
        if (hwnd) {
            DWORD processId{};
            GetWindowThreadProcessId(hwnd, &processId);
            if (processId == GetCurrentProcessId()) {
                const auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
                if (current && current != StableMainProc) {
                    // Publish the chain target before replacing GWLP_WNDPROC so
                    // even an immediate UI message can always reach v78 safely.
                    g_originalMainProc.store(current, std::memory_order_release);
                    SetLastError(ERROR_SUCCESS);
                    const LONG_PTR previous = SetWindowLongPtrW(
                        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(StableMainProc));
                    if (previous != 0 || GetLastError() == ERROR_SUCCESS) {
                        if (previous != 0)
                            g_originalMainProc.store(reinterpret_cast<WNDPROC>(previous),
                                                     std::memory_order_release);
                        g_mainWindow.store(hwnd, std::memory_order_release);
                        SetWindowTextW(hwnd, kV79Title);
                        return 0;
                    }
                    g_originalMainProc.store(nullptr, std::memory_order_release);
                } else if (current == StableMainProc) {
                    SetWindowTextW(hwnd, kV79Title);
                    return 0;
                }
            }
        }
        Sleep(25);
    }
}

struct Bootstrap {
    Bootstrap() {
        HANDLE thread = CreateThread(nullptr, 0, InstallStabilityGuard, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
};

Bootstrap g_bootstrap;

}  // namespace autosync_v79_stability
