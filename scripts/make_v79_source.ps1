param(
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText($InputPath)
$text = $text -replace "`r`n", "`n"

function Replace-Required {
    param(
        [Parameter(Mandatory = $true)][string]$Old,
        [Parameter(Mandatory = $true)][string]$New,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $oldNormalized = $Old -replace "`r`n", "`n"
    $newNormalized = $New -replace "`r`n", "`n"
    if (-not $script:text.Contains($oldNormalized)) {
        throw "v80 patch failed: expected source block not found: $Name"
    }
    $script:text = $script:text.Replace($oldNormalized, $newNormalized)
}

Replace-Required 'v.78 Clean' 'v80' 'version title'

Replace-Required @'
bool g_thumbnailDragMoved{};
bool g_bulkChecking{};
int g_arrangeSizeIndex{2};
'@ @'
bool g_thumbnailDragMoved{};
bool g_bulkChecking{};
int g_contextMenuRow{-1};
int g_arrangeSizeIndex{2};
'@ 'context-menu row state'

Replace-Required @'
    const int selectedRow = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    const int topRow = ListView_GetTopIndex(g_list);
    // The status timer rebuilds this list every three seconds. Suppress
'@ @'
    const int selectedRow = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
    const int topRow = ListView_GetTopIndex(g_list);
    RECT savedTopRect{};
    const bool hadTopRect = topRow >= 0 &&
        ListView_GetItemRect(g_list, topRow, &savedTopRect, LVIR_BOUNDS) != FALSE;
    const int savedTopPixel = hadTopRect ? savedTopRect.top : 0;
    // The status timer rebuilds this list every three seconds. Suppress
'@ 'capture list scroll position'

Replace-Required @'
    if (topRow >= 0 && topRow < static_cast<int>(g_windows.size()))
        ListView_EnsureVisible(g_list, topRow, FALSE);
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
'@ @'
    if (topRow >= 0 && !g_windows.empty()) {
        const int restoreRow = std::min(topRow, static_cast<int>(g_windows.size()) - 1);
        ListView_EnsureVisible(g_list, restoreRow, FALSE);
        if (hadTopRect) {
            RECT restoredRect{};
            if (ListView_GetItemRect(g_list, restoreRow, &restoredRect, LVIR_BOUNDS)) {
                // EnsureVisible can place the old top row near the bottom.
                // Apply the remaining pixel delta so the user's viewport stays put.
                ListView_Scroll(g_list, 0, restoredRect.top - savedTopPixel);
            }
        }
    }
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
'@ 'restore exact list scroll position'

Replace-Required @'
void ShowContextMenu(POINT p) {
    POINT clientPoint = p;
'@ @'
void ShowContextMenu(POINT p) {
    g_contextMenuRow = -1;
    POINT clientPoint = p;
'@ 'reset context row'

Replace-Required @'
    if (row < 0 || row >= static_cast<int>(g_windows.size())) {
        return;
    }
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
'@ @'
    if (row < 0 || row >= static_cast<int>(g_windows.size())) {
        return;
    }
    g_contextMenuRow = row;
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
'@ 'remember right-clicked row'

Replace-Required @'
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, g_main, nullptr);
    DestroyMenu(menu);
}
'@ @'
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, g_main, nullptr);
    DestroyMenu(menu);
    g_contextMenuRow = -1;
}
'@ 'clear context row after menu'

Replace-Required @'
        case IDM_REMOVE_ONE: {
            SyncChecksFromList();
            std::unordered_set<HWND> targets;
'@ @'
        case IDM_REMOVE_ONE: {
            SyncChecksFromList();

            // A right-click menu action always applies to the row that was clicked.
            // This also works for OFFLINE rows whose HWND has already become null.
            if (g_contextMenuRow >= 0 && g_contextMenuRow < static_cast<int>(g_windows.size())) {
                const size_t index = static_cast<size_t>(g_contextMenuRow);
                const HWND target = g_windows[index].hwnd;
                if (target && target == g_source) {
                    if (g_sync) SetSync(false);
                    g_source = nullptr;
                }
                if (target) g_ignored.insert(target);
                g_windows.erase(g_windows.begin() + index);
                RebuildList();
                RefreshThumbnailViewer(true);
                break;
            }

            std::unordered_set<HWND> targets;
'@ 'delete right-clicked row without checkbox'

$directory = Split-Path -Parent $OutputPath
if ($directory) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, $text, $utf8NoBom)
Write-Host "Generated v80 source: $OutputPath"
