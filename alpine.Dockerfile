FROM alpine:latest

RUN apk update && apk add --no-cache \
    build-base \
    cmake \
    git \
    ninja \
    gcc \
    g++ \
    clang \
    curl \
    zip \
    unzip \
    tar \
    pkgconfig \
    linux-headers \
    bash \
    dos2unix

RUN ln -sf /usr/bin/ar /usr/bin/gcc-ar && ln -sf /usr/bin/ranlib /usr/bin/gcc-ranlib

ENTRYPOINT ["/bin/sh", "-c", "if [ -f /workspace_src/.run_alpine_tests.sh ]; then dos2unix /workspace_src/.run_alpine_tests.sh; fi && exec \"$@\"", "--"]
