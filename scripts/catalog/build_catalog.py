#!/usr/bin/env python3
"""Builds the built-in preset catalog (FEATURES P9) from catalog.tsv.

Each row names a Lightroom preset of EmulsionPresets (MIT), the PixelFx category it goes in and
its PixelFx name. The presets are converted with the library's own importer (`pfx import`), so
the catalog maps settings exactly as importing them in the app would.

    scripts/catalog/build_catalog.py SOURCE_FOLDER [--pfx build/lib/pfx] [--out app/src/resources/presets]

SOURCE_FOLDER holds the .xmp files of an EmulsionPresets release. The output folder is replaced.
"""
import argparse
import pathlib
import re
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def slug(name):
    # Signs are spelled out, so "Exposure +1" and "Exposure -1" get files of their own.
    name = re.sub(r'-(?=\d)', ' minus ', name.replace('+', ' plus '))
    return re.sub(r'[^a-z0-9]+', '-', name.lower()).strip('-')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('--pfx', type=pathlib.Path, default=ROOT / 'build/lib/pfx')
    parser.add_argument('--out', type=pathlib.Path, default=ROOT / 'app/src/resources/presets')
    args = parser.parse_args()
    rows = [line.split('\t') for line in (HERE / 'catalog.tsv').read_text(encoding='utf-8').splitlines()
            if line and not line.startswith('#')]
    if args.out.exists():
        shutil.rmtree(args.out)
    skipped = {}
    for stem, category, name in rows:
        source = args.source / f'{stem}.xmp'
        target = args.out / category / f'{slug(name)}.pfxpreset'
        target.parent.mkdir(parents=True, exist_ok=True)
        done = subprocess.run([str(args.pfx), 'import', str(source), str(target), name], capture_output=True,
                              text=True, check=False)
        if done.returncode != 0:
            print(f'{stem}: {done.stderr.strip()}', file=sys.stderr)
            return 1
        for line in done.stdout.splitlines():
            if line.startswith('skipped '):
                skipped.setdefault(line[len('skipped '):], []).append(name)
    print(f'{len(rows)} presets written to {args.out}')
    for key, names in sorted(skipped.items()):
        print(f'skipped {key}: {", ".join(names)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
