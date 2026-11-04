#!/usr/bin/env python3
"""Downloads Qt binary archives from download.qt.io for CI, checking each archive's SHA-1.

Qt 6.11 moved the Windows packages into one repository folder per compiler
(qt6_6113/qt6_6113_msvc2022_64/), which aqtinstall 3.3.0 cannot read yet; this reads that layout.

  fetch-qt.py --host windows_x86 --version 6.11.3 --compiler msvc2022_64 \
      --modules qtbase qtsvg qtimageformats --dest C:/Qt

Extracts with 7-Zip (`7z`) or `bsdtar` into DEST; prints the Qt prefix (DEST/<version>/<compiler>).
"""
import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

BASE = "https://download.qt.io/online/qtsdkrepository"


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def archives_for(updates: ET.Element, version_tag: str, arch: str, modules: list[str]) -> list[tuple[str, str, str]]:
    """(package name, package version, archive file) for every wanted module."""
    found = []
    wanted = set(modules)
    for package in updates.findall("PackageUpdate"):
        name = package.findtext("Name") or ""
        if not name.endswith(arch) or "debug" in name:
            continue
        is_main = name == f"qt.qt6.{version_tag}.{arch}"
        is_addon = any(name == f"qt.qt6.{version_tag}.addons.{m}.{arch}" for m in modules)
        if not (is_main or is_addon):
            continue
        for archive in (package.findtext("DownloadableArchives") or "").split(","):
            archive = archive.strip()
            module = archive.split("-", 1)[0]
            if archive and module in wanted:
                found.append((name, package.findtext("Version") or "", archive))
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="windows_x86, linux_x64, mac_x64")
    parser.add_argument("--version", required=True, help="for example 6.11.3")
    parser.add_argument("--compiler", required=True, help="for example msvc2022_64")
    parser.add_argument("--arch", help="package suffix, default win64_<compiler> on Windows")
    parser.add_argument("--modules", nargs="+", required=True)
    parser.add_argument("--dest", required=True, type=Path)
    args = parser.parse_args()

    tag = args.version.replace(".", "")
    arch = args.arch or (f"win64_{args.compiler}" if args.host.startswith("windows") else args.compiler)
    repo = f"{BASE}/{args.host}/desktop/qt6_{tag}/qt6_{tag}_{args.compiler}"
    updates = ET.fromstring(fetch(f"{repo}/Updates.xml"))
    archives = archives_for(updates, tag, arch, args.modules)
    missing = set(args.modules) - {a.split("-", 1)[0] for _, _, a in archives}
    if missing:
        print(f"not in {repo}: {', '.join(sorted(missing))}", file=sys.stderr)
        return 1
    extractor = shutil.which("7z") or shutil.which("bsdtar")
    if extractor is None:
        print("7z (7-Zip) or bsdtar is needed on PATH", file=sys.stderr)
        return 1
    # Qt 6.11's archives hold the prefix's contents directly (bin, lib, plugins ...).
    prefix = args.dest / args.version / args.compiler
    prefix.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for package, version, archive in archives:
            url = f"{repo}/{package}/{version}{archive}"
            data = fetch(url)
            expected = fetch(url + ".sha1").decode().split()[0].strip()
            actual = hashlib.sha1(data).hexdigest()
            if actual != expected:
                print(f"checksum mismatch for {archive}: {actual} != {expected}", file=sys.stderr)
                return 1
            path = Path(tmp) / archive
            path.write_bytes(data)
            if Path(extractor).stem == "7z":
                command = [extractor, "x", "-y", f"-o{prefix}", str(path)]
            else:
                command = [extractor, "-xf", str(path), "-C", str(prefix)]
            subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
            print(f"installed {archive}", file=sys.stderr)
    print(prefix)
    return 0


if __name__ == "__main__":
    sys.exit(main())
