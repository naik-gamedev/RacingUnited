param(
    [Parameter(Mandatory=$true)]
    [string]$Root
)

$ErrorActionPreference = 'Stop'
$target = Join-Path $Root 'Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceBake.cpp'

if (-not (Test-Path -LiteralPath $target)) {
    Write-Host "OPT03C4 convergence FAIL: missing $target" -ForegroundColor Red
    exit 1
}

# OPT03C retired DynamicSurfaceSystem's production CPU-Hydro member. Two stale
# cleanup statements survived in the static bake/cache-load implementation.
# They have no replacement owner and must be absent; restoring the member would
# recreate the second production water authority that OPT03C intentionally removed.
$text = [System.IO.File]::ReadAllText($target)
$before = ([regex]::Matches($text, '(?m)^\s*m_hydrology\.clear\(\);\s*\r?$')).Count

if ($before -gt 0) {
    $text = [regex]::Replace($text, '(?m)^\s*m_hydrology\.clear\(\);\s*\r?\n', '')
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($target, $text, $utf8NoBom)
}

$verifyText = [System.IO.File]::ReadAllText($target)
$remaining = ([regex]::Matches($verifyText, '\bm_hydrology\.clear\s*\(')).Count
if ($remaining -ne 0) {
    Write-Host "OPT03C4 convergence FAIL: $remaining stale m_hydrology.clear() call(s) remain in DynamicSurfaceBake.cpp" -ForegroundColor Red
    exit 1
}

if ($before -gt 0) {
    Write-Host "OPT03C4 convergence: removed $before stale DynamicSurfaceBake CPU-Hydro clear call(s); source now matches single-GPU-water authority."
} else {
    Write-Host "OPT03C4 convergence: DynamicSurfaceBake already clean; 0 stale CPU-Hydro clear calls."
}
exit 0
