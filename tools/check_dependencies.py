#!/usr/bin/env python3
"""Update and validate Conan dependencies."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(cmd):
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def main():
    lockfile = ROOT / "conan.lock"
    conanfile = ROOT / "conanfile.py"
    run(["conan", "lock", "update", str(lockfile), str(conanfile)])
    run(["conan", "install", str(conanfile), "--lockfile", str(lockfile), "--build=never"])


if __name__ == "__main__":
    main()
