# Design review: more-apps.html, about.html, menu.html (More apps from Ktechpit)

Reviewed 2026-11-10 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: people who like PixelFx can find the developer's other apps where they
already look for "who made this", without PixelFx ever interrupting them. Signature: two cards
at the end of About, drawn afresh each time, and the full list one click away.

## Where it comes from

- The owner's request of 2026-11-10: integrate the ktechpit-apps module so people discover the
  other Ktechpit apps.
- The module (`~/DCode/ktechpit-apps`, MIT, the owner's, v1.0.1): `AppCatalog` (the list from
  ktechpit.com's `api/apps.php`, a copy built in, refreshed at most weekly), `AppStrip`, `AppCard`
  and `MoreAppsDialog`. Its own rules: only where the user opens something, at most one strip per
  screen, a setting turns it off. The owner decided on 2026-11-10 that PixelFx has no such setting:
  the strip is always shown in About, which people open on purpose.
- Module change needed (1.1.0, a minor version): the full list as a widget (`MoreAppsView`) a host
  can put in its own sheet, and a host hook for See all, so PixelFx shows the list as a sheet
  like every other window of PixelFx instead of the module's plain dialog.

## Measurements

- About: under the credits, a 1 px `--border` divider, then the strip: a 12 px semibold heading
  "More apps from Ktechpit" with See all (12 px `--link`) at its right, 6 px, then two cards side
  by side with 10 px between, each half of the sheet's 520 px content width. A card: 10 px
  padding, 1 px `--border`, radius 8, a 40 px icon, 10 px, the name 12 px semibold in `--text` and
  up to two lines of 12 px `--muted`, then Get, a small button (26 px). The About sheet grows by
  about 96 px.
- More apps from Ktechpit: the Wide sheet (820 px), resizable from its edges like the Picker
  sheets; a 12 px `--muted` intro line, the search field with Refresh (small button) beside it, a
  grid of three columns of the same cards, 8 px apart; the footer's status line in 12 px
  `--muted` and Close.
- Main menu: "More apps from Ktechpit" with the grid glyph, under About PixelFx.
- Contrast on `--panel`, from the tokens: `--text` 14.1:1 dark, 16.0:1 light; `--muted` 7.0:1 and
  6.8:1; `--link` 8.2:1 and 5.3:1; the card outline `--border` is decoration, the card's Get
  button carries the action.

## Accessibility

- Each card is one focus stop for its page (Enter opens it) and Get is its own button with a
  tooltip ("Install ColorWall"); Installed is disabled and says why in its tooltip. The search
  field is labelled "Search apps"; Refresh has a tooltip. Escape closes the sheet.
- Nothing by colour alone: Installed is a word, not a tint.

## Platform conventions

- Get opens the software centre for the way PixelFx was installed: `snap://<name>` in a snap,
  `appstream://<id>` in the Flatpak (GNOME Software or Discover), the product page on Windows and
  macOS. The card opens the app's page on ktechpit.com in the browser, with `?ref=pixelfx-about`
  or `pixelfx-menu`.
- Network: one GET of the list and the icons, at most once a week, about 20 seconds after start,
  with a plain user agent; nothing about the user. The Flatpak already has the network (for the
  purchase check); the snap has the network plug.

## Visual craft

- Cards use the sheet's tokens: no new colour, no accent fill; Get is the same small neutral
  button as Copy. The strip sits after the credits, so About still reads as About.
- Template check: a "more from this developer" strip is common in mobile app stores; on the
  desktop it is right only behind something the user opened, which is the module's rule and this
  placement.

## Interaction

- Nothing appears by itself: not at start, not in What's new (it opens on its own after an
  update), not on the welcome page. About and the menu entry are opened on purpose.
- The strip draws two apps at random each time About opens (trending apps more often), never one
  installed here, so over a few visits people see most of the list.

## Writing

- "More apps from Ktechpit", "See all", "Get", "Installed", "Search apps", "Refresh", "Up to date:
  21 apps." "Other apps by the developer of PixelFx. Get opens your software centre." No sales
  words.
- The descriptions come from the site's catalog; some are dated (Glate is still "Google
  translator for Linux Desktop" there): fix them on the site, not in PixelFx.

## Remove one thing

Removed: the module's "More apps..." button under the strip; See all in the heading does the
same, and one way to the list is enough in a sheet this size.
