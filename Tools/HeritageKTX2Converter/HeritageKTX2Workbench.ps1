# Heritage KTX2 Workbench v2.6
# ASCII-only Windows PowerShell 5.1 / WinForms frontend.
# Large HDR/EXR path uses Basis Universal as the image frontend and Khronos KTX
# for final native GPU transcoding, avoiding giant uncompressed EXR->KTX stages.

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()

$script:KtxPath = $null
$script:BasisPath = $null
$script:MagickPath = $null
$script:CompressonatorPath = $null
$script:Running = $false
$script:TempFiles = New-Object System.Collections.Generic.List[string]

function Quote-Arg([string]$s) {
    if ($null -eq $s) { return '""' }
    return '"' + ($s -replace '"','\"') + '"'
}

function Split-ExtraArgs([string]$text) {
    $result = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($text)) { return @() }
    $matches = [regex]::Matches($text, '(?:[^\s"]+|"[^"]*")+')
    foreach ($m in $matches) {
        $v = $m.Value.Trim()
        if ($v.StartsWith('"') -and $v.EndsWith('"') -and $v.Length -ge 2) {
            $v = $v.Substring(1, $v.Length - 2)
        }
        if ($v.Length -gt 0) { [void]$result.Add($v) }
    }
    return $result.ToArray()
}

function Find-Exe([string]$name, [string[]]$extraCandidates) {
    $candidates = New-Object System.Collections.Generic.List[string]
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { [void]$candidates.Add($cmd.Source) }
    $scriptDir = Split-Path -Parent $MyInvocation.ScriptName
    [void]$candidates.Add((Join-Path $scriptDir $name))
    [void]$candidates.Add((Join-Path $scriptDir ('bin\' + $name)))
    foreach ($p in $extraCandidates) { if ($p) { [void]$candidates.Add($p) } }
    foreach ($p in $candidates) {
        if ($p -and (Test-Path $p -PathType Leaf)) { return (Resolve-Path $p).Path }
    }
    return $null
}

function Find-KtxExe {
    return Find-Exe 'ktx.exe' @(
        (Join-Path ${env:ProgramFiles} 'KTX-Software\bin\ktx.exe'),
        (Join-Path ${env:ProgramFiles} 'KTX\bin\ktx.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'KTX-Software\bin\ktx.exe')
    )
}

function Find-BasisExe {
    return Find-Exe 'basisu.exe' @(
        (Join-Path ${env:ProgramFiles} 'Basis Universal\basisu.exe'),
        (Join-Path ${env:ProgramFiles} 'basis_universal\basisu.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Basis Universal\basisu.exe')
    )
}

function Resolve-ExecutableCandidate([string]$candidate, [string]$exeName) {
    if ([string]::IsNullOrWhiteSpace($candidate)) { return $null }
    try {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
        if (Test-Path -LiteralPath $candidate -PathType Container) {
            $found = Get-ChildItem -LiteralPath $candidate -Filter $exeName -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    } catch {}
    return $null
}

function Find-CompressonatorExe {
    $scriptDir = Split-Path -Parent $MyInvocation.ScriptName
    $searchRoots = New-Object System.Collections.Generic.List[string]
    if ($scriptDir) { [void]$searchRoots.Add($scriptDir) }
    foreach ($root in @(${env:ProgramFiles}, ${env:ProgramFiles(x86)})) {
        if ($root) { [void]$searchRoots.Add($root) }
    }

    # PATH first.
    $cmd = Get-Command 'CompressonatorCLI.exe' -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }

    # Portable/downloaded folders are common, so recursively resolve only
    # directories whose names look like Compressonator packages.
    foreach ($root in $searchRoots) {
        if ([string]::IsNullOrWhiteSpace($root) -or -not (Test-Path -LiteralPath $root -PathType Container)) { continue }
        $direct = Resolve-ExecutableCandidate $root 'CompressonatorCLI.exe'
        if ($root -eq $scriptDir -and $direct) { return $direct }
        foreach ($dir in (Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '*Compressonator*' -or $_.Name -like '*compressonatorcli*' })) {
            $resolved = Resolve-ExecutableCandidate $dir.FullName 'CompressonatorCLI.exe'
            if ($resolved) { return $resolved }
        }
    }
    return $null
}
function Find-MagickExe {
    $roots = New-Object System.Collections.Generic.List[string]
    foreach ($root in @(${env:ProgramFiles}, ${env:ProgramFiles(x86)})) {
        if (-not $root -or -not (Test-Path $root)) { continue }
        Get-ChildItem -Path $root -Directory -Filter 'ImageMagick*' -ErrorAction SilentlyContinue | ForEach-Object {
            [void]$roots.Add((Join-Path $_.FullName 'magick.exe'))
        }
    }
    return Find-Exe 'magick.exe' $roots.ToArray()
}

function Append-Log([string]$text) {
    if ($null -ne $txtLog) {
        $txtLog.AppendText($text + [Environment]::NewLine)
        $txtLog.SelectionStart = $txtLog.TextLength
        $txtLog.ScrollToCaret()
        [System.Windows.Forms.Application]::DoEvents()
    }
}

function Run-Tool([string]$exe, [string]$displayName, [string[]]$arguments) {
    if (-not $exe -or -not (Test-Path $exe -PathType Leaf)) { throw "$displayName executable was not found." }

    $resolvedExe = (Resolve-Path $exe).Path
    $displayArgs = (($arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    Append-Log ("> " + (Quote-Arg $resolvedExe) + " " + $displayArgs)
    Append-Log ("Argument count: " + $arguments.Count)

    # v2.4: invoke the executable with PowerShell's native argument array.
    # Do not manually build ProcessStartInfo.Arguments: legacy CLI parsers can
    # misread reconstructed/escaped Windows command lines, especially paths.
    $captured = @()
    try {
        $captured = & $resolvedExe @arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    catch {
        throw "$displayName failed to start: $($_.Exception.Message)"
    }
    foreach ($line in $captured) {
        if ($null -ne $line) { Append-Log ([string]$line) }
    }
    if ($exitCode -ne 0) { throw "$displayName exited with code $exitCode." }
}
function Run-Ktx([string[]]$arguments) { Run-Tool $script:KtxPath 'ktx' $arguments }
function Run-Basis([string[]]$arguments) { Run-Tool $script:BasisPath 'basisu' $arguments }
function Run-Magick([string[]]$arguments) { Run-Tool $script:MagickPath 'ImageMagick' $arguments }
function Run-Compressonator([string[]]$arguments) { Run-Tool $script:CompressonatorPath 'CompressonatorCLI' $arguments }

function New-TempFile([string]$suffix) {
    $p = Join-Path ([System.IO.Path]::GetTempPath()) ('heritage_ktx2_' + [guid]::NewGuid().ToString('N') + $suffix)
    [void]$script:TempFiles.Add($p)
    return $p
}

function New-TempKtx([string]$suffix) {
    $p = Join-Path ([System.IO.Path]::GetTempPath()) ('heritage_ktx2_' + [guid]::NewGuid().ToString('N') + $suffix)
    [void]$script:TempFiles.Add($p)
    return $p
}

function Update-ToolStatus {
    if ($script:KtxPath -and (Test-Path $script:KtxPath)) {
        $lblKtx.Text = "KTX: $script:KtxPath"
        $lblKtx.ForeColor = [System.Drawing.Color]::DarkGreen
    } else {
        $lblKtx.Text = 'KTX: not found'
        $lblKtx.ForeColor = [System.Drawing.Color]::DarkRed
    }
    if ($script:BasisPath -and (Test-Path $script:BasisPath)) {
        $lblBasis.Text = "BasisU: $script:BasisPath"
        $lblBasis.ForeColor = [System.Drawing.Color]::DarkGreen
    } else {
        $lblBasis.Text = 'BasisU: not found - required for HDR/UASTC paths'
        $lblBasis.ForeColor = [System.Drawing.Color]::DarkOrange
    }
    if ($null -ne $lblCompressonator) {
        if ($script:CompressonatorPath) {
            $resolvedCmp = Resolve-ExecutableCandidate $script:CompressonatorPath 'CompressonatorCLI.exe'
            if ($resolvedCmp) { $script:CompressonatorPath = $resolvedCmp }
        }
        if ($script:CompressonatorPath -and (Test-Path -LiteralPath $script:CompressonatorPath -PathType Leaf)) {
            $lblCompressonator.Text = "Compressonator: $script:CompressonatorPath"
            $lblCompressonator.ForeColor = [System.Drawing.Color]::DarkGreen
        } else {
            $lblCompressonator.Text = 'Compressonator: not found - strongly recommended for direct BC6H/BC7'
            $lblCompressonator.ForeColor = [System.Drawing.Color]::DarkOrange
        }
    }
    if ($null -ne $lblMagick) {
        if ($script:MagickPath -and (Test-Path $script:MagickPath)) {
            $lblMagick.Text = "EXR helper: $script:MagickPath"
            $lblMagick.ForeColor = [System.Drawing.Color]::DarkGreen
        } else {
            $lblMagick.Text = 'EXR helper: ImageMagick not found - only needed if BasisU cannot read an EXR directly'
            $lblMagick.ForeColor = [System.Drawing.Color]::DarkOrange
        }
    }
}

function Is-HdrPreset([string]$name) {
    return $name.StartsWith('HDR')
}

function Needs-Basis([string]$preset) {
    return -not $preset.StartsWith('KTX direct')
}

function Needs-Ktx([string]$preset) {
    if ($preset -eq 'HDR - UASTC HDR 4x4 (portable)') { return $false }
    if ($preset -eq 'HDR - UASTC HDR 6x6i (portable/smaller)') { return $false }
    if ($preset -eq 'LDR - UASTC 4x4 (portable)') { return $false }
    if ($preset -eq 'LDR - BasisLZ / ETC1S (small portable)') { return $false }
    return $true
}

function Basis-CommonArgs([bool]$linear, [bool]$mips, [double]$scale) {
    $a = New-Object System.Collections.Generic.List[string]
    [void]$a.Add('-ktx2')
    if ($linear) { [void]$a.Add('-linear') }
    if ($mips) { [void]$a.Add('-mipmap') }
    if ([math]::Abs($scale - 1.0) -gt 0.000001) {
        [void]$a.Add('-resample_factor')
        [void]$a.Add($scale.ToString('0.######', [System.Globalization.CultureInfo]::InvariantCulture))
    }
    $extra = Split-ExtraArgs $txtBasisExtra.Text
    foreach ($x in $extra) { [void]$a.Add($x) }
    return $a
}

function Add-BasisQuality([System.Collections.Generic.List[string]]$argsList, [int]$level) {
    [void]$argsList.Add('-uastc_level')
    [void]$argsList.Add([string]$level)
}

function Add-KtxExtra([System.Collections.Generic.List[string]]$argsList) {
    $extra = Split-ExtraArgs $txtKtxExtra.Text
    foreach ($x in $extra) { [void]$argsList.Add($x) }
}

function Invoke-BasisEncodeHdrRaw([string]$sourcePath, [string]$destinationPath, [bool]$sixBySix, [bool]$linear, [bool]$mips, [double]$scale, [int]$level) {
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($x in (Basis-CommonArgs $linear $mips $scale)) { [void]$list.Add($x) }
    if ($sixBySix) { [void]$list.Add('-hdr_6x6i') } else { [void]$list.Add('-hdr_4x4') }
    Add-BasisQuality $list $level
    [void]$list.Add('-file'); [void]$list.Add($sourcePath)
    [void]$list.Add('-output_file'); [void]$list.Add($destinationPath)
    Run-Basis $list.ToArray()
}

function Normalize-ExrForBasis([string]$sourcePath) {
    if (-not $script:MagickPath) { $script:MagickPath = Find-MagickExe }
    if (-not $script:MagickPath) {
        throw 'BasisU could not read this EXR and ImageMagick was not found for the automatic compatibility fallback. Install ImageMagick 7 or use the Get ImageMagick button.'
    }
    $normalized = New-TempFile '_basis_compat.exr'
    Append-Log ''
    Append-Log 'BasisU could not decode the original EXR. Retrying through an uncompressed RGB EXR compatibility copy...'
    Run-Magick @($sourcePath, '-alpha', 'off', '-define', 'exr:color-type=RGB', '-compress', 'None', $normalized)
    return $normalized
}

function Convert-ExrToRadianceHdr([string]$sourcePath) {
    if (-not $script:MagickPath) { $script:MagickPath = Find-MagickExe }
    if (-not $script:MagickPath) {
        throw 'ImageMagick is required for the Radiance HDR compatibility fallback.'
    }
    $hdr = New-TempFile '_basis_compat.hdr'
    Append-Log ''
    Append-Log 'Uncompressed EXR retry also failed. Retrying through Radiance HDR...'
    Run-Magick @($sourcePath, '-alpha', 'off', $hdr)
    return $hdr
}

function Basis-EncodeHdr([string]$sourcePath, [string]$destinationPath, [bool]$sixBySix, [bool]$linear, [bool]$mips, [double]$scale, [int]$level) {
    try {
        Invoke-BasisEncodeHdrRaw $sourcePath $destinationPath $sixBySix $linear $mips $scale $level
        return
    }
    catch {
        if ([System.IO.Path]::GetExtension($sourcePath).ToLowerInvariant() -ne '.exr') { throw }
        Append-Log ('Direct BasisU EXR read failed: ' + $_.Exception.Message)
    }

    $normalized = Normalize-ExrForBasis $sourcePath
    try {
        Invoke-BasisEncodeHdrRaw $normalized $destinationPath $sixBySix $linear $mips $scale $level
        return
    }
    catch {
        Append-Log ('Uncompressed EXR retry failed: ' + $_.Exception.Message)
    }

    $hdr = Convert-ExrToRadianceHdr $sourcePath
    Invoke-BasisEncodeHdrRaw $hdr $destinationPath $sixBySix $linear $mips $scale $level
}


function Run-ToolCapture([string]$exe, [string]$displayName, [string[]]$arguments) {
    if (-not $exe -or -not (Test-Path $exe)) { throw "$displayName executable was not found." }
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.Arguments = (($arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    Append-Log ("> " + $displayName + " " + ($arguments -join ' '))
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    if ($stderr.Trim()) { Append-Log $stderr.TrimEnd() }
    if ($p.ExitCode -ne 0) { throw "$displayName exited with code $($p.ExitCode)." }
    return $stdout.Trim()
}

function Get-ImageDimensions([string]$sourcePath) {
    if (-not $script:MagickPath) { $script:MagickPath = Find-MagickExe }
    if (-not $script:MagickPath) { throw 'ImageMagick is required to inspect dimensions for the large-HDR raw bridge.' }
    $result = Run-ToolCapture $script:MagickPath 'ImageMagick identify' @('identify','-format','%w %h',$sourcePath)
    $parts = $result -split '\s+'
    if ($parts.Count -lt 2) { throw "Could not determine image dimensions: $result" }
    return @([int]$parts[0], [int]$parts[1])
}

function Encode-HdrViaRawHalfBridge([string]$sourcePath, [string]$uastcOutput, [double]$scale, [int]$level) {
    if (-not $script:MagickPath) { $script:MagickPath = Find-MagickExe }
    if (-not $script:MagickPath) {
        throw 'The large HDR raw bridge needs ImageMagick 7 x64 HDRI. Use Locate Magick or Get ImageMagick.'
    }
    if (-not $script:KtxPath) { $script:KtxPath = Find-KtxExe }
    if (-not $script:KtxPath) { throw 'The large HDR raw bridge needs Khronos ktx.exe.' }
    if ($chkMips.Checked) {
        throw 'Large HDR raw bridge currently requires mipmaps OFF. This avoids creating several huge temporary raw levels.'
    }

    $dims = Get-ImageDimensions $sourcePath
    $srcW = [int]$dims[0]
    $srcH = [int]$dims[1]
    $dstW = [math]::Max(1, [int][math]::Round($srcW * $scale))
    $dstH = [math]::Max(1, [int][math]::Round($srcH * $scale))
    $raw = New-TempFile '_rgb16f.raw'
    $rawMiB = ([double]$dstW * [double]$dstH * 6.0) / 1MB

    Append-Log ''
    Append-Log 'Large HDR raw-half bridge engaged.'
    Append-Log ("Source dimensions: {0}x{1}" -f $srcW,$srcH)
    Append-Log ("Bridge dimensions: {0}x{1}" -f $dstW,$dstH)
    Append-Log ("Temporary RGB16F raw payload: ~{0:N1} MiB" -f $rawMiB)
    Append-Log 'This path bypasses BasisU/TinyEXR image loading and the ~2 GiB decoded-buffer boundary.'

    $magickArgs = New-Object System.Collections.Generic.List[string]
    [void]$magickArgs.Add($sourcePath)
    [void]$magickArgs.Add('-alpha'); [void]$magickArgs.Add('off')
    if (($dstW -ne $srcW) -or ($dstH -ne $srcH)) {
        [void]$magickArgs.Add('-filter'); [void]$magickArgs.Add('Lanczos')
        [void]$magickArgs.Add('-resize'); [void]$magickArgs.Add(("{0}x{1}!" -f $dstW,$dstH))
    }
    [void]$magickArgs.Add('-depth'); [void]$magickArgs.Add('16')
    [void]$magickArgs.Add('-define'); [void]$magickArgs.Add('quantum:format=floating-point')
    [void]$magickArgs.Add('-endian'); [void]$magickArgs.Add('LSB')
    [void]$magickArgs.Add(('RGB:' + $raw))
    Run-Magick $magickArgs.ToArray()

    if (-not (Test-Path $raw)) { throw 'ImageMagick did not produce the RGB16F raw bridge file.' }
    $actual = (Get-Item $raw).Length
    $expected = [int64]$dstW * [int64]$dstH * 6
    if ($actual -ne $expected) {
        throw "RGB16F raw bridge size mismatch. Expected $expected bytes, got $actual bytes."
    }

    $ktxArgs = New-Object System.Collections.Generic.List[string]
    [void]$ktxArgs.Add('create')
    [void]$ktxArgs.Add('--format'); [void]$ktxArgs.Add('R16G16B16_SFLOAT')
    [void]$ktxArgs.Add('--raw')
    [void]$ktxArgs.Add('--width'); [void]$ktxArgs.Add([string]$dstW)
    [void]$ktxArgs.Add('--height'); [void]$ktxArgs.Add([string]$dstH)
    [void]$ktxArgs.Add('--assign-tf'); [void]$ktxArgs.Add('linear')
    [void]$ktxArgs.Add('--assign-primaries'); [void]$ktxArgs.Add('bt709')
    [void]$ktxArgs.Add('--encode'); [void]$ktxArgs.Add('uastc-hdr-4x4')
    if ($level -ge 3) { [void]$ktxArgs.Add('--uastc-hdr-ultra-quant') }
    Add-KtxExtra $ktxArgs
    [void]$ktxArgs.Add($raw)
    [void]$ktxArgs.Add($uastcOutput)
    Run-Ktx $ktxArgs.ToArray()
}

function Encode-HdrRobust([string]$sourcePath, [string]$destinationPath, [bool]$sixBySix, [bool]$linear, [bool]$mips, [double]$scale, [int]$level) {
    # UASTC HDR 6x6 remains a BasisU path. The raw-half bridge targets the
    # 4x4 intermediate used for BC6H/ASTC4/RGB16F transcodes.
    if ($sixBySix) {
        Basis-EncodeHdr $sourcePath $destinationPath $true $linear $mips $scale $level
        return
    }

    try {
        Basis-EncodeHdr $sourcePath $destinationPath $false $linear $mips $scale $level
        return
    }
    catch {
        Append-Log ''
        Append-Log ('BasisU HDR path failed: ' + $_.Exception.Message)
        if ([System.IO.Path]::GetExtension($sourcePath).ToLowerInvariant() -ne '.exr') { throw }
        Append-Log 'Switching to the RGB16F raw bridge instead of retrying another full-size EXR decoder.'
    }
    Encode-HdrViaRawHalfBridge $sourcePath $destinationPath $scale $level
}

function Basis-EncodeLdrUastc([string]$sourcePath, [string]$destinationPath, [bool]$linear, [bool]$mips, [double]$scale, [int]$level) {
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($x in (Basis-CommonArgs $linear $mips $scale)) { [void]$list.Add($x) }
    [void]$list.Add('-uastc')
    Add-BasisQuality $list $level
    [void]$list.Add('-file'); [void]$list.Add($sourcePath)
    [void]$list.Add('-output_file'); [void]$list.Add($destinationPath)
    Run-Basis $list.ToArray()
}

function Basis-EncodeEtc1s([string]$sourcePath, [string]$destinationPath, [bool]$linear, [bool]$mips, [double]$scale, [int]$quality) {
    $list = New-Object System.Collections.Generic.List[string]
    foreach ($x in (Basis-CommonArgs $linear $mips $scale)) { [void]$list.Add($x) }
    [void]$list.Add('-q'); [void]$list.Add([string]$quality)
    [void]$list.Add('-file'); [void]$list.Add($sourcePath)
    [void]$list.Add('-output_file'); [void]$list.Add($destinationPath)
    Run-Basis $list.ToArray()
}

function Ktx-Transcode([string]$sourcePath, [string]$destinationPath, [string]$target) {
    $list = New-Object System.Collections.Generic.List[string]
    [void]$list.Add('transcode'); [void]$list.Add('--target'); [void]$list.Add($target)
    Add-KtxExtra $list
    [void]$list.Add($sourcePath); [void]$list.Add($destinationPath)
    Run-Ktx $list.ToArray()
}

function Ktx-Deflate([string]$sourcePath, [string]$destinationPath, [int]$level) {
    $list = New-Object System.Collections.Generic.List[string]
    [void]$list.Add('deflate'); [void]$list.Add('--zstd'); [void]$list.Add([string]$level)
    Add-KtxExtra $list
    [void]$list.Add($sourcePath); [void]$list.Add($destinationPath)
    Run-Ktx $list.ToArray()
}

function Compressonator-DirectBc6h([string]$sourcePath, [string]$destinationPath, [bool]$mips, [int]$qualityLevel) {
    $script:CompressonatorPath = Resolve-ExecutableCandidate $script:CompressonatorPath 'CompressonatorCLI.exe'
    if (-not $script:CompressonatorPath) { $script:CompressonatorPath = Find-CompressonatorExe }
    if ([string]::IsNullOrWhiteSpace($script:CompressonatorPath)) {
        throw 'Direct BC6H mode could not resolve CompressonatorCLI.exe. Use Locate CMP and select either the EXE or its containing folder.'
    }
    if ([string]::IsNullOrWhiteSpace($sourcePath) -or -not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw 'BC6H source image does not exist.' }

    $outputParent = Split-Path -Parent $destinationPath
    if ($outputParent -and -not (Test-Path -LiteralPath $outputParent)) {
        [void](New-Item -ItemType Directory -Path $outputParent -Force)
    }
    if (-not [string]::IsNullOrWhiteSpace($destinationPath) -and (Test-Path -LiteralPath $destinationPath -PathType Leaf)) { Remove-Item -LiteralPath $destinationPath -Force }

    $q = switch ($qualityLevel) {
        0 { '0.05' }
        1 { '0.20' }
        2 { '0.45' }
        3 { '0.70' }
        default { '0.90' }
    }

    # Keep this command intentionally minimal. Compressonator defaults to one
    # mip level, so no-mipmap needs no switch. InExposure/Gamma are for
    # float-to-non-float tonemapping and are inappropriate for BC6H HDR.
    $a = New-Object System.Collections.Generic.List[string]
    [void]$a.Add('-fd'); [void]$a.Add('BC6H')
    [void]$a.Add('-NumThreads'); [void]$a.Add('0')
    [void]$a.Add('-Quality'); [void]$a.Add($q)
    if ($mips) { [void]$a.Add('-mipsize'); [void]$a.Add('1') }
    [void]$a.Add($sourcePath)
    [void]$a.Add($destinationPath)

    Append-Log 'Direct BC6H path: EXR -> BC6H KTX2 via AMD CompressonatorCLI only.'
    Append-Log ('BC6H source path: ' + $sourcePath)
    Append-Log ('BC6H destination path: ' + $destinationPath)
    Append-Log ('Resolved CMP executable: ' + $script:CompressonatorPath)
    Append-Log ('Source is file: ' + (Test-Path -LiteralPath $sourcePath -PathType Leaf))
    Append-Log ('Destination extension: ' + [System.IO.Path]::GetExtension($destinationPath))
    Append-Log ('BC6H quality scalar: ' + $q)
    Run-Compressonator $a.ToArray()

    if ([string]::IsNullOrWhiteSpace($destinationPath) -or -not (Test-Path -LiteralPath $destinationPath -PathType Leaf)) {
        throw 'Compressonator returned success but did not create the requested KTX2 file.'
    }
    $bytes = (Get-Item -LiteralPath $destinationPath).Length
    if ($bytes -le 0) { throw 'Compressonator created an empty output file.' }
    Append-Log ('BC6H output created: ' + [math]::Round($bytes / 1MB, 2) + ' MiB')
}
function Convert-Texture {
    $sourcePath = $txtInput.Text.Trim()
    $destinationPath = $txtOutput.Text.Trim()
    if ([string]::IsNullOrWhiteSpace($sourcePath) -or -not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw 'Choose an existing input image.' }
    if ([string]::IsNullOrWhiteSpace($destinationPath)) { throw 'Choose an output path.' }
    if ([System.IO.Path]::GetExtension($destinationPath).ToLowerInvariant() -ne '.ktx2') {
        $destinationPath += '.ktx2'
        $txtOutput.Text = $destinationPath
    }

    $preset = [string]$cmbPreset.SelectedItem
    $linear = $cmbTransfer.SelectedIndex -eq 0
    $mips = $chkMips.Checked
    $scale = [double]$numScale.Value
    $level = [int]$numQuality.Value
    $basisQuality = [math]::Max(1, [math]::Min(255, 32 + $level * 48))
    $zstdLevel = [int]$numZstd.Value

    if (-not $script:KtxPath) { $script:KtxPath = Find-KtxExe }
    if (-not $script:BasisPath) { $script:BasisPath = Find-BasisExe }
    if (-not $script:CompressonatorPath) { $script:CompressonatorPath = Find-CompressonatorExe }
    Update-ToolStatus

    $nativeBc6Direct = ($preset -eq 'HDR - BC6H unsigned (native desktop GPU)') -and $script:CompressonatorPath
    if ((Needs-Basis $preset) -and (-not $script:BasisPath) -and (-not $nativeBc6Direct)) {
        throw 'This preset needs basisu.exe. Put basisu.exe next to this script or use Locate BasisU.'
    }
    if ((Needs-Ktx $preset) -and (-not $script:KtxPath) -and (-not $nativeBc6Direct)) {
        throw 'This preset needs Khronos ktx.exe. Install KTX-Software or use Locate KTX.'
    }

    Append-Log 'Heritage KTX2 Workbench v2.6'
    Append-Log "Input : $sourcePath"
    Append-Log "Output: $destinationPath"
    Append-Log "Preset: $preset"
    Append-Log "Transfer: $([string]$cmbTransfer.SelectedItem)"
    Append-Log "Scale: $scale"
    Append-Log "Mipmaps: $mips"
    Append-Log ''

    if ($preset -eq 'HDR - BC6H unsigned (native desktop GPU)') {
        if ($script:CompressonatorPath) {
            if ([math]::Abs($scale - 1.0) -gt 0.000001) {
                if (-not $script:MagickPath) { $script:MagickPath = Find-MagickExe }
                if (-not $script:MagickPath) { throw 'Scaling direct BC6H requires ImageMagick. Use scale 1.0 or install ImageMagick.' }
                $dims = Get-ImageDimensions $sourcePath
                $scaledW = [math]::Max(1, [int][math]::Round([int]$dims[0] * $scale))
                $scaledH = [math]::Max(1, [int][math]::Round([int]$dims[1] * $scale))
                $scaled = New-TempFile '_scaled.exr'
                Append-Log ("Pre-scaling HDR source to {0}x{1} with ImageMagick HDRI." -f $scaledW,$scaledH)
                Run-Magick @($sourcePath,'-alpha','off','-filter','Lanczos','-resize',("{0}x{1}!" -f $scaledW,$scaledH),'-define','exr:color-type=RGB',$scaled)
                Compressonator-DirectBc6h $scaled $destinationPath $mips $level
            } else {
                Compressonator-DirectBc6h $sourcePath $destinationPath $mips $level
            }
        } else {
            Append-Log 'Compressonator not found; falling back to UASTC HDR -> KTX BC6H path.'
            $u = New-TempKtx '_hdr4_uastc.ktx2'
            Encode-HdrRobust $sourcePath $u $false $true $mips $scale $level
            Ktx-Transcode $u $destinationPath 'bc6hu'
        }
    }
    elseif ($preset -eq 'HDR - UASTC HDR 4x4 (portable)') {
        Encode-HdrRobust $sourcePath $destinationPath $false $true $mips $scale $level
    }
    elseif ($preset -eq 'HDR - UASTC HDR 6x6i (portable/smaller)') {
        Basis-EncodeHdr $sourcePath $destinationPath $true $true $mips $scale $level
    }
    elseif ($preset -eq 'HDR - ASTC HDR 4x4 (native GPU)') {
        $u = New-TempKtx '_hdr4_uastc.ktx2'
        Encode-HdrRobust $sourcePath $u $false $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'astc-hdr-4x4'
    }
    elseif ($preset -eq 'HDR - ASTC HDR 6x6 (native GPU)') {
        $u = New-TempKtx '_hdr6_uastc.ktx2'
        Basis-EncodeHdr $sourcePath $u $true $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'astc-hdr-6x6'
    }
    elseif ($preset -eq 'HDR - RGB16F uncompressed') {
        $u = New-TempKtx '_hdr4_uastc.ktx2'
        Encode-HdrRobust $sourcePath $u $false $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'rgb16f'
    }
    elseif ($preset -eq 'HDR - RGB16F + Zstd') {
        $u = New-TempKtx '_hdr4_uastc.ktx2'
        $raw = New-TempKtx '_rgb16f.ktx2'
        Encode-HdrRobust $sourcePath $u $false $true $mips $scale $level
        Ktx-Transcode $u $raw 'rgb16f'
        Ktx-Deflate $raw $destinationPath $zstdLevel
    }
    elseif ($preset -eq 'HDR - RGB9E5 shared exponent') {
        $u = New-TempKtx '_hdr4_uastc.ktx2'
        Encode-HdrRobust $sourcePath $u $false $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'rgb9e5'
    }
    elseif ($preset -eq 'LDR - BC7 (native desktop GPU)') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $destinationPath 'bc7'
    }
    elseif ($preset -eq 'LDR - BC5 RG (normal/data)') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'bc5'
    }
    elseif ($preset -eq 'LDR - BC4 R (single channel/data)') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $true $mips $scale $level
        Ktx-Transcode $u $destinationPath 'bc4'
    }
    elseif ($preset -eq 'LDR - BC3 RGBA') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $destinationPath 'bc3'
    }
    elseif ($preset -eq 'LDR - BC1 RGB') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $destinationPath 'bc1'
    }
    elseif ($preset -eq 'LDR - ASTC 4x4') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $destinationPath 'astc'
    }
    elseif ($preset -eq 'LDR - UASTC 4x4 (portable)') {
        Basis-EncodeLdrUastc $sourcePath $destinationPath $linear $mips $scale $level
    }
    elseif ($preset -eq 'LDR - BasisLZ / ETC1S (small portable)') {
        Basis-EncodeEtc1s $sourcePath $destinationPath $linear $mips $scale $basisQuality
    }
    elseif ($preset -eq 'LDR - RGBA8 uncompressed') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $destinationPath 'rgba8'
    }
    elseif ($preset -eq 'LDR - RGBA8 + Zstd') {
        $u = New-TempKtx '_ldr_uastc.ktx2'
        $raw = New-TempKtx '_rgba8.ktx2'
        Basis-EncodeLdrUastc $sourcePath $u $linear $mips $scale $level
        Ktx-Transcode $u $raw 'rgba8'
        Ktx-Deflate $raw $destinationPath $zstdLevel
    }
    elseif ($preset -eq 'KTX direct - custom create arguments') {
        if (-not $script:KtxPath) { throw 'KTX direct mode needs ktx.exe.' }
        $custom = Split-ExtraArgs $txtKtxCreateArgs.Text
        if ($custom.Count -eq 0) { throw 'Enter custom ktx create arguments, for example: --format R8G8B8A8_SRGB' }
        $list = New-Object System.Collections.Generic.List[string]
        [void]$list.Add('create')
        foreach ($x in $custom) { [void]$list.Add($x) }
        if ($mips) { [void]$list.Add('--generate-mipmap') }
        [void]$list.Add($sourcePath); [void]$list.Add($destinationPath)
        Run-Ktx $list.ToArray()
    }
    else {
        throw 'Unknown preset.'
    }

    if ($chkValidate.Checked -and (Test-Path $destinationPath)) {
        if ($script:KtxPath) { Run-Ktx @('validate', $destinationPath) }
        else { Append-Log 'Validation skipped: ktx.exe is not installed.' }
    }

    if (Test-Path $destinationPath) {
        $bytes = (Get-Item -LiteralPath $destinationPath).Length
        $mib = $bytes / 1MB
        Append-Log ''
        Append-Log ('SUCCESS - output file: {0:N2} MiB' -f $mib)
        [System.Windows.Forms.MessageBox]::Show(
            "Conversion complete.`n`n$destinationPath`n`nFile size: $([math]::Round($mib,2)) MiB",
            'Heritage KTX2 Workbench',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = 'Heritage KTX2 Workbench v2.6'
$form.StartPosition = 'CenterScreen'
$form.Size = New-Object System.Drawing.Size(1040, 900)
$form.MinimumSize = New-Object System.Drawing.Size(920, 790)
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)

$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = 'Heritage KTX2 Workbench'
$lblTitle.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 17)
$lblTitle.AutoSize = $true
$lblTitle.Location = New-Object System.Drawing.Point(18, 14)
$form.Controls.Add($lblTitle)

$lblSubtitle = New-Object System.Windows.Forms.Label
$lblSubtitle.Text = 'General HDR/LDR texture conversion. Large EXR/HDR uses BasisU first, then KTX native GPU transcoding.'
$lblSubtitle.AutoSize = $true
$lblSubtitle.Location = New-Object System.Drawing.Point(20, 50)
$form.Controls.Add($lblSubtitle)

$lblInput = New-Object System.Windows.Forms.Label
$lblInput.Text = 'Input image'
$lblInput.AutoSize = $true
$lblInput.Location = New-Object System.Drawing.Point(20, 82)
$form.Controls.Add($lblInput)

$txtInput = New-Object System.Windows.Forms.TextBox
$txtInput.Location = New-Object System.Drawing.Point(20, 103)
$txtInput.Size = New-Object System.Drawing.Size(820, 25)
$txtInput.Anchor = 'Top,Left,Right'
$form.Controls.Add($txtInput)

$btnInput = New-Object System.Windows.Forms.Button
$btnInput.Text = 'Browse...'
$btnInput.Location = New-Object System.Drawing.Point(852, 101)
$btnInput.Size = New-Object System.Drawing.Size(145, 29)
$btnInput.Anchor = 'Top,Right'
$form.Controls.Add($btnInput)

$lblOutput = New-Object System.Windows.Forms.Label
$lblOutput.Text = 'Output KTX2'
$lblOutput.AutoSize = $true
$lblOutput.Location = New-Object System.Drawing.Point(20, 139)
$form.Controls.Add($lblOutput)

$txtOutput = New-Object System.Windows.Forms.TextBox
$txtOutput.Location = New-Object System.Drawing.Point(20, 160)
$txtOutput.Size = New-Object System.Drawing.Size(820, 25)
$txtOutput.Anchor = 'Top,Left,Right'
$form.Controls.Add($txtOutput)

$btnOutput = New-Object System.Windows.Forms.Button
$btnOutput.Text = 'Browse...'
$btnOutput.Location = New-Object System.Drawing.Point(852, 158)
$btnOutput.Size = New-Object System.Drawing.Size(145, 29)
$btnOutput.Anchor = 'Top,Right'
$form.Controls.Add($btnOutput)

$lblPreset = New-Object System.Windows.Forms.Label
$lblPreset.Text = 'Output / compression preset'
$lblPreset.AutoSize = $true
$lblPreset.Location = New-Object System.Drawing.Point(20, 201)
$form.Controls.Add($lblPreset)

$cmbPreset = New-Object System.Windows.Forms.ComboBox
$cmbPreset.DropDownStyle = 'DropDownList'
$cmbPreset.Location = New-Object System.Drawing.Point(20, 222)
$cmbPreset.Size = New-Object System.Drawing.Size(490, 28)
$presets = @(
    'HDR - BC6H unsigned (native desktop GPU)',
    'HDR - UASTC HDR 4x4 (portable)',
    'HDR - UASTC HDR 6x6i (portable/smaller)',
    'HDR - ASTC HDR 4x4 (native GPU)',
    'HDR - ASTC HDR 6x6 (native GPU)',
    'HDR - RGB16F uncompressed',
    'HDR - RGB16F + Zstd',
    'HDR - RGB9E5 shared exponent',
    'LDR - BC7 (native desktop GPU)',
    'LDR - BC5 RG (normal/data)',
    'LDR - BC4 R (single channel/data)',
    'LDR - BC3 RGBA',
    'LDR - BC1 RGB',
    'LDR - ASTC 4x4',
    'LDR - UASTC 4x4 (portable)',
    'LDR - BasisLZ / ETC1S (small portable)',
    'LDR - RGBA8 uncompressed',
    'LDR - RGBA8 + Zstd',
    'KTX direct - custom create arguments'
)
foreach ($p in $presets) { [void]$cmbPreset.Items.Add($p) }
$cmbPreset.SelectedIndex = 0
$form.Controls.Add($cmbPreset)

$lblTransfer = New-Object System.Windows.Forms.Label
$lblTransfer.Text = 'Transfer / content'
$lblTransfer.AutoSize = $true
$lblTransfer.Location = New-Object System.Drawing.Point(530, 201)
$form.Controls.Add($lblTransfer)

$cmbTransfer = New-Object System.Windows.Forms.ComboBox
$cmbTransfer.DropDownStyle = 'DropDownList'
$cmbTransfer.Location = New-Object System.Drawing.Point(530, 222)
$cmbTransfer.Size = New-Object System.Drawing.Size(180, 28)
[void]$cmbTransfer.Items.Add('Linear / data / HDR')
[void]$cmbTransfer.Items.Add('sRGB color')
$cmbTransfer.SelectedIndex = 0
$form.Controls.Add($cmbTransfer)

$chkMips = New-Object System.Windows.Forms.CheckBox
$chkMips.Text = 'Mipmaps'
$chkMips.Checked = $false
$chkMips.AutoSize = $true
$chkMips.Location = New-Object System.Drawing.Point(730, 226)
$form.Controls.Add($chkMips)

$chkValidate = New-Object System.Windows.Forms.CheckBox
$chkValidate.Text = 'Validate'
$chkValidate.Checked = $true
$chkValidate.AutoSize = $true
$chkValidate.Location = New-Object System.Drawing.Point(825, 226)
$form.Controls.Add($chkValidate)

$lblScale = New-Object System.Windows.Forms.Label
$lblScale.Text = 'Input scale factor'
$lblScale.AutoSize = $true
$lblScale.Location = New-Object System.Drawing.Point(20, 269)
$form.Controls.Add($lblScale)

$numScale = New-Object System.Windows.Forms.NumericUpDown
$numScale.Location = New-Object System.Drawing.Point(20, 290)
$numScale.Size = New-Object System.Drawing.Size(120, 25)
$numScale.DecimalPlaces = 3
$numScale.Minimum = [decimal]0.001
$numScale.Maximum = [decimal]16.0
$numScale.Increment = [decimal]0.125
$numScale.Value = [decimal]1.0
$form.Controls.Add($numScale)

$lblQuality = New-Object System.Windows.Forms.Label
$lblQuality.Text = 'UASTC quality level (0 fast - 4 max)'
$lblQuality.AutoSize = $true
$lblQuality.Location = New-Object System.Drawing.Point(165, 269)
$form.Controls.Add($lblQuality)

$numQuality = New-Object System.Windows.Forms.NumericUpDown
$numQuality.Location = New-Object System.Drawing.Point(165, 290)
$numQuality.Size = New-Object System.Drawing.Size(100, 25)
$numQuality.Minimum = 0
$numQuality.Maximum = 4
$numQuality.Value = 3
$form.Controls.Add($numQuality)

$lblZstd = New-Object System.Windows.Forms.Label
$lblZstd.Text = 'Zstd level (1-22)'
$lblZstd.AutoSize = $true
$lblZstd.Location = New-Object System.Drawing.Point(295, 269)
$form.Controls.Add($lblZstd)

$numZstd = New-Object System.Windows.Forms.NumericUpDown
$numZstd.Location = New-Object System.Drawing.Point(295, 290)
$numZstd.Size = New-Object System.Drawing.Size(100, 25)
$numZstd.Minimum = 1
$numZstd.Maximum = 22
$numZstd.Value = 12
$form.Controls.Add($numZstd)

$lblAdvice = New-Object System.Windows.Forms.Label
$lblAdvice.Text = 'NASA 16K sky: HDR BC6H, Linear, scale 1.0, mipmaps OFF, quality 3 or 4.'
$lblAdvice.AutoSize = $true
$lblAdvice.ForeColor = [System.Drawing.Color]::MidnightBlue
$lblAdvice.Location = New-Object System.Drawing.Point(430, 293)
$form.Controls.Add($lblAdvice)

$grpTools = New-Object System.Windows.Forms.GroupBox
$grpTools.Text = 'Encoder tools'
$grpTools.Location = New-Object System.Drawing.Point(20, 332)
$grpTools.Size = New-Object System.Drawing.Size(977, 174)
$grpTools.Anchor = 'Top,Left,Right'
$form.Controls.Add($grpTools)

$lblKtx = New-Object System.Windows.Forms.Label
$lblKtx.Location = New-Object System.Drawing.Point(12, 22)
$lblKtx.Size = New-Object System.Drawing.Size(710, 20)
$grpTools.Controls.Add($lblKtx)

$lblBasis = New-Object System.Windows.Forms.Label
$lblBasis.Location = New-Object System.Drawing.Point(12, 47)
$lblBasis.Size = New-Object System.Drawing.Size(710, 20)
$grpTools.Controls.Add($lblBasis)

$btnLocateKtx = New-Object System.Windows.Forms.Button
$btnLocateKtx.Text = 'Locate KTX...'
$btnLocateKtx.Location = New-Object System.Drawing.Point(730, 18)
$btnLocateKtx.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnLocateKtx)

$btnGetKtx = New-Object System.Windows.Forms.Button
$btnGetKtx.Text = 'Get KTX'
$btnGetKtx.Location = New-Object System.Drawing.Point(845, 18)
$btnGetKtx.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnGetKtx)

$btnLocateBasis = New-Object System.Windows.Forms.Button
$btnLocateBasis.Text = 'Locate BasisU...'
$btnLocateBasis.Location = New-Object System.Drawing.Point(730, 49)
$btnLocateBasis.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnLocateBasis)

$btnGetBasis = New-Object System.Windows.Forms.Button
$btnGetBasis.Text = 'Get BasisU'
$btnGetBasis.Location = New-Object System.Drawing.Point(845, 49)
$btnGetBasis.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnGetBasis)

$lblCompressonator = New-Object System.Windows.Forms.Label
$lblCompressonator.Location = New-Object System.Drawing.Point(12, 72)
$lblCompressonator.Size = New-Object System.Drawing.Size(710, 20)
$grpTools.Controls.Add($lblCompressonator)

$btnLocateCompressonator = New-Object System.Windows.Forms.Button
$btnLocateCompressonator.Text = 'Locate CMP...'
$btnLocateCompressonator.Location = New-Object System.Drawing.Point(730, 72)
$btnLocateCompressonator.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnLocateCompressonator)

$btnGetCompressonator = New-Object System.Windows.Forms.Button
$btnGetCompressonator.Text = 'Get CMP'
$btnGetCompressonator.Location = New-Object System.Drawing.Point(845, 72)
$btnGetCompressonator.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnGetCompressonator)

$lblMagick = New-Object System.Windows.Forms.Label
$lblMagick.Location = New-Object System.Drawing.Point(12, 103)
$lblMagick.Size = New-Object System.Drawing.Size(710, 20)
$grpTools.Controls.Add($lblMagick)

$btnLocateMagick = New-Object System.Windows.Forms.Button
$btnLocateMagick.Text = 'Locate Magick...'
$btnLocateMagick.Location = New-Object System.Drawing.Point(730, 103)
$btnLocateMagick.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnLocateMagick)

$btnGetMagick = New-Object System.Windows.Forms.Button
$btnGetMagick.Text = 'Get ImageMagick'
$btnGetMagick.Location = New-Object System.Drawing.Point(845, 103)
$btnGetMagick.Size = New-Object System.Drawing.Size(105, 27)
$grpTools.Controls.Add($btnGetMagick)

$lblToolHint = New-Object System.Windows.Forms.Label
$lblToolHint.Text = 'BC6H direct mode needs only CompressonatorCLI. KTX is optional for validation; BasisU/ImageMagick are only for other presets/scaling.'
$lblToolHint.AutoSize = $true
$lblToolHint.Location = New-Object System.Drawing.Point(12, 142)
$grpTools.Controls.Add($lblToolHint)

$grpAdvanced = New-Object System.Windows.Forms.GroupBox
$grpAdvanced.Text = 'Advanced - optional extra command-line arguments'
$grpAdvanced.Location = New-Object System.Drawing.Point(20, 516)
$grpAdvanced.Size = New-Object System.Drawing.Size(977, 130)
$grpAdvanced.Anchor = 'Top,Left,Right'
$form.Controls.Add($grpAdvanced)

$lblBasisExtra = New-Object System.Windows.Forms.Label
$lblBasisExtra.Text = 'Extra BasisU args'
$lblBasisExtra.AutoSize = $true
$lblBasisExtra.Location = New-Object System.Drawing.Point(12, 25)
$grpAdvanced.Controls.Add($lblBasisExtra)

$txtBasisExtra = New-Object System.Windows.Forms.TextBox
$txtBasisExtra.Location = New-Object System.Drawing.Point(135, 22)
$txtBasisExtra.Size = New-Object System.Drawing.Size(815, 25)
$txtBasisExtra.Anchor = 'Top,Left,Right'
$grpAdvanced.Controls.Add($txtBasisExtra)

$lblKtxExtra = New-Object System.Windows.Forms.Label
$lblKtxExtra.Text = 'Extra KTX args'
$lblKtxExtra.AutoSize = $true
$lblKtxExtra.Location = New-Object System.Drawing.Point(12, 57)
$grpAdvanced.Controls.Add($lblKtxExtra)

$txtKtxExtra = New-Object System.Windows.Forms.TextBox
$txtKtxExtra.Location = New-Object System.Drawing.Point(135, 54)
$txtKtxExtra.Size = New-Object System.Drawing.Size(815, 25)
$txtKtxExtra.Anchor = 'Top,Left,Right'
$grpAdvanced.Controls.Add($txtKtxExtra)

$lblCreateArgs = New-Object System.Windows.Forms.Label
$lblCreateArgs.Text = 'Custom create args'
$lblCreateArgs.AutoSize = $true
$lblCreateArgs.Location = New-Object System.Drawing.Point(12, 89)
$grpAdvanced.Controls.Add($lblCreateArgs)

$txtKtxCreateArgs = New-Object System.Windows.Forms.TextBox
$txtKtxCreateArgs.Location = New-Object System.Drawing.Point(135, 86)
$txtKtxCreateArgs.Size = New-Object System.Drawing.Size(815, 25)
$txtKtxCreateArgs.Anchor = 'Top,Left,Right'
$txtKtxCreateArgs.Text = '--format R8G8B8A8_SRGB'
$grpAdvanced.Controls.Add($txtKtxCreateArgs)

$btnConvert = New-Object System.Windows.Forms.Button
$btnConvert.Text = 'CONVERT'
$btnConvert.Font = New-Object System.Drawing.Font('Segoe UI Semibold', 11)
$btnConvert.Location = New-Object System.Drawing.Point(842, 659)
$btnConvert.Size = New-Object System.Drawing.Size(155, 40)
$btnConvert.Anchor = 'Top,Right'
$form.Controls.Add($btnConvert)

$btnClear = New-Object System.Windows.Forms.Button
$btnClear.Text = 'Clear log'
$btnClear.Location = New-Object System.Drawing.Point(727, 664)
$btnClear.Size = New-Object System.Drawing.Size(100, 30)
$btnClear.Anchor = 'Top,Right'
$form.Controls.Add($btnClear)

$txtLog = New-Object System.Windows.Forms.TextBox
$txtLog.Multiline = $true
$txtLog.ReadOnly = $true
$txtLog.ScrollBars = 'Both'
$txtLog.WordWrap = $false
$txtLog.Location = New-Object System.Drawing.Point(20, 710)
$txtLog.Size = New-Object System.Drawing.Size(977, 120)
$txtLog.Anchor = 'Top,Bottom,Left,Right'
$txtLog.Font = New-Object System.Drawing.Font('Consolas', 9)
$form.Controls.Add($txtLog)

$btnInput.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = 'Supported images|*.exr;*.hdr;*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.qoi|EXR/HDR|*.exr;*.hdr|PNG|*.png|All files|*.*'
    $dlg.Title = 'Choose source texture'
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtInput.Text = $dlg.FileName
        if (-not $txtOutput.Text) { $txtOutput.Text = [System.IO.Path]::ChangeExtension($dlg.FileName, '.ktx2') }
    }
})

$btnOutput.Add_Click({
    $dlg = New-Object System.Windows.Forms.SaveFileDialog
    $dlg.Filter = 'KTX2 texture (*.ktx2)|*.ktx2'
    $dlg.DefaultExt = 'ktx2'
    $dlg.AddExtension = $true
    if ($txtInput.Text) { $dlg.FileName = [System.IO.Path]::GetFileNameWithoutExtension($txtInput.Text) + '.ktx2' }
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $txtOutput.Text = $dlg.FileName }
})

$btnLocateKtx.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = 'Khronos KTX tool (ktx.exe)|ktx.exe|Executable (*.exe)|*.exe'
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $script:KtxPath = $dlg.FileName; Update-ToolStatus }
})

$btnLocateBasis.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = 'Basis Universal encoder (basisu.exe)|basisu.exe|Executable (*.exe)|*.exe'
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $script:BasisPath = $dlg.FileName; Update-ToolStatus }
})

$btnLocateMagick.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = 'ImageMagick (magick.exe)|magick.exe|Executable (*.exe)|*.exe'
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $script:MagickPath = $dlg.FileName; Update-ToolStatus }
})

$btnLocateCompressonator.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Title = 'Select CompressonatorCLI.exe (Cancel to choose its folder instead)'
    $dlg.Filter = 'AMD Compressonator CLI (CompressonatorCLI.exe)|CompressonatorCLI.exe|Executable (*.exe)|*.exe'
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $script:CompressonatorPath = $dlg.FileName
    } else {
        $folderDlg = New-Object System.Windows.Forms.FolderBrowserDialog
        $folderDlg.Description = 'Select the folder containing CompressonatorCLI.exe'
        if ($folderDlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            $script:CompressonatorPath = Resolve-ExecutableCandidate $folderDlg.SelectedPath 'CompressonatorCLI.exe'
            if (-not $script:CompressonatorPath) {
                [System.Windows.Forms.MessageBox]::Show('CompressonatorCLI.exe was not found anywhere under that folder.','Compressonator not found') | Out-Null
            }
        }
    }
    Update-ToolStatus
})
$btnGetCompressonator.Add_Click({ Start-Process 'https://gpuopen.com/compressonator/' })
$btnGetKtx.Add_Click({ Start-Process 'https://github.com/KhronosGroup/KTX-Software/releases' })
$btnGetBasis.Add_Click({ Start-Process 'https://github.com/BinomialLLC/basis_universal' })
$btnGetMagick.Add_Click({ Start-Process 'https://imagemagick.org/script/download.php#windows' })
$btnClear.Add_Click({ $txtLog.Clear() })

$cmbPreset.Add_SelectedIndexChanged({
    $p = [string]$cmbPreset.SelectedItem
    if (Is-HdrPreset $p) { $cmbTransfer.SelectedIndex = 0 }
    if ($p.Contains('BC5') -or $p.Contains('BC4')) { $cmbTransfer.SelectedIndex = 0 }
    if ($p -eq 'KTX direct - custom create arguments') {
        $txtKtxCreateArgs.Enabled = $true
    } else {
        $txtKtxCreateArgs.Enabled = $false
    }
})

$btnConvert.Add_Click({
    if ($script:Running) { return }
    try {
        $script:Running = $true
        $btnConvert.Enabled = $false
        $script:TempFiles.Clear()
        $txtLog.Clear()
        Convert-Texture
    }
    catch {
        Append-Log ''
        Append-Log ('ERROR: ' + $_.Exception.Message)
        if ($_.InvocationInfo) {
            Append-Log ('At script line: ' + $_.InvocationInfo.ScriptLineNumber)
            Append-Log ('Statement: ' + $_.InvocationInfo.Line.Trim())
        }
        if ($_.ScriptStackTrace) { Append-Log ('Stack: ' + $_.ScriptStackTrace) }
        [System.Windows.Forms.MessageBox]::Show(
            $_.Exception.Message,
            'Conversion failed',
            [System.Windows.Forms.MessageBoxButtons]::OK,
            [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
    }
    finally {
        foreach ($f in $script:TempFiles) {
            if ($f -and (Test-Path $f)) { Remove-Item $f -Force -ErrorAction SilentlyContinue }
        }
        $script:Running = $false
        $btnConvert.Enabled = $true
    }
})

$script:KtxPath = Find-KtxExe
$script:BasisPath = Find-BasisExe
$script:MagickPath = Find-MagickExe
$script:CompressonatorPath = Find-CompressonatorExe
Update-ToolStatus
$txtKtxCreateArgs.Enabled = $false
Append-Log 'Ready - v2.5 resolves Compressonator folders/executables safely and logs exact PowerShell failure locations.'
Append-Log 'For the 16384x8192 NASA EXR: HDR BC6H unsigned, Linear, scale 1.0, mipmaps OFF, quality 3.'
Append-Log 'Fallback path remains available: BasisU / ImageMagick raw bridge / Khronos KTX for other HDR targets.'

[void]$form.ShowDialog()
