param(
    [Parameter(Mandatory=$true)][string]$Root
)
$ErrorActionPreference = 'Stop'

# OPT03 renames the production GPU runtime and retires the old renderer-side
# page mirror. OPT03C also removes the production CPU Dynamic Surface Hydro
# solver after tire physics is bridged to the GPU authority. ZIP overlays cannot
# delete historical files, so converge the checkout before code-health/validation.
$retired = @(
    'Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuLodPrototype.cpp',
    'Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuLodPrototype.hpp',
    'Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuPagePool.cpp',
    'Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuPagePool.hpp',
    'Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.cpp',
    'Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.hpp'
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
        throw "OPT03 retirement failed; stale file remains: $relative"
    }
}

Write-Host "OPT03 retirement convergence: removed $removed stale overlay file(s); prototype names, unused renderer GPU page mirror, and production CPU Hydro are absent."
exit 0
