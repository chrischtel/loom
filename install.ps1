# Loom Programming Language Installer for Windows
# Usage: iwr -useb https://raw.githubusercontent.com/chrischtel/loom/main/install.ps1 | iex

param(
    [switch]$Stable,
    [string]$Version,
    [switch]$Help
)

# Configuration
$REPO = "chrischtel/loom"
$INSTALL_DIR = if ($env:LOOM_INSTALL_DIR) { $env:LOOM_INSTALL_DIR } else { "$env:USERPROFILE\.loom" }
$BIN_DIR = if ($env:LOOM_BIN_DIR) { $env:LOOM_BIN_DIR } else { "$env:USERPROFILE\.local\bin" }

# Helper functions
function Write-Info {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Show help
if ($Help) {
    Write-Host "Loom Programming Language Installer for Windows"
    Write-Host ""
    Write-Host "Usage: install.ps1 [OPTIONS]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Stable     Install the latest stable release"
    Write-Host "  -Version    Install specific version (e.g., 'v0.1.0-alpha.3')"
    Write-Host "  -Help       Show this help message"
    Write-Host ""
    Write-Host "Environment variables:"
    Write-Host "  LOOM_INSTALL_DIR    Installation directory (default: ~/.loom)"
    Write-Host "  LOOM_BIN_DIR        Binary directory (default: ~/.local/bin)"
    exit 0
}

# Detect architecture
function Get-Architecture {
    $arch = $env:PROCESSOR_ARCHITECTURE
    switch ($arch) {
        "AMD64" { return "x64" }
        "ARM64" { return "arm64" }
        default { return "unknown" }
    }
}

# Get latest release
function Get-LatestRelease {
    param([bool]$UseStable = $false)
    
    try {
        if ($UseStable) {
            return "stable"
        } else {
            $response = Invoke-RestMethod -Uri "https://api.github.com/repos/$REPO/releases/latest"
            return $response.tag_name
        }
    } catch {
        Write-Error "Failed to get latest release information"
        exit 1
    }
}

# Download and install
function Install-Loom {
    param(
        [string]$Tag,
        [bool]$UseStable = $false
    )
    
    $arch = Get-Architecture
    if ($arch -eq "unknown") {
        Write-Error "Unsupported architecture: $env:PROCESSOR_ARCHITECTURE"
        exit 1
    }
    
    $platform = "windows-$arch"
    $filename = "loom-$platform.zip"
    
    $downloadUrl = if ($UseStable) {
        "https://github.com/$REPO/releases/download/stable/$filename"
    } else {
        "https://github.com/$REPO/releases/download/$Tag/$filename"
    }
    
    Write-Info "Downloading $filename..."
    
    # Create temporary file
    $tempFile = [System.IO.Path]::GetTempFileName()
    $tempZip = $tempFile -replace '\.tmp$', '.zip'
    
    try {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $tempZip -UseBasicParsing
    } catch {
        Write-Error "Failed to download from $downloadUrl"
        Write-Error $_.Exception.Message
        exit 1
    }
    
    # Create install directories
    if (!(Test-Path $INSTALL_DIR)) {
        New-Item -ItemType Directory -Path $INSTALL_DIR -Force | Out-Null
    }
    if (!(Test-Path $BIN_DIR)) {
        New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null
    }
    
    # Extract
    Write-Info "Extracting to $INSTALL_DIR..."
    
    try {
        Expand-Archive -Path $tempZip -DestinationPath $INSTALL_DIR -Force
    } catch {
        Write-Error "Failed to extract archive"
        Write-Error $_.Exception.Message
        exit 1
    }
    
    # Copy to bin directory
    $exePath = Join-Path $INSTALL_DIR "loom.exe"
    $binPath = Join-Path $BIN_DIR "loom.exe"
    
    if (Test-Path $exePath) {
        Copy-Item -Path $exePath -Destination $binPath -Force
        Write-Success "Loom installed to $INSTALL_DIR"
    } else {
        Write-Error "Executable not found after extraction"
        exit 1
    }
    
    # Cleanup
    Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
    Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
}

# Check PATH
function Test-PathConfiguration {
    $currentPath = $env:PATH
    if ($currentPath -notlike "*$BIN_DIR*") {
        Write-Warning "Add $BIN_DIR to your PATH to use 'loom' command globally"
        Write-Host "You can add it permanently by running:"
        Write-Host "  `$env:PATH += ';$BIN_DIR'"
        Write-Host "Or add it to your PowerShell profile."
    }
}

# Main execution
Write-Info "Installing Loom Programming Language..."

$arch = Get-Architecture
Write-Info "Detected architecture: $arch"

# Get release tag
if ($Version) {
    $tag = $Version
} else {
    $tag = Get-LatestRelease -UseStable $Stable
}

Write-Info "Installing version: $tag"

# Install
Install-Loom -Tag $tag -UseStable $Stable

# Check PATH
Test-PathConfiguration

# Test installation
$loomPath = Join-Path $BIN_DIR "loom.exe"
if (Test-Path $loomPath) {
    Write-Success "Installation completed successfully!"
    Write-Info "Testing installation..."
    try {
        & $loomPath version
    } catch {
        Write-Error "Failed to run loom executable"
        exit 1
    }
} else {
    Write-Error "Installation failed - executable not found"
    exit 1
}
