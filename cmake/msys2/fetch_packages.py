#!/usr/bin/env python3
"""Fetch prebuilt MSYS2 UCRT64 packages into a cross-compilation sysroot.

The MinGW-w64 cross toolchain on Linux targets the same UCRT runtime as
MSYS2's UCRT64 environment, so MSYS2's binary packages can serve as the
Windows sysroot for libraries that are impractical to cross-build locally
(the GTK 4 / gtkmm stack used by ts-gtk).

Usage:
    fetch_packages.py <destination> <package> [<package> ...]

Resolves runtime dependencies from the repository database, verifies every
package against the SHA-256 recorded there, and unpacks them so that
<destination>/ucrt64/{bin,include,lib,share} form the sysroot. A stamp file
records the requested packages; rerunning with the same packages, or a
subset of them, is a no-op.
"""

import hashlib
import io
import re
import shutil
import sys
import tarfile
import urllib.request
from compression import zstd
from pathlib import Path

REPOSITORY = "https://repo.msys2.org/mingw/ucrt64"
PREFIX = "mingw-w64-ucrt-x86_64-"

# Python is only needed by MSYS2's developer scripts (gdbus-codegen and
# friends), never at runtime, and would otherwise pull in Tcl/Tk, SQLite and
# more. Python packages are skipped along with it.
SKIPPED_PREFIX = PREFIX + "python"


def Download(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def ParseDatabase(data: bytes) -> tuple[dict[str, dict[str, list[str]]], dict[str, str]]:
    """Returns name -> desc fields, and provided name -> package name."""
    packages: dict[str, dict[str, list[str]]] = {}
    provides: dict[str, str] = {}

    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        for member in archive.getmembers():
            if not member.name.endswith("/desc"):
                continue

            fields: dict[str, list[str]] = {}
            key = None
            for line in archive.extractfile(member).read().decode().splitlines():
                if line.startswith("%") and line.endswith("%"):
                    key = line.strip("%")
                    fields[key] = []
                elif line and key:
                    fields[key].append(line)

            name = fields["NAME"][0]
            packages[name] = fields
            for provided in fields.get("PROVIDES", []):
                provides[StripVersion(provided)] = name

    return packages, provides


def StripVersion(dependency: str) -> str:
    return re.split(r"[<>=]", dependency, maxsplit=1)[0]


def Resolve(roots: list[str], packages, provides) -> list[str]:
    resolved: list[str] = []
    pending = [PREFIX + root for root in roots]

    while pending:
        name = StripVersion(pending.pop())
        name = name if name in packages else provides.get(name, name)

        if name.startswith(SKIPPED_PREFIX) or name in resolved:
            continue
        if name not in packages:
            raise SystemExit(f"error: package not found in {REPOSITORY}: {name}")

        resolved.append(name)
        pending.extend(packages[name].get("DEPENDS", []))

    return sorted(resolved)


def Unpack(data: bytes, destination: Path) -> None:
    with tarfile.open(fileobj=io.BytesIO(zstd.decompress(data))) as archive:
        members = [member for member in archive.getmembers() if member.name.startswith("ucrt64/")]
        archive.extractall(destination, members=members, filter="data")


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2

    destination = Path(sys.argv[1])
    roots = sorted(sys.argv[2:])
    stamp = destination / "packages.stamp"
    request = " ".join(roots)

    # A subset of what's already there (e.g. a build without GTK) needs nothing new.
    if stamp.exists() and set(roots) <= set(stamp.read_text().splitlines()[0].split()):
        return 0

    print(f"Fetching MSYS2 UCRT64 packages for: {request}")
    packages, provides = ParseDatabase(Download(f"{REPOSITORY}/ucrt64.db"))
    selected = Resolve(roots, packages, provides)

    shutil.rmtree(destination, ignore_errors=True)
    destination.mkdir(parents=True)

    for name in selected:
        fields = packages[name]
        filename = fields["FILENAME"][0]
        data = Download(f"{REPOSITORY}/{filename}")

        if hashlib.sha256(data).hexdigest() != fields["SHA256SUM"][0]:
            raise SystemExit(f"error: checksum mismatch for {filename}")

        print(f"  {filename}")
        Unpack(data, destination)

    stamp.write_text(request + "\n" + "\n".join(packages[name]["FILENAME"][0] for name in selected) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
