FROM ubuntu:24.04

# Prevent interactive prompts during apt-get install
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    ninja-build \
    gcc \
    g++ \
    clang \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    dos2unix \
    && rm -rf /var/lib/apt/lists/*

# Fix CRLF from the generated script before it executes
ENTRYPOINT ["/bin/bash", "-c", "if [ -f /workspace_src/.run_ubuntu_tests.sh ]; then dos2unix /workspace_src/.run_ubuntu_tests.sh; fi && exec \"$@\"", "--"]
