# Build environment for the SylvaniaLauncher AppImage.
#
# Ubuntu 22.04 = Qt 6.2 + glibc 2.35: broad compatibility while still
# providing Qt6 from the distro repos (20.04 only ships Qt5).
#
# Build the image:
#   docker build -t sylvania-appimage -f linux/Dockerfile linux
#
# Produce the AppImage (run from the repo root; outputs to ./build-appimage):
#   docker run --rm -v "${PWD}:/src" sylvania-appimage
#
# linuxdeploy / appimagetool are themselves AppImages and need FUSE, which is
# not available in a container, so APPIMAGE_EXTRACT_AND_RUN=1 makes them
# self-extract instead of mounting.
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
        libfuse2 \
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
