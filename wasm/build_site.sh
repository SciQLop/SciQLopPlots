#!/bin/bash
# Assemble the browser library, its gallery and its tests from a wasm build.
# Usage: wasm/build_site.sh <wasm-builddir> <outdir>
#   <outdir>/                 the library: sciqlopplots.mjs, .js, .wasm, qtloader.js, coi-serviceworker.js
#   <outdir>/index.html       the gallery, <outdir>/gallery/*.html its examples
#   <outdir>/tests/*.html     pages wasm/tests/run.mjs drives in Firefox
set -euo pipefail

builddir="$1"
outdir="$2"
here="$(dirname "$0")"

coi_version=0.1.7
coi_sha256=03d2f35be3249f5eeb4e214c5daf2be8067e2b24630aaef26a1247b723f2d05d

rm -rf "$outdir"
mkdir -p "$outdir"
cp "$builddir"/wasm/sciqlopplots.{js,wasm} "$builddir/wasm/qtloader.js" "$here/sciqlopplots.mjs" "$outdir/"
cp -r "$here/gallery" "$here/tests" "$outdir/"
mv "$outdir/gallery/index.html" "$outdir/index.html"

curl -sfL "https://cdn.jsdelivr.net/npm/coi-serviceworker@${coi_version}/coi-serviceworker.min.js" \
    -o "$outdir/coi-serviceworker.js"
echo "${coi_sha256}  $outdir/coi-serviceworker.js" | sha256sum -c --quiet

echo "Site assembled in $outdir/"
