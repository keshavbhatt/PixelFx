#!/usr/bin/env python3
"""Fails when a build file names another version than scripts/deps-pins.sh (ADR-015).

Every build ships the same libraries: dev builds, Linux CI and the snap read the pins through
scripts/build-deps.sh; Windows and macOS build them through vcpkg.json and the ports in
packaging/vcpkg/ports, and CI installs Qt at QT_VERSION. This checks those files say the same.

    python3 scripts/check-pins.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# vcpkg port name -> the pin's prefix in deps-pins.sh.
VCPKG_PINS = {
    "libvips": "VIPS", "libheif": "HEIF", "libde265": "DE265", "dav1d": "DAV1D", "libjxl": "JXL",
    "highway": "HWY", "libraw": "RAW", "lensfun": "LENSFUN", "libjpeg-turbo": "JPEG",
    "libpng": "PNG", "tiff": "TIFF", "libwebp": "WEBP", "lcms": "LCMS", "libexif": "EXIF",
    "zlib": "ZLIB", "brotli": "BROTLI", "expat": "EXPAT", "glib": "GLIB",
}


def pins():
    out = {}
    for line in (ROOT / "scripts/deps-pins.sh").read_text().splitlines():
        m = re.match(r"^([A-Z0-9_]+)=(\S+)$", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def main():
    p = pins()
    problems = []

    ci = (ROOT / ".github/workflows/ci.yml").read_text()
    m = re.search(r'QT_VERSION:\s*"([^"]+)"', ci)
    if not m or m.group(1) != p["QT_VERSION"]:
        problems.append(f"ci.yml installs Qt {m.group(1) if m else '?'}, the pins say {p['QT_VERSION']}")

    # The snap builds through scripts/build-deps.sh; a source of its own would be a second pin.
    snap = (ROOT / "snap/snapcraft.yaml").read_text()
    if "source-checksum" in snap or re.search(r"^\s+source:\s*https?://", snap, re.M):
        problems.append("snapcraft.yaml downloads a library itself; it must build through scripts/build-deps.sh")

    manifest = json.loads((ROOT / "vcpkg.json").read_text())
    for o in manifest.get("overrides", []):
        prefix = VCPKG_PINS.get(o["name"])
        if prefix and o.get("version") != p[f"{prefix}_VERSION"]:
            problems.append(f"vcpkg.json pins {o['name']} {o.get('version')}, the pins say {p[prefix + '_VERSION']}")
    ports = ROOT / "packaging/vcpkg/ports"
    for port in sorted(ports.glob("*/vcpkg.json")) if ports.exists() else []:
        data = json.loads(port.read_text())
        prefix = VCPKG_PINS.get(data["name"])
        version = data.get("version") or data.get("version-semver") or data.get("version-string")
        if prefix and version != p[f"{prefix}_VERSION"]:
            problems.append(f"the {data['name']} port is {version}, the pins say {p[prefix + '_VERSION']}")

    for problem in problems:
        print(f"check-pins: {problem}", file=sys.stderr)
    if not problems:
        print(f"check-pins: every build file agrees with scripts/deps-pins.sh ({len(p)} pins)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
