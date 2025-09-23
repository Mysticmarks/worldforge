"""Pytest configuration for the Worldforge monorepo.

This test suite mixes Python-only components with libraries that are
normally built as part of the full CMake workflow.  The Atlas Python
package lives under ``libs/atlas/src/Atlas-Python`` and is not installed
into the interpreter's site-packages when running the in-tree tests.

Historically, downstream projects relied on ``PYTHONPATH`` tweaks to make
``import atlas`` succeed.  In the CI environment used for these kata
exercises we execute the tests directly from the repository root, so we
replicate that behaviour programmatically to keep the suite hermetic.
"""

from __future__ import annotations

import sys
from pathlib import Path


def _ensure_atlas_on_path() -> None:
    """Add the in-tree Atlas Python package to ``sys.path``.

    The Atlas package ships as part of the source tree and is expected to
    be importable during unit testing without performing a full
    installation.  We preprend the package directory so local changes are
    picked up before any system-wide installation that might exist on the
    developer machine.
    """

    atlas_root = Path(__file__).resolve().parent / "libs" / "atlas" / "src" / "Atlas-Python"
    atlas_pkg = atlas_root / "atlas"
    if not atlas_pkg.exists():
        return

    path_str = str(atlas_root)
    if path_str not in sys.path:
        sys.path.insert(0, path_str)


_ensure_atlas_on_path()
