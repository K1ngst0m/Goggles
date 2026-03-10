FROM ghcr.io/prefix-dev/pixi:0.63.2

USER root

RUN apt-get update -o Acquire::Retries=3 \
    && apt-get install -y --no-install-recommends \
        bash \
        binutils \
        ca-certificates \
        git \
        libdrm2 \
        libegl1 \
        libgbm1 \
        libgles2 \
        libvulkan1 \
        mesa-vulkan-drivers \
        xwayland \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
