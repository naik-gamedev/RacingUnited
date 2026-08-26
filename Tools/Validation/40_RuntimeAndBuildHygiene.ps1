# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

$lifecyclePath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\Lifecycle.lua"
$lifecycle = if (Test-Path $lifecyclePath) { [IO.File]::ReadAllText($lifecyclePath) } else { "" }
Check ($lifecycle.Contains("EnterDefaultPlayerWorld()")) "prototype lifecycle schedules the creator Player World by default"
$commonRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\Common.lua"
$physicsDemoRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\PhysicsDemo.lua"
$commonRuntime = if (Test-Path $commonRuntimePath) { [IO.File]::ReadAllText($commonRuntimePath) } else { "" }
$physicsDemoRuntime = if (Test-Path $physicsDemoRuntimePath) { [IO.File]::ReadAllText($physicsDemoRuntimePath) } else { "" }
Check ($lifecycle.Contains("PhysicsDemoFixedUpdate(fixedDeltaTime)")) "CLEAN08 lifecycle dispatches physics-demo fixed update"
Check (-not $lifecycle.Contains("Physics.Raycast(")) "CLEAN08 lifecycle no longer implements physics-demo queries"
Check ($physicsDemoRuntime.Contains("function PhysicsDemoFixedUpdate")) "CLEAN08 physics demo owns fixed-step implementation"
Check ($physicsDemoRuntime.Contains("function DestroyPhysicsDemo")) "CLEAN08 physics demo owns its destruction"
Check ($physicsDemoRuntime.Contains("function RemoveExistingPhysicsDemo")) "CLEAN08 physics demo owns legacy-demo cleanup"
Check (-not $commonRuntime.Contains("function DestroyPhysicsDemo")) "CLEAN08 Common.lua no longer owns physics-demo destruction"
$scenePresetsPath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\ScenePresets.lua"
$scenePresets = if (Test-Path $scenePresetsPath) { [IO.File]::ReadAllText($scenePresetsPath) } else { "" }
Check ($scenePresets.Contains("playerWorld.loaded")) "UI presets preserve an active creator Player World"

$compiledMain = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\main.cpp"
$obsoleteMain = Join-Path $Root "Engine\HeritageEngine\main.cpp"
Check (Test-Path $compiledMain) "authoritative compiled main.cpp exists"
Check (-not (Test-Path $obsoleteMain)) "obsolete outer main.cpp is absent"

$headerPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\GeneratedBuildIdentity.hpp"
$headerText = if (Test-Path $headerPath) { [IO.File]::ReadAllText($headerPath) } else { "" }
Check ($headerText.Contains("kMilestone")) "generated build identity header is valid"


# CLEAN10B: ZIP overlays may preserve source/header timestamps older than existing
# MSVC object files. Normal /t:Build remains healthy only if changed content is
# promoted to a fresh timestamp before MSBuild evaluates dependencies.
$incrementalFreshnessPath = Join-Path $Root "Tools\EnsureIncrementalBuildFreshness.ps1"
$incrementalFreshness = if (Test-Path $incrementalFreshnessPath) { [IO.File]::ReadAllText($incrementalFreshnessPath) } else { "" }
$rollingBuildHelper = if (Test-Path (Join-Path $Root "Tools\00_BuildAndRunCurrent.cmd")) { [IO.File]::ReadAllText((Join-Path $Root "Tools\00_BuildAndRunCurrent.cmd")) } else { "" }
$launchCurrentPath = Join-Path $Root "Tools\01_LaunchCurrent.cmd"
$launchCurrent = if (Test-Path $launchCurrentPath) { [IO.File]::ReadAllText($launchCurrentPath) } else { "" }
Check (Test-Path $incrementalFreshnessPath) "CLEAN10B content-hash incremental-build freshness guard exists"
Check ($incrementalFreshness.Contains("Get-FileHash") -and $incrementalFreshness.Contains("IncrementalSourceHashes.tsv") -and $incrementalFreshness.Contains("LastWriteTimeUtc")) "CLEAN10B freshness guard hashes build inputs and touches changed content"
Check ($rollingBuildHelper.Contains("EnsureIncrementalBuildFreshness.ps1") -and $rollingBuildHelper.Contains("Incremental-build freshness guard")) "rolling build helper invokes CLEAN10B freshness guard before incremental MSBuild"
Check ($rollingBuildHelper.Contains('set "ENGINE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritageEngine.exe"') -and $rollingBuildHelper.Contains('set "TEST_EXE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritagePhysicsTests.exe"')) "build helper launches solution-level Release outputs rather than stale project-local binaries"
Check ($rollingBuildHelper.Contains('set "SOLUTION=%ROOT%\Engine\HeritageEngine\HeritageEngine.slnx"') -and $rollingBuildHelper.Contains('"%MSBUILD%" "%SOLUTION%"') -and -not $rollingBuildHelper.Contains('"%MSBUILD%" "%ENGINE_PROJECT%"') -and -not $rollingBuildHelper.Contains('"%MSBUILD%" "%TEST_PROJECT%"')) "build helper builds through the solution so MSBuild and launch paths share one output directory"
Check ($launchCurrent.Contains('set "ENGINE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritageEngine.exe"') -and -not $launchCurrent.Contains('HeritageEngine\HeritageEngine\x64')) "launch-only helper uses the current solution-level engine executable"



# CLEAN13: the supported process is the single HeritageEngine executable. The
# historical standalone launcher may remain as reference source but is not an
# active solution project. Project state remains concise; full history is archived.
$activeSolutionPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine.slnx"
$activeSolution = ReadText $activeSolutionPath
Check (-not $activeSolution.Contains("Launcher.vcxproj") -and $activeSolution.Contains("HeritageEngine.vcxproj") -and $activeSolution.Contains("HeritagePhysicsTests.vcxproj")) "CLEAN13 active solution contains engine/tests but not legacy standalone Launcher"
Check (Test-Path (Join-Path $Root "Engine\Launcher\LEGACY_NOT_BUILT.md")) "CLEAN13 retained launcher source is explicitly labelled legacy/non-built"
$projectStatePath = Join-Path $Root "Docs\PROJECT_STATE.md"
$projectStateLines = if (Test-Path $projectStatePath) { (Get-Content $projectStatePath).Count } else { 999999 }
$projectStateHistoryPath = Join-Path $Root "Docs\History\PROJECT_STATE_MILESTONES_THROUGH_CLEAN12.md"
Check ($projectStateLines -lt 300) "CLEAN13 PROJECT_STATE.md remains focused on current state"
Check ((Test-Path $projectStateHistoryPath) -and (Get-Content $projectStateHistoryPath).Count -gt 1000) "CLEAN13 deep milestone narrative is preserved under Docs/History"
$rollingMilestoneMatch = [regex]::Match($rollingBuildHelper, 'set "MILESTONE=([^"]+)"')
$rollingMilestone = if ($rollingMilestoneMatch.Success) { $rollingMilestoneMatch.Groups[1].Value } else { "" }
Check ($rollingMilestoneMatch.Success -and -not [string]::IsNullOrWhiteSpace($rollingMilestone) -and $rollingMilestone -ne "CLEAN13-FINAL-OWNERSHIP-PASS") "rolling build helper declares an active post-CLEAN13 milestone instead of being pinned to the cleanup checkpoint"
