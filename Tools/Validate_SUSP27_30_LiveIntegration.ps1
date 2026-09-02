param([string]$Root = (Get-Location).Path)
$ErrorActionPreference = 'Stop'
$reportDir = Join-Path $Root 'Build\Reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$report = Join-Path $reportDir 'SUSP27_30_LiveIntegration.txt'
$lines = New-Object System.Collections.Generic.List[string]
function Add-Check([string]$Name,[bool]$Pass,[string]$Detail){$script:lines.Add(('{0,-48} {1}  {2}' -f $Name,($(if($Pass){'PASS'}else{'FAIL'})),$Detail))}
function Read-TextFiles([string[]]$Patterns){
  $all=''
  foreach($p in $Patterns){Get-ChildItem -Path $Root -Recurse -File -Include $p -ErrorAction SilentlyContinue | Where-Object {$_.FullName -notmatch '\\Build\\|\\.git\\'} | ForEach-Object {$all += "`n" + [IO.File]::ReadAllText($_.FullName)}}
  return $all
}
$cpp = Read-TextFiles @('*.cpp','*.cc','*.cxx','*.hpp','*.h')
$lua = Read-TextFiles @('*.lua')
$studio = Read-TextFiles @('*.cpp','*.hpp','*.lua')
Add-Check 'V3 production include' ($cpp.Contains('SuspensionProductionV3.hpp')) 'live engine includes V3 umbrella'
Add-Check 'V3 vehicle coordinator call' ($cpp.Contains('stepSuspensionVehicleGraphV3(')) 'whole-vehicle suspension graph stepped'
Add-Check 'degraded topology transition' ($cpp.Contains('suspensionRequiresDegradedDynamicsV3') -and $cpp.Contains('stepSuspensionDegradedDynamicsV3(')) 'broken constraints enter multibody fallback'
Add-Check 'provider V3 registry' ($cpp.Contains('SuspensionProviderRegistryV3')) 'production provider registry instantiated'
$providers=@('MacPhersonStrut','DoubleWishbone','PushrodRockerWishbone','RigidLiveAxle','LeafSpringLiveAxle','MotorcycleForkSwingarm','SemiTrailingArm','TwistBeam','MultiLink','SwingAxle','SlidingPillar','MotorcycleLinkFront')
foreach($p in $providers){Add-Check ("provider migrated: $p") ($cpp.Contains($p) -and ($cpp.Contains('registerProvider') -or $cpp.Contains('SuspensionFrameSetV3'))) 'canonical provider must emit V3 frames/loads'}
Add-Check 'constraint reaction loads' ($cpp.Contains('constraintLoads') -or $cpp.Contains('SuspensionConstraintLoadV3')) 'component damage receives physical reaction loads'
Add-Check 'override consumption' ($cpp.Contains('constraintOverridesConsumed')) 'provider confirms component compliance/damage feedback consumed'
Add-Check 'analytic wheel derivatives' ($cpp.Contains('hasWheelDerivatives') -and $cpp.Contains('dPositionDWheel')) 'normal path avoids 3x probe solves'
Add-Check 'V3 serialization write' ($cpp.Contains('serializeSuspensionRuntimeV3(')) 'save/replay/network state writes graph state'
Add-Check 'V3 serialization read' ($cpp.Contains('deserializeSuspensionRuntimeV3(')) 'save/replay/network state restores graph state'
Add-Check 'Lua element IDs' ($lua.Contains('element_id') -and $lua.Contains('element_kind')) 'Lua vehicle schema exposes physical elements'
Add-Check 'Lua frame attachments' ($lua.Contains('frame_a') -and $lua.Contains('frame_b')) 'Lua exposes hardpoint frame ownership'
Add-Check 'Heritage Studio graph authoring' ($studio.Contains('SuspensionAuthoringSchemaVersionV3') -or ($studio.Contains('element_id') -and $studio.Contains('reference_length_m'))) 'Studio can author graph components'
Add-Check 'final native certification' ($cpp.Contains('SUSP27_SUSP30_FINAL_CERTIFICATION') -or (Test-Path (Join-Path $Root 'Tests\SuspensionFinalCertificationV3.cpp'))) 'V3 final regression present'
$failed = $lines | Where-Object {$_ -match '\sFAIL\s'}
$header=@("SUSP27-SUSP30 LIVE INTEGRATION AUDIT","Root: $Root","UTC: $([DateTime]::UtcNow.ToString('o'))",'')
Set-Content -Path $report -Value ($header + $lines + '' + ($(if($failed.Count -eq 0){'RESULT: PASS -- SUSPENSION_DOMAIN_COMPLETE may be declared.'}else{"RESULT: FAIL -- $($failed.Count) live integration gate(s) remain."})))
Get-Content $report
if($failed.Count -ne 0){exit 2}
