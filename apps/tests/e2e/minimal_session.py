#!/usr/bin/env python3
"""Minimal end-to-end harness for cyphesis and ember.

This script launches a cyphesis server and an ember client, performs a
very small interaction and verifies that the connection succeeds.

If the required executables are not found it will simply exit which makes
it safe to run in environments where the project has not been built.
"""
from __future__ import annotations

import atexit
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path


def _find_build_dir(preset: str) -> Path | None:
    """Return the CMake build directory for the given preset.

    The CI pipeline generates a CMakeUserPresets.json which is inspected
    here to locate the build tree.
    """
    preset_files = ["CMakeUserPresets.json", "CMakePresets.json"]
    for filename in preset_files:
        path = Path(filename)
        if path.exists():
            with path.open("r", encoding="utf-8") as fh:
                data = json.load(fh)
            for entry in data.get("buildPresets", []):
                if entry.get("name") == preset:
                    return Path(entry["binaryDir"])
    return None


def main() -> int:
    preset = os.environ.get("PROFILE_CONAN", "conan-debug")
    build_dir = _find_build_dir(preset)
    if build_dir is None:
        print("Build preset not found; skipping e2e test")
        return 0

    cyphesis = build_dir / "apps" / "cyphesis" / "cyphesis"
    ember = build_dir / "apps" / "ember" / "ember"
    if not cyphesis.exists() or not ember.exists():
        print("Required executables missing; skipping e2e test")
        return 0

    port = 6767
    tmpdir = Path(tempfile.mkdtemp(prefix="wf-e2e-"))
    processes: list[subprocess.Popen] = []

    def _cleanup() -> None:
        for proc in processes:
            try:
                proc.terminate()
                proc.wait(timeout=5)
            except Exception:
                proc.kill()
        shutil.rmtree(tmpdir, ignore_errors=True)

    atexit.register(_cleanup)

    # Start cyphesis server
    server = subprocess.Popen(
        [
            str(cyphesis),
            f"--cyphesis:tcpport={port}",
            f"--cyphesis:vardir={tmpdir}",
            "--cyphesis:dbcleanup=true",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    processes.append(server)

    # Wait for the server to start listening
    deadline = time.time() + 30
    started = False
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1) as sock:
                # Reading banner or any data verifies that the server responded.
                sock.settimeout(1)
                try:
                    sock.recv(1024)
                except socket.timeout:
                    pass
                started = True
                break
        except OSError:
            time.sleep(0.5)
    if not started:
        raise RuntimeError("cyphesis did not start in time")

    # Ensure the database has been cleaned before running tests.
    # Use a short connection timeout to fail fast when the database is unreachable.
    conn = None
    try:
        import psycopg
        try:
            conn = psycopg.connect("dbname=cyphesis", connect_timeout=5)
        except psycopg.OperationalError as exc:
            print(f"Database unavailable: {exc}")
    except Exception:
        try:
            import psycopg2 as psycopg
            try:
                conn = psycopg.connect(dbname="cyphesis", connect_timeout=5)
            except psycopg.OperationalError as exc:
                print(f"Database unavailable: {exc}")
        except Exception:
            conn = None
    if conn is not None:
        try:
            with conn:
                with conn.cursor() as cur:
                    cur.execute("SELECT COUNT(*) FROM entities")
                    count = cur.fetchone()[0]
                    if count != 1:
                        raise RuntimeError("database not clean")
        finally:
            conn.close()
    else:
        print("Database check skipped")

    # Launch ember client and attempt connection
    client = subprocess.Popen(
        [str(ember), f"--connect=localhost:{port}", "--quit"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    processes.append(client)
    client.wait(timeout=60)
    output = client.stdout.read() if client.stdout else ""

    if "Connected" not in output and client.returncode != 0:
        raise RuntimeError("ember failed to connect to cyphesis")

    print("E2E session completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
