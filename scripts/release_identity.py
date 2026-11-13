#!/usr/bin/env python3
"""Resolve one Frontend package identity for every release packaging step."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

VERSION_RE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
RELEASE_TAG_RE = re.compile(r"^v(?P<version>[0-9]+\.[0-9]+\.[0-9]+)$")
DRY_RUN_TAG_RE = re.compile(r"^v0\.0\.0-dev\.[0-9a-f]{7}$")
CMAKE_DEFAULT_RE = re.compile(
    r'(?m)^set\(CDIN_FRONTEND_VERSION "([0-9]+\.[0-9]+\.[0-9]+)" '
    r'CACHE STRING "[^"]+"\)$'
)


def is_pipeline_dry_run(environ: dict[str, str] | None = None) -> bool:
    env = os.environ if environ is None else environ
    return (
        env.get("PIPELINE_DRY_RUN", "false").lower() == "true"
        and env.get("GITHUB_REF_TYPE", "") != "tag"
    )


def read_local_project_version(project_root: Path) -> str:
    cmake_file = project_root / "CMakeLists.txt"
    try:
        content = cmake_file.read_text(encoding="utf-8")
    except OSError as error:
        raise SystemExit(f"cannot read Frontend development version from {cmake_file}: {error}") from error
    match = CMAKE_DEFAULT_RE.search(content)
    if match is None:
        raise SystemExit(
            "cannot determine the local Frontend version from "
            "set(CDIN_FRONTEND_VERSION ... CACHE STRING ...) in CMakeLists.txt"
        )
    version = match.group(1)
    if not VERSION_RE.fullmatch(version):
        raise SystemExit(f"invalid local Frontend package version: {version}")
    return version


def git_revision(project_root: Path, ref: str) -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--verify", ref],
            cwd=project_root,
            text=True,
        ).strip().lower()
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"cannot resolve Git revision {ref!r}: {error}") from error


def resolve_release_identity(
    tag: str,
    *,
    dry_run: bool,
    project_root: Path,
    allow_untagged: bool = False,
    environ: dict[str, str] | None = None,
) -> tuple[str, str]:
    """Return (package_version, source_revision).

    Real releases take their version exclusively from an immutable vMAJOR.MINOR.PATCH
    Git tag and require that tag to resolve to the checked-out source commit.
    Pipeline branch previews use the synthetic candidate tag but keep the local
    development package version. A tagless local source archive may use that same
    development version without requiring a .git directory.
    """
    env = os.environ if environ is None else environ

    if dry_run:
        if not DRY_RUN_TAG_RE.fullmatch(tag):
            raise SystemExit(
                "branch dry-runs require the Pipeline synthetic "
                "v0.0.0-dev.<7-hex> candidate tag"
            )
        head_revision = git_revision(project_root, "HEAD")
        pipeline_revision = env.get("PIPELINE_SOURCE_SHA", "").strip().lower()
        if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", head_revision):
            raise SystemExit("checked-out source HEAD is not a full Git commit ID")
        if pipeline_revision != head_revision:
            raise SystemExit(
                "refusing branch dry-run: "
                f"Pipeline source is {pipeline_revision or '<missing>'}, "
                f"but checked-out HEAD is {head_revision}"
            )
        return read_local_project_version(project_root), head_revision

    if not tag:
        if not allow_untagged:
            raise SystemExit("release packaging requires an immutable vMAJOR.MINOR.PATCH Git tag")
        return read_local_project_version(project_root), ""

    match = RELEASE_TAG_RE.fullmatch(tag)
    if match is None:
        raise SystemExit("release tag must be the exact immutable vMAJOR.MINOR.PATCH form")

    version = match.group("version")
    tag_revision = git_revision(project_root, f"refs/tags/{tag}^{{commit}}")
    head_revision = git_revision(project_root, "HEAD")
    if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", tag_revision):
        raise SystemExit("release tag did not resolve to a full Git commit ID")
    if tag_revision != head_revision:
        raise SystemExit(
            f"refusing release: tag {tag} resolves to {tag_revision}, "
            f"but checked-out source HEAD is {head_revision}"
        )
    return version, tag_revision


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--allow-untagged",
        action="store_true",
        help="allow a local tagless source archive to use the CMake development version",
    )
    args = parser.parse_args()

    tag = (
        os.environ.get("PIPELINE_RELEASE_TAG", "").strip()
        or os.environ.get("GITHUB_REF_NAME", "").strip()
    )
    version, revision = resolve_release_identity(
        tag,
        dry_run=is_pipeline_dry_run(),
        project_root=Path(__file__).resolve().parents[1],
        allow_untagged=args.allow_untagged,
    )
    print(f"{version}\t{revision}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"Frontend release identity resolution failed: {error}", file=sys.stderr)
        raise SystemExit(1)
