# Design review: editor-presets-catalog.html (the preset catalog)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: find a style for this photo among hundreds, see it on your own photo,
apply it and set how much. Signature: categories of tiles of your photo, and one Amount that
blends the preset in from 0 to 200, while the real sliders show what it did (owner,
2026-11-05: presets, not looks, so every value can be seen and changed).

## Where it comes from

- **Lightroom** groups presets into collapsible folders, previews on hover, and since 2023 gives
  every preset an Amount from 0 to 200. The mock keeps all three.
- **Lightroom mobile and VSCO** show presets as pictures of the photo, by category. The mock
  shows tiles, as the approved Looks tab does, so both tabs work the same way.
- The approved Presets list (`editor-presets.html`) had rows with small pictures; at hundreds of
  presets, rows of 44 px pictures are too small to judge a look, so the catalog uses the Looks
  tab's tiles. My presets keep their place at the top.

## Measurements

- Tiles: two columns of 121 px in the 272 px panel, 3:2 pictures, names 12 px on one line, cut
  with "..." and complete in the tooltip (the first draft let a long name wrap and break the
  row). Name text 14.1:1 dark, 16.0:1 light on the tile.
- Each open category shows six tiles (three rows, 348 px), so four categories fit in a window
  900 px high; "All 64" opens the rest. Six is also what the app renders at once per category,
  so opening the tab stays fast with 451 presets.
- Category headings: 11 px bold muted, 7.0:1 dark and 6.8:1 light; "All 64" in the accent,
  6.9:1 and 5.3:1. The whole heading row is the 32 px target that opens or closes it.
- Favourite star: #FFD43B on a 55% black chip over the picture: 14.7:1 over black, 3.3:1 over
  pure white, so at least the 3:1 a graphic needs whatever the photo; the star is also said in
  the tile's name ("Portrait film 400, in Favorites").
- Amount: the approved slider row, 0 to 200, 100 by default, double-click back to 100, typed
  values as everywhere.

## Interaction

- Hover previews on the photo, with a line at the top ("Previewing Everyday gold 200"), so a
  preview is never mistaken for an edit. Click applies, as one history step.
- Amount blends every number the preset sets, from the photo's value before the preset (0) to
  the preset's (100) and past it (200), within each slider's range. Switches (black and white,
  chromatic aberration) and choices (profile, vignette style) are not blended: they follow the
  preset from Amount 1 up, as in Lightroom. Moving a slider by hand takes that slider out of
  the blend; the Amount goes on for the rest.
- Keyboard: Tab reaches the search, the chips, each category heading (Enter opens), the tiles
  (arrow keys move, Enter applies, F adds to Favorites), then the Amount.

## Remove one thing

Removed: the toast's second line ("click to apply"). The toast's first line already says it is
a preview, and the tile's tooltip says what a click does.

## Questions for the owner

- **Names.** The tiles show draft names: descriptive, without film brands, makers or real
  people. The full list of 451 renamed presets comes with the curation step, for your review
  before it ships.
- **Categories.** Film color, Film black and white, Slide film, Cinematic, Vintage and
  processes, Portrait, Landscape and nature, Food and product, Seasons and festivals, Places,
  Social, Tools. Tools holds the one-step presets (Exposure +0.3, Grain fine, WB Cloudy): keep
  them in the catalog, or leave them out?

## What works

- Looks and Presets tabs now browse the same way; the difference is only what a click does
  (a look adds a style layer, a preset sets the sliders).
- Every preset is also in search, so a user who knows the name never has to open a category.
