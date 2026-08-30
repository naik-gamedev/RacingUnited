param(
    [string]$Root = "",
    [switch]$VerboseOutput
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $Root = (Resolve-Path $Root).Path
}

$reportRoot = Join-Path $Root "Build\Reports"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
$reportPath = Join-Path $reportRoot "ValidationReport.txt"
$results = New-Object System.Collections.Generic.List[string]
$failed = $false

function Check([bool]$Condition, [string]$Label) {
    if ($Condition) {
        $script:results.Add("PASS: $Label")
    } else {
        $script:results.Add("FAIL: $Label")
        $script:failed = $true
    }
}

function ReadText([string]$Path) {
    if (Test-Path $Path) { return [IO.File]::ReadAllText($Path) }
    return ""
}

function ReadTextSet([System.IO.FileInfo[]]$Files) {
    if ($null -eq $Files -or $Files.Count -eq 0) { return "" }
    return (($Files | Sort-Object FullName | ForEach-Object {
        [IO.File]::ReadAllText($_.FullName)
    }) -join "`n")
}

$ToolsRoot = $PSScriptRoot
$validationModules = @(
    "Validation\00_FoundationAndLuaApi.ps1",
    "Validation\10_CodeArchitecture.ps1",
    "Validation\20_PhysicsAndRegression.ps1",
    "Validation\30_VehicleAndContentArchitecture.ps1",
    "Validation\40_RuntimeAndBuildHygiene.ps1",
    "Validation\50_HeritageStudio.ps1"
)
foreach ($relativeModule in $validationModules) {
    $modulePath = Join-Path $ToolsRoot $relativeModule
    if (-not (Test-Path $modulePath)) {
        Write-Host "FAIL: validation module missing: $relativeModule"
        exit 1
    }
    . $modulePath
}

$summary = @(
    "Heritage Engine static validation",
    "utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffZ'))",
    "root=$Root",
    "result=$(if ($failed) { 'FAIL' } else { 'PASS' })",
    ""
) + $results
[IO.File]::WriteAllLines($reportPath, $summary, [Text.UTF8Encoding]::new($false))

if ($VerboseOutput) {
    foreach ($line in $results) { Write-Host $line }
} else {
    foreach ($line in $results) {
        if ($line.StartsWith("FAIL:")) { Write-Host $line }
    }
    $passCount = @($results | Where-Object { $_.StartsWith("PASS:") }).Count
    $failCount = @($results | Where-Object { $_.StartsWith("FAIL:") }).Count
    if ($failed) {
        Write-Host "Validation: FAIL ($failCount failed, $passCount passed)"
    } else {
        Write-Host "Validation: PASS ($passCount checks)"
    }
}
Write-Host "Validation report: $reportPath"
if ($failed) { exit 1 }
exit 0
