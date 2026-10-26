#!/usr/bin/env bash
# Extracts libvorbis compiled to WebAssembly from the @wasm-audio-decoders/ogg-vorbis npm package into WebRuntime's
# resources. The package ships the module compressed inside a JavaScript file. Only the module is kept: the package's
# Ogg parser (codec-parser) is LGPL, and ogg-vorbis.js has its own.
#   scripts/update-vorbis-wasm.sh [version]
set -euo pipefail
version=${1:-0.1.20}
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
npm init -y >/dev/null
npm install --ignore-scripts --no-audit --no-fund "@wasm-audio-decoders/ogg-vorbis@$version" >/dev/null
cat > extract.mjs <<'JS'
import { writeFileSync } from "node:fs";
import { WASMAudioDecoderCommon } from "@wasm-audio-decoders/common";
const { default: EmscriptenWASM } = await import(new URL("./node_modules/@wasm-audio-decoders/ogg-vorbis/src/EmscriptenWasm.js", import.meta.url));
new WASMAudioDecoderCommon();
new EmscriptenWASM(WASMAudioDecoderCommon);
writeFileSync(process.argv[2], await WASMAudioDecoderCommon.inflateDynEncodeString(EmscriptenWASM.wasm));
JS
out="$root/Packages/VNDegenKit/Sources/WebRuntime/Resources/vorbis.wasm"
node extract.mjs "$out"
sha256sum "$out"
echo "Update the version and SHA-256 in Packages/VNDegenKit/Sources/WebRuntime/vorbis.wasm.LICENSE."
