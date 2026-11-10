#!/usr/bin/env bash
# Build the three immutable Frontend package variants for one native runner.
# Every package is installed by CMake's declared component boundary; the
# adapter archives intentionally do not bundle Core or SDL3.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"

tag="${PIPELINE_RELEASE_TAG:-${GITHUB_REF_NAME:-}}"
version="$(sed -n 's/^project(cdin_frontend VERSION \([^ ]*\) LANGUAGES C)$/\1/p' CMakeLists.txt)"
version="${version:-}"
[[ -n "$version" ]] || { echo "Could not determine Frontend package version" >&2; exit 2; }
[[ "$tag" == "v$version" ]] || {
  echo "Release tag '$tag' does not match Frontend version '$version'." >&2
  exit 1
}
source_revision="$(git rev-parse --verify HEAD)"
tag_revision="$(git rev-parse --verify "refs/tags/$tag^{commit}")"
[[ "$source_revision" == "$tag_revision" ]] || {
  echo "Release tag $tag resolves to $tag_revision, but package source HEAD is $source_revision." >&2
  exit 1
}
git diff --quiet && git diff --cached --quiet || {
  echo "Refusing to package a dirty Frontend source checkout." >&2
  exit 1
}

target_os="${FRONTEND_TARGET_OS:-}"
target_arch="${FRONTEND_TARGET_ARCH:-}"
target_abi="${FRONTEND_TARGET_ABI:-}"
target_runner="${FRONTEND_TARGET_RUNNER:-${RUNNER_OS:-local}-${RUNNER_ARCH:-$(uname -m)}}"
[[ -n "$target_os" && -n "$target_arch" && -n "$target_abi" ]] || {
  echo "Set FRONTEND_TARGET_OS, FRONTEND_TARGET_ARCH and FRONTEND_TARGET_ABI explicitly." >&2
  exit 2
}

host_arch="$(uname -m)"
case "$host_arch" in
  x86_64|amd64|AMD64) host_arch=x86_64 ;;
  aarch64|arm64) host_arch=arm64 ;;
  *) echo "Unsupported native build architecture: $host_arch" >&2; exit 2 ;;
esac
[[ "$host_arch" == "$target_arch" ]] || {
  echo "Refusing cross-build: runner architecture $host_arch != requested $target_arch." >&2
  exit 1
}

cmake_extra=()
case "$(uname -s)" in
  Linux)
    [[ "$target_os" == linux ]] || { echo "Linux runner cannot produce $target_os." >&2; exit 1; }
    glibc_version="$(getconf GNU_LIBC_VERSION | awk '{print $2}')"
    [[ "$glibc_version" == 2.39 && "$target_abi" == gnu-glibc-2.39 ]] || {
      echo "Expected Ubuntu 24.04 / glibc 2.39, found $glibc_version / $target_abi." >&2
      exit 1
    }
    ;;
  Darwin)
    [[ "$target_os" == darwin && "$target_abi" == macos-15 ]] || {
      echo "macOS package ABI must be macos-15." >&2
      exit 1
    }
    macos_major="$(sw_vers -productVersion | cut -d. -f1)"
    [[ "$macos_major" == 15 ]] || { echo "Expected macOS 15, found $macos_major." >&2; exit 1; }
    cmake_extra+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0")
    ;;
  MINGW*|MSYS*|CYGWIN*)
    [[ "$target_os" == windows && "$target_abi" == windows-ucrt64 && "${MSYSTEM:-}" == UCRT64 ]] || {
      echo "Windows packages require the MSYS2 UCRT64 ABI." >&2
      exit 1
    }
    ;;
  *)
    echo "Unsupported native build host: $(uname -s)" >&2
    exit 2
    ;;
esac

bash scripts/install-deps.sh
if [[ -z "${SDL3_PREFIX:-}" ]]; then
  bash scripts/install-sdl3-ci.sh
  SDL3_PREFIX="$repo_root/.deps/sdl3"
fi
logger_prefix="${LOGGER_PREFIX:-$repo_root/.deps/log}"
[[ -f "$logger_prefix/lib/cmake/m-mdy-m-log/m-mdy-m-log-config.cmake" ]] || {
  find "$logger_prefix" -maxdepth 5 -type f -iname '*log*config.cmake' -print
  echo "Pinned m-mdy-m/log.c package config was not found in $logger_prefix." >&2
  exit 1
}
[[ -f "$SDL3_PREFIX/lib/cmake/SDL3/SDL3Config.cmake" ]] || {
  echo "Pinned SDL3 SDK package config was not found in $SDL3_PREFIX." >&2
  exit 1
}

# pkg-config does not inherit CMake's CMAKE_PREFIX_PATH. Make the separately
# installed logger package visible for version/provenance recording.
export PKG_CONFIG_PATH="$logger_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake_logger="$logger_prefix"
cmake_sdl="$SDL3_PREFIX"
if command -v cygpath >/dev/null 2>&1; then
  cmake_logger="$(cygpath -m "$logger_prefix")"
  cmake_sdl="$(cygpath -m "$SDL3_PREFIX")"
fi
cmake_prefix="$cmake_logger;$cmake_sdl"

build_dir="$repo_root/build/release-package-$target_os-$target_arch"
stage_root="$repo_root/build/release-stage-$target_os-$target_arch"
rm -rf "$build_dir" "$stage_root"
mkdir -p "$stage_root"

cmake -S . -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$stage_root/unused" \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_INSTALL_INCLUDEDIR=include \
  -DCMAKE_PREFIX_PATH="$cmake_prefix" \
  -DCDIN_FRONTEND_USE_LOG_STUB=OFF \
  -DCDIN_FRONTEND_BUILD_UI_TUI=ON \
  -DCDIN_FRONTEND_BUILD_UI_SDL=ON \
  -DCDIN_FRONTEND_BUILD_UI_HEADLESS=OFF \
  -DCDIN_FRONTEND_BUILD_APPS=OFF \
  -DCDIN_FRONTEND_BUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  "${cmake_extra[@]}"
cmake --build "$build_dir" --config Release --parallel

for variant in core tui gui-sdl; do
  prefix="$stage_root/$variant"
  mkdir -p "$prefix"
  cmake --install "$build_dir" --config Release --prefix "$prefix" --component "$variant"
  build_info="$prefix/share/cdin-frontend"
  mkdir -p "$build_info"
  {
    printf 'component=frontend\n'
    printf 'variant=%s\n' "$variant"
    printf 'package_version=%s\n' "$version"
    printf 'source_revision=%s\n' "$source_revision"
    printf 'target_os=%s\n' "$target_os"
    printf 'target_architecture=%s\n' "$target_arch"
    printf 'target_abi=%s\n' "$target_abi"
    printf 'runner=%s\n' "$target_runner"
    printf 'frontend_protocol=1.0.0\n'
    printf 'logger=%s\n' "$(pkg-config --modversion m-mdy-m-log)"
    printf 'sdl3=%s\n' "$(grep -m1 'PACKAGE_VERSION ' "$SDL3_PREFIX/lib/cmake/SDL3/SDL3ConfigVersion.cmake" 2>/dev/null | sed 's/.*"\\([^"]*\\)".*/\\1/' || echo 3.4.16)"
    printf 'build_recipe=scripts/package-variants.sh\n'
  } > "$build_info/BUILD-INFO.txt"
done

asset_arch="${target_arch//_/-}"
target_key="$target_os-$asset_arch-$target_abi"
output_dir="$repo_root/dist/release-build-$target_key"
mkdir -p "$output_dir"

python3 - "$stage_root" "$output_dir" "$version" "$target_os" "$target_arch" "$target_abi" "$target_runner" "$source_revision" "$tag" <<'PY'
from __future__ import annotations
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys
import tarfile

stage_root, output_root = map(Path, sys.argv[1:3])
version, target_os, architecture, abi, runner, revision, tag = sys.argv[3:]
variants = ("core", "tui", "gui-sdl")
target_slug = architecture.replace("_", "-")
release_prefix = f"https://github.com/cdin-editor/frontend/releases/download/{tag}/"

for variant in variants:
    stage = stage_root / variant
    required = (
        stage / "include",
        stage / "lib" / "cmake",
        stage / "share" / "cdin-frontend" / "BUILD-INFO.txt",
    )
    missing = [str(path.relative_to(stage)) for path in required if not path.exists()]
    if missing:
        raise SystemExit(f"{variant}: incomplete CMake component install: {missing}")
    if variant == "core" and not (stage / "lib" / "cmake" / "cdinFrontendCore" / "cdinFrontendCoreConfig.cmake").is_file():
        raise SystemExit("core: missing cdinFrontendCoreConfig.cmake")
    if variant == "tui" and not (stage / "lib" / "cmake" / "cdinFrontendTUI" / "cdinFrontendTUIConfig.cmake").is_file():
        raise SystemExit("tui: missing cdinFrontendTUIConfig.cmake")
    if variant == "gui-sdl" and not (stage / "lib" / "cmake" / "cdinFrontendGUISDL" / "cdinFrontendGUISDLConfig.cmake").is_file():
        raise SystemExit("gui-sdl: missing cdinFrontendGUISDLConfig.cmake")

    filename = f"cdin-frontend-{version}-{variant}-{target_os}-{target_slug}-{abi}.tar.gz"
    archive_path = output_root / filename
    temporary_tar = archive_path.with_suffix("")
    with tarfile.open(temporary_tar, "w", format=tarfile.PAX_FORMAT) as archive:
        for path in sorted(stage.rglob("*"), key=lambda item: item.relative_to(stage).as_posix()):
            relative = path.relative_to(stage).as_posix()
            if path.is_symlink() or not (path.is_file() or path.is_dir()):
                raise SystemExit(f"{variant}: unsupported install entry {relative}")
            info = archive.gettarinfo(str(path), arcname=relative)
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            info.mtime = 0
            info.mode &= 0o777
            if path.is_file():
                with path.open("rb") as source:
                    archive.addfile(info, source)
            else:
                archive.addfile(info)
    with temporary_tar.open("rb") as source, archive_path.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as compressed:
            while chunk := source.read(1024 * 1024):
                compressed.write(chunk)
    temporary_tar.unlink()
    digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
    size = archive_path.stat().st_size
    sidecar = Path(str(archive_path) + ".sha256")
    sidecar.write_text(f"{digest}  {filename}\n", encoding="ascii")
    descriptor = {
        "source_revision": revision,
        "artifact": {
            "artifact_id": f"frontend-{variant}-{target_os}-{target_slug}-{abi}",
            "kind": "native-development-package",
            "target": {"os": target_os, "architecture": architecture, "abi": abi},
            "format": "tar.gz",
            "file_name": filename,
            "uri": release_prefix + filename,
            "sha256": digest,
            "size_bytes": size,
            "layout": "cmake-prefix-v1",
            "source_unit_ids": ["frontend-source"],
            "build": {
                "recipe": {"source_unit_id": "frontend-source", "path": "scripts/package-variants.sh"},
                "toolchain": [
                    {"name": "compiler", "version": "see BUILD-INFO.txt"},
                    {"name": "cmake", "version": "see BUILD-INFO.txt"},
                    {"name": "logger", "version": "0.1.0"},
                    {"name": "sdl3", "version": "3.4.16"},
                ],
                "runner": runner,
            },
        },
    }
    descriptor_path = Path(str(archive_path) + ".artifact.json")
    descriptor_path.write_text(json.dumps(descriptor, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Created {filename}: {size} bytes sha256={digest}")
PY

python3 scripts/test-variant-packages.py --artifact-dir "$output_dir" --logger-prefix "$logger_prefix" --sdl3-prefix "$SDL3_PREFIX"
echo "Frontend native packages verified for $target_key at $source_revision"
