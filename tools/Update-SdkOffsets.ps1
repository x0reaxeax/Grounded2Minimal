param(
    [Parameter(Mandatory = $true)]
    [string]$BasicHpp,

    [Parameter(Mandatory = $true)]
    [string]$SteamJson,

    [Parameter(Mandatory = $true)]
    [string]$XgpJson
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RequiredOffsets = @(
    "GObjects",
    "AppendString",
    "GNames",
    "GWorld",
    "ProcessEvent"
)

function Read-OffsetsJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$PlatformName
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$PlatformName offsets JSON does not exist: $Path"
    }

    try {
        $Offsets = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    }
    catch {
        throw "Failed to parse $PlatformName offsets JSON '$Path': $($_.Exception.Message)"
    }

    foreach ($Name in $RequiredOffsets) {
        $Property = $Offsets.PSObject.Properties[$Name]

        if ($null -eq $Property) {
            throw "$PlatformName offsets JSON is missing required field '$Name'."
        }

        $Value = [string]$Property.Value

        if ($Value -notmatch '^0x[0-9A-Fa-f]{1,16}$') {
            throw "$PlatformName offset '$Name' has invalid value '$Value'. Expected hexadecimal form such as 0x01234567."
        }

        if ([UInt64]::Parse(
            $Value.Substring(2),
            [System.Globalization.NumberStyles]::HexNumber
        ) -eq 0) {
            throw "$PlatformName offset '$Name' is zero."
        }
    }

    return $Offsets
}

function Get-UniqueMarkerIndex {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Content,

        [Parameter(Mandatory = $true)]
        [string]$Marker
    )

    $First = $Content.IndexOf(
        $Marker,
        [System.StringComparison]::Ordinal
    )

    if ($First -lt 0) {
        throw "Required marker not found in Basic.hpp: $Marker"
    }

    $Second = $Content.IndexOf(
        $Marker,
        $First + $Marker.Length,
        [System.StringComparison]::Ordinal
    )

    if ($Second -ge 0) {
        throw "Marker appears more than once in Basic.hpp: $Marker"
    }

    return $First
}

function Update-OffsetBlock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Block,

        [Parameter(Mandatory = $true)]
        [object]$Offsets,

        [Parameter(Mandatory = $true)]
        [string]$PlatformName
    )

    $UpdatedBlock = $Block

    foreach ($Name in $RequiredOffsets) {
        $NewValue = [string]$Offsets.PSObject.Properties[$Name].Value

        #
        # Match e.g.:
        #
        #     constexpr int32 GWorld = 0x0A913300;
        #
        # Preserve all existing spacing and replace only the value.
        #
        $Pattern = (
            '(?m)^' +
            '(?<Prefix>\s*constexpr\s+int32\s+' +
            [Regex]::Escape($Name) +
            '\s*=\s*)' +
            '(?<Value>0x[0-9A-Fa-f]+)' +
            '(?<Suffix>\s*;\s*)$'
        )

        $Matches = [Regex]::Matches(
            $UpdatedBlock,
            $Pattern
        )

        if ($Matches.Count -ne 1) {
            throw (
                "$PlatformName block: expected exactly one '$Name' definition, " +
                "found $($Matches.Count)."
            )
        }

        $OldValue = $Matches[0].Groups["Value"].Value

        Write-Host (
            "[{0}] {1,-14}: {2} -> {3}" -f
            $PlatformName,
            $Name,
            $OldValue,
            $NewValue
        )

        $Evaluator = {
            param($Match)

            return (
                $Match.Groups["Prefix"].Value +
                $NewValue.ToUpperInvariant() +
                $Match.Groups["Suffix"].Value
            )
        }.GetNewClosure()

        $UpdatedBlock = [Regex]::Replace(
            $UpdatedBlock,
            $Pattern,
            $Evaluator,
            1
        )
    }

    return $UpdatedBlock
}

#
# Validate inputs.
#

if (-not (Test-Path -LiteralPath $BasicHpp -PathType Leaf)) {
    throw "Basic.hpp does not exist: $BasicHpp"
}

$SteamOffsets = Read-OffsetsJson `
    -Path $SteamJson `
    -PlatformName "STEAM"

$XgpOffsets = Read-OffsetsJson `
    -Path $XgpJson `
    -PlatformName "XGP"

#
# Read Basic.hpp.
#

$Content = [System.IO.File]::ReadAllText(
    (Resolve-Path -LiteralPath $BasicHpp)
)

$SteamMarker = '#if (TARGET_PLATFORM == TARGET_PLATFORM_STEAM)'
$XgpMarker   = '#elif (TARGET_PLATFORM == TARGET_PLATFORM_XGP)'
$ElseMarker  = '#else'

$SteamStart = Get-UniqueMarkerIndex `
    -Content $Content `
    -Marker $SteamMarker

$XgpStart = Get-UniqueMarkerIndex `
    -Content $Content `
    -Marker $XgpMarker

if ($XgpStart -le $SteamStart) {
    throw "TARGET_PLATFORM_XGP block occurs before TARGET_PLATFORM_STEAM block."
}

#
# Find the #else belonging to this platform selection.
#
$ElseStart = $Content.IndexOf(
    $ElseMarker,
    $XgpStart + $XgpMarker.Length,
    [System.StringComparison]::Ordinal
)

if ($ElseStart -lt 0) {
    throw "Failed to find #else following TARGET_PLATFORM_XGP block."
}

#
# Split:
#
#   Prefix
#   Steam block
#   XGP block
#   Suffix
#
# We leave the preprocessor markers themselves untouched.
#

$SteamBodyStart = $SteamStart + $SteamMarker.Length
$XgpBodyStart   = $XgpStart + $XgpMarker.Length

$Prefix = $Content.Substring(
    0,
    $SteamBodyStart
)

$SteamBlock = $Content.Substring(
    $SteamBodyStart,
    $XgpStart - $SteamBodyStart
)

$BetweenMarkers = $Content.Substring(
    $XgpStart,
    $XgpMarker.Length
)

$XgpBlock = $Content.Substring(
    $XgpBodyStart,
    $ElseStart - $XgpBodyStart
)

$Suffix = $Content.Substring(
    $ElseStart
)

Write-Host ""
Write-Host "Updating Steam offsets..."

$SteamBlock = Update-OffsetBlock `
    -Block $SteamBlock `
    -Offsets $SteamOffsets `
    -PlatformName "STEAM"

Write-Host ""
Write-Host "Updating XGP offsets..."

$XgpBlock = Update-OffsetBlock `
    -Block $XgpBlock `
    -Offsets $XgpOffsets `
    -PlatformName "XGP"

#
# Reassemble file.
#

$UpdatedContent =
    $Prefix +
    $SteamBlock +
    $BetweenMarkers +
    $XgpBlock +
    $Suffix

#
# Sanity check: ProcessEventIdx must remain untouched by this script.
#
$OriginalProcessEventIdx = [Regex]::Matches(
    $Content,
    '(?m)^\s*constexpr\s+int32\s+ProcessEventIdx\s*=\s*0x[0-9A-Fa-f]+\s*;\s*$'
)

$UpdatedProcessEventIdx = [Regex]::Matches(
    $UpdatedContent,
    '(?m)^\s*constexpr\s+int32\s+ProcessEventIdx\s*=\s*0x[0-9A-Fa-f]+\s*;\s*$'
)

if (
    $OriginalProcessEventIdx.Count -ne $UpdatedProcessEventIdx.Count -or
    $OriginalProcessEventIdx.Count -eq 0
) {
    throw "Unexpected ProcessEventIdx definitions encountered during validation."
}

for ($i = 0; $i -lt $OriginalProcessEventIdx.Count; ++$i) {
    if (
        $OriginalProcessEventIdx[$i].Value -cne
        $UpdatedProcessEventIdx[$i].Value
    ) {
        throw "ProcessEventIdx was unexpectedly modified."
    }
}

#
# Don't touch the file if nothing actually changed.
#
if ($UpdatedContent -ceq $Content) {
    Write-Host ""
    Write-Host "[+] Basic.hpp already contains the requested offsets."
    exit 0
}

#
# Atomic-ish write:
#
# Write a sibling temporary file first, then replace Basic.hpp.
#
$ResolvedBasicHpp = (Resolve-Path -LiteralPath $BasicHpp).Path
$TemporaryPath = "$ResolvedBasicHpp.tmp"

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

try {
    [System.IO.File]::WriteAllText(
        $TemporaryPath,
        $UpdatedContent,
        $Utf8NoBom
    )

    Move-Item `
        -LiteralPath $TemporaryPath `
        -Destination $ResolvedBasicHpp `
        -Force
}
finally {
    if (Test-Path -LiteralPath $TemporaryPath) {
        Remove-Item -LiteralPath $TemporaryPath -Force
    }
}

Write-Host ""
Write-Host "[+] Successfully updated SDK offsets:"
Write-Host "    $ResolvedBasicHpp"