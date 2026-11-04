# Licensing

PixelFx is open source. Three licences apply, by path.

## The library: Apache-2.0

`libs/pixelfx-core/` (libpixelfx) is licensed under the Apache License 2.0
(`libs/pixelfx-core/LICENSE`, `LICENSES/Apache-2.0.txt`), so any program, open or closed, may
use it. Its dependencies are permissive or LGPL (`THIRD_PARTY.md`).

## The program: GPL-3.0-or-later

Everything else in this repository, except the licensing module (next section), is licensed
under the GNU General Public License, version 3 or, at your option, any later version. The
text is in `LICENSE` and `LICENSES/GPL-3.0-or-later.txt`. You may use, study, change and share
it under the GPL's terms: derived works stay under the GPL and come with their source.

### Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by combining it with the Ktechpit Licensing
Module (the files listed below), the licensors of this program grant you additional permission
to convey the resulting work, provided the Module is unmodified and the resulting work is an
official build published by Ktechpit. A work under the GPL that is built or conveyed by anyone
else must not contain the Module.

## The licensing module: Ktechpit Licensing Module License

The account and licence code is not part of the GPL-licensed work. It is source-available under
the Ktechpit Licensing Module License (`LICENSES/LicenseRef-Ktechpit-Licensing-Module.txt`): you
may read it and build it unmodified for your own use, and you may not modify it, bypass it,
redistribute it or use its endpoints in other software. These paths (reserved; the module
arrives with a later milestone):

- `app/src/modules/AccountAndLicense/`
- `app/src/core/licensing/`
- `app/src/services/licensing/`

`REUSE.toml` records the same mapping in machine-readable form.

## What this means for forks and contributors

- Contributions to the GPL part are welcome under the GPL, and to the library under Apache-2.0.
- A fork that ships its own builds must leave the module out and replace what it provides, and
  must not talk to Ktechpit's licence servers.
- Official builds (the snap, the Flatpak and other packages published by Ktechpit) combine both
  parts under the additional permission above.

## Earlier versions

PixelFx 1 (2020 to 2022) is on the `legacy-v1` branch and the `v1-final` tag, under the GPL-3.0
text it shipped with. Its image tool, `pixel`, was MIT and is archived at
github.com/keshavbhatt/pixel; libpixelfx replaces it.
