# Build environment for the SylvaniaLauncher AppImage.
#
# Base: Ubuntu 22.04 -> the AppImage requires glibc >= 2.35 (Ubuntu 22.04+,
# Debian 12+, Fedora 36+). Qt comes from aqtinstall (Qt 6.5.3 LTS official
# binaries) because Ubuntu 22.04's distro Qt 6.2 ships qtmultimedia WITHOUT a
# working backend plugin (QSoundEffect stays silent); the official 6.5
# binaries ship the FFmpeg multimedia backend and its libav* libraries.
#
# Build the image:
#   docker build -t sylvania-appimage -f linux/Dockerfile linux
#
# Produce the AppImage (run from the repo root; outputs to ./build-appimage):
#   docker run --rm -v "${PWD}:/src" sylvania-appimage
#
# linuxdeploy / appimagetool are themselves AppImages and would need FUSE,
# which is not available in a container, so APPIMAGE_EXTRACT_AND_RUN=1 makes
# them self-extract instead of mounting.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        g++ \
        make \
        pkg-config \
        file \
        wget \
        xz-utils \
        ca-certificates \
        desktop-file-utils \
        patchelf \
        python3-pip \
        libzip-dev \
        libgl-dev \
        libglu1-mesa-dev \
        libopengl0 \
        libxkbcommon-dev \
        libxkbcommon-x11-0 \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-randr0 \
        libxcb-render-util0 \
        libxcb-shape0 \
        libxcb-xinerama0 \
        libfontconfig1 \
        libfreetype6 \
        zsync \
    && rm -rf /var/lib/apt/lists/*

# Official Qt 6.5.3 binaries: base + multimedia (FFmpeg backend) +
# imageformats + linguist tools. aqtinstall is pinned for reproducibility.
RUN pip3 install --no-cache-dir "aqtinstall==3.1.18" \
    && aqt install-qt linux desktop 6.5.3 gcc_64 \
        -m qtmultimedia qtimageformats \
        -O /opt/qt

ENV QT_ROOT=/opt/qt/6.5.3/gcc_64
ENV PATH="/opt/qt/6.5.3/gcc_64/bin:${PATH}"

# Build out-of-tree from a copy so the mounted /src (Windows host) stays clean
# and the build runs on the container's native filesystem (fast). The Wine
# tarball cache is copied back so later builds skip the download.
WORKDIR /work
CMD set -e; \
    rm -rf /work/repo && cp -r /src /work/repo && cd /work/repo && \
    bash linux/build-appimage.sh && \
    mkdir -p /src/build-appimage /src/linux/.cache && \
    cp /work/repo/build-appimage/*.AppImage /src/build-appimage/ && \
    cp -n /work/repo/linux/.cache/* /src/linux/.cache/ 2>/dev/null || true; \
    cp /work/repo/build-appimage/ldd-report.txt /src/build-appimage/ 2>/dev/null || true; \
    echo "AppImage copied to ./build-appimage/ on the host."
