#!/usr/bin/env python3
"""Aggregate the 5x3 native Frontend packages into ADR-0028 Release Metadata."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import urlparse

EXPECTED_TARGETS = {
    ("linux", "x86_64", "gnu-glibc-2.39"),
    ("linux", "arm64", "gnu-glibc-2.39"),
    ("darwin", "x86_64", "macos-15"),
    ("darwin", "arm64", "macos-15"),
    ("windows", "x86_64", "windows-ucrt64"),
}
VARIANTS = ("core", "tui", "gui-sdl")
REPOSITORY_URI = "https://github.com/cdin-editor/frontend"
PROTOCOL_REQUIREMENT = {
    "contract_id": "org.cdin.frontend-protocol",
    "version_constraint": ">=1.0.0 <2.0.0",
}
LOGGER_REQUIREMENT = {
    "contract_id": "org.m-mdy-m.log-c-api",
    "version_constraint": "=0.1.0",
}
SDL3_REQUIREMENT = {
    "contract_id": "org.cdin.sdl3-sdk",
    "version_constraint": "=3.4.16",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_release_identity(
    tag: str,
    *,
    dry_run: bool,
    project_root: Path,
) -> tuple[str, str]:
    """Resolve package version and source commit for a release or branch dry-run."""
    if dry_run:
        if not re.fullmatch(r"v0\.0\.0-dev\.[0-9a-f]{7}", tag):
            raise SystemExit(
                "--tag must be the Pipeline synthetic v0.0.0-dev.<7-hex> candidate during dry-run"
            )

        try:
            head_revision = subprocess.check_output(
                ["git", "rev-parse", "--verify", "HEAD"], text=True
            ).strip().lower()
        except subprocess.CalledProcessError as error:
            raise SystemExit(f"cannot resolve checked-out source HEAD: {error}") from error

        pipeline_revision = os.environ.get("PIPELINE_SOURCE_SHA", "").strip().lower()
        if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", head_revision):
            raise SystemExit("checked-out source HEAD is not a full Git commit ID")
        if pipeline_revision != head_revision:
            raise SystemExit(
                "refusing dry-run metadata generation: "
                f"Pipeline source is {pipeline_revision or '<missing>'}, HEAD is {head_revision}"
            )

        cmake_file = project_root / "CMakeLists.txt"
        try:
            cmake_content = cmake_file.read_text(encoding="utf-8")
        except OSError as error:
            raise SystemExit(f"cannot read Frontend project version from {cmake_file}: {error}") from error
        project_match = re.search(
            r"(?m)^project\(cdin_frontend VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES C\)$",
            cmake_content,
        )
        if project_match is None:
            raise SystemExit("cannot determine the Frontend package version from CMakeLists.txt")
        return project_match.group(1), head_revision

    if not re.fullmatch(r"v[0-9]+\.[0-9]+\.[0-9]+", tag):
        raise SystemExit("--tag must be the exact immutable vMAJOR.MINOR.PATCH release tag")

    version = tag[1:]
    try:
        tag_revision = subprocess.check_output(
            ["git", "rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}"], text=True
        ).strip().lower()
        head_revision = subprocess.check_output(
            ["git", "rev-parse", "--verify", "HEAD"], text=True
        ).strip().lower()
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"cannot resolve immutable release source/tag: {error}") from error
    if tag_revision != head_revision:
        raise SystemExit(
            f"refusing metadata generation: tag {tag} resolves to {tag_revision}, "
            f"but release source HEAD is {head_revision}"
        )
    if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", tag_revision):
        raise SystemExit("release tag did not resolve to a full Git object ID")
    return version, tag_revision


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", default=os.environ.get("PIPELINE_RELEASE_TAG", ""))
    parser.add_argument("--artifact-dir", type=Path, default=Path("release-assets"))
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()

    dry_run = (
        os.environ.get("PIPELINE_DRY_RUN", "false").lower() == "true"
        and os.environ.get("GITHUB_REF_TYPE", "") != "tag"
    )
    version, tag_revision = resolve_release_identity(
        args.tag,
        dry_run=dry_run,
        project_root=Path(__file__).resolve().parents[1],
    )

    descriptors = sorted(args.artifact_dir.rglob("cdin-frontend-*.tar.gz.artifact.json"))
    if len(descriptors) != len(EXPECTED_TARGETS) * len(VARIANTS):
        raise SystemExit(
            f"expected {len(EXPECTED_TARGETS) * len(VARIANTS)} Frontend package descriptors; "
            f"found {len(descriptors)}"
        )

    artifacts: dict[str, list[dict]] = {variant: [] for variant in VARIANTS}
    selected: set[tuple[str, str, str, str]] = set()
    release_prefix = f"https://github.com/cdin-editor/frontend/releases/download/{args.tag}/"

    for descriptor_path in descriptors:
        try:
            record = json.loads(descriptor_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise SystemExit(f"{descriptor_path}: invalid artifact descriptor: {error}") from error
        if record.get("source_revision") != tag_revision:
            raise SystemExit(f"{descriptor_path}: package source revision does not match tag {args.tag}")
        artifact = record.get("artifact")
        if not isinstance(artifact, dict):
            raise SystemExit(f"{descriptor_path}: missing artifact object")

        artifact_id = str(artifact.get("artifact_id", ""))
        target = artifact.get("target", {})
        target_key = (target.get("os"), target.get("architecture"), target.get("abi"))
        filename_hint = descriptor_path.name.removesuffix(".artifact.json")
        variant_match = re.match(r"^cdin-frontend-[0-9]+\.[0-9]+\.[0-9]+-(core|tui|gui-sdl)-", filename_hint)
        if variant_match is None:
            raise SystemExit(f"{descriptor_path}: cannot determine Frontend package variant")
        variant = variant_match.group(1)
        if target_key not in EXPECTED_TARGETS or (variant, *target_key) in selected:
            raise SystemExit(f"{descriptor_path}: unexpected or duplicate target {target_key} for {variant}")
        target_os, arch, abi = target_key
        expected_artifact_id = f"frontend-{variant}-{target_os}-{arch.replace('_', '-')}-{abi}"
        if artifact_id != expected_artifact_id:
            raise SystemExit(f"{descriptor_path}: artifact_id and target tuple disagree")
        if artifact.get("kind") != "native-development-package" or artifact.get("format") != "tar.gz":
            raise SystemExit(f"{descriptor_path}: unexpected package kind or format")
        if artifact.get("layout") != "cmake-prefix-v1":
            raise SystemExit(f"{descriptor_path}: package layout must be cmake-prefix-v1")
        if artifact.get("source_unit_ids") != ["frontend-source"]:
            raise SystemExit(f"{descriptor_path}: artifact must reference frontend-source")
        build = artifact.get("build")
        if not isinstance(build, dict) or build.get("recipe") != {
            "source_unit_id": "frontend-source",
            "path": "scripts/package-variants.sh",
        } or not build.get("toolchain") or not build.get("runner"):
            raise SystemExit(f"{descriptor_path}: incomplete package build provenance")

        filename = artifact.get("file_name", "")
        expected_arch_slug = arch.replace("_", "-")
        expected_filename = (
            f"cdin-frontend-{version}-{variant}-{target_os}-{expected_arch_slug}-{abi}.tar.gz"
        )
        if filename != expected_filename or Path(filename).name != filename:
            raise SystemExit(f"{descriptor_path}: unexpected package filename {filename!r}")
        if descriptor_path.name != filename + ".artifact.json":
            raise SystemExit(f"{descriptor_path}: descriptor basename does not match its archive")
        if artifact.get("uri") != release_prefix + filename:
            raise SystemExit(f"{descriptor_path}: artifact URI does not identify exact tag release asset")
        parsed = urlparse(artifact["uri"])
        if parsed.scheme != "https" or parsed.netloc != "github.com" or parsed.query or parsed.fragment:
            raise SystemExit(f"{descriptor_path}: artifact URI must be canonical HTTPS GitHub release URL")
        if not re.fullmatch(r"[0-9a-f]{64}", str(artifact.get("sha256", ""))):
            raise SystemExit(f"{descriptor_path}: invalid SHA-256")
        if not isinstance(artifact.get("size_bytes"), int) or artifact["size_bytes"] <= 0:
            raise SystemExit(f"{descriptor_path}: invalid package byte size")

        archive_path = descriptor_path.parent / filename
        sidecar = Path(str(archive_path) + ".sha256")
        if not archive_path.is_file() or not sidecar.is_file():
            raise SystemExit(f"{descriptor_path}: package archive or checksum sidecar is missing")
        actual_digest = sha256(archive_path)
        if actual_digest != artifact["sha256"] or archive_path.stat().st_size != artifact["size_bytes"]:
            raise SystemExit(f"{descriptor_path}: archive digest/size differs from descriptor")
        checksum_fields = sidecar.read_text(encoding="ascii").split()
        if len(checksum_fields) < 2 or checksum_fields[0] != actual_digest or checksum_fields[1].lstrip("*") != filename:
            raise SystemExit(f"{descriptor_path}: checksum sidecar does not match archive")

        artifacts[variant].append(artifact)
        selected.add((variant, *target_key))

    for variant in VARIANTS:
        found = {(os_name, arch, abi) for item_variant, os_name, arch, abi in selected if item_variant == variant}
        if found != EXPECTED_TARGETS:
            raise SystemExit(f"{variant}: missing native package targets: {sorted(EXPECTED_TARGETS - found)}")

    if args.output_dir is None:
        output_root = args.artifact_dir
    else:
        output_root = args.output_dir
    output_root.mkdir(parents=True, exist_ok=True)

    for variant in VARIANTS:
        dependencies = []
        requirements = []
        if variant == "core":
            requirements.append(LOGGER_REQUIREMENT)
        elif variant == "tui":
            dependencies.append({
                "component_id": "frontend",
                "variant": "core",
                "version_constraint": f"={version}",
            })
            requirements.extend([PROTOCOL_REQUIREMENT, LOGGER_REQUIREMENT])
        else:
            dependencies.append({
                "component_id": "frontend",
                "variant": "core",
                "version_constraint": f"={version}",
            })
            requirements.extend([PROTOCOL_REQUIREMENT, SDL3_REQUIREMENT])

        metadata_name = f"frontend-{variant}-{version}.json"
        metadata = {
            "schema_version": "1.2.0",
            "component_id": "frontend",
            "variant": variant,
            "version": version,
            "status": "active",
            "source_units": [{
                "source_unit_id": "frontend-source",
                "repository_uri": REPOSITORY_URI,
                "revision": tag_revision,
            }],
            "artifacts": sorted(
                artifacts[variant],
                key=lambda item: (
                    item["target"]["os"],
                    item["target"]["architecture"],
                    item["target"]["abi"],
                ),
            ),
            "dependencies": dependencies,
            "contracts": {"provides": [], "requires": requirements},
            "publication": {
                "provider": "github-releases-v1",
                "repository_uri": REPOSITORY_URI,
                "tag": args.tag,
                "tag_commit": tag_revision,
                "metadata_asset_name": metadata_name,
            },
        }
        output = output_root / metadata_name
        if output.exists():
            raise SystemExit(f"refusing to overwrite immutable release metadata: {output}")
        output.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"Wrote {output} with {len(artifacts[variant])} verified native packages")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"Frontend release metadata generation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
