param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $Root = (Resolve-Path $Root).Path
}

$sourcePath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp"
$annotationPath = Join-Path $Root "Docs\LuaApiAnnotations.json"
$reportRoot = Join-Path $Root "Build\Reports"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null

if (-not (Test-Path $sourcePath)) {
    throw "Lua binding source not found: $sourcePath"
}

$source = [IO.File]::ReadAllText($sourcePath)
$pattern = 'registerFunction\(\s*"(?<namespace>[^"]+)"\s*,\s*"(?<function>[^"]+)"\s*,\s*&LuaModuleRuntime::(?<handler>[A-Za-z0-9_]+)\s*\)\s*;'
$matches = [regex]::Matches($source, $pattern, [Text.RegularExpressions.RegexOptions]::Singleline)
if ($matches.Count -eq 0) {
    throw "No registerFunction calls were found in $sourcePath"
}

$annotations = $null
if (Test-Path $annotationPath) {
    $annotations = [IO.File]::ReadAllText($annotationPath) | ConvertFrom-Json
}

$bindings = foreach ($match in $matches) {
    $namespace = $match.Groups['namespace'].Value
    $function = $match.Groups['function'].Value
    $handler = $match.Groups['handler'].Value
    $qualified = "$namespace.$function"

    $arguments = "UNANNOTATED - inspect C++ handler; do not guess"
    $returns = "UNANNOTATED - inspect C++ handler; do not guess"
    $description = "Exact binding name verified from LuaModuleRuntime.cpp."

    if ($null -ne $annotations) {
        $property = $annotations.PSObject.Properties[$qualified]
        if ($null -ne $property) {
            if ($property.Value.arguments) { $arguments = [string]$property.Value.arguments }
            if ($property.Value.returns) { $returns = [string]$property.Value.returns }
            if ($property.Value.description) { $description = [string]$property.Value.description }
        }
    }

    [pscustomobject]@{
        namespace = $namespace
        function = $function
        qualified_name = $qualified
        cpp_handler = "LuaModuleRuntime::$handler"
        arguments = $arguments
        returns = $returns
        description = $description
        source = "Engine/HeritageEngine/Core/Modules/LuaModuleRuntime.cpp"
    }
}

$bindings = @($bindings | Sort-Object qualified_name)
$duplicates = @($bindings | Group-Object qualified_name | Where-Object Count -gt 1)
if ($duplicates.Count -gt 0) {
    $names = ($duplicates | ForEach-Object Name) -join ", "
    throw "Duplicate Lua API registrations found: $names"
}

$sourceHash = (Get-FileHash -Algorithm SHA256 $sourcePath).Hash.ToLowerInvariant()
$generatedUtc = [DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
$manifest = [pscustomobject]@{
    generated_utc = $generatedUtc
    source_file = "Engine/HeritageEngine/Core/Modules/LuaModuleRuntime.cpp"
    source_sha256 = $sourceHash
    binding_count = $bindings.Count
    signature_policy = "Names and C++ handlers are source-verified. Unannotated signatures must be inspected in the handler and must not be guessed."
    bindings = $bindings
}

$jsonPath = Join-Path $reportRoot "LuaAPI.json"
$mdPath = Join-Path $reportRoot "LuaAPI.md"
$json = $manifest | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText($jsonPath, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# Heritage Engine Lua API")
$lines.Add("")
$lines.Add("Generated: ``$generatedUtc``")
$lines.Add("")
$lines.Add("Source SHA-256: ``$sourceHash``")
$lines.Add("")
$lines.Add("Registered functions: **$($bindings.Count)**")
$lines.Add("")
$lines.Add("> Function names and C++ handlers below are parsed from the current source. Any signature marked UNANNOTATED must be inspected in that handler; do not invent arguments from memory.")
$lines.Add("")

foreach ($group in ($bindings | Group-Object namespace | Sort-Object Name)) {
    $lines.Add("## $($group.Name)")
    $lines.Add("")
    foreach ($binding in $group.Group) {
        $lines.Add("### ``$($binding.qualified_name)``")
        $lines.Add("")
        $lines.Add("- C++ handler: ``$($binding.cpp_handler)``")
        $lines.Add("- Arguments: ``$($binding.arguments)``")
        $lines.Add("- Returns: ``$($binding.returns)``")
        $lines.Add("- $($binding.description)")
        $lines.Add("")
    }
}

[IO.File]::WriteAllLines($mdPath, $lines, [Text.UTF8Encoding]::new($false))
Write-Host "Generated Lua API manifest with $($bindings.Count) unique bindings."
