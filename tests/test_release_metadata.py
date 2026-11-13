from __future__ import annotations

import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import importlib.util
import sys

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "scripts" / "generate-release-metadata.py"
SPEC = importlib.util.spec_from_file_location("frontend_release_metadata", SCRIPT_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load release metadata generator from {SCRIPT_PATH}")
metadata_module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = metadata_module
SPEC.loader.exec_module(metadata_module)
resolve_release_identity = metadata_module.resolve_release_identity


class ReleaseIdentityTests(unittest.TestCase):
    revision = "a" * 40

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.project_root = Path(self.temporary.name)
        (self.project_root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.22)\n"
            "project(cdin_frontend VERSION 1.2.0 LANGUAGES C)\n",
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_dry_run_uses_project_version_and_head_without_resolving_synthetic_tag(self) -> None:
        calls: list[list[str]] = []

        def git_output(command: list[str], **_: object) -> str:
            calls.append(command)
            return self.revision + "\n"

        with (
            mock.patch(
                "frontend_release_metadata.subprocess.check_output",
                side_effect=git_output,
            ),
            mock.patch.dict(os.environ, {"PIPELINE_SOURCE_SHA": self.revision}),
        ):
            version, revision = resolve_release_identity(
                "v0.0.0-dev.07e2ec0",
                dry_run=True,
                project_root=self.project_root,
            )

        self.assertEqual(version, "1.2.0")
        self.assertEqual(revision, self.revision)
        self.assertEqual(calls, [["git", "rev-parse", "--verify", "HEAD"]])

    def test_dry_run_rejects_pipeline_source_mismatch(self) -> None:
        with (
            mock.patch(
                "frontend_release_metadata.subprocess.check_output",
                return_value=self.revision + "\n",
            ),
            mock.patch.dict(os.environ, {"PIPELINE_SOURCE_SHA": "b" * 40}),
        ):
            with self.assertRaisesRegex(SystemExit, "Pipeline source"):
                resolve_release_identity(
                    "v0.0.0-dev.07e2ec0",
                    dry_run=True,
                    project_root=self.project_root,
                )

    def test_dry_run_rejects_non_pipeline_candidate_tag(self) -> None:
        with self.assertRaisesRegex(SystemExit, "synthetic"):
            resolve_release_identity(
                "v1.2.0",
                dry_run=True,
                project_root=self.project_root,
            )

    def test_real_release_requires_exact_tag_and_matching_head(self) -> None:
        calls: list[list[str]] = []

        def git_output(command: list[str], **_: object) -> str:
            calls.append(command)
            return self.revision + "\n"

        with mock.patch(
            "frontend_release_metadata.subprocess.check_output",
            side_effect=git_output,
        ):
            version, revision = resolve_release_identity(
                "v1.2.0",
                dry_run=False,
                project_root=self.project_root,
            )

        self.assertEqual(version, "1.2.0")
        self.assertEqual(revision, self.revision)
        self.assertEqual(
            calls,
            [
                ["git", "rev-parse", "--verify", "refs/tags/v1.2.0^{commit}"],
                ["git", "rev-parse", "--verify", "HEAD"],
            ],
        )

    def test_real_release_rejects_tag_head_mismatch(self) -> None:
        with mock.patch(
            "frontend_release_metadata.subprocess.check_output",
            side_effect=[self.revision + "\n", "b" * 40 + "\n"],
        ):
            with self.assertRaisesRegex(SystemExit, "but release source HEAD"):
                resolve_release_identity(
                    "v1.2.0",
                    dry_run=False,
                    project_root=self.project_root,
                )


class ReleaseArtifactLayoutTests(unittest.TestCase):
    def test_release_assets_are_flat_and_pipeline_pin_is_consistent(self) -> None:
        repository_root = Path(__file__).resolve().parents[1]
        package_script = (repository_root / "scripts" / "package-variants.sh").read_text(
            encoding="utf-8"
        )
        release_workflow = (
            repository_root / ".github" / "workflows" / "release.yml"
        ).read_text(encoding="utf-8")

        # The metadata generator records direct, tag-qualified Release asset URIs.
        # Keep their physical output at dist/ so Pipeline uploads each basename,
        # not an enclosing target directory.
        self.assertIn('output_dir="$repo_root/dist"', package_script)
        self.assertIn("dist/cdin-frontend-*", release_workflow)
        self.assertNotIn("dist/release-build-*", release_workflow)

        # Pipeline's reusable workflow ref and helper implementation must be
        # the same immutable commit for reproducible package assembly.
        pinned_pipeline = "7cfdffd574d3b438228bc6b0faae7c61bb14f055"
        self.assertIn(f"release.yml@{pinned_pipeline}", release_workflow)
        self.assertIn(f"pipeline-ref: {pinned_pipeline}", release_workflow)


    def test_package_script_keeps_branch_dry_runs_separate_from_immutable_tag_releases(self) -> None:
        repository_root = Path(__file__).resolve().parents[1]
        package_script = (
            repository_root / "scripts" / "package-variants.sh"
        ).read_text(encoding="utf-8")
        changelog = (repository_root / "CHANGELOG.md").read_text(encoding="utf-8")

        # Branch previews use the synthetic candidate supplied by Pipeline and
        # must not try to resolve it as a Git tag. Actual tag builds still bind
        # package version to the exact tag-resolved commit.
        self.assertIn('PIPELINE_DRY_RUN:-false', package_script)
        self.assertIn('GITHUB_REF_TYPE:-', package_script)
        self.assertIn('v0.0.0-dev.*', package_script)
        self.assertIn('"$tag" == "v$version"', package_script)
        self.assertIn('refs/tags/$tag^{commit}', package_script)
        self.assertIn(
            "https://github.com/cdin-editor/spec/blob/main/docs/adr/"
            "ADR-0028-github-release-metadata-discovery.md#L19-L24",
            changelog,
        )


if __name__ == "__main__":
    unittest.main()
