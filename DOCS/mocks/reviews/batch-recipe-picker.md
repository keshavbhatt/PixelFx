# Design review: batch-recipe-picker.html (Choose the recipe)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: choose a batch's look or preset by seeing it on the batch's own photo,
among hundreds, as quickly as in the editor. Signature: the editor's tiles and groups, in a sheet,
on the first photo of the batch.

## Where it comes from

- The Batch audit (2026-11-06): the recipe card's Change and Then add preset list every preset
  in one flat menu, about 430 with the catalog, with no groups and no search.
- The approved `editor-presets-catalog.html`: categories, Favorites, Recent, Mine, tiles of
  the photo, the favourite star; `settings.html`: a wide sheet with a list of groups on its left.

## Measurements

- The sheet is 900 by 640 px (fits the 1024 by 640 minimum window; shorter windows scroll its
  body, as every sheet now does). The group list is 210 px, rows 32 px with their counts in
  12 px `--muted`; the selected group in `--accent` on `--accentSoft` (4.9:1 light, 6.0:1 dark).
- Tiles: four columns of about 156 px, the photo at 3:2 with the name under it in 12 px
  `--text`, 8 px apart, the selected tile's 2 px `--accent` border, as the editor's.
- The footer line says what the selected one does, 12 px `--muted`; the primary button names
  it ("Use Portrait 400", "Add Product white").

## Accessibility

- Tab goes search, groups, tiles, footer; arrows move through the tiles, Enter uses one,
  Escape cancels. Tiles have the look's or preset's name as their accessible name and its
  description as their description. The star has its words ("In Favorites").

## Interaction

- Only the tiles on show are rendered, on the first photo, as in the editor (hundreds of
  presets never render at once). The group and the search stay while the app runs, so a second
  batch starts where the first left off.

## Remove one thing

Removed: a strength slider in the sheet. A look comes in at its own strength, which the card
then says ("Look, strength 72"); the card is where the recipe is read and changed, not the
picker.

## Questions for the owner

- Approve Choose the recipe as drawn, and the same sheet with presets only for Then add preset.
