#!/bin/bash
# Runs inside ubuntu:24.04 with the mkxp-z libretro checkout at /src. Builds stage 1 as the branch's CI does.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq git curl build-essential automake libtool zip universal-ctags ruby ca-certificates >/dev/null
mkdir -p /opt/wasi-sdk /opt/binaryen
curl -sSfL https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-30/wasi-sdk-30.0-x86_64-linux.tar.gz | tar -xz -C /opt/wasi-sdk --strip-components=1
curl -sSfL https://github.com/WebAssembly/binaryen/releases/download/version_126/binaryen-version_126-x86_64-linux.tar.gz | tar -xz -C /opt/binaryen --strip-components=1
git config --global --add safe.directory '*'
cd /src/libretro
make -j "$(nproc)" WASI_SDK=/opt/wasi-sdk WASM_OPT=/opt/binaryen/bin/wasm-opt
