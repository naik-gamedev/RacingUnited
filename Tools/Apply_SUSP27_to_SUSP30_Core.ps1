param([string]$Root = (Get-Location).Path)
$ErrorActionPreference='Stop'
$here=Split-Path -Parent $MyInvocation.MyCommand.Path
$package=Split-Path -Parent $here
$src=Join-Path $package 'Engine\HeritageEngine\Vehicles\Suspension'
$dst=Join-Path $Root 'Engine\HeritageEngine\Vehicles\Suspension'
if(!(Test-Path $dst)){throw "RacingUnited suspension directory not found: $dst"}
# Refuse an obsolete checkout. SUSP13/SUSP14 keys are the minimum accepted baseline.
$all=(Get-ChildItem $Root -Recurse -File -Include *.cpp,*.hpp,*.h,*.lua -ErrorAction SilentlyContinue | Where-Object {$_.FullName -notmatch '\\Build\\|\\.git\\'} | ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"
if(!$all.Contains('semi_trailing_arm_v1') -or !$all.Contains('multi_link_v1')){throw 'SUSP27-30 requires the SUSP13 + SUSP14 baseline.'}
$files=@('SuspensionElementGraphV3.hpp','SuspensionGeometryJacobianV3.hpp','SuspensionProductionRuntimeV3.hpp','SuspensionSerializationV3.hpp','SuspensionCertificationV3.hpp','SuspensionDegradedDynamicsV3.hpp','SuspensionProviderRegistryV3.hpp','SuspensionAuthoringSchemaV3.hpp','SuspensionProductionV3.hpp')
foreach($f in $files){Copy-Item -Force (Join-Path $src $f) (Join-Path $dst $f)}
$testDst=Join-Path $Root 'Tests';New-Item -ItemType Directory -Force -Path $testDst|Out-Null;Copy-Item -Force (Join-Path $package 'Tests\SuspensionFinalCertificationV3.cpp') (Join-Path $testDst 'SuspensionFinalCertificationV3.cpp')
Write-Host 'Installed SUSP27-SUSP30 physical suspension core.'
Write-Host 'Running strict live-integration audit; FAIL means the milestone is NOT complete.'
& (Join-Path $here 'Validate_SUSP27_30_LiveIntegration.ps1') -Root $Root
exit $LASTEXITCODE
