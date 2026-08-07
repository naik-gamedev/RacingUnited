$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$LuaDirectory = Join-Path $ProjectRoot "ThirdParty\Lua\bin"
$LuaDll = Join-Path $LuaDirectory "lua54.dll"
$LuaUrl = "https://github.com/dyne/luabinaries/releases/latest/download/lua54.dll"

Write-Host "Heritage Engine - Lua 5.4 setup" -ForegroundColor Cyan
Write-Host "Project root: $ProjectRoot"

New-Item -ItemType Directory -Force -Path $LuaDirectory | Out-Null

$TemporaryFile = "$LuaDll.download"
if (Test-Path $TemporaryFile) {
    Remove-Item -Force $TemporaryFile
}

Write-Host "Downloading the Windows x64 Lua 5.4 shared library..."
Invoke-WebRequest -UseBasicParsing -Uri $LuaUrl -OutFile $TemporaryFile

$DownloadedFile = Get-Item $TemporaryFile
if ($DownloadedFile.Length -lt 100000) {
    Remove-Item -Force $TemporaryFile
    throw "The downloaded lua54.dll is unexpectedly small ($($DownloadedFile.Length) bytes)."
}

Move-Item -Force $TemporaryFile $LuaDll
$Hash = (Get-FileHash -Algorithm SHA256 $LuaDll).Hash

# Make already-built development copies immediately usable. Future builds also
# copy the DLL through the HeritageEngine.vcxproj CopyLuaRuntime target.
Get-ChildItem -Path (Join-Path $ProjectRoot "Engine") -Filter "HeritageEngine.exe" -Recurse -ErrorAction SilentlyContinue |
    ForEach-Object {
        Copy-Item -Force $LuaDll (Join-Path $_.DirectoryName "lua54.dll")
    }

Write-Host ""
Write-Host "Lua runtime installed:" -ForegroundColor Green
Write-Host "  $LuaDll"
Write-Host "SHA256: $Hash"
Write-Host ""
Write-Host "Now rebuild HeritageEngine, then launch the Racing United module."
