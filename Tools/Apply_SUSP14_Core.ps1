param(
    [Parameter(Mandatory=$false)]
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\.." ) -ErrorAction SilentlyContinue)
)
$ErrorActionPreference = "Stop"
if (-not $Root -or -not (Test-Path $Root)) { throw "RacingUnited root not found. Pass -Root C:\path\to\RacingUnited" }

$rootPath = (Resolve-Path $Root).Path
$all = Get-ChildItem $rootPath -Recurse -File -Include *.cpp,*.hpp,*.h,*.lua,*.ps1,*.md -ErrorAction SilentlyContinue
$hasSemi = $false
$hasTwist = $false
foreach ($f in $all) {
    $text = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if ($text -match 'semi_trailing_arm_v1') { $hasSemi = $true }
    if ($text -match 'twist_beam_v1') { $hasTwist = $true }
    if ($hasSemi -and $hasTwist) { break }
}
if (-not ($hasSemi -and $hasTwist)) {
    throw "SUSP13 baseline not detected (semi_trailing_arm_v1 + twist_beam_v1). Refusing to install SUSP14 core into an older tree."
}

$dst = Join-Path $rootPath "Engine\HeritageEngine\Vehicles\Suspension\MultiLinkKinematics.hpp"
New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
Copy-Item (Join-Path $PSScriptRoot "..\Engine\HeritageEngine\Vehicles\Suspension\MultiLinkKinematics.hpp") $dst -Force

Write-Host "Installed SUSP14 MultiLinkKinematics.hpp core into the confirmed SUSP13 tree."
Write-Host "Runtime registration is intentionally not regex-guessed. Follow SUSP14_INTEGRATION.md against the exact provider dispatcher in this checkout."
