param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

$ErrorActionPreference = 'Stop'

$pathSeparators = [char[]]@('\', '/')
$rootPath = (Resolve-Path -LiteralPath $Root).Path.TrimEnd($pathSeparators)
$cacheDir = Join-Path $rootPath 'Build\Cache'
$cachePath = Join-Path $cacheDir 'IncrementalSourceHashes.tsv'

if (-not (Test-Path -LiteralPath $cacheDir)) {
    New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
}

# These are the inputs whose content can affect native object files or project
# composition. ZIP extraction preserves archived timestamps; content hashing
# prevents a changed file with an older timestamp from being skipped by
# MSBuild's otherwise-correct incremental build logic. The v2 marker also
# prevents an overlay ZIP from importing a foreign hash cache that does not
# describe the object files already present on the destination machine.
$trackedExtensions = @(
    '.c', '.cc', '.cpp', '.cxx',
    '.h', '.hh', '.hpp', '.hxx', '.inl', '.ipp',
    '.rc', '.vcxproj', '.props', '.targets'
)

$excludedDirectoryNames = @('.git', '.vs', 'x64', 'Debug', 'Release')

$cacheFormatMarker = '# heritage-incremental-source-hashes-v2'
$previousHashes = @{}
$cacheCompatible = $false
if (Test-Path -LiteralPath $cachePath) {
    $cacheLines = @(Get-Content -LiteralPath $cachePath)
    if ($cacheLines.Count -gt 0 -and $cacheLines[0] -eq $cacheFormatMarker) {
        $cacheCompatible = $true
        foreach ($line in $cacheLines | Select-Object -Skip 1) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            $tab = $line.IndexOf("`t")
            if ($tab -le 0) { continue }
            $hash = $line.Substring(0, $tab)
            $relative = $line.Substring($tab + 1)
            if ($hash -and $relative) {
                $previousHashes[$relative] = $hash
            }
        }
    }
}

if (-not $cacheCompatible -and (Test-Path -LiteralPath $cachePath)) {
    Write-Host 'Incremental freshness: legacy/foreign cache detected; invalidating once to reconcile native objects with current sources.'
}

$trackedFiles = Get-ChildItem -LiteralPath $rootPath -Recurse -File | Where-Object {
    $extension = $_.Extension.ToLowerInvariant()
    if ($trackedExtensions -notcontains $extension) { return $false }

    $relative = $_.FullName.Substring($rootPath.Length).TrimStart($pathSeparators)
    $parts = $relative -split '[\\/]'
    foreach ($part in $parts) {
        if ($excludedDirectoryNames -contains $part) { return $false }
    }
    return $true
}

$currentHashes = @{}
$changedFiles = New-Object System.Collections.Generic.List[string]
$nowUtc = [DateTime]::UtcNow

foreach ($file in $trackedFiles) {
    $relative = $file.FullName.Substring($rootPath.Length).TrimStart($pathSeparators) -replace '/', '\\'
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $currentHashes[$relative] = $hash

    $previous = $previousHashes[$relative]
    if (-not $previous -or $previous -ne $hash) {
        # Touch only changed/new build inputs. This is intentionally content-
        # based rather than timestamp-based, so overlay ZIP timestamps cannot
        # leave a stale .obj compiled against an older header/API.
        $file.LastWriteTimeUtc = $nowUtc
        $changedFiles.Add($relative)
    }
}

$tempPath = "$cachePath.tmp"
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add($cacheFormatMarker)
foreach ($relative in ($currentHashes.Keys | Sort-Object)) {
    $lines.Add("$($currentHashes[$relative])`t$relative")
}
[System.IO.File]::WriteAllLines($tempPath, $lines, (New-Object System.Text.UTF8Encoding($false)))
Move-Item -LiteralPath $tempPath -Destination $cachePath -Force

if ($changedFiles.Count -gt 0) {
    Write-Host ("Incremental freshness: touched {0} changed/new native build input(s); {1} unchanged." -f $changedFiles.Count, ($trackedFiles.Count - $changedFiles.Count))
} else {
    Write-Host ("Incremental freshness: 0 changed native build inputs; {0} unchanged." -f $trackedFiles.Count)
}

exit 0
