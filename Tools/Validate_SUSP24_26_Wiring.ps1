param([string]$RepoRoot = (Get-Location).Path)
$ErrorActionPreference='Stop'
$root=(Resolve-Path $RepoRoot).Path
$reportDir=Join-Path $root 'Build\Reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$report=Join-Path $reportDir 'SUSP24_26_WiringAudit.txt'
$lines=New-Object System.Collections.Generic.List[string]
function Add([string]$s){$lines.Add($s);Write-Host $s}
function Search([string]$pattern,[string[]]$paths){
  $hits=@()
  foreach($p in $paths){if(Test-Path $p){$hits += Get-ChildItem $p -Recurse -File -Include *.cpp,*.hpp,*.h,*.lua,*.ps1,*.md,*.txt -ErrorAction SilentlyContinue | Select-String -Pattern $pattern -SimpleMatch -List -ErrorAction SilentlyContinue}}
  return @($hits)
}
$engine=Join-Path $root 'Engine\HeritageEngine'
$module=Join-Path $root 'Modules\RacingUnited'
$studioCandidates=@((Join-Path $root 'HeritageStudio'),(Join-Path $root 'Tools'),(Join-Path $root 'Engine'))
$vehicleFiles=Get-ChildItem $engine -Recurse -File -Include VehicleSystem.cpp,VehicleSystem.hpp,*Vehicle*Suspension*.cpp,*Vehicle*Suspension*.hpp -ErrorAction SilentlyContinue
Add '============================================================'
Add 'SUSP24-SUSP26 STRICT PRODUCTION WIRING AUDIT'
Add "Root: $root"
Add '============================================================'
$coreNames=@('SuspensionProduction.hpp','SuspensionProductionRuntime.hpp','SuspensionProviderRegistryV2.hpp','SuspensionComplianceDynamic.hpp','DamperModelV3.hpp','PneumaticHydraulicSpringV2.hpp','SuspensionDamageV2.hpp','MotorcycleFront3D.hpp','SuspensionSerializationV2.hpp','SuspensionRuntimeValidation.hpp')
$coreOK=$true
foreach($n in $coreNames){$p=Join-Path $root "Engine\HeritageEngine\Vehicles\Suspension\$n";if(Test-Path $p){Add "PASS core: $n"}else{Add "FAIL core missing: $n";$coreOK=$false}}

$runtimeHits=$vehicleFiles | Select-String -Pattern 'stepVehicleSuspensionV2' -SimpleMatch -List -ErrorAction SilentlyContinue
$feedbackHits=$vehicleFiles | Select-String -Pattern 'suspensionMountOffsetForNextKinematicsV2' -SimpleMatch -List -ErrorAction SilentlyContinue
$registryHits=$vehicleFiles | Select-String -Pattern 'SuspensionProviderRegistryV2' -SimpleMatch -List -ErrorAction SilentlyContinue
$serialHits=Search 'serializeSuspensionRuntimeV2' @($engine,$module)
$legacyGuardHits=$vehicleFiles | Select-String -Pattern 'legacyScalarSpringDamperDisabled' -SimpleMatch -List -ErrorAction SilentlyContinue

if($runtimeHits){Add 'PASS runtime: VehicleSystem calls stepVehicleSuspensionV2';$runtimeHits|ForEach-Object{Add "  $($_.Path):$($_.LineNumber)"}}else{Add 'FAIL runtime: no VehicleSystem call to stepVehicleSuspensionV2'}
if($feedbackHits){Add 'PASS feedback: compliance/damage mount offset reaches VehicleSystem kinematics';$feedbackHits|ForEach-Object{Add "  $($_.Path):$($_.LineNumber)"}}else{Add 'FAIL feedback: no VehicleSystem use of suspensionMountOffsetForNextKinematicsV2'}
if($registryHits){Add 'PASS providers: VehicleSystem owns/uses SuspensionProviderRegistryV2';$registryHits|ForEach-Object{Add "  $($_.Path):$($_.LineNumber)"}}else{Add 'FAIL providers: no VehicleSystem use of SuspensionProviderRegistryV2'}
if($serialHits){Add 'PASS state: suspension serialization has a live consumer';$serialHits|ForEach-Object{Add "  $($_.Path):$($_.LineNumber)"}}else{Add 'FAIL state: serializeSuspensionRuntimeV2 has no live consumer'}

$luaProvider=Search 'multi_link_v1' @($module)
$luaHardware=Search 'HydroPneumatic' @($module)
if($luaProvider -and $luaHardware){Add 'PASS Lua: provider and advanced hardware authoring markers found'}else{Add 'FAIL Lua: provider/advanced suspension hardware authoring not both visible in RacingUnited Lua'}

$studioProvider=Search 'multi_link_v1' $studioCandidates
$studioDamper=Search 'damper' $studioCandidates
if($studioProvider -and $studioDamper){Add 'PASS Studio: suspension provider + damper authoring markers found'}else{Add 'FAIL Studio: production suspension authoring markers incomplete'}

$physicsTests=Get-ChildItem $engine -Recurse -File -Include *.cpp,*.hpp -ErrorAction SilentlyContinue | Where-Object {$_.FullName -match 'Test|Regression'}
$testHits=$physicsTests | Select-String -Pattern 'SUSP24_SUSP26' -SimpleMatch -List -ErrorAction SilentlyContinue
if($testHits){Add 'PASS regressions: SUSP24_SUSP26 marker is in native project regression sources'}else{Add 'FAIL regressions: portable certification is not yet represented in native project tests'}

$productionOK=$coreOK -and [bool]$runtimeHits -and [bool]$feedbackHits -and [bool]$registryHits -and [bool]$serialHits -and [bool]$luaProvider -and [bool]$luaHardware -and [bool]$studioProvider -and [bool]$studioDamper -and [bool]$testHits
Add '------------------------------------------------------------'
if($productionOK){Add 'SUSP24-SUSP26 LIVE WIRING: PASS';Add 'Suspension may proceed to canonical build/runtime certification.'}else{Add 'SUSP24-SUSP26 LIVE WIRING: FAIL';Add 'Core can compile, but suspension is NOT allowed to be labeled production-complete yet.'}
$lines | Set-Content -Encoding UTF8 $report
Write-Host "Report: $report"
if(!$productionOK){exit 24}
exit 0
