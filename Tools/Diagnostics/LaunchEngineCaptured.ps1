param(
    [Parameter(Mandatory=$true)][string]$Engine,
    [Parameter(Mandatory=$true)][string]$Root,
    [Parameter(Mandatory=$true)][string]$ModulePath,
    [Parameter(Mandatory=$true)][string]$DiagnosticsDirectory
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $DiagnosticsDirectory | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$timestampedLog = Join-Path $DiagnosticsDirectory ("RuntimeConsole_{0}.log" -f $stamp)
$latestLog = Join-Path $DiagnosticsDirectory 'RuntimeConsoleLatest.log'
$crashText = Join-Path $DiagnosticsDirectory 'RuntimeCrashLatest.txt'
$crashDump = Join-Path $DiagnosticsDirectory 'RuntimeCrashLatest.dmp'

Remove-Item -Force -ErrorAction SilentlyContinue $latestLog, $crashText, $crashDump

# Route the native process through cmd.exe so stderr is merged into stdout
# before PowerShell sees it. Tee-Object can therefore display and continuously
# persist every engine line without PowerShell turning native stderr into a
# transient red ErrorRecord. cmd.exe returns the engine process exit code.
$commandLine = ('""{0}" --project-root "{1}" --module-path "{2}" --module "RacingUnited" 2>&1"' -f $Engine, $Root, $ModulePath)
& $env:ComSpec /d /s /c $commandLine | Tee-Object -FilePath $timestampedLog
$exitCode = $LASTEXITCODE

Copy-Item -Force $timestampedLog $latestLog
Write-Host ''
Write-Host ('Runtime console log: {0}' -f $latestLog)
if (Test-Path $crashText) { Write-Host ('Native crash report: {0}' -f $crashText) }
if (Test-Path $crashDump) { Write-Host ('Native minidump:    {0}' -f $crashDump) }
exit $exitCode
