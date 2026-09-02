param([string]$RepoRoot = (Get-Location).Path)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $RepoRoot 'Engine\HeritageEngine\Vehicles\Suspension'
if (!(Test-Path $RepoRoot)) { throw "Repository root not found: $RepoRoot" }

$markers = @('semi_trailing_arm_v1','twist_beam_v1','multi_link_v1')
$sourceFiles = Get-ChildItem -Path $RepoRoot -Recurse -File -Include *.hpp,*.h,*.cpp,*.lua,*.txt -ErrorAction SilentlyContinue
foreach ($marker in $markers) {
  $found = $sourceFiles | Select-String -Pattern $marker -SimpleMatch -List -ErrorAction SilentlyContinue
  if (!$found) { throw "Refusing install: expected SUSP13/SUSP14 marker '$marker' was not found." }
}
New-Item -ItemType Directory -Force -Path $dest | Out-Null
$names = @(
 'SuspensionScalarElements.hpp','DamperModelV2.hpp','InterconnectedSuspension.hpp',
 'ActiveSuspension.hpp','SuspensionCompliance.hpp','SuspensionDamage.hpp',
 'LegacyAndSpecialKinematics.hpp','MotorcycleAlternativeFront.hpp',
 'SuspensionProviderCatalog.hpp','SuspensionClosure.hpp')
foreach ($name in $names) {
  Copy-Item -Force (Join-Path $here "Engine\HeritageEngine\Vehicles\Suspension\$name") (Join-Path $dest $name)
}
Write-Host "Installed SUSP15-SUSP23 core headers into $dest"
Write-Host "Runtime wiring checklist: $here\SUSP15_TO_SUSP23_INTEGRATION.md"
Write-Host "Candidate registry/runtime files containing suspension provider keys:"
$sourceFiles | Select-String -Pattern 'semi_trailing_arm_v1|twist_beam_v1|multi_link_v1|SuspensionProvider|damper|anti.roll' -List | Select-Object -First 40 Path | Format-Table -AutoSize
