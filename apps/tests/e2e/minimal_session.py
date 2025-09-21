#!/usr/bin/env python3
"""Minimal end-to-end harness for cyphesis and ember.

This script launches a cyphesis server and an ember client, performs a
very small interaction and verifies that the connection succeeds.

If the required executables are not found it will simply exit which makes
it safe to run in environments where the project has not been built.
"""
from __future__ import annotations

import json
import os
import shutil
import socket
import subprocess
import tempfile
import time
from contextlib import ExitStack, closing, contextmanager
from pathlib import Path
from typing import Callable, Iterable, Sequence


def _find_build_dir(preset: str) -> Path | None:
    """Return the CMake build directory for the given preset.

    The CI pipeline generates preset files which are inspected here to locate
    the build tree.  Invalid JSON or missing entries are ignored so that we can
    fall back to other candidates.
    """

    preset_files = ("CMakeUserPresets.json", "CMakePresets.json")
    for filename in preset_files:
        path = Path(filename)
        if not path.exists():
            continue
        try:
            with path.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
        except json.JSONDecodeError as exc:
            print(f"Failed to parse {path}: {exc}")
            continue
        for entry in data.get("buildPresets", []):
            if entry.get("name") != preset:
                continue
            binary_dir = entry.get("binaryDir")
            if isinstance(binary_dir, str) and binary_dir:
                return Path(binary_dir)
    return None


_PG_DSN_ENV = "WF_E2E_PG_DSN"


def _resolve_build_dir(preset: str) -> Path | None:
    """Locate the build directory honouring manual overrides."""

    override = os.environ.get("WF_E2E_BINARY_DIR")
    if override:
        candidate = Path(override).expanduser()
        if candidate.exists():
            return candidate
        print(f"Override path {candidate} does not exist; falling back to presets")
        return None
    return _find_build_dir(preset)


class DatabaseDriver:
    """Definition of a database connector and its error semantics."""

    __slots__ = ("name", "connect", "operational_errors")

    def __init__(
        self,
        name: str,
        connect: Callable[[], object],
        operational_errors: tuple[type[BaseException], ...],
    ) -> None:
        self.name = name
        self.connect = connect
        self.operational_errors = operational_errors


def _managed_connection(conn: object):
    """Yield a context manager for ``conn`` closing it on exit if needed."""

    if hasattr(conn, "__enter__") and hasattr(conn, "__exit__"):
        return conn  # type: ignore[return-value]
    return closing(conn)  # type: ignore[arg-type]


@contextmanager
def _managed_cursor(conn: object):
    """Provide a context-managed cursor for any DB-API connection."""

    cursor = conn.cursor()
    if hasattr(cursor, "__enter__") and hasattr(cursor, "__exit__"):
        with cursor:
            yield cursor
    else:
        with closing(cursor):
            yield cursor


def _load_database_drivers() -> list[DatabaseDriver]:
    """Discover optional PostgreSQL client libraries available locally."""

    drivers: list[DatabaseDriver] = []
    dsn = _database_dsn()
    for module_name in ("psycopg", "psycopg2"):
        try:
            module = __import__(module_name)
        except Exception:
            continue
        connect = getattr(module, "connect", None)
        if connect is None:
            continue
        operational_error = getattr(module, "OperationalError", Exception)

        def _connector(connect=connect):
            return connect(dsn, connect_timeout=5)

        drivers.append(
            DatabaseDriver(
                name=module_name,
                connect=_connector,
                operational_errors=(operational_error,),
            )
        )
    return drivers


def _database_dsn(default: str = "dbname=cyphesis") -> str:
    """Return the connection string used for PostgreSQL health checks.

    Operators can override the default DSN through the ``WF_E2E_PG_DSN``
    environment variable.  Empty strings are ignored so that misconfigured
    environments still fall back to the safe default which targets the local
    ``cyphesis`` database created by the build.
    """

    override = os.environ.get(_PG_DSN_ENV)
    if override is not None:
        candidate = override.strip()
        if candidate:
            return candidate
    return default


def _ensure_database_clean(drivers: Iterable[DatabaseDriver]) -> None:
    """Verify that the Cyphesis database is in a pristine state."""

    for driver in drivers:
        try:
            conn = driver.connect()
        except driver.operational_errors as exc:  # type: ignore[misc]
            print(f"Database unavailable via {driver.name}: {exc}")
            continue
        except Exception:
            continue

        try:
            with _managed_connection(conn) as connection:
                with _managed_cursor(connection) as cursor:
                    cursor.execute("SELECT COUNT(*) FROM entities")
                    count = cursor.fetchone()[0]
                    if count != 1:
                        raise RuntimeError("database not clean")
            return
        finally:
            close = getattr(conn, "close", None)
            if callable(close):
                close()

    print("Database check skipped")


def _spawn_process(args: Sequence[object], stack: ExitStack) -> subprocess.Popen[str]:
    """Launch ``args`` as a child process and register cleanup handlers."""

    proc = subprocess.Popen(
        [str(part) for part in args],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    def _terminate(process: subprocess.Popen[str]) -> None:
        if process.poll() is None:
            try:
                process.terminate()
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if process.stdout:
            process.stdout.close()

    stack.callback(_terminate, proc)
    return proc


def _wait_for_port(host: str, port: int, timeout: float) -> bool:
    """Return ``True`` if ``host:port`` becomes reachable within ``timeout``."""

    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=1) as sock:
                sock.settimeout(1)
                try:
                    sock.recv(1024)
                except socket.timeout:
                    pass
                return True
        except OSError:
            time.sleep(0.5)
    return False


def _select_free_port() -> int:
    """Reserve an available TCP port on the loopback interface."""

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main() -> int:
    preset = os.environ.get("PROFILE_CONAN", "conan-debug")
    build_dir = _resolve_build_dir(preset)
    if build_dir is None:
        print("Build preset not found; skipping e2e test")
        return 0

    cyphesis = build_dir / "apps" / "cyphesis" / "cyphesis"
    ember = build_dir / "apps" / "ember" / "ember"
    if not cyphesis.exists() or not ember.exists():
        print("Required executables missing; skipping e2e test")
        return 0

    port = _select_free_port()
    print(f"Using TCP port {port}")

    with ExitStack() as stack:
        tmpdir = Path(stack.enter_context(tempfile.TemporaryDirectory(prefix="wf-e2e-")))
        stack.callback(shutil.rmtree, tmpdir, True)

        server = _spawn_process(
            [
                cyphesis,
                f"--cyphesis:tcpport={port}",
                f"--cyphesis:vardir={tmpdir}",
                "--cyphesis:dbcleanup=true",
            ],
            stack,
        )

        if not _wait_for_port("127.0.0.1", port, timeout=30):
            raise RuntimeError(f"cyphesis did not start in time on port {port}")

        _ensure_database_clean(_load_database_drivers())

        client = _spawn_process([
            ember,
            f"--connect=localhost:{port}",
            "--quit",
        ], stack)
        try:
            stdout, _ = client.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            client.kill()
            stdout, _ = client.communicate()
            raise

    if "Connected" not in stdout or client.returncode != 0:
        print(f"ember failed to connect to cyphesis on port {port}")
        return client.returncode or 1

    print("E2E session completed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(exc)
        raise SystemExit(1)
