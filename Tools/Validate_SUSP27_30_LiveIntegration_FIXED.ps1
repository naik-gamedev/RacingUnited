param([string]$Root = '')
$ErrorActionPreference = 'Stop'

# Robust repository-root discovery. The validator lives in <repo>\Tools,
# so callers do not need to pass a quoted path at all.
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent $PSScriptRoot
} else {
    $Root = $Root.Trim('"')
    $Root = [System.IO.Path]::GetFullPath($Root)
}
$Root = $Root.TrimEnd('\','/')

$reportDir = Join-Path -Path $Root -ChildPath 'Build\Reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$report = Join-Path $reportDir 'SUSP27_30_LiveIntegration.txt'

# Write immediately so the user always gets a report, even if scanning later fails.
@(
  'SUSP27-SUSP30 LIVE INTEGRATION AUDIT v2'
  "Root: $Root"
  "UTC: $([DateTime]::UtcNow.ToString('o'))"
  'STATUS: RUNNING'
  ''
) | Set-Content -Path $report -Encoding UTF8

$results = New-Object System.Collections.Generic.List[object]
function Add-Result([string]$Name,[string]$Status,[string]$Detail,[string[]]$Evidence=@()) {
    $results.Add([PSCustomObject]@{Name=$Name;Status=$Status;Detail=$Detail;Evidence=$Evidence})
}

$coreFiles = @(
 'SuspensionElementGraphV3.hpp',
 'SuspensionGeometryJacobianV3.hpp',
 'SuspensionProductionRuntimeV3.hpp',
 'SuspensionSerializationV3.hpp',
 'SuspensionCertificationV3.hpp',
 'SuspensionDegradedDynamicsV3.hpp',
 'SuspensionProviderRegistryV3.hpp',
 'SuspensionAuthoringSchemaV3.hpp',
 'SuspensionProductionV3.hpp',
 'SuspensionFinalCertificationV3.cpp'
)

function Is-IgnoredPath([string]$Path) {
    return ($Path -match '\\Build\\|\\\.git\\|\\Docs\\|\\Tools\\|\\Tests\\|\\ThirdParty\\|\\External\\')
}

function Get-ProductionSourceFiles {
    Get-ChildItem -Path $Root -Recurse -File -ErrorAction SilentlyContinue |
      Where-Object {
        $_.Extension -in @('.cpp','.cc','.cxx','.hpp','.h') -and
        -not (Is-IgnoredPath $_.FullName) -and
        ($coreFiles -notcontains $_.Name)
      }
}

function Find-InFiles([System.IO.FileInfo[]]$Files,[string]$Pattern,[int]$Limit=8) {
    $hits = New-Object System.Collections.Generic.List[string]
    foreach($f in $Files) {
        try {
            $m = Select-String -Path $f.FullName -Pattern $Pattern -AllMatches -ErrorAction Stop
            foreach($x in $m) {
                $rel = $f.FullName.Substring($Root.Length).TrimStart('\')
                $hits.Add("$rel`:$($x.LineNumber): $($x.Line.Trim())")
                if($hits.Count -ge $Limit){ return $hits.ToArray() }
            }
        } catch {}
    }
    return $hits.ToArray()
}

function Critical-Call([string]$Name,[string]$Pattern,[string]$Detail,[System.IO.FileInfo[]]$Files) {
    $h = Find-InFiles $Files $Pattern
    Add-Result $Name ($(if($h.Count){'PASS'}else{'FAIL'})) $Detail $h
}

try {
    $prod = @(Get-ProductionSourceFiles)

    if($prod.Count -eq 0) {
        Add-Result 'production source discovery' 'FAIL' 'No production C/C++ source files were found outside the suspension package/tests.'
    } else {
        Add-Result 'production source discovery' 'PASS' "$($prod.Count) production C/C++ files scanned."
    }

    Critical-Call 'V3 whole-vehicle coordinator call' '\bstepSuspensionVehicleGraphV3\s*\(' 'A production call site must step the V3 suspension graph.' $prod
    Critical-Call 'degraded-topology decision call' '\bsuspensionRequiresDegradedDynamicsV3\s*\(' 'Production must detect broken/under-constrained topology.' $prod
    Critical-Call 'degraded multibody step call' '\bstepSuspensionDegradedDynamicsV3\s*\(' 'Production must step damaged under-constrained suspension dynamically.' $prod
    Critical-Call 'V3 provider registry live use' '\bSuspensionProviderRegistryV3\b' 'A production owner must instantiate/use the V3 provider registry.' $prod
    Critical-Call 'provider registration call' '\bregisterProvider\s*\(' 'Canonical suspension providers must actually be registered.' $prod
    Critical-Call 'provider completeness gate' '\bcompleteForProduction\s*\(' 'Runtime/startup must reject an incomplete provider registry.' $prod
    Critical-Call 'constraint reaction load consumption' '\bconstraintLoads\b|\bSuspensionConstraintLoadV3\b' 'Physical component loads must leave the kinematic solver and feed damage/compliance.' $prod
    Critical-Call 'component override feedback' '\bconstraintOverridesConsumed\b' 'Providers must confirm that per-component bend/compliance/failure overrides were consumed.' $prod
    Critical-Call 'analytic frame derivative consumption' '\bhasWheelDerivatives\b|\bdPositionDWheel\b' 'Production adapters must consume analytic geometry derivatives/Jacobians.' $prod
    Critical-Call 'V3 serialization write consumer' '\bserializeSuspensionRuntimeV3\s*\(' 'Save/replay/network code must serialize V3 suspension state.' $prod
    Critical-Call 'V3 serialization read consumer' '\bdeserializeSuspensionRuntimeV3\s*\(' 'Save/replay/network code must restore V3 suspension state.' $prod

    # Every canonical provider token must appear in live registration/adaptation code outside the package definitions.
    $providers = @(
      'MacPhersonStrut','DoubleWishbone','PushrodRockerWishbone','RigidLiveAxle',
      'LeafSpringLiveAxle','MotorcycleForkSwingarm','SemiTrailingArm','TwistBeam',
      'MultiLink','SwingAxle','SlidingPillar','MotorcycleLinkFront'
    )
    foreach($p in $providers) {
        $h = Find-InFiles $prod ("\b" + [regex]::Escape($p) + "\b") 4
        Add-Result "provider live adapter: $p" ($(if($h.Count){'PASS'}else{'FAIL'})) 'Canonical provider must be referenced by live production adapter/registration code.' $h
    }

    # Aliases should be installed in live registry setup.
    $aliasHits = Find-InFiles $prod '\bregisterStandardSuspensionAliasesV3\s*\('
    Add-Result 'standard suspension aliases registered' ($(if($aliasHits.Count){'PASS'}else{'FAIL'})) 'Chapman, pure trailing arm and De Dion aliases must be installed.' $aliasHits

    # Detect old V2 force authority still being called outside the V3 package.
    $legacyCorner = Find-InFiles $prod '\bstepSuspensionCornerV2\s*\('
    $legacyAxle = Find-InFiles $prod '\bstepSuspensionAxleCouplingV2\s*\('
    $legacy = @($legacyCorner + $legacyAxle)
    Add-Result 'legacy V2 force authority disabled' ($(if($legacy.Count){'FAIL'}else{'PASS'})) 'No production call sites may continue stepping the retired V2 corner/axle force authority in parallel.' $legacy

    # Lua authoring/runtime exposure.
    $luaFiles = @(Get-ChildItem -Path $Root -Recurse -File -Filter *.lua -ErrorAction SilentlyContinue |
      Where-Object { -not (Is-IgnoredPath $_.FullName) })
    $luaId = Find-InFiles $luaFiles '\belement_id\b'
    $luaKind = Find-InFiles $luaFiles '\belement_kind\b'
    $luaFramesA = Find-InFiles $luaFiles '\bframe_a\b'
    $luaFramesB = Find-InFiles $luaFiles '\bframe_b\b'
    $luaPass = $luaId.Count -and $luaKind.Count -and $luaFramesA.Count -and $luaFramesB.Count
    Add-Result 'Lua physical-element schema' ($(if($luaPass){'PASS'}else{'FAIL'})) 'Lua vehicle definitions must expose element_id, element_kind, frame_a and frame_b.' @($luaId + $luaKind + $luaFramesA + $luaFramesB)

    # Heritage Studio must reference the schema in actual Studio/authoring files.
    $studioFiles = @($prod | Where-Object {$_.FullName -match 'HeritageStudio|\\Studio\\|Authoring'})
    $studioHits = Find-InFiles $studioFiles 'SuspensionAuthoringSchemaV3|SuspensionAuthoringSchemaVersionV3|element_id|reference_length_m'
    Add-Result 'Heritage Studio suspension graph authoring' ($(if($studioHits.Count){'PASS'}else{'FAIL'})) 'Studio/authoring code must consume the V3 suspension authoring schema.' $studioHits

    # Certification must be in an actual build/test invocation, not merely copied into Tests/.
    $buildFiles = @(Get-ChildItem -Path $Root -Recurse -File -ErrorAction SilentlyContinue |
      Where-Object {
        -not (Is-IgnoredPath $_.FullName) -and
        ($_.Name -eq 'CMakeLists.txt' -or $_.Extension -in @('.cmake','.vcxproj','.vcxproj.filters','.cmd','.bat','.ps1','.py'))
      })
    $certBuildHits = Find-InFiles $buildFiles 'SuspensionFinalCertificationV3\.cpp|SUSP27_SUSP30_FINAL_CERTIFICATION'
    Add-Result 'final certification wired into build/test flow' ($(if($certBuildHits.Count){'PASS'}else{'FAIL'})) 'The final certification must be invoked by the repository build/test system, not just exist as a source file.' $certBuildHits

    # Optional explicit completion marker is only accepted when the rest passes.
    $markerHits = Find-InFiles $prod '\bSUSP30_SUSPENSION_DOMAIN_COMPLETE\b'
    Add-Result 'completion marker present' ($(if($markerHits.Count){'PASS'}else{'WARN'})) 'Marker is informational; it is not accepted unless every critical gate passes.' $markerHits

} catch {
    Add-Result 'audit execution' 'FAIL' ("Validator exception: " + $_.Exception.Message)
}

$failCount = @($results | Where-Object {$_.Status -eq 'FAIL'}).Count
$warnCount = @($results | Where-Object {$_.Status -eq 'WARN'}).Count

$out = New-Object System.Collections.Generic.List[string]
$out.Add('SUSP27-SUSP30 LIVE INTEGRATION AUDIT v2')
$out.Add("Root: $Root")
$out.Add("UTC: $([DateTime]::UtcNow.ToString('o'))")
$out.Add('')
foreach($r in $results) {
    $out.Add(('{0,-52} {1,-4}  {2}' -f $r.Name,$r.Status,$r.Detail))
    foreach($e in $r.Evidence){ $out.Add("    $e") }
}
$out.Add('')
if($failCount -eq 0) {
    $out.Add("RESULT: PASS -- 0 critical failures, $warnCount warning(s). SUSPENSION_DOMAIN_COMPLETE may be declared.")
    $exitCode = 0
} else {
    $out.Add("RESULT: FAIL -- $failCount critical live-integration gate(s) remain; $warnCount warning(s).")
    $out.Add('Do NOT declare suspension complete until every FAIL above has a real production call site.')
    $exitCode = 2
}
$out | Set-Content -Path $report -Encoding UTF8
Get-Content $report
Write-Host ''
Write-Host "Report written to: $report"
exit $exitCode
