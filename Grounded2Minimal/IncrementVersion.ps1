# IncrementVersion.ps1
# Robust, single-write, atomic, preserves encoding, retries on lock

$ErrorActionPreference = 'Stop'

$rcPath = Join-Path $PSScriptRoot 'Grounded2Minimal.rc'
if (-not (Test-Path $rcPath)) {
    Write-Host "RC file not found at: $rcPath"
    exit 1
}

Write-Host "Modifying RC file: $rcPath"

function Get-FileEncoding {
    param([byte[]]$Bytes)
    if ($Bytes.Length -ge 2 -and $Bytes[0] -eq 0xFF -and $Bytes[1] -eq 0xFE) { return 'Unicode' }       # UTF-16 LE BOM
    if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF) { return 'UTF8' } # UTF-8 BOM
    # Many .rc files are ANSI; use Default to avoid mojibake in resource strings
    return 'Default'
}

# Read bytes first to detect encoding
$rawBytes = [System.IO.File]::ReadAllBytes($rcPath)
$enc = Get-FileEncoding -Bytes $rawBytes

# Read as text using detected encoding (Raw to preserve exact content)
# Note: Set-Content will re-encode using -Encoding we pass later.
switch ($enc) {
    'Unicode' { $text = [System.Text.Encoding]::Unicode.GetString($rawBytes) }
    'UTF8'    { $text = [System.Text.Encoding]::UTF8.GetString($rawBytes) }
    default   { $text = [System.Text.Encoding]::Default.GetString($rawBytes) }
}

# Regexes
# 1) FILEVERSION/PRODUCTVERSION lines (comma-separated numeric)
$rxVersionLine = [regex]'(?m)^\s*(FILEVERSION|PRODUCTVERSION)\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+)'
# 2) String table VALUE "FileVersion"/"ProductVersion" dotted version
$rxValueVersion = [regex]'(?m)(^\s*VALUE\s+"(?:FileVersion|ProductVersion)",\s*")(\d+\.\d+\.\d+\.\d+)(")'

$versionNumbers = $null
$updated = $rxVersionLine.Replace($text, {
    param($m)
    $tag = $m.Groups[1].Value
    $v1  = [int]$m.Groups[2].Value
    $v2  = [int]$m.Groups[3].Value
    $v3  = [int]$m.Groups[4].Value
    $v4  = ([int]$m.Groups[5].Value + 1)
    $script:versionNumbers = @($v1,$v2,$v3,$v4)
    Write-Host "Updated $tag to $v1,$v2,$v3,$v4"
    "$tag $v1,$v2,$v3,$v4"
})

if ($versionNumbers) {
    $vstr = '{0}.{1}.{2}.{3}' -f $versionNumbers[0],$versionNumbers[1],$versionNumbers[2],$versionNumbers[3]
    Write-Host "Patching VALUE strings to: $vstr"
    $updated = $rxValueVersion.Replace($updated, { param($m) $m.Groups[1].Value + $vstr + $m.Groups[3].Value })
} else {
    Write-Host "No version numbers found to patch VALUE strings."
}

# Normalize trailing whitespace: collapse multiple blank lines at EOF to a single newline
$updated = $updated -replace '(\r?\n)+\s*\z', "`r`n"

# Atomic write with retries (handles VS holding the file briefly)
$tempPath = [System.IO.Path]::GetTempFileName()
try {
    switch ($enc) {
        'Unicode' { Set-Content -Path $tempPath -Value $updated -Encoding Unicode -NoNewline }
        'UTF8'    { Set-Content -Path $tempPath -Value $updated -Encoding UTF8    -NoNewline }
        default   { Set-Content -Path $tempPath -Value $updated -Encoding Default -NoNewline }
    }

    $retries = 10
    for ($i=0; $i -lt $retries; $i++) {
        try {
            # Try to replace atomically (same volume)
            Move-Item -Path $tempPath -Destination $rcPath -Force
            $tempPath = $null
            break
        } catch {
            if ($i -eq $retries - 1) { throw }
            Start-Sleep -Milliseconds 200
        }
    }
}
finally {
    if ($tempPath -and (Test-Path $tempPath)) { Remove-Item $tempPath -Force -ErrorAction SilentlyContinue }
}
