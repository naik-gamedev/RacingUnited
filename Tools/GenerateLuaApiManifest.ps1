param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $Root = (Resolve-Path $Root).Path
}

$runtimeSourcePath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp"
$bindingSourceRoot = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings"
$annotationPath = Join-Path $Root "Docs\LuaApiAnnotations.json"
$reportRoot = Join-Path $Root "Build\Reports"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null

function RelativeSourcePath([string]$FullName) {
    $relative = $FullName.Substring($Root.Length)
    $relative = $relative -replace '^[\\/]+', ''
    return $relative.Replace('\', '/')
}

if (-not (Test-Path $runtimeSourcePath)) {
    throw "Lua runtime source not found: $runtimeSourcePath"
}
if (-not (Test-Path $bindingSourceRoot)) {
    throw "Lua binding source directory not found: $bindingSourceRoot"
}

$handlerOwnerPattern = 'Lua(?:Core|Physics|Vehicle|Entity)BindingHandlers'
$pattern = 'registerFunction\(\s*"(?<namespace>[^"]+)"\s*,\s*"(?<function>[^"]+)"\s*,\s*&(?<owner>' + $handlerOwnerPattern + ')::(?<handler>[A-Za-z0-9_]+)\s*\)\s*;' 

# CLEAN07: binding domains own their registration tables. The runtime keeps only
# the small ordered registerBindings() orchestrator and the global print override.
$implementationFiles = @(
    Get-ChildItem -Path $bindingSourceRoot -Recurse -File -Filter "*.cpp" |
        Sort-Object FullName
)
if ($implementationFiles.Count -eq 0) {
    throw "No Lua binding implementation files were found below $bindingSourceRoot"
}

$registrationFiles = @((Get-Item $runtimeSourcePath)) + $implementationFiles
$registrations = New-Object System.Collections.Generic.List[object]
foreach ($file in $registrationFiles) {
    $text = [IO.File]::ReadAllText($file.FullName)
    foreach ($match in [regex]::Matches(
            $text,
            $pattern,
            [Text.RegularExpressions.RegexOptions]::Singleline)) {
        $registrations.Add([pscustomobject]@{
            Namespace = $match.Groups['namespace'].Value
            Function = $match.Groups['function'].Value
            Handler = $match.Groups['handler'].Value
            Owner = $match.Groups['owner'].Value
            Source = RelativeSourcePath $file.FullName
        })
    }
}
if ($registrations.Count -eq 0) {
    throw "No registerFunction calls were found in the Lua runtime/binding source set."
}

$handlerSources = @{}
foreach ($file in $implementationFiles) {
    $text = [IO.File]::ReadAllText($file.FullName)
    $definitionMatches = [regex]::Matches(
        $text,
        '(?<owner>' + $handlerOwnerPattern + ')::(?<handler>[A-Za-z0-9_]+)\s*\(',
        [Text.RegularExpressions.RegexOptions]::Singleline)
    foreach ($definitionMatch in $definitionMatches) {
        $handler = $definitionMatch.Groups['handler'].Value
        $owner = $definitionMatch.Groups['owner'].Value
        if ($handlerSources.ContainsKey($handler)) {
            throw "Lua handler '$handler' is implemented in more than one binding source file."
        }
        $relative = RelativeSourcePath $file.FullName
        $handlerSources[$handler] = [pscustomobject]@{ Source = $relative; Owner = $owner }
    }
}

$annotations = $null
if (Test-Path $annotationPath) {
    $annotations = [IO.File]::ReadAllText($annotationPath) | ConvertFrom-Json
}

$bindings = foreach ($registration in $registrations) {
    $namespace = $registration.Namespace
    $function = $registration.Function
    $handler = $registration.Handler
    $owner = $registration.Owner
    $qualified = "$namespace.$function"

    if (-not $handlerSources.ContainsKey($handler)) {
        throw "Registered Lua handler '$handler' has no implementation under Core/Modules/LuaBindings."
    }

    if ($handlerSources[$handler].Owner -ne $owner) {
        throw "Registered Lua handler '$handler' uses owner '$owner' but its implementation belongs to '$($handlerSources[$handler].Owner)'."
    }

    $arguments = "UNANNOTATED - inspect C++ handler; do not guess"
    $returns = "UNANNOTATED - inspect C++ handler; do not guess"
    $description = "Exact binding name verified from its domain-owned registration table."

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
        cpp_handler = "${owner}::$handler"
        arguments = $arguments
        returns = $returns
        description = $description
        registration_source = $registration.Source
        source = $handlerSources[$handler].Source
    }
}

$bindings = @($bindings | Sort-Object qualified_name)
$duplicates = @($bindings | Group-Object qualified_name | Where-Object Count -gt 1)
if ($duplicates.Count -gt 0) {
    $names = ($duplicates | ForEach-Object Name) -join ", "
    throw "Duplicate Lua API registrations found: $names"
}

# Deterministic hash of the registration source plus every implementation file.
$sourceFingerprintLines = New-Object System.Collections.Generic.List[string]
$registrationHash = (Get-FileHash -Algorithm SHA256 $runtimeSourcePath).Hash.ToLowerInvariant()
$sourceFingerprintLines.Add("Engine/HeritageEngine/Core/Modules/LuaModuleRuntime.cpp=$registrationHash")
foreach ($file in $implementationFiles) {
    $relative = RelativeSourcePath $file.FullName
    $hash = (Get-FileHash -Algorithm SHA256 $file.FullName).Hash.ToLowerInvariant()
    $sourceFingerprintLines.Add("$relative=$hash")
}
$fingerprintText = ($sourceFingerprintLines -join "`n")
$sha = [System.Security.Cryptography.SHA256]::Create()
try {
    $bytes = [Text.Encoding]::UTF8.GetBytes($fingerprintText)
    $sourceHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
} finally {
    $sha.Dispose()
}

$generatedUtc = [DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
$sourceFiles = @("Engine/HeritageEngine/Core/Modules/LuaModuleRuntime.cpp") + @(
    $implementationFiles | ForEach-Object {
        RelativeSourcePath $_.FullName
    }
)
$manifest = [pscustomobject]@{
    generated_utc = $generatedUtc
    source_file = "distributed domain registration under Core/Modules/LuaBindings"
    source_files = $sourceFiles
    source_sha256 = $sourceHash
    binding_count = $bindings.Count
    signature_policy = "Names are source-verified from domain-owned registration tables. Handler source paths are source-verified from binding translation units. Unannotated signatures must be inspected in the handler and must not be guessed."
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
$lines.Add("Lua binding source-set SHA-256: ``$sourceHash``")
$lines.Add("")
$lines.Add("Registered functions: **$($bindings.Count)**")
$lines.Add("")
$lines.Add("> Function names are parsed from domain-owned registration tables. Handler source paths are resolved from the split binding implementation files. Any signature marked UNANNOTATED must be inspected in that handler; do not invent arguments from memory.")
$lines.Add("")

foreach ($group in ($bindings | Group-Object namespace | Sort-Object Name)) {
    $lines.Add("## $($group.Name)")
    $lines.Add("")
    foreach ($binding in $group.Group) {
        $lines.Add("### ``$($binding.qualified_name)``")
        $lines.Add("")
        $lines.Add("- C++ handler: ``$($binding.cpp_handler)``")
        $lines.Add("- Source: ``$($binding.source)``")
        $lines.Add("- Arguments: ``$($binding.arguments)``")
        $lines.Add("- Returns: ``$($binding.returns)``")
        $lines.Add("- $($binding.description)")
        $lines.Add("")
    }
}

[IO.File]::WriteAllLines($mdPath, $lines, [Text.UTF8Encoding]::new($false))
Write-Host "Generated Lua API manifest with $($bindings.Count) unique bindings across $($implementationFiles.Count) implementation files."
