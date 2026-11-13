from __future__ import annotations

import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

SCRIPTS_DIR = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS_DIR))
from release_identity import resolve_release_identity


class ReleaseIdentityTests(unittest.TestCase):
    revision = "a" * 40

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.project_root = Path(self.temporary.name)
        (self.project_root / "CMakeLists.txt").write_text(
            'set(CDIN_FRONTEND_VERSION "1.2.2" CACHE STRING "Local/development Frontend package version")\n'
            'project(cdin_frontend VERSION "${CDIN_FRONTEND_VERSION}" LANGUAGES C)\n',
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_branch_dry_run_uses_local_version_and_verifies_pipeline_head(self) -> None:
        calls: list[list[str]] = []

        def git_output(command: list[str], **_: object) -> str:
            calls.append(command)
            return self.revision + "\n"

        with (
            mock.patch("release_identity.subprocess.check_output", side_effect=git_output),
            mock.patch.dict(os.environ, {"PIPELINE_SOURCE_SHA": self.revision}),
        ):
            version, revision = resolve_release_identity(
                "v0.0.0-dev.07e2ec0",
                dry_run=True,
                project_root=self.project_root,
            )

        self.assertEqual(version, "1.2.2")
        self.assertEqual(revision, self.revision)
        self.assertEqual(calls, [["git", "rev-parse", "--verify", "HEAD"]])

    def test_branch_dry_run_rejects_pipeline_source_mismatch(self) -> None:
        with (
            mock.patch(
                "release_identity.subprocess.check_output",
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

    def test_branch_dry_run_allows_pipeline_omitting_optional_source_sha(self) -> None:
        with (
            mock.patch(
                "release_identity.subprocess.check_output",
                return_value=self.revision + "\n",
            ),
            mock.patch.dict(os.environ, {}, clear=True),
        ):
            version, revision = resolve_release_identity(
                "v0.0.0-dev.07e2ec0",
                dry_run=True,
                project_root=self.project_root,
            )

        self.assertEqual(version, "1.2.2")
        self.assertEqual(revision, self.revision)

    def test_branch_dry_run_rejects_non_pipeline_candidate_tag(self) -> None:
        with self.assertRaisesRegex(SystemExit, "synthetic"):
            resolve_release_identity(
                "v1.2.2",
                dry_run=True,
                project_root=self.project_root,
            )

    def test_real_release_takes_version_from_exact_tag_and_matching_head(self) -> None:
        calls: list[list[str]] = []

        def git_output(command: list[str], **_: object) -> str:
            calls.append(command)
            return self.revision + "\n"

        with mock.patch(
            "release_identity.subprocess.check_output",
            side_effect=git_output,
        ):
            version, revision = resolve_release_identity(
                "v1.2.2",
                dry_run=False,
                project_root=self.project_root,
            )

        self.assertEqual(version, "1.2.2")
        self.assertEqual(revision, self.revision)
        self.assertEqual(
            calls,
            [
                ["git", "rev-parse", "--verify", "refs/tags/v1.2.2^{commit}"],
                ["git", "rev-parse", "--verify", "HEAD"],
            ],
        )

    def test_real_release_rejects_tag_head_mismatch(self) -> None:
        with mock.patch(
            "release_identity.subprocess.check_output",
            side_effect=[self.revision + "\n", "b" * 40 + "\n"],
        ):
            with self.assertRaisesRegex(SystemExit, "but checked-out source HEAD"):
                resolve_release_identity(
                    "v1.2.2",
                    dry_run=False,
                    project_root=self.project_root,
                )

    def test_real_release_rejects_non_semver_tag(self) -> None:
        with self.assertRaisesRegex(SystemExit, "vMAJOR.MINOR.PATCH"):
            resolve_release_identity(
                "v1.2.2-rc.1",
                dry_run=False,
                project_root=self.project_root,
            )

    def test_tagless_local_source_archive_uses_development_version(self) -> None:
        version, revision = resolve_release_identity(
            "",
            dry_run=False,
            project_root=self.project_root,
            allow_untagged=True,
        )
        self.assertEqual(version, "1.2.2")
        self.assertEqual(revision, "")

    def test_tagless_release_packaging_is_not_allowed_without_opt_in(self) -> None:
        with self.assertRaisesRegex(SystemExit, "requires an immutable"):
            resolve_release_identity(
                "",
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

        self.assertIn('output_dir="$repo_root/dist"', package_script)
        self.assertIn("dist/cdin-frontend-*", release_workflow)
        self.assertNotIn("dist/release-build-*", release_workflow)

        pinned_pipeline = "7cfdffd574d3b438228bc6b0faae7c61bb14f055"
        self.assertIn(f"release.yml@{pinned_pipeline}", release_workflow)
        self.assertIn(f"pipeline-ref: {pinned_pipeline}", release_workflow)

    def test_all_release_outputs_use_the_shared_git_tag_identity(self) -> None:
        repository_root = Path(__file__).resolve().parents[1]
        package_script = (
            repository_root / "scripts" / "package-variants.sh"
        ).read_text(encoding="utf-8")
        source_script = (
            repository_root / "scripts" / "package-release.sh"
        ).read_text(encoding="utf-8")
        metadata_script = (
            repository_root / "scripts" / "generate-release-metadata.py"
        ).read_text(encoding="utf-8")
        cmake = (repository_root / "CMakeLists.txt").read_text(encoding="utf-8")
        release_workflow = (
            repository_root / ".github" / "workflows" / "release.yml"
        ).read_text(encoding="utf-8")
        version_template = (repository_root / "cmake" / "frontend-version.h.in").read_text(encoding="utf-8")
        version_test = (repository_root / "tests" / "core" / "unit" / "version.c").read_text(encoding="utf-8")

        self.assertIn("python3 scripts/release_identity.py", package_script)
        self.assertIn('-DCDIN_FRONTEND_VERSION="$version"', package_script)
        self.assertIn("python3 scripts/release_identity.py --allow-untagged", source_script)
        self.assertIn("from release_identity import", metadata_script)
        self.assertIn(
            'set(CDIN_FRONTEND_VERSION "1.2.2" CACHE STRING',
            cmake,
        )
        self.assertIn('project(cdin_frontend VERSION "${CDIN_FRONTEND_VERSION}" LANGUAGES C)', cmake)
        self.assertIn("frontend-version.h.in", cmake)
        self.assertIn("@PROJECT_VERSION_MAJOR@", version_template)
        self.assertIn("@PROJECT_VERSION_MINOR@", version_template)
        self.assertIn("@PROJECT_VERSION_PATCH@", version_template)
        self.assertIn('"${PROJECT_VERSION}"', cmake)
        self.assertIn("CDIN_FRONTEND_PROJECT_VERSION_STRING", version_test)
        source_version_header = (repository_root / "include" / "cdin" / "frontend" / "core" / "version.h").read_text(encoding="utf-8")
        self.assertIn('CDIN_FRONTEND_FOUNDATION_VERSION_STRING "1.2.2"', source_version_header)
        self.assertIn("release_identity.py | cut -f1", release_workflow)
        ci_check = (repository_root / "scripts" / "ci-check.sh").read_text(encoding="utf-8")
        self.assertIn('cmake -S . -B build "-DCDIN_FRONTEND_VERSION=${CDIN_FRONTEND_VERSION}" "$@"', ci_check)
        self.assertIn("configure_frontend -DCMAKE_BUILD_TYPE=Release", ci_check)
        self.assertNotIn("cmake_version_args=()", ci_check)
        self.assertNotIn('"$tag" == "v$version"', package_script)

    def test_package_script_keeps_branch_dry_runs_separate_from_immutable_tag_releases(self) -> None:
        repository_root = Path(__file__).resolve().parents[1]
        package_script = (
            repository_root / "scripts" / "package-variants.sh"
        ).read_text(encoding="utf-8")
        changelog = (repository_root / "CHANGELOG.md").read_text(encoding="utf-8")

        self.assertIn('PIPELINE_DRY_RUN:-false', package_script)
        self.assertIn('GITHUB_REF_TYPE:-', package_script)
        identity_helper = (repository_root / "scripts" / "release_identity.py").read_text(encoding="utf-8")
        self.assertIn("v0.0.0-dev.", identity_helper)
        self.assertIn("immutable Git tag", package_script)
        self.assertIn(
            "https://github.com/cdin-editor/spec/blob/main/docs/adr/"
            "ADR-0028-github-release-metadata-discovery.md#L19-L24",
            changelog,
        )


if __name__ == "__main__":
    unittest.main()
