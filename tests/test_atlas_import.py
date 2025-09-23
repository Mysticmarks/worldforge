"""Regression tests for importing the in-tree Atlas Python package."""

from __future__ import annotations

from pathlib import Path


def test_atlas_package_is_importable() -> None:
    """The Atlas Python package should be importable from the source tree."""

    import atlas  # noqa: PLC0415  # imported inside the test on purpose

    atlas_path = Path(atlas.__file__).resolve()
    assert atlas_path.is_file()
    # The package should resolve from the repository checkout rather than
    # any global site-packages installation.
    assert "Atlas-Python" in atlas_path.parts
