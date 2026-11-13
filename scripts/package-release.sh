#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"

version="${1:-${PIPELINE_RELEASE_TAG:-${GITHUB_REF_NAME:-}}}"
version="${version#v}"
if [[ -z "$version" ]]; then
    version="$(sed -n 's/^project(cdin_frontend VERSION \([^ ]*\) LANGUAGES C)$/\1/p' CMakeLists.txt)"
fi
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+([+-][0-9A-Za-z.-]+)?$ ]]; then
    echo "invalid release version: ${version:-<empty>}" >&2
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
