"""Unit tests for helpers inside :mod:`apps.tests.e2e.minimal_session`."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path


_SPEC = importlib.util.spec_from_file_location(
    "apps.tests.e2e.minimal_session",
    Path(__file__).resolve().parent / "minimal_session.py",
)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("Unable to import minimal_session module for testing")
minimal_session = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(minimal_session)  # type: ignore[union-attr]


def test_find_build_dir_from_presets(tmp_path, monkeypatch):
    """The locator should return the binary directory from preset files."""

    build_dir = tmp_path / "debug"
    data = {"buildPresets": [{"name": "conan-debug", "binaryDir": str(build_dir)}]}
    (tmp_path / "CMakePresets.json").write_text(json.dumps(data))
    monkeypatch.chdir(tmp_path)
    monkeypatch.delenv("WF_E2E_BINARY_DIR", raising=False)

    assert minimal_session._find_build_dir("conan-debug") == build_dir


def test_find_build_dir_handles_invalid_json(tmp_path, monkeypatch, capsys):
    """Invalid JSON should not raise and should emit a helpful diagnostic."""

    (tmp_path / "CMakePresets.json").write_text("{not json")
    monkeypatch.chdir(tmp_path)
    monkeypatch.delenv("WF_E2E_BINARY_DIR", raising=False)

    assert minimal_session._find_build_dir("conan-debug") is None
    assert "Failed to parse" in capsys.readouterr().out


def test_resolve_build_dir_prefers_override(tmp_path, monkeypatch):
    """When the override is present it takes precedence over presets."""

    override = tmp_path / "custom"
    override.mkdir()
    monkeypatch.setenv("WF_E2E_BINARY_DIR", str(override))

    assert minimal_session._resolve_build_dir("anything") == override


def test_resolve_build_dir_invalid_override(tmp_path, monkeypatch, capsys):
    """A missing override should return ``None`` and warn the caller."""

    missing = tmp_path / "missing"
    monkeypatch.setenv("WF_E2E_BINARY_DIR", str(missing))

    assert minimal_session._resolve_build_dir("anything") is None
    assert "does not exist" in capsys.readouterr().out
