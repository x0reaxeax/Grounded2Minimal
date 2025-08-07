# IncrementVersion.ps1

$rcPath = "$PSScriptRoot\Grounded2Minimal.rc"

if (-Not (Test-Path $rcPath)) {
    Write-Host "RC file not found at: $rcPath"
    exit 1
}

Write-Host "Modifying RC file: $rcPath"

$versionNumbers = $null
$lines = Get-Content $rcPath | ForEach-Object {
    if ($_ -match '^\s*(FILEVERSION|PRODUCTVERSION)\s+(\d+),\s*(\d+),\s*(\d+),\s*(\d+)') {
        $tag = $matches[1]
        $v1 = [int]$matches[2]
        $v2 = [int]$matches[3]
        $v3 = ([int]$matches[4] + 1)  # increment build number
        $v4 = [int]$matches[5]
        $versionNumbers = @($v1, $v2, $v3, $v4)
        Write-Host "Updated $tag to $v1,$v2,$v3,$v4"
        "$tag $v1,$v2,$v3,$v4"
    } else {
        $_
    }
}

$lines | Set-Content $rcPath

# Also patch VALUE strings
if ($versionNumbers) {
    $vstr = "$($versionNumbers[0]).$($versionNumbers[1]).$($versionNumbers[2]).$($versionNumbers[3])"
    Write-Host "Patching VALUE strings to: $vstr"

    $raw = Get-Content $rcPath -Raw
    $raw = $raw -replace 'VALUE\s+"FileVersion",\s*"\d+\.\d+\.\d+\.\d+"', "VALUE `"FileVersion`", `"$vstr`""
    $raw = $raw -replace 'VALUE\s+"ProductVersion",\s*"\d+\.\d+\.\d+\.\d+"', "VALUE `"ProductVersion`", `"$vstr`""
    Set-Content $rcPath -Value $raw
} else {
    Write-Host "No version numbers found to patch VALUE strings."
}
