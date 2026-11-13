#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"

identity="$(python3 scripts/release_identity.py --allow-untagged)"
IFS=$'\t' read -r version source_revision <<< "$identity"
if [[ -n "${1:-}" && "${1#v}" != "$version" ]]; then
    echo "requested source archive version '${1#v}' does not match resolved Frontend version '$version'" >&2
    exit 2
fi
mkdir -p dist
archive="dist/cdin-frontend-${version}-source.tar.gz"

tar \
    --exclude='./.git' \
    --exclude='./.deps' \
    --exclude='./build' \
    --exclude='./build-*' \
    --exclude='./dist' \
    --exclude='./coverage' \
    --exclude='./.pipeline' \
    --exclude='./.psx' \
    -czf "$archive" .
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$archive" > "$archive.sha256"
elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$archive" > "$archive.sha256"
else
    echo "sha256sum or shasum is required to create release checksums" >&2
    exit 2
fi
echo "Created $archive"
