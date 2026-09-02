param([string]$RepoRoot = (Get-Location).Path)
$ErrorActionPreference='Stop'
$packageRoot=Split-Path -Parent $PSScriptRoot
$root=(Resolve-Path $RepoRoot).Path
$dest=Join-Path $root 'Engine\HeritageEngine\Vehicles\Suspension'
if(!(Test-Path $dest)){throw "Suspension directory not found: $dest"}
# Require the previous milestones by actual files, not marker strings that this package itself contains.
foreach($required in @('MultiLinkKinematics.hpp','SuspensionClosure.hpp','LegacyAndSpecialKinematics.hpp')){
  if(!(Test-Path (Join-Path $dest $required))){throw "Refusing SUSP24-26 install: prior SUSP14/SUSP15-23 file missing: $required"}
}
$src=Join-Path $packageRoot 'Engine\HeritageEngine\Vehicles\Suspension'
Get-ChildItem $src -File -Filter *.hpp | ForEach-Object {
  $target=Join-Path $dest $_.Name
  $same=$false
  if(Test-Path $target){try{$same=((Resolve-Path $_.FullName).Path -eq (Resolve-Path $target).Path)}catch{}}
  if(!$same){Copy-Item -Force $_.FullName $target}
  Write-Host "Installed $($_.Name)"
}
$docDest=Join-Path $root 'Docs\Vehicles\Suspension'
New-Item -ItemType Directory -Force -Path $docDest | Out-Null
Copy-Item -Force (Join-Path $packageRoot 'Docs\SUSP24_SUSP26_PRODUCTION_INTEGRATION.md') (Join-Path $docDest 'SUSP24_SUSP26_PRODUCTION_INTEGRATION.md')
Copy-Item -Force (Join-Path $packageRoot 'Docs\SUSPENSION_FINAL_AUDIT.md') (Join-Path $docDest 'SUSPENSION_FINAL_AUDIT.md')
$toolDest=Join-Path $root 'Tools\Validate_SUSP24_26_Wiring.ps1'
if((Resolve-Path $PSScriptRoot).Path -ne (Resolve-Path (Split-Path $toolDest -Parent) -ErrorAction SilentlyContinue).Path){Copy-Item -Force (Join-Path $PSScriptRoot 'Validate_SUSP24_26_Wiring.ps1') $toolDest}
Write-Host ''
Write-Host 'SUSP24-SUSP26 physical/runtime core installed.'
Write-Host 'Running STRICT live-wiring audit. A non-zero result is intentional if VehicleSystem/Lua/Studio have not yet been wired.'
& (Join-Path $PSScriptRoot 'Validate_SUSP24_26_Wiring.ps1') -RepoRoot $root
exit $LASTEXITCODE
