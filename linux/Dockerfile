# Build environment for the SylvaniaLauncher AppImage.
#
# Ubuntu 24.04 = Qt 6.4 + glibc 2.39. We need 24.04 (not 22.04) because its
# libqt6multimedia6 actually ships a working multimedia BACKEND plugin
# (plugins/multimedia/libgstreamermediaplugin.so + libffmpegmediaplugin.so);
# 22.04's Qt 6.2 ships the library with no backend, so QSoundEffect is silent.
#
# Build the image:
#   docker build -t sylvania-appimage -f linux/Dockerfile linux
#
# Produce the AppImage (run from the repo root; outputs to ./build-appimage):
#   docker run --rm -v "${PWD}:/src" sylvania-appimage
#
# linuxdeploy / appimagetool are themselves AppImages and would need FUSE, which
# is not available in a container, so APPIMAGE_EXTRACT_AND_RUN=1 makes them
# self-extract instead of mounting (no libfuse needed at all).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        g++ \
        make \
        pkg-config \
        file \
        wget \
        ca-certificates \
        desktop-file-utils \
        qt6-base-dev \
        qt6-multimedia-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        qt6-l10n-tools \
        libzip-dev \
        libgl-dev \
        libxkbcommon-dev \
        libgstreamer-plugins-base1.0-dev \
        gstreamer1.0-plugins-good \
        zsync \
    && rm -rf /var/lib/apt/lists/*

# Build out-of-tree from a copy so the mounted /src (Windows host) stays clean
# and the build runs on the container's native filesystem (fast).
WORKDIR /work
CMD set -e; \
    rm -rf /work/repo && cp -r /src /work/repo && cd /work/repo && \
    bash linux/build-appimage.sh && \
    mkdir -p /src/build-appimage && \
    cp /work/repo/build-appimage/*.AppImage /src/build-appimage/ && \
    echo "AppImage copied to ./build-appimage/ on the host."
