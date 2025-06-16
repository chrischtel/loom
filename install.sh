#!/bin/bash
# Loom Programming Language Installer
# Usage: curl -sSL https://raw.githubusercontent.com/chrischtel/loom/main/install.sh | bash

set -e

# Configuration
REPO="chrischtel/loom"
INSTALL_DIR="${LOOM_INSTALL_DIR:-$HOME/.loom}"
BIN_DIR="${LOOM_BIN_DIR:-$HOME/.local/bin}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
RESET='\033[0m'

# Helper functions
print_info() {
    echo -e "${BLUE}[INFO]${RESET} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${RESET} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${RESET} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${RESET} $1"
}

# Detect platform
detect_platform() {
    local os
    local arch
    
    case "$(uname -s)" in
        Linux*)     os="linux" ;;
        Darwin*)    os="macos" ;;
        CYGWIN*|MINGW*|MSYS*) os="windows" ;;
        *)          os="unknown" ;;
    esac
    
    case "$(uname -m)" in
        x86_64|amd64)   arch="x64" ;;
        arm64|aarch64)  arch="arm64" ;;
        *)              arch="unknown" ;;
    esac
    
    echo "${os}-${arch}"
}

# Get latest release info
get_latest_release() {
    local use_stable=${1:-false}
    local tag_name
    
    if [[ "$use_stable" == "true" ]]; then
        tag_name="stable"
    else
        # Get the latest actual release (not pre-release)
        tag_name=$(curl -s "https://api.github.com/repos/${REPO}/releases/latest" | grep '"tag_name"' | cut -d'"' -f4)
    fi
    
    if [[ -z "$tag_name" ]]; then
        print_error "Failed to get latest release information"
        exit 1
    fi
    
    echo "$tag_name"
}

# Download and extract
download_and_install() {
    local platform="$1"
    local tag="$2"
    local use_stable="$3"
    
    local filename
    local download_url
    
    # Determine filename and URL
    if [[ "$platform" == "windows"* ]]; then
        filename="loom-${platform}.zip"
    else
        filename="loom-${platform}.tar.gz"
    fi
    
    if [[ "$use_stable" == "true" ]]; then
        download_url="https://github.com/${REPO}/releases/download/stable/${filename}"
    else
        download_url="https://github.com/${REPO}/releases/download/${tag}/${filename}"
    fi
    
    print_info "Downloading ${filename}..."
    
    # Create temporary directory
    local temp_dir
    temp_dir=$(mktemp -d)
    
    # Download
    if command -v curl > /dev/null 2>&1; then
        curl -fsSL "$download_url" -o "${temp_dir}/${filename}"
    elif command -v wget > /dev/null 2>&1; then
        wget -q "$download_url" -O "${temp_dir}/${filename}"
    else
        print_error "Neither curl nor wget is available"
        exit 1
    fi
    
    # Create install directory
    mkdir -p "$INSTALL_DIR"
    mkdir -p "$BIN_DIR"
    
    # Extract
    print_info "Extracting to ${INSTALL_DIR}..."
    
    if [[ "$filename" == *.zip ]]; then
        if command -v unzip > /dev/null 2>&1; then
            unzip -q "${temp_dir}/${filename}" -d "$INSTALL_DIR"
        else
            print_error "unzip is not available"
            exit 1
        fi
    else
        tar -xzf "${temp_dir}/${filename}" -C "$INSTALL_DIR"
    fi
    
    # Make executable and create symlink
    local executable_name="loom"
    if [[ "$platform" == "windows"* ]]; then
        executable_name="loom.exe"
    fi
    
    chmod +x "${INSTALL_DIR}/${executable_name}"
    
    # Create symlink in bin directory
    if [[ "$platform" != "windows"* ]]; then
        ln -sf "${INSTALL_DIR}/${executable_name}" "${BIN_DIR}/loom"
    fi
    
    # Cleanup
    rm -rf "$temp_dir"
    
    print_success "Loom installed to ${INSTALL_DIR}"
}

# Check if PATH includes bin directory
check_path() {
    if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
        print_warning "Add ${BIN_DIR} to your PATH to use 'loom' command globally"
        echo "Add this line to your shell profile (~/.bashrc, ~/.zshrc, etc.):"
        echo "  export PATH=\"\$PATH:${BIN_DIR}\""
    fi
}

# Main installation function
main() {
    local use_stable=false
    local tag
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --stable)
                use_stable=true
                shift
                ;;
            --version)
                tag="$2"
                shift 2
                ;;
            --help)
                echo "Loom Programming Language Installer"
                echo ""
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --stable    Install the latest stable release"
                echo "  --version   Install specific version (e.g., v0.1.0-alpha.3)"
                echo "  --help      Show this help message"
                echo ""
                echo "Environment variables:"
                echo "  LOOM_INSTALL_DIR    Installation directory (default: ~/.loom)"
                echo "  LOOM_BIN_DIR        Binary directory (default: ~/.local/bin)"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    print_info "Installing Loom Programming Language..."
    
    # Detect platform
    local platform
    platform=$(detect_platform)
    
    if [[ "$platform" == *"unknown"* ]]; then
        print_error "Unsupported platform: $(uname -s) $(uname -m)"
        exit 1
    fi
    
    print_info "Detected platform: $platform"
    
    # Get release tag
    if [[ -z "$tag" ]]; then
        tag=$(get_latest_release "$use_stable")
    fi
    
    print_info "Installing version: $tag"
    
    # Download and install
    download_and_install "$platform" "$tag" "$use_stable"
    
    # Check PATH
    check_path
    
    # Test installation
    local loom_path
    if [[ -f "${BIN_DIR}/loom" ]]; then
        loom_path="${BIN_DIR}/loom"
    else
        loom_path="${INSTALL_DIR}/loom"
    fi
    
    if [[ -x "$loom_path" ]]; then
        print_success "Installation completed successfully!"
        print_info "Testing installation..."
        "$loom_path" version
    else
        print_error "Installation failed - executable not found or not executable"
        exit 1
    fi
}

# Run main function
main "$@"
