param([string]$Root = '')
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = [IO.Path]::GetFullPath($Root).TrimEnd('\','/')

$outZip = Join-Path $Root 'RacingUnited_SUSP31_LiveSourceBundle.zip'
$temp = Join-Path $env:TEMP ('RacingUnited_SUSP31_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $temp | Out-Null

function Copy-Rel([string]$Rel) {
    $src = Join-Path $Root $Rel
    if (Test-Path $src -PathType Leaf) {
        $dst = Join-Path $temp $Rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }
}
function Copy-Tree([string]$Rel) {
    $src = Join-Path $Root $Rel
    if (Test-Path $src -PathType Container) {
        Get-ChildItem $src -Recurse -File | ForEach-Object {
            $relFile = $_.FullName.Substring($Root.Length).TrimStart('\')
            Copy-Rel $relFile
        }
    }
}

# Entire suspension domain: this is the authoritative migration surface and is
# normally small enough to upload even when the whole repository ZIP is problematic.
Copy-Tree 'Engine\HeritageEngine\Vehicles\Suspension'

# Live vehicle ownership / geometry.
@(
 'Engine\HeritageEngine\Vehicles\VehicleSystem.cpp',
 'Engine\HeritageEngine\Vehicles\VehicleSystem.hpp',
 'Engine\HeritageEngine\Vehicles\VehicleWheelSimulation.cpp',
 'Engine\HeritageEngine\Vehicles\SuspensionGeometry.cpp',
 'Engine\HeritageEngine\Vehicles\SuspensionGeometry.hpp',
 'Engine\HeritageEngine\Vehicles\SuspensionModel.cpp',
 'Engine\HeritageEngine\Vehicles\SuspensionModel.hpp',
 'Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.cpp',
 'Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.hpp',
 'Engine\HeritageEngine\Vehicles\VehicleDefinitionLoader.cpp',
 'Engine\HeritageEngine\Vehicles\VehicleDefinitionLoader.hpp'
) | ForEach-Object { Copy-Rel $_ }

# Lua bindings that the audit identified.
Copy-Rel 'Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleSuspensionBindings.cpp'
Copy-Rel 'Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleSuspensionBindings.hpp'

# Project/build metadata so SUSP30 certification can be wired into the real build.
Get-ChildItem (Join-Path $Root 'Engine\HeritageEngine') -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object {$_.Extension -in @('.vcxproj','.filters','.props','.targets') -or $_.Name -eq 'CMakeLists.txt'} |
  ForEach-Object {$rel=$_.FullName.Substring($Root.Length).TrimStart('\'); Copy-Rel $rel}
Copy-Rel 'CMakeLists.txt'

# Find production files that mention suspension/save/replay/network or Studio authoring.
$scanRoots = @(
  (Join-Path $Root 'Engine\HeritageEngine'),
  (Join-Path $Root 'Modules\RacingUnited')
) | Where-Object {Test-Path $_}

$patterns = 'Suspension|suspension|Replay|replay|SaveState|serialize|deserialize|rollback|network|HeritageStudio|Studio'
foreach($scan in $scanRoots) {
    Get-ChildItem $scan -Recurse -File -ErrorAction SilentlyContinue |
      Where-Object {
        $_.Extension -in @('.cpp','.cc','.cxx','.hpp','.h','.inl','.lua','.ps1','.cmd','.cmake') -and
        $_.FullName -notmatch '\\Build\\|\\\.git\\|\\ThirdParty\\'
      } |
      ForEach-Object {
        try {
            if(Select-String -LiteralPath $_.FullName -Pattern $patterns -Quiet) {
                $rel=$_.FullName.Substring($Root.Length).TrimStart('\')
                Copy-Rel $rel
            }
        } catch {}
      }
}

# Include existing reports proving the live state.
Copy-Rel 'Build\Reports\SUSP27_30_LiveIntegration.txt'
Copy-Rel 'Build\Reports\ValidationReport.txt'

$manifest = Join-Path $temp 'SUSP31_SOURCE_BUNDLE_MANIFEST.txt'
$files = Get-ChildItem $temp -Recurse -File | Sort-Object FullName
@(
  'Racing United SUSP31 targeted live-source bundle'
  "Root: $Root"
  "UTC: $([DateTime]::UtcNow.ToString('o'))"
  "Files: $($files.Count)"
  ''
  ($files | ForEach-Object {$_.FullName.Substring($temp.Length).TrimStart('\')})
) | Set-Content $manifest -Encoding UTF8

if(Test-Path $outZip){Remove-Item $outZip -Force}
Compress-Archive -Path (Join-Path $temp '*') -DestinationPath $outZip -CompressionLevel Optimal
Remove-Item $temp -Recurse -Force

Write-Host ''
Write-Host 'SUSP31 source capture complete.'
Write-Host "Upload this file to ChatGPT:"
Write-Host "  $outZip"
Write-Host ''
