param(
    [Parameter(Mandatory = $true)][string]$InputPath,
    [Parameter(Mandatory = $true)][string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText($InputPath) -replace "`r`n", "`n"

function Replace-Required {
    param([string]$Old, [string]$New, [string]$Name)
    $oldNormalized = $Old -replace "`r`n", "`n"
    $newNormalized = $New -replace "`r`n", "`n"
    if (-not $script:text.Contains($oldNormalized)) {
        throw "v82 patch failed: $Name"
    }
    $script:text = $script:text.Replace($oldNormalized, $newNormalized)
}

$text = $text.Replace('v81', 'v82')

Replace-Required @'
void RebuildList() {
    if (!g_list) return;
    const int selectedRow = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
'@ @'
void RebuildList() {
    if (!g_list) return;
    const bool previousBulkChecking = g_bulkChecking;
    g_bulkChecking = true;
    const int selectedRow = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
'@ 'protect checkbox state during rebuild'

Replace-Required @'
    g_lastRenderedListSignature = BuildListSignature();
'@ @'
    g_bulkChecking = previousBulkChecking;
    g_lastRenderedListSignature = BuildListSignature();
'@ 'restore checkbox notification mode'

Replace-Required @'
                if (checkStateChanged && checkboxChanged->iItem >= 0 &&
'@ @'
                if (!g_bulkChecking && checkStateChanged && checkboxChanged->iItem >= 0 &&
'@ 'ignore rebuild-generated checkbox notifications'

Replace-Required @'
            HWND proxy = button(IDC_PROXY, L"◉");
'@ '' 'remove Proxy toolbar button'

Replace-Required @'
            AddToolbarTooltip(proxy, L"Proxy");
'@ '' 'remove Proxy tooltip'

Replace-Required @'
    MoveWindow(GetDlgItem(hwnd, IDC_PROXY), right - 28, top, 28, buttonH, TRUE); right -= 32;
'@ '' 'remove Proxy toolbar slot'

$directory = Split-Path -Parent $OutputPath
if ($directory) { New-Item -ItemType Directory -Force -Path $directory | Out-Null }
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutputPath, $text, $utf8NoBom)
Write-Host "Generated v82 source: $OutputPath"
