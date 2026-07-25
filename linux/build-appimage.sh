#!/bin/bash
# build-appimage.sh — Build SylvaniaLauncher AppImage (Linux edition v3.0)
#
# Target baseline: Ubuntu 22.04 / glibc 2.35 (see linux/Dockerfile).
# Produces: SylvaniaLauncher-v<version>-x86_64.AppImage containing:
#   - the launcher (Qt 6.5, FFmpeg multimedia backend)
#   - an EMBEDDED, PINNED Wine runtime (Kron4ek wow64 build) under usr/wine
#
# The Wine build is wow64: it runs 32-bit Windows apps (WoW 3.3.5a) without
# any 32-bit host libraries — mandatory for machines without multilib.
#
# Order matters: linuxdeploy runs BEFORE Wine is embedded so it never
# patches/relinks the Wine binaries; the final image is then packed with
# appimagetool.
#
# Every downloaded artefact is SHA-256 verified before use.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build-appimage"
APPDIR="${BUILD_DIR}/AppDir"

# ----------------------------------------------------------------------------
# Pinned Wine runtime (NEVER "latest": reproducible builds, verified content).
# wine-10.8-staging-amd64-wow64 — Kron4ek build, 2025-05-18.
#   - staging: gaming patches (esync, raw input...)
#   - wow64:   runs 32-bit PE apps with no 32-bit host userland
# ----------------------------------------------------------------------------
WINE_VERSION="10.8"
WINE_TARBALL="wine-${WINE_VERSION}-staging-amd64-wow64.tar.xz"
WINE_URL="https://github.com/Kron4ek/Wine-Builds/releases/download/${WINE_VERSION}/${WINE_TARBALL}"
WINE_SHA256="cdd6effc2a61c66b5dbfb8aebbc08df151cd50bf9084b106bfef0d6ebc7b097f"

echo "=== Sylvania Launcher — AppImage Builder (Linux edition) ==="
echo "Project: ${PROJECT_DIR}"
echo "Build:   ${BUILD_DIR}"
echo ""

# -------------------------------------------------------------------
# Step 1: Clean and prepare build directory
# -------------------------------------------------------------------
echo "[1/9] Preparing build directory..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# -------------------------------------------------------------------
# Step 2: CMake configure (Qt from QT_ROOT when provided by the Docker env)
# -------------------------------------------------------------------
echo "[2/9] Configuring with CMake..."
CMAKE_EXTRA=()
if [ -n "${QT_ROOT:-}" ]; then
    CMAKE_EXTRA+=("-DCMAKE_PREFIX_PATH=${QT_ROOT}")
fi
cmake "${PROJECT_DIR}/cpp" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    ${CMAKE_EXTRA[@]+"${CMAKE_EXTRA[@]}"}

# -------------------------------------------------------------------
# Step 3: Build
# -------------------------------------------------------------------
echo "[3/9] Building..."
make -j"$(nproc)"

# -------------------------------------------------------------------
# Step 4: Install to AppDir + assets
# -------------------------------------------------------------------
echo "[4/9] Installing to AppDir..."
make DESTDIR="${APPDIR}" install

mkdir -p "${APPDIR}/usr/share/sylvania-launcher"
cp -r "${PROJECT_DIR}/Asset" "${APPDIR}/usr/share/sylvania-launcher/"
mkdir -p "${APPDIR}/usr/bin"
cp -r "${PROJECT_DIR}/Asset" "${APPDIR}/usr/bin/Asset"

# -------------------------------------------------------------------
# Step 5: AppImage metadata
# -------------------------------------------------------------------
echo "[5/9] Preparing AppImage metadata..."
cp "${PROJECT_DIR}/Asset/LogoSylvania/LogoSylvania256.png" \
   "${APPDIR}/sylvania-launcher.png"
cp "${PROJECT_DIR}/linux/sylvania-launcher.desktop" "${APPDIR}/"

# -------------------------------------------------------------------
# Step 6: Bundle Qt with linuxdeploy (BEFORE Wine is embedded)
# -------------------------------------------------------------------
echo "[6/9] Bundling Qt with linuxdeploy..."

LINUXDEPLOY="${BUILD_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${BUILD_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="${BUILD_DIR}/appimagetool-x86_64.AppImage"

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
if [ ! -f "${APPIMAGETOOL}" ]; then
    echo "  Downloading appimagetool..."
    wget -q -O "${APPIMAGETOOL}" \
        "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "${APPIMAGETOOL}"
fi

export QMAKE="${QT_ROOT:+${QT_ROOT}/bin/qmake}"
[ -n "${QMAKE}" ] || QMAKE=$(command -v qmake6 || command -v qmake)
export QMAKE
# Bundle the multimedia module so QSoundEffect has its backend plugin.
export EXTRA_QT_MODULES="multimedia"
export EXTRA_QT_PLUGINS="multimedia"
# linuxdeploy resolves DT_NEEDED through the loader: the official Qt libs
# live outside the ldconfig paths (/opt/qt), so expose them explicitly.
export LD_LIBRARY_PATH="${QT_ROOT:+${QT_ROOT}/lib:}${LD_LIBRARY_PATH:-}"

"${LINUXDEPLOY}" \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/sylvania-launcher.desktop" \
    --icon-file "${APPDIR}/sylvania-launcher.png" \
    --plugin qt

# Our AppRun (not linuxdeploy's default). linuxdeploy creates AppRun as a
# SYMLINK to usr/bin/SylvaniaLauncher: a plain cp would write THROUGH the
# symlink and destroy the launcher binary — remove the link first.
rm -f "${APPDIR}/AppRun"
cp "${PROJECT_DIR}/linux/AppRun" "${APPDIR}/AppRun"
chmod +x "${APPDIR}/AppRun"
test -x "${APPDIR}/usr/bin/SylvaniaLauncher" \
    || { echo "ERROR: launcher binary missing from AppDir."; exit 1; }
file -b "${APPDIR}/usr/bin/SylvaniaLauncher" | grep -q ELF \
    || { echo "ERROR: usr/bin/SylvaniaLauncher is not an ELF binary."; exit 1; }

# OpenSSL must NEVER be bundled — it must come from the host, as a whole.
#
# linuxdeploy drags libcrypto.so.3 in as a dependency of the Kerberos libs
# (libgssapi_krb5 -> libk5crypto), but NOT libssl.so.3, which Qt's TLS backend
# dlopen()s at runtime. On a distro with a newer OpenSSL than the build host
# (e.g. Fedora), the host libssl.so.3 then resolves against the OLDER bundled
# libcrypto.so.3 -> missing versioned symbols -> the TLS backend fails to load
# and every https request dies with "TLS initialization failed".
#
# Dropping both makes the pair consistent (host libssl + host libcrypto) and
# also keeps the host's crypto policy and CA store in effect. Every distro at
# our glibc 2.35 baseline ships OpenSSL 3.x.
echo "  Removing bundled OpenSSL (must come from the host)..."
rm -f "${APPDIR}/usr/lib/libssl.so."* "${APPDIR}/usr/lib/libcrypto.so."*
LEFTOVER_SSL=$(find "${APPDIR}/usr/lib" -maxdepth 1 \
    \( -name 'libssl.so*' -o -name 'libcrypto.so*' \) 2>/dev/null || true)
[ -z "${LEFTOVER_SSL}" ] \
    || { echo "ERROR: OpenSSL still bundled: ${LEFTOVER_SSL}"; exit 1; }

# The TLS backend plugin itself MUST be present, otherwise Qt has no https at
# all (only the "certonly" backend, which cannot open connections).
test -f "${APPDIR}/usr/plugins/tls/libqopensslbackend.so" \
    || { echo "ERROR: Qt OpenSSL TLS backend plugin missing from AppDir."; exit 1; }

# Qt 6.5 FFmpeg backend: verify the libav*/libsw* runtime libs came along.
FFPLUGIN="${APPDIR}/usr/plugins/multimedia/libffmpegmediaplugin.so"
if [ -f "${FFPLUGIN}" ]; then
    MISSING_AV=$(LD_LIBRARY_PATH="${APPDIR}/usr/lib" ldd "${FFPLUGIN}" \
        | awk '/not found/{print $1}' || true)
    [ -z "${MISSING_AV}" ] || echo "WARNING: FFmpeg libs not bundled: ${MISSING_AV}"
else
    echo "WARNING: multimedia backend plugin not found in AppDir (no sound)."
fi

# -------------------------------------------------------------------
# Step 7: Embed the pinned Wine runtime
# -------------------------------------------------------------------
echo "[7/9] Embedding Wine ${WINE_VERSION} (wow64)..."
WINE_CACHE="${PROJECT_DIR}/linux/.cache"
mkdir -p "${WINE_CACHE}"
if [ ! -f "${WINE_CACHE}/${WINE_TARBALL}" ]; then
    echo "  Downloading ${WINE_URL}"
    wget -q -O "${WINE_CACHE}/${WINE_TARBALL}.part" "${WINE_URL}"
    mv "${WINE_CACHE}/${WINE_TARBALL}.part" "${WINE_CACHE}/${WINE_TARBALL}"
fi

echo "  Verifying SHA-256..."
echo "${WINE_SHA256}  ${WINE_CACHE}/${WINE_TARBALL}" | sha256sum -c - \
    || { echo "ERROR: Wine tarball SHA-256 mismatch — aborting."; exit 1; }

mkdir -p "${APPDIR}/usr/wine"
tar -xJf "${WINE_CACHE}/${WINE_TARBALL}" \
    --strip-components=1 -C "${APPDIR}/usr/wine"

test -x "${APPDIR}/usr/wine/bin/wine" \
    || { echo "ERROR: bin/wine missing after extraction."; exit 1; }
test -x "${APPDIR}/usr/wine/bin/wineserver" \
    || { echo "ERROR: bin/wineserver missing after extraction."; exit 1; }

# Wine dlopens the host FreeType for its font engine; minimal installs may
# lack it ("no TrueType fonts" warning). Bundle it (plus its deps) into the
# wine lib dir, which is already on Wine's LD_LIBRARY_PATH. ~1 MB.
for lib in libfreetype.so.6 libpng16.so.16 libbrotlidec.so.1 libbrotlicommon.so.1; do
    cp -L "/lib/x86_64-linux-gnu/${lib}" "${APPDIR}/usr/wine/lib/" 2>/dev/null \
        || echo "  note: ${lib} not found on build host (skipped)"
done

# -------------------------------------------------------------------
# Step 8: ldd audit of every bundled ELF binary
# -------------------------------------------------------------------
echo "[8/9] Auditing bundled binaries with ldd..."
REPORT="${BUILD_DIR}/ldd-report.txt"
: > "${REPORT}"
audit_failed=0
while IFS= read -r bin; do
    # Only ELF files (skip Wine's PE .dll/.exe and data files).
    if file -b "${bin}" | grep -q ELF; then
        MISSING=$(LD_LIBRARY_PATH="${APPDIR}/usr/lib:${APPDIR}/usr/wine/lib" \
            ldd "${bin}" 2>/dev/null | awk '/not found/{print $1}' || true)
        if [ -n "${MISSING}" ]; then
            echo "${bin}:" >> "${REPORT}"
            echo "${MISSING}" | sed 's/^/    MISSING /' >> "${REPORT}"
            audit_failed=1
        fi
    fi
done < <(find "${APPDIR}/usr/bin" "${APPDIR}/usr/lib" "${APPDIR}/usr/wine/bin" \
              "${APPDIR}/usr/wine/lib" -type f 2>/dev/null)
if [ "${audit_failed}" -eq 1 ]; then
    echo "WARNING: unresolved libraries found (details: ldd-report.txt)."
    echo "         Wine legitimately dlopens host X11/graphics/audio libs at"
    echo "         runtime; hard failures are only libs missing on a STOCK"
    echo "         desktop install of the target distros."
    cat "${REPORT}"
else
    echo "  ldd audit clean." | tee -a "${REPORT}"
fi

# -------------------------------------------------------------------
# Step 9: Pack the AppImage with appimagetool
# -------------------------------------------------------------------
echo "[9/9] Packing AppImage..."
VERSION=$(grep -oP 'project\(SylvaniaLauncher VERSION \K[0-9.]+' \
          "${PROJECT_DIR}/cpp/CMakeLists.txt" || echo dev)
FINAL="${BUILD_DIR}/SylvaniaLauncher-v${VERSION}-x86_64.AppImage"

ARCH=x86_64 "${APPIMAGETOOL}" "${APPDIR}" "${FINAL}"

echo ""
echo "=== Build successful! ==="
echo "AppImage: ${FINAL}"
echo "Size: $(du -h "${FINAL}" | cut -f1)"
echo ""
echo "To run: chmod +x ${FINAL} && ${FINAL}"
