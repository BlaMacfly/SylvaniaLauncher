#!/bin/bash
# build-appimage.sh — Build SylvaniaLauncher AppImage
#
# Target: Ubuntu 20.04 (glibc 2.31) for maximum compatibility
# Produces: SylvaniaLauncher-x86_64.AppImage
#
# Prerequisites:
#   sudo apt install qt6-base-dev qt6-multimedia-dev qt6-tools-dev \
#                    qt6-l10n-tools libzip-dev cmake g++ libgl-dev \
#                    libxkbcommon-dev libgstreamer-plugins-base1.0-dev \
#                    file

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/AppDir"

echo "=== Sylvania Launcher — AppImage Builder ==="
echo "Project: ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"
echo ""

# -------------------------------------------------------------------
# Step 1: Clean and prepare build directory
# -------------------------------------------------------------------
echo "[1/6] Preparing build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# -------------------------------------------------------------------
# Step 2: CMake configure
# -------------------------------------------------------------------
echo "[2/6] Configuring with CMake..."
cmake "${PROJECT_DIR}/cpp" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release

# -------------------------------------------------------------------
# Step 3: Build
# -------------------------------------------------------------------
echo "[3/6] Building..."
make -j"$(nproc)"

# -------------------------------------------------------------------
# Step 4: Install to AppDir
# -------------------------------------------------------------------
echo "[4/6] Installing to AppDir..."
make DESTDIR="${APPDIR}" install

# Ensure Asset directory is present
mkdir -p "${APPDIR}/usr/share/sylvania-launcher"
cp -r "${PROJECT_DIR}/Asset" "${APPDIR}/usr/share/sylvania-launcher/"

# Also copy assets next to binary (fallback path for PathUtils)
mkdir -p "${APPDIR}/usr/bin"
cp -r "${PROJECT_DIR}/Asset" "${APPDIR}/usr/bin/Asset"

# -------------------------------------------------------------------
# Step 5: Prepare AppImage metadata
# -------------------------------------------------------------------
echo "[5/6] Preparing AppImage metadata..."

# Copy icon
cp "${PROJECT_DIR}/Asset/LogoSylvania/LogoSylvania256.png" \
   "${APPDIR}/sylvania-launcher.png"

# Copy .desktop file
cp "${PROJECT_DIR}/linux/sylvania-launcher.desktop" "${APPDIR}/"

# Copy AppRun
cp "${PROJECT_DIR}/linux/AppRun" "${APPDIR}/"
chmod +x "${APPDIR}/AppRun"

# -------------------------------------------------------------------
# Step 6: Create AppImage with linuxdeploy
# -------------------------------------------------------------------
echo "[6/6] Creating AppImage..."

# Download linuxdeploy if not present
LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "${LINUXDEPLOY}" ]; then
    echo "  Downloading linuxdeploy..."
    wget -q -O "${LINUXDEPLOY}" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY}"
fi

if [ ! -f "${LINUXDEPLOY_QT}" ]; then
    echo "  Downloading linuxdeploy Qt plugin..."
    wget -q -O "${LINUXDEPLOY_QT}" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "${LINUXDEPLOY_QT}"
fi

# Run linuxdeploy to bundle all dependencies
export QMAKE=$(which qmake6 2>/dev/null || which qmake 2>/dev/null)
export EXTRA_QT_PLUGINS="multimedia"

"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/sylvania-launcher.desktop" \
    --icon-file "${APPDIR}/sylvania-launcher.png" \
    --plugin qt \
    --output appimage

# -------------------------------------------------------------------
# Done!
# -------------------------------------------------------------------
APPIMAGE_FILE=$(ls -1 "${BUILD_DIR}"/Sylvania_Launcher*.AppImage 2>/dev/null || ls -1 "${BUILD_DIR}"/*.AppImage 2>/dev/null | head -1)

if [ -n "${APPIMAGE_FILE}" ]; then
    echo ""
    echo "=== Build successful! ==="
    echo "AppImage: ${APPIMAGE_FILE}"
    echo "Size: $(du -h "${APPIMAGE_FILE}" | cut -f1)"
    echo ""
    echo "To run: chmod +x ${APPIMAGE_FILE} && ./${APPIMAGE_FILE}"
else
    echo "ERROR: AppImage not found in build directory"
    exit 1
fi
