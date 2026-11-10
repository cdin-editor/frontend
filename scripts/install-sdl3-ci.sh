#!/usr/bin/env bash
# Build the pinned SDL3 SDK in component CI only. Consumer installation uses
# the separately published target-native SDK assets from CDIN #124.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
version="3.4.16"
sha256="7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68"
prefix="${SDL3_PREFIX:-$repo_root/.deps/sdl3}"
work="${RUNNER_TEMP:-$repo_root/build}/frontend-sdl3-ci"
archive="$work/SDL3-$version.tar.gz"
source_dir="$work/source"
build_dir="$work/build"

mkdir -p "$work"
if [[ -d "$prefix" && -f "$prefix/lib/cmake/SDL3/SDL3Config.cmake" ]]; then
    echo "Using existing SDL3 $version SDK at $prefix"
    exit 0
fi

command -v curl >/dev/null 2>&1 || { echo "curl is required to prepare SDL3 in CI" >&2; exit 2; }
curl --fail --location --retry 3 \
  "https://github.com/libsdl-org/SDL/releases/download/release-$version/SDL3-$version.tar.gz" \
  --output "$archive"
echo "$sha256  $archive" | sha256sum --check

rm -rf "$source_dir" "$build_dir" "$prefix"
mkdir -p "$source_dir" "$prefix"
python3 - "$archive" "$source_dir" <<'PY'
import pathlib
import sys
import tarfile

archive = pathlib.Path(sys.argv[1])
destination = pathlib.Path(sys.argv[2]).resolve()
with tarfile.open(archive, "r:gz") as source:
    members = source.getmembers()
    if not members or len(members) > 100000:
        raise SystemExit("invalid SDL3 source archive member count")
    for member in members:
        path = pathlib.PurePosixPath(member.name)
        if path.is_absolute() or not path.parts or any(part in ("", ".", "..") for part in path.parts):
            raise SystemExit(f"unsafe SDL3 source path: {member.name!r}")
        if not (member.isfile() or member.isdir()):
            raise SystemExit(f"unsupported SDL3 archive entry: {member.name!r}")
        target = (destination / pathlib.Path(*path.parts)).resolve()
        if destination not in target.parents and target != destination:
            raise SystemExit(f"SDL3 archive path escapes extraction root: {member.name!r}")
        source.extract(member, destination, filter="data")
roots = [path for path in destination.iterdir() if path.is_dir()]
if len(roots) != 1 or not (roots[0] / "CMakeLists.txt").is_file():
    raise SystemExit("SDL3 source archive does not contain one CMake project root")
( destination / ".source-root" ).write_text(str(roots[0]), encoding="utf-8")
PY
actual_source="$(cat "$source_dir/.source-root")"
cmake -S "$actual_source" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DSDL_SHARED=OFF -DSDL_STATIC=ON \
  -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
cmake --build "$build_dir" --parallel
cmake --install "$build_dir"
test -f "$prefix/lib/cmake/SDL3/SDL3Config.cmake"
test -f "$prefix/include/SDL3/SDL.h"
echo "Prepared pinned SDL3 $version SDK for Frontend release CI: $prefix"
