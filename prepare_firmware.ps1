# Prepare firmware files for release
# This script copies compiled firmware, renames it properly, and generates MD5 hashes

param(
    [Parameter(Mandatory=$true)]
    [string]$Version
)

# Validate version format (e.g., 1.1.0)
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Error "Version must be in format X.Y.Z (e.g., 1.1.0)"
    exit 1
}

$ErrorActionPreference = "Stop"

# Define paths
$BuildDirRev2 = ".pio\build\ratgdo_esp32dev"
$BuildDirRev1 = ".pio\build\ratgdo_esp32dev_rev1"
$OutputDir = "docs\firmware"

# Create output directory if it doesn't exist
if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
    Write-Host "Created output directory: $OutputDir"
}

# Function to process firmware
function Process-Firmware {
    param(
        [string]$SourceDir,
        [string]$Revision,
        [string]$Version
    )

    $BaseName = "homekit-grgdo1-$Revision-v$Version"

    # Check if source firmware exists
    $SourceFirmware = Join-Path $SourceDir "firmware.bin"
    if (-not (Test-Path $SourceFirmware)) {
        Write-Error "Source firmware not found: $SourceFirmware"
        return $false
    }

    # Copy and rename firmware files
    Write-Host "`nProcessing $Revision firmware..."

    # Firmware binary
    $DestFirmware = Join-Path $OutputDir "$BaseName.firmware.bin"
    Copy-Item $SourceFirmware -Destination $DestFirmware -Force
    Write-Host "  Copied: $BaseName.firmware.bin"

    # Generate MD5 hash
    $Hash = (Get-FileHash -Path $DestFirmware -Algorithm MD5).Hash.ToLower()
    $DestMD5 = Join-Path $OutputDir "$BaseName.firmware.md5"
    $Hash | Out-File -FilePath $DestMD5 -Encoding ascii -NoNewline
    Write-Host "  MD5:    $Hash"

    # Optional: Copy other files (bootloader, partitions, elf)
    $SourceBootloader = Join-Path $SourceDir "bootloader.bin"
    if (Test-Path $SourceBootloader) {
        Copy-Item $SourceBootloader -Destination (Join-Path $OutputDir "$BaseName.bootloader.bin") -Force
        Write-Host "  Copied: $BaseName.bootloader.bin"
    }

    $SourcePartitions = Join-Path $SourceDir "partitions.bin"
    if (Test-Path $SourcePartitions) {
        Copy-Item $SourcePartitions -Destination (Join-Path $OutputDir "$BaseName.partitions.bin") -Force
        Write-Host "  Copied: $BaseName.partitions.bin"
    }

    $SourceElf = Join-Path $SourceDir "firmware.elf"
    if (Test-Path $SourceElf) {
        Copy-Item $SourceElf -Destination (Join-Path $OutputDir "$BaseName.elf") -Force
        Write-Host "  Copied: $BaseName.elf"
    }

    # Display file sizes
    $Size = (Get-Item $DestFirmware).Length
    Write-Host "  Size:   $Size bytes ($([math]::Round($Size/1KB, 2)) KB)"

    return $true
}

# Process both revisions
Write-Host "Preparing firmware v$Version for release..."
Write-Host "="*50

$Success = $true

# Process rev2 (default)
if (-not (Process-Firmware -SourceDir $BuildDirRev2 -Revision "rev2" -Version $Version)) {
    $Success = $false
}

# Process rev1
if (-not (Process-Firmware -SourceDir $BuildDirRev1 -Revision "rev1" -Version $Version)) {
    $Success = $false
}

Write-Host "`n" + ("="*50)

if ($Success) {
    Write-Host "SUCCESS: All firmware files prepared in $OutputDir" -ForegroundColor Green
    Write-Host "`nReady to commit and create GitHub release v$Version"
} else {
    Write-Host "FAILED: Some firmware files could not be processed" -ForegroundColor Red
    exit 1
}

# List all files created
Write-Host "`nCreated files:"
Get-ChildItem -Path "$OutputDir\homekit-grgdo1-*-v$Version.*" |
    Select-Object Name, Length |
    Format-Table -AutoSize
