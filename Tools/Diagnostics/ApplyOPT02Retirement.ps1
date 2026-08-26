param(
    [Parameter(Mandatory=$true)][string]$Root
)
$ErrorActionPreference = 'Stop'

# OPT02: ZIP overlays cannot delete files from an existing checkout. Converge the
# single newly retired hydrology artifact before code-health audit/validation so
# an extracted milestone exactly matches the intended source tree.
$retired = @(
    'Engine\HeritageEngine\Physics\Surfaces\Water\VirtualPipeFlow.hpp'
)

$removed = 0
foreach ($relative in $retired) {
    $path = Join-Path $Root $relative
    if (Test-Path -LiteralPath $path) {
        Remove-Item -LiteralPath $path -Force
        $removed++
    }
}

foreach ($relative in $retired) {
    $path = Join-Path $Root $relative
    if (Test-Path -LiteralPath $path) {
        throw "OPT02 retirement failed; stale file remains: $relative"
    }
}

Write-Host "OPT02 retirement convergence: removed $removed stale overlay file(s); retired virtual-pipe hydrology is absent."
exit 0
