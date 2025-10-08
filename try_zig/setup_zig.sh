#!/bin/bash
# Source this script to add Zig to PATH (downloads on-demand if needed)
# Usage: source scripts/export_zig.sh [version]

VERSION=${1:-0.15.1}
TARGET_DIR="$PWD/local_data/zig"

if [ ! -f "${TARGET_DIR}/zig" ]; then
    echo "Downloading Zig ${VERSION}..."

    ARCH=$(uname -m)
    case "${ARCH}" in
        x86_64) PLATFORM="linux-x86_64" ;;
        aarch64|arm64) PLATFORM="aarch64-linux" ;;
        *) echo "Error: Unsupported architecture: ${ARCH}"; return 1 2>/dev/null || exit 1 ;;
    esac

    TARBALL="zig-${PLATFORM}-${VERSION}.tar.xz"
    URL="https://ziglang.org/download/${VERSION}/${TARBALL}"

    mkdir -p "${TARGET_DIR}"
    curl -L "${URL}" -o "${TARGET_DIR}/${TARBALL}" || return 1 2>/dev/null || exit 1

    cd "${TARGET_DIR}" && tar -xf "${TARBALL}" && mv zig-${PLATFORM}-${VERSION}/* . && rmdir zig-${PLATFORM}-${VERSION} && rm "${TARBALL}"
    cd - > /dev/null

    [ -f "${TARGET_DIR}/zig" ] && echo "Zig ${VERSION} installed" || return 1 2>/dev/null || exit 1
fi

export PATH="${TARGET_DIR}:$PATH"
