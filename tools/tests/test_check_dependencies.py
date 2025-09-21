"""Tests for :mod:`tools.check_dependencies`."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools import check_dependencies


class BuildCommandSequenceTests(unittest.TestCase):
    """Verify the generated Conan command invocations."""

    def test_includes_install_step_by_default(self) -> None:
        commands = check_dependencies.build_command_sequence(
            "conan",
            Path("/tmp/conan.lock"),
            Path("/tmp/conanfile.py"),
            "never",
            True,
            (),
        )
        self.assertEqual(
            [
                ["conan", "lock", "update", "/tmp/conan.lock", "/tmp/conanfile.py"],
                [
                    "conan",
                    "install",
                    "/tmp/conanfile.py",
                    "--lockfile",
                    "/tmp/conan.lock",
                    "--build=never",
                ],
            ],
            commands,
        )

    def test_optional_install_step(self) -> None:
        commands = check_dependencies.build_command_sequence(
            "conan",
            Path("/tmp/conan.lock"),
            Path("/tmp/conanfile.py"),
            "never",
            False,
            ("--profile", "default"),
        )
        self.assertEqual(
            [["conan", "lock", "update", "/tmp/conan.lock", "/tmp/conanfile.py"]],
            commands,
        )


class PathValidationTests(unittest.TestCase):
    """Ensure path resolution provides clear diagnostics."""

    def test_resolve_artifact_paths_requires_existing_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            lockfile = tmp_path / "conan.lock"
            recipe = tmp_path / "conanfile.py"
            lockfile.write_text("lock")
            recipe.write_text("from conan import ConanFile")

            resolved_lock, resolved_recipe = check_dependencies.resolve_artifact_paths(
                lockfile, recipe
            )
            self.assertEqual(lockfile, resolved_lock)
            self.assertEqual(recipe, resolved_recipe)

            missing = tmp_path / "missing.lock"
            with self.assertRaises(FileNotFoundError):
                check_dependencies.resolve_artifact_paths(missing, recipe)

    def test_missing_conan_cli_produces_clear_error(self) -> None:
        with self.assertRaises(FileNotFoundError):
            check_dependencies.ensure_conan_available("__definitely_missing_executable__")


class MainFlowTests(unittest.TestCase):
    """Integration-style coverage for the ``main`` entry point."""

    def setUp(self) -> None:
        self.tmpdir = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmpdir.cleanup)
        self.tmp_path = Path(self.tmpdir.name)
        self.lockfile = self.tmp_path / "conan.lock"
        self.recipe = self.tmp_path / "conanfile.py"
        self.lockfile.write_text("lock")
        self.recipe.write_text("from conan import ConanFile")

    def test_dry_run_skips_environment_validation(self) -> None:
        with mock.patch.object(check_dependencies, "ensure_conan_available") as ensure_mock, \
            mock.patch.object(check_dependencies, "run_commands") as run_mock:
            exit_code = check_dependencies.main(
                [
                    "--dry-run",
                    "--conan",
                    "__definitely_missing_executable__",
                    "--lockfile",
                    str(self.lockfile),
                    "--conanfile",
                    str(self.recipe),
                ]
            )
        self.assertEqual(0, exit_code)
        ensure_mock.assert_not_called()
        run_mock.assert_not_called()

    def test_executes_commands_when_not_dry_run(self) -> None:
        with mock.patch.object(check_dependencies, "ensure_conan_available") as ensure_mock, \
            mock.patch.object(check_dependencies, "run_commands") as run_mock:
            ensure_mock.return_value = None
            run_mock.return_value = None
            exit_code = check_dependencies.main(
                [
                    "--conan",
                    "conan",
                    "--lockfile",
                    str(self.lockfile),
                    "--conanfile",
                    str(self.recipe),
                    "--skip-install",
                ]
            )
        self.assertEqual(0, exit_code)
        ensure_mock.assert_called_once_with("conan")
        run_mock.assert_called_once_with(
            [["conan", "lock", "update", str(self.lockfile), str(self.recipe)]]
        )

    def test_require_clean_lock_detects_modification(self) -> None:
        with mock.patch.object(check_dependencies, "ensure_conan_available") as ensure_mock, \
            mock.patch.object(check_dependencies, "run_commands") as run_mock:
            ensure_mock.return_value = None

            def mutate_lockfile(commands: list[list[str]]) -> None:
                _ = commands  # suppress linter complaints
                self.lockfile.write_text("mutated")

            run_mock.side_effect = mutate_lockfile
            exit_code = check_dependencies.main(
                [
                    "--conan",
                    "conan",
                    "--lockfile",
                    str(self.lockfile),
                    "--conanfile",
                    str(self.recipe),
                    "--skip-install",
                    "--require-clean-lock",
                ]
            )
        self.assertEqual(1, exit_code)
        self.assertEqual("lock", self.lockfile.read_text())

    def test_require_clean_lock_restores_after_failure(self) -> None:
        with mock.patch.object(check_dependencies, "ensure_conan_available") as ensure_mock, \
            mock.patch.object(check_dependencies, "run_commands") as run_mock:
            ensure_mock.return_value = None

            def mutate_then_fail(commands: list[list[str]]) -> None:
                _ = commands
                self.lockfile.write_text("mutated")
                raise subprocess.CalledProcessError(returncode=3, cmd=["conan"])

            run_mock.side_effect = mutate_then_fail
            exit_code = check_dependencies.main(
                [
                    "--conan",
                    "conan",
                    "--lockfile",
                    str(self.lockfile),
                    "--conanfile",
                    str(self.recipe),
                    "--skip-install",
                    "--require-clean-lock",
                ]
            )
        self.assertEqual(3, exit_code)
        self.assertEqual("lock", self.lockfile.read_text())

    def test_require_clean_lock_allows_clean_execution(self) -> None:
        with mock.patch.object(check_dependencies, "ensure_conan_available") as ensure_mock, \
            mock.patch.object(check_dependencies, "run_commands") as run_mock:
            ensure_mock.return_value = None
            run_mock.return_value = None
            exit_code = check_dependencies.main(
                [
                    "--conan",
                    "conan",
                    "--lockfile",
                    str(self.lockfile),
                    "--conanfile",
                    str(self.recipe),
                    "--skip-install",
                    "--require-clean-lock",
                ]
            )
        self.assertEqual(0, exit_code)
        self.assertEqual("lock", self.lockfile.read_text())


if __name__ == "__main__":
    unittest.main()
