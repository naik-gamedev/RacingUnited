param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

$ErrorActionPreference = 'Stop'
$rootPath = [System.IO.Path]::GetFullPath($Root)
$engineRoot = Join-Path $rootPath 'Engine\HeritageEngine'
$moduleScriptsRoot = Join-Path $rootPath 'Modules\RacingUnited\Scripts'
$reportPath = Join-Path $rootPath 'Build\Reports\CodeHealthSnapshot.txt'

function Relative-ToRoot([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    $prefix = $rootPath.TrimEnd('\') + '\'
    if ($full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $full.Substring($prefix.Length).Replace('\', '/')
    }
    return $full.Replace('\', '/')
}

function Count-Lines([string]$Path) {
    $count = 0
    $reader = [System.IO.File]::OpenText($Path)
    try {
        while ($null -ne $reader.ReadLine()) { $count++ }
    }
    finally { $reader.Dispose() }
    return $count
}

function Read-CompiledSources([string]$ProjectPath) {
    $result = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    if (-not (Test-Path $ProjectPath)) { return $result }
    [xml]$xml = Get-Content -LiteralPath $ProjectPath -Raw
    $projectDir = Split-Path -Parent $ProjectPath
    foreach ($node in $xml.Project.ItemGroup.ClCompile) {
        if ($null -eq $node.Include) { continue }
        $include = [string]$node.Include
        if ($include.StartsWith('$(ProjectDir)', [System.StringComparison]::OrdinalIgnoreCase)) {
            $include = $include.Substring('$(ProjectDir)'.Length)
        }
        $candidate = [System.IO.Path]::GetFullPath((Join-Path $projectDir $include))
        [void]$result.Add($candidate)
    }
    return $result
}

$cppExtensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl')
$cppFiles = Get-ChildItem -LiteralPath $engineRoot -Recurse -File | Where-Object {
    $cppExtensions -contains $_.Extension.ToLowerInvariant()
}
$luaFiles = Get-ChildItem -LiteralPath $moduleScriptsRoot -Recurse -File -Filter '*.lua'

$cppStats = foreach ($file in $cppFiles) {
    [pscustomobject]@{ Path = Relative-ToRoot $file.FullName; Lines = Count-Lines $file.FullName }
}
$luaStats = foreach ($file in $luaFiles) {
    [pscustomobject]@{ Path = Relative-ToRoot $file.FullName; Lines = Count-Lines $file.FullName }
}

$engineProject = Join-Path $engineRoot 'HeritageEngine\HeritageEngine.vcxproj'
$testsProject = Join-Path $engineRoot 'Tests\HeritagePhysicsTests.vcxproj'
$compiled = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
foreach ($set in @((Read-CompiledSources $engineProject), (Read-CompiledSources $testsProject))) {
    foreach ($item in $set) { [void]$compiled.Add($item) }
}
$allCpp = Get-ChildItem -LiteralPath $engineRoot -Recurse -File -Filter '*.cpp'
$uncompiledCpp = foreach ($file in $allCpp) {
    $full = [System.IO.Path]::GetFullPath($file.FullName)
    if (-not $compiled.Contains($full)) { Relative-ToRoot $full }
}

# Lua reachability is structural, not execution-based. Heritage's module uses
# literal include coordinators; recursively follow any top-level helper call
# whose sole argument is a quoted .lua path.
$reachableLua = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$queue = New-Object 'System.Collections.Generic.Queue[string]'
$mainLua = [System.IO.Path]::GetFullPath((Join-Path $moduleScriptsRoot 'Main.lua'))
$queue.Enqueue($mainLua)
$includePattern = '(?m)^\s*[A-Za-z_][A-Za-z0-9_]*\s*\(\s*["'']([^"'']+\.lua)["'']\s*\)'
while ($queue.Count -gt 0) {
    $current = $queue.Dequeue()
    if (-not (Test-Path -LiteralPath $current)) { continue }
    if (-not $reachableLua.Add($current)) { continue }
    $content = Get-Content -LiteralPath $current -Raw
    foreach ($match in [regex]::Matches($content, $includePattern)) {
        $relative = $match.Groups[1].Value.Replace('/', '\')
        $next = [System.IO.Path]::GetFullPath((Join-Path $moduleScriptsRoot $relative))
        if ((Test-Path -LiteralPath $next) -and -not $reachableLua.Contains($next)) {
            $queue.Enqueue($next)
        }
    }
}
$unreachableLua = foreach ($file in $luaFiles) {
    $full = [System.IO.Path]::GetFullPath($file.FullName)
    if (-not $reachableLua.Contains($full)) { Relative-ToRoot $full }
}

$topCpp = $cppStats | Sort-Object Lines -Descending | Select-Object -First 30
$topLua = $luaStats | Sort-Object Lines -Descending | Select-Object -First 20
$largeCpp = @($cppStats | Where-Object { $_.Lines -ge 1000 })
$largeLua = @($luaStats | Where-Object { $_.Lines -ge 500 })


# OPT06: final optimization-freeze inventory. This is deliberately structural:
# it records the hot-path anti-patterns retired during OPT03-OPT05 without
# introducing runtime probes or synchronization that could perturb timings.
function Count-Substring([string]$Text, [string]$Needle) {
    if ([string]::IsNullOrEmpty($Text) -or [string]::IsNullOrEmpty($Needle)) { return 0 }
    $count = 0
    $offset = 0
    while (($index = $Text.IndexOf($Needle, $offset, [System.StringComparison]::Ordinal)) -ge 0) {
        $count++
        $offset = $index + $Needle.Length
    }
    return $count
}

$entityRendererText = Get-Content -LiteralPath (Join-Path $engineRoot 'Graphics\Renderer\EntityMeshRenderer.cpp') -Raw
$weatherRendererText = Get-Content -LiteralPath (Join-Path $engineRoot 'Graphics\Renderer\WeatherPresentationRenderer.cpp') -Raw
$skyRendererText = Get-Content -LiteralPath (Join-Path $engineRoot 'Graphics\Renderer\SkyRenderer.cpp') -Raw
$debugRendererText = Get-Content -LiteralPath (Join-Path $engineRoot 'Graphics\Renderer\EntityDebugRenderer.cpp') -Raw
$debugDrawMarker = 'void EntityDebugRenderer::draw('
$debugDrawIndex = $debugRendererText.IndexOf($debugDrawMarker, [System.StringComparison]::Ordinal)
$debugDrawText = if ($debugDrawIndex -ge 0) { $debugRendererText.Substring($debugDrawIndex) } else { $debugRendererText }
$graphicsSourceText = (($cppFiles | Where-Object { $_.FullName -like (Join-Path $engineRoot 'Graphics\*') } | ForEach-Object {
    Get-Content -LiteralPath $_.FullName -Raw
}) -join "`n")
$productionCpuHydroCandidates = @(
    (Join-Path $engineRoot 'Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.cpp')
    (Join-Path $engineRoot 'Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.hpp')
)
$productionCpuHydroFiles = @($productionCpuHydroCandidates | Where-Object { Test-Path -LiteralPath $_ })
$opt06DebugDrawUniformLookups = Count-Substring $debugDrawText 'glGetUniformLocation('
$opt06HotStateQueries =
    (Count-Substring $entityRendererText 'glIsEnabled(GL_BLEND)') +
    (Count-Substring $weatherRendererText 'glIsEnabled(GL_DEPTH_TEST)') +
    (Count-Substring $weatherRendererText 'glGetBooleanv(GL_DEPTH_WRITEMASK') +
    (Count-Substring $weatherRendererText 'glGetIntegerv(GL_ACTIVE_TEXTURE') +
    (Count-Substring $weatherRendererText 'glGetIntegerv(GL_VIEWPORT') +
    (Count-Substring $skyRendererText 'glGetIntegerv(GL_SAMPLES')
$opt06CloudHistoryCopies = Count-Substring $skyRendererText 'glCopyImageSubData(m_cloudTemporalTexture'
$opt06BlockingGpuCalls = (Count-Substring $graphicsSourceText 'glFinish(') + (Count-Substring $graphicsSourceText 'glGetTextureSubImage(') + (Count-Substring $graphicsSourceText 'glReadPixels(')
$opt06SurfaceRootLines = Count-Lines (Join-Path $engineRoot 'Graphics\Renderer\SurfacePresentationRenderer.cpp')
$opt06SkyRootLines = Count-Lines (Join-Path $engineRoot 'Graphics\Renderer\SkyRenderer.cpp')

$lines = New-Object 'System.Collections.Generic.List[string]'
$lines.Add('Heritage Engine code-health snapshot')
$lines.Add(('Generated: {0:yyyy-MM-dd HH:mm:ss}' -f (Get-Date)))
$lines.Add('Mode: static structural inventory; no source is modified by this audit')
$lines.Add('')
$lines.Add(('C/C++/INL files: {0} | lines: {1}' -f $cppStats.Count, (($cppStats | Measure-Object Lines -Sum).Sum)))
$lines.Add(('Lua files: {0} | lines: {1}' -f $luaStats.Count, (($luaStats | Measure-Object Lines -Sum).Sum)))
$lines.Add(('C/C++ files >=1000 lines: {0}' -f $largeCpp.Count))
$lines.Add(('Lua files >=500 lines: {0}' -f $largeLua.Count))
$lines.Add(('Compiled C++ translation units (engine + physics tests): {0}' -f $compiled.Count))
$lines.Add(('Project-tree .cpp not compiled by those active projects: {0}' -f @($uncompiledCpp).Count))
$lines.Add(('Reachable Racing United Lua files from Scripts/Main.lua: {0}/{1}' -f $reachableLua.Count, $luaFiles.Count))
$lines.Add(('Unreachable Racing United Lua files: {0}' -f @($unreachableLua).Count))
$lines.Add('')
$lines.Add('TOP C/C++ FILES BY LINE COUNT')
foreach ($item in $topCpp) { $lines.Add(('{0,6}  {1}' -f $item.Lines, $item.Path)) }
$lines.Add('')
$lines.Add('TOP LUA FILES BY LINE COUNT')
foreach ($item in $topLua) { $lines.Add(('{0,6}  {1}' -f $item.Lines, $item.Path)) }
$lines.Add('')
$lines.Add('UNCOMPILED .CPP INVENTORY')
if (@($uncompiledCpp).Count -eq 0) { $lines.Add('(none)') }
else { foreach ($item in ($uncompiledCpp | Sort-Object)) { $lines.Add($item) } }
$lines.Add('')
$lines.Add('OPT06 OPTIMIZATION-FREEZE INVENTORY')
$lines.Add(('Debug renderer draw-time glGetUniformLocation calls: {0}' -f $opt06DebugDrawUniformLookups))
$lines.Add(('Known synchronous hot renderer state queries: {0}' -f $opt06HotStateQueries))
$lines.Add(('Cloud temporal full-image copy calls: {0}' -f $opt06CloudHistoryCopies))
$lines.Add(('Blocking graphics readback/finish calls (glFinish/glGetTextureSubImage/glReadPixels): {0}' -f $opt06BlockingGpuCalls))
$lines.Add(('Production DynamicSurfaceHydrology files present: {0}' -f @($productionCpuHydroFiles).Count))
$lines.Add(('Renderer orchestration roots: SurfacePresentation {0} lines | Sky {1} lines' -f $opt06SurfaceRootLines, $opt06SkyRootLines))
$lines.Add('')
$lines.Add('UNREACHABLE LUA INVENTORY')
if (@($unreachableLua).Count -eq 0) { $lines.Add('(none)') }
else { foreach ($item in ($unreachableLua | Sort-Object)) { $lines.Add($item) } }

$reportDir = Split-Path -Parent $reportPath
if (-not (Test-Path $reportDir)) { New-Item -ItemType Directory -Path $reportDir | Out-Null }
[System.IO.File]::WriteAllLines($reportPath, $lines)
Write-Host ('Code-health snapshot: {0}' -f $reportPath)
Write-Host ('  C/C++/INL {0} files / {1} lines | Lua {2} files / {3} lines' -f
    $cppStats.Count, (($cppStats | Measure-Object Lines -Sum).Sum),
    $luaStats.Count, (($luaStats | Measure-Object Lines -Sum).Sum))
Write-Host ('  >=1000-line C/C++: {0} | uncompiled .cpp: {1} | unreachable Lua: {2}' -f
    $largeCpp.Count, @($uncompiledCpp).Count, @($unreachableLua).Count)
Write-Host ('  OPT06 freeze: debug uniform lookups {0} | hot state queries {1} | blocking GPU calls {2} | production CPU Hydro files {3}' -f
    $opt06DebugDrawUniformLookups, $opt06HotStateQueries, $opt06BlockingGpuCalls, @($productionCpuHydroFiles).Count)
