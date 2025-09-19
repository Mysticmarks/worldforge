#!/usr/bin/env python3
"""Update and validate Conan dependencies.

The script is typically executed by continuous integration jobs to ensure
that ``conan.lock`` remains in sync with ``conanfile.py``.  Historically the
implementation provided minimal validation which made troubleshooting
failures harder for developers running the helper locally.  The refactored
variant adds lightweight argument parsing, path validation and a check for
the Conan executable so that feedback is immediate and actionable.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, List, Sequence

ROOT = Path(__file__).resolve().parent.parent


def ensure_conan_available(executable: str) -> None:
    """Ensure that the Conan CLI is available on the system ``PATH``.

    Parameters
    ----------
    executable:
        Name or path of the Conan executable.

    Raises
    ------
    FileNotFoundError
        If the executable could not be found on the ``PATH``.
    """

    if shutil.which(executable) is None:
        raise FileNotFoundError(
            f"Unable to locate '{executable}'. Install Conan or adjust PATH."
        )


def resolve_artifact_paths(lockfile: Path, conanfile: Path) -> tuple[Path, Path]:
    """Validate and normalise the key Conan metadata file paths."""

    if not lockfile.exists():
        raise FileNotFoundError(f"Missing Conan lock file: {lockfile}")
    if not conanfile.exists():
        raise FileNotFoundError(f"Missing Conan recipe: {conanfile}")
    return lockfile, conanfile


def build_command_sequence(
    conan: str,
    lockfile: Path,
    conanfile: Path,
    build_policy: str,
    run_install: bool,
    install_args: Sequence[str],
) -> List[List[str]]:
    """Construct the list of Conan invocations required for validation."""

    commands: List[List[str]] = [
        [conan, "lock", "update", str(lockfile), str(conanfile)]
    ]
    if run_install:
        install_cmd = [
            conan,
            "install",
            str(conanfile),
            "--lockfile",
            str(lockfile),
        ]
        if build_policy:
            install_cmd.append(f"--build={build_policy}")
        install_cmd.extend(install_args)
        commands.append(install_cmd)
    return commands


def run_commands(commands: Iterable[Sequence[str]]) -> None:
    """Execute the provided commands and stream their output."""

    for cmd in commands:
        print("+", " ".join(cmd))
        subprocess.run(cmd, check=True, cwd=ROOT)


def make_absolute(path: Path) -> Path:
    """Resolve ``path`` relative to the repository root if necessary."""

    return path if path.is_absolute() else ROOT / path


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    """Parse command line arguments for the helper script."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--conan",
        default="conan",
        help="Name or path of the Conan executable to invoke",
    )
    parser.add_argument(
        "--lockfile",
        type=Path,
        default=ROOT / "conan.lock",
        help="Path to the Conan lockfile (defaults to repository root)",
    )
    parser.add_argument(
        "--conanfile",
        type=Path,
        default=ROOT / "conanfile.py",
        help="Path to the Conan recipe file (defaults to repository root)",
    )
    parser.add_argument(
        "--build",
        dest="build_policy",
        default="never",
        help="Build policy passed to 'conan install' (e.g. 'missing')",
    )
    parser.add_argument(
        "--skip-install",
        action="store_true",
        help="Skip the 'conan install' step and only refresh the lockfile",
    )
    parser.add_argument(
        "--install-arg",
        dest="install_args",
        action="append",
        default=[],
        help="Additional arguments forwarded to 'conan install'",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the commands without executing them",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Program entry point."""

    args = parse_args(argv)
    try:
        ensure_conan_available(args.conan)
        lockfile, conanfile = resolve_artifact_paths(
            make_absolute(args.lockfile), make_absolute(args.conanfile)
        )
        commands = build_command_sequence(
            args.conan,
            lockfile,
            conanfile,
            args.build_policy,
            not args.skip_install,
            args.install_args,
        )
        if args.dry_run:
            for cmd in commands:
                print("+", " ".join(cmd))
            return 0
        run_commands(commands)
    except FileNotFoundError as error:
        print(error, file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        return error.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
