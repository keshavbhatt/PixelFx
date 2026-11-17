# Design review: settings.html (every page)

Reviewed 2026-11-17 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: change how PixelFx behaves once, in the place people look for it, and
never need Settings to edit a photo. Signature: every row takes effect as it is changed, with
nothing to confirm, and each page fits without scrolling.

## Where it comes from

- FEATURES T1: a sheet with a sidebar of General, Editor, Export, Batch, Presets and looks,
  Performance and Advanced. 2.0 built the sheet with one General page (Appearance, Editing,
  Display, Performance); the rest are this mock.
- Every row is a setting PixelFx already keeps (`core/settings/settings_keys.h`) or one a
  scheduled feature brings: the preview on the graphics card (E18, 2.1), which appears with its
  feature, not before. The monitor profile (T4) was in this mock and was dropped (owner,
  2026-11-17).
- Rows that are new as settings: the batch notification switch (S6 notifies always today),
  Restore default settings, and the counts with Show folder on Looks and presets.

## The pages

- **General**: Appearance (Theme, Canvas background, as built); Notifications (Batch finished:
  notify when PixelFx is in the background); Updates (What's new: Show this after each update, the
  switch the What's new sheet has).
- **Editor**: Editing (Remember edits, On open, as built); Adjustments (One group open at a time,
  the switch of the group heading menu); Display (Clipping warnings, as built).
- **Export**: what new exports start with: Save to, File name with its tokens, If it exists,
  Metadata, Color space. The same settings as the Export sheet, which remembers each choice; a
  change in either place shows in the other. Format, quality and size stay in the sheet: they
  belong to a photo, not to PixelFx.
- **Looks and presets**: the user's looks and presets counted, each with Show folder; Import
  presets and looks; Recent presets with Clear recent.
- **Performance**: Preview (graphics card or processor, the card's name, and what happens when its
  driver fails); Batch (Photos at a time, moved from General, as built).
- **Advanced**: Diagnostics (Copy, Open the log folder, as in About); Restore default
  settings, which asks first and says what stays.

## Departure from FEATURES T1, for the owner

T1 names seven pages. Batch has only two settings of its own: Photos at a time is about speed and
memory (Performance), the notification is one of the app's notifications (General). A Batch page
with one or two rows reads as unfinished, so this mock folds it in and has six pages. "Presets
and looks" reads "Looks and presets", the order of the editor's tabs.

## Measurements

- The Wide sheet (820 by 560 px): a 200 px sidebar of pages (rows about 34 px tall), a 1 px
  `--border` line, the page with 14 px by 22 px padding. Section headings 11 px semibold caps in
  `--muted`; rows on the 150 px label column of `.field`, 6 px above and below; helper lines
  12 px `--muted` under their control.
- The tallest page (Export) is about 330 px of content: no page scrolls at 560 px. At the
  smallest window (1024 by 640) the sheet keeps 40 px above and below.
- Contrast on `--panel`, from the tokens: labels and helper lines `--muted` 7.0:1 dark, 6.8:1
  light; values `--text` 14.1:1 and 16.0:1; the selected page `--accent` on `--accentSoft` 6.0:1
  and 4.9:1; the monitor and card names `--faint` 5.1:1 and 5.2:1. A switch that is off is
  `--track` (3.0:1), on is `--accentStrong`; a segmented control's chosen part is filled
  `--accentSoft` with `--accent` text (6.0:1 and 4.9:1).

## Lenses

1. **Accessibility.** Text and controls pass (above). Targets: small buttons 26 px tall,
   segmented parts about 30 px, switches 34 by 20 px with their label, sidebar rows 34 px. Up and
   Down move between pages in the sidebar, Tab goes into the page; the focus ring is the
   standard one. The sidebar is named "Settings pages". Nothing relies on colour alone: the
   selected page and the chosen segment are filled, not only tinted.
2. **Platform.** Ctrl+, opens Settings, as now; on macOS it sits under the app menu (with S14).
   No OK or Apply: every desktop's own settings windows work this way now. Esc closes.
3. **Visual craft.** One accent, for the selected page and the switches that are on. Pages share
   one column grid, so labels and controls line up page to page.
4. **Interaction.** Every change applies at once (theme, canvas, clipping, colour management,
   the preview), as the built General page does. Only Restore default settings asks first,
   because undo cannot bring the settings back.
5. **Writing.** "Draw the preview with: Graphics card, Processor" in place of GPU and CPU. Labels
   name the thing, the line beside the control says what it does. Words match the rest of the
   app: Show folder, Clear recent, Import presets and looks, Open the log folder, Show this
   after each update.

## Then remove one thing

Report a bug on Advanced: the main menu and About both have it, and Advanced is not where
someone with a problem looks first. Removed from the mock. Diagnostics stays there, beside
Restore default settings, because both are about the app itself, not a photo.

## Improvements

- Medium: Export repeats the Export sheet's rows. One place would be leaner; the Settings page
  stays because "where do my exports go" is the commonest settings question, and it writes the
  same settings, so the two cannot disagree.
- Low: Looks and presets has no favourites row (clearing favourites cannot be undone, and the F
  key in the panels already removes one at a time).

## What works

- Six short pages; no page scrolls.
- No row for a feature that does not exist yet ships before its feature.

## After approval (2026-11-17)

- The mock drew the on and off rows as checkboxes; Settings has always used switches (the first
  approved mock too). The mock now shows switches, as built.
