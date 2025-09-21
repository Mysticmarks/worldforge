"""Unit tests for helpers inside :mod:`apps.tests.e2e.minimal_session`."""

from __future__ import annotations

import importlib.util
import json
import sys
import types
from pathlib import Path

import pytest


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


class _DummyCursor:
    def __init__(self, count: int) -> None:
        self._count = count

    def __enter__(self) -> "_DummyCursor":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[override]
        return None

    def execute(self, query: str) -> None:
        assert query == "SELECT COUNT(*) FROM entities"

    def fetchone(self) -> tuple[int]:
        return (self._count,)


class _DummyConnection:
    def __init__(self, count: int) -> None:
        self._count = count
        self.closed = False

    def __enter__(self) -> "_DummyConnection":  # type: ignore[override]
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[override]
        self.close()
        return None

    def cursor(self) -> _DummyCursor:
        return _DummyCursor(self._count)

    def close(self) -> None:
        self.closed = True


def test_load_database_drivers_discovers_available_modules(monkeypatch):
    """Optional database clients should be converted into driver objects."""

    fake_module = types.SimpleNamespace(
        OperationalError=RuntimeError,
        connect=lambda dsn, connect_timeout: _DummyConnection(count=1),
    )
    monkeypatch.setitem(sys.modules, "psycopg", fake_module)

    drivers = minimal_session._load_database_drivers()
    assert [driver.name for driver in drivers] == ["psycopg"]

    # The connector should return a usable connection without raising.
    connection = drivers[0].connect()
    assert isinstance(connection, _DummyConnection)


def test_ensure_database_clean_handles_operational_errors(capsys):
    """Operational failures should emit diagnostics and continue."""

    driver = minimal_session.DatabaseDriver(
        name="failing",
        connect=lambda: (_ for _ in ()).throw(RuntimeError("down")),
        operational_errors=(RuntimeError,),
    )

    minimal_session._ensure_database_clean([driver])

    out = capsys.readouterr().out
    assert "Database unavailable via failing" in out
    assert "Database check skipped" in out


def test_ensure_database_clean_validates_entity_count():
    """A database with unexpected entities should raise an error."""

    driver = minimal_session.DatabaseDriver(
        name="dirty",
        connect=lambda: _DummyConnection(count=2),
        operational_errors=(RuntimeError,),
    )

    with pytest.raises(RuntimeError, match="database not clean"):
        minimal_session._ensure_database_clean([driver])


def test_ensure_database_clean_succeeds_for_clean_database():
    """When a driver succeeds and the database is clean, no output is emitted."""

    driver = minimal_session.DatabaseDriver(
        name="clean",
        connect=lambda: _DummyConnection(count=1),
        operational_errors=(RuntimeError,),
    )

    minimal_session._ensure_database_clean([driver])
