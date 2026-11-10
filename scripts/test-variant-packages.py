#!/usr/bin/env python3
"""Verify the downloaded Core/TUI/GUI-SDL packages outside the Frontend source tree."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile
import tempfile


VARIANTS = ("core", "tui", "gui-sdl")


def run(command: list[str], *, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=env)


def safe_extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    seen: set[str] = set()
    with tarfile.open(archive, "r:gz") as package:
        members = package.getmembers()
        if not members or len(members) > 50000:
            raise RuntimeError(f"{archive.name}: invalid package member count")
        for member in members:
            name = PurePosixPath(member.name)
            if name.is_absolute() or not name.parts or any(part in ("", ".", "..") for part in name.parts):
                raise RuntimeError(f"{archive.name}: unsafe package path {member.name!r}")
            normalized = name.as_posix().casefold()
            if normalized in seen:
                raise RuntimeError(f"{archive.name}: duplicate normalized path {member.name!r}")
            seen.add(normalized)
            if not (member.isfile() or member.isdir()):
                raise RuntimeError(f"{archive.name}: unsupported package entry {member.name!r}")
            target = (destination / Path(*name.parts)).resolve()
            if target != root and root not in target.parents:
                raise RuntimeError(f"{archive.name}: package path escapes extraction root")
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            source = package.extractfile(member)
            if source is None:
                raise RuntimeError(f"{archive.name}: cannot read package member")
            target.parent.mkdir(parents=True, exist_ok=True)
            with source, target.open("wb") as output:
                shutil.copyfileobj(source, output)
            target.chmod(0o755 if member.mode & 0o111 else 0o644)


def verify_archive(artifact_dir: Path, version: str, variant: str, target_os: str, arch: str, abi: str) -> Path:
    filename = f"cdin-frontend-{version}-{variant}-{target_os}-{arch}-{abi}.tar.gz"
    archive = artifact_dir / filename
    descriptor_path = Path(str(archive) + ".artifact.json")
    sidecar = Path(str(archive) + ".sha256")
    if not archive.is_file() or not descriptor_path.is_file() or not sidecar.is_file():
        raise RuntimeError(f"missing {variant} package, descriptor or checksum for {filename}")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    descriptor = json.loads(descriptor_path.read_text(encoding="utf-8"))
    artifact = descriptor.get("artifact", {})
    if artifact.get("file_name") != filename or artifact.get("sha256") != digest:
        raise RuntimeError(f"{filename}: release descriptor does not match archive bytes")
    if artifact.get("size_bytes") != archive.stat().st_size:
        raise RuntimeError(f"{filename}: descriptor byte size mismatch")
    if descriptor.get("source_revision") != os.environ.get("GITHUB_SHA", descriptor.get("source_revision")):
        raise RuntimeError(f"{filename}: package source revision differs from the checked-out source")
    sidecar_fields = sidecar.read_text(encoding="ascii").split()
    if len(sidecar_fields) < 2 or sidecar_fields[0] != digest or sidecar_fields[1].lstrip("*") != filename:
        raise RuntimeError(f"{filename}: checksum sidecar mismatch")
    if artifact.get("target") != {"os": target_os, "architecture": arch, "abi": abi}:
        raise RuntimeError(f"{filename}: descriptor target identity mismatch")
    return archive


def configure_and_test(source: Path, build: Path, prefix_paths: list[Path]) -> None:
    separator = ";" if os.name == "nt" or os.environ.get("MSYSTEM") else ";"
    prefix_value = separator.join(str(path) for path in prefix_paths)
    run([
        "cmake", "-S", str(source), "-B", str(build),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={prefix_value}",
    ])
    run(["cmake", "--build", str(build), "--config", "Release", "--parallel"])
    run(["ctest", "--test-dir", str(build), "-C", "Release", "--output-on-failure"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, required=True)
    parser.add_argument("--logger-prefix", type=Path, required=True)
    parser.add_argument("--sdl3-prefix", type=Path, required=True)
    parser.add_argument("--version", default="")
    parser.add_argument("--target-os", default=os.environ.get("FRONTEND_TARGET_OS", ""))
    parser.add_argument("--target-arch", default=os.environ.get("FRONTEND_TARGET_ARCH", ""))
    parser.add_argument("--target-abi", default=os.environ.get("FRONTEND_TARGET_ABI", ""))
    args = parser.parse_args()

    if not (args.target_os and args.target_arch and args.target_abi):
        parser.error("target OS, architecture and ABI must be supplied explicitly")
    if not args.version:
        tag = os.environ.get("PIPELINE_RELEASE_TAG") or os.environ.get("GITHUB_REF_NAME", "")
        if not tag.startswith("v"):
            parser.error("release version is missing")
        args.version = tag[1:]

    archives = {
        variant: verify_archive(
            args.artifact_dir, args.version, variant,
            args.target_os, args.target_arch.replace("_", "-"), args.target_abi,
        )
        for variant in VARIANTS
    }
    with tempfile.TemporaryDirectory(prefix="frontend-variant-consumer-") as temporary:
        root = Path(temporary)
        prefix = root / "installed-prefix"
        for variant in VARIANTS:
            safe_extract(archives[variant], prefix)

        source_root = Path(__file__).resolve().parents[1] / "tests" / "variant-consumer"
        # Core and TUI must be independently consumable without SDL3 present.
        configure_and_test(source_root / "core", root / "core-consumer-build", [prefix, args.logger_prefix])
        configure_and_test(source_root / "tui", root / "tui-consumer-build", [prefix, args.logger_prefix])
        # GUI-SDL consumes the same single Core install plus its target SDK.
        configure_and_test(source_root / "gui-sdl", root / "gui-consumer-build", [
            prefix, args.logger_prefix, args.sdl3_prefix,
        ])
        configure_and_test(source_root / "combined", root / "combined-consumer-build", [
            prefix, args.logger_prefix, args.sdl3_prefix,
        ])
    print("Verified Core, TUI, GUI-SDL and combined CMake consumers from extracted packages.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
