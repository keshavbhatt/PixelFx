# Design review: pro-sheet.html, pro-gate.html, pro-export.html, pro-trial-end.html (PixelFx Pro)

Reviewed 2026-11-08 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: PixelFx is free and complete for one photo; Pro shows up only when the
person reaches for many photos or the files they already own, and says plainly what it adds.
Signature: try freely, pay to save, with the box in Export that lists exactly what in this photo
is Pro and lets a Free user remove it.

## Where it comes from

- The owner's decisions of 2026-11-08: a 14-day trial for every new install, try freely and pay
  to save (RAW, imported Lightroom presets and LUTs, watermarks), the counting gates offering
  the first 10, the plans sold on ktechpit.com, the Ktechpit Pass, the checkout's `source`.
- The free and paid split of 2026-11-04 (FEATURES section K), with RAW changed to open and edit
  free, export Pro.
- The shared licensing module (AccountAndLicense, Model B), as Playlist Downloader and Red use
  it: an Account ID per install, the server's answer cached, a check on start and weekly.
- The server: the checkout takes the Account ID and `source`; the self-service portal's Ktechpit
  Pass page takes `?app=pixelfx&acc_id=` to add an app to a pass.

## Measurements

- PixelFx Pro: the regular sheet (560 px). A status box on `--hover`, 12 by 14 px padding, a
  16 px glyph, a 13 px semibold line and a 12 px `--muted` line; during the trial a 4 px meter
  (`--text` on `--border`). The table: 11 px capitals for the column heads, rows 13 px with an
  optional 12 px `--muted` line, 1 px `--border` rules, ticks in `--success`, dashes in
  `--faint`. The Account ID in the mono face on `--input`, Copy beside it. Footer: Manage
  purchases (link, left), Close, Choose a plan (primary, the external glyph).
- Pro prompt: the narrow sheet (440 px), title naming the job ("Batch 47 photos with Pro"),
  13 px text, a 12 px `--muted` line; footer: What Pro adds (link), the free way out, Choose a
  plan (primary).
- Export: the existing sheet with the status box at the top (three 12 px rows, Remove as links);
  Export's place taken by Choose a plan while the box shows.
- The Pro mark: 16 px tall, 11 px semibold, 1 px `--track` outline, `--muted` text, radius 4.
- Contrast, computed from the tokens: Pro mark text `--muted` on `--panel` 7.0:1 dark, 6.8:1
  light; its outline `--track` on `--panel` 3.0:1 both (the non-text minimum). Ticks `--success`
  on `--panel` 8.2:1 and 6.1:1; dashes `--faint` 5.1:1 and 5.2:1. The status box's `--muted` on
  `--hover` 5.9:1 and 6.0:1; a link on `--hover` 6.9:1 and 4.6:1. The meter's fill on its track
  11.2:1 and 11.7:1.

## Accessibility

- The table is a real table with column heads, so a screen reader says "Watermarks, Free: not
  included, Pro: included"; the tick and dash carry titles. Nothing by colour alone: a tick, a
  dash or a number in every cell.
- The Pro mark has a tooltip naming the feature; screen readers read "Watermark, Pro".
- Every sheet: Escape closes it, focus starts on the primary button, Tab goes through the
  links. The trial note in the window is announced once and closes with its button.

## Platform conventions

- PixelFx Pro is a main menu entry (above Quit; in the app menu after About on macOS), so it is
  reachable without waiting for a prompt. Choose a plan and Manage purchases open the browser
  (QDesktopServices), as the online guide does. Copy uses the clipboard.
- The command line gets one line and an exit code (3), as tools do, not a window.

## Visual craft

- The accent keeps its one meaning: the Pro mark is a neutral outline, not blue; only the
  primary button is in `--accentStrong`, as everywhere.
- One status box shape for the sheet's three states and the Export box, so "Pro" always looks
  the same.
- Template check: a pricing sheet with a comparison table is the common pattern for desktop apps
  with a paid tier; it is the right one here because the question people have is "what is free?".
  The table answers it first, with "Everything for one photo" as the top row.

## Interaction

- Nothing happens at start. During the trial nothing asks; afterwards a prompt appears only
  when a paid feature is used, and always with a free way out where one exists (the first 10
  photos, Remove the preset or watermark, Make files smaller).
- After paying, Check again asks the server at once; the module also checks on start and
  weekly, and PixelFx keeps working offline with the last answer.
- The trial's end is one dismissible line in the window, on the first start after it. PixelFx
  keeps no count of what the trial was used for.

## Writing

- "Choose a plan", not "Upgrade now" or "Buy"; "What Pro adds"; "Batch 47 photos with Pro"
  names the job. "Pro trial: 12 days left", "Your Pro trial has ended. PixelFx stays free for one
  photo at a time." No "licence", "key" or "activate" (DESIGN section 4).
- "Account ID" is the word the checkout, the portal and the e-mails use.

## Remove one thing

Removed: prices in the app. They live on the checkout, in the buyer's currency and with any
sale, so the app never shows a stale or wrong price; the sheet says what Pro adds and Choose a
plan shows the price.

## Owner's review (2026-11-08)

- The four mocks approved.
- The free limit for Batch and paste stays 10 photos per run; a Free user can run the next 10
  afterwards.
- No trial recap: the first prompt after the trial is the ordinary one, and PixelFx counts
  nothing about its use.

## Revision 2026-11-10: the Ktechpit Pass line (pro-sheet.html, pin 4)

The owner's suggestion: instead of asking people to add PixelFx to a pass they mostly do not
have, say that PixelFx is included in Ktechpit Pass and take them to the Pass checkout with
this Account ID.

- What changes: the line under the Account ID (Trial and Free; hidden with Pro). Before: "A
  Ktechpit Pass unlocks PixelFx and every other Ktechpit app." with the link "Add PixelFx to
  it", which opened the portal for people who own a pass. Now: "PixelFx is included in Ktechpit
  Pass" and "One purchase unlocks every Ktechpit app.", with Get the Pass, which opens the Pass
  checkout in the browser with this Account ID and `source=in_app_pixelfx`. Buying the pass
  with this Account ID unlocks PixelFx at once, with nothing to link afterwards. "Have one
  already? Add PixelFx to it." stays as a third line for the few who own a pass bought with
  another app's Account ID; it opens the portal page as before.
- Measurements: the same box as before (10 by 12 px padding, 1 px `--border`, radius 8) on the
  sheet's `--panel`, the 16 px layers glyph in `--muted` at the top. Three 12 px lines: the first
  semibold in `--text`, the other two in `--muted`, the link in `--link`. Get the Pass is a small
  button (26 px, 12 px text, the external glyph), the same kind as Copy above it, vertically
  centred on the box.
- Contrast on `--panel`: `--text` 14.1:1 dark, 16.0:1 light; `--muted` 7.0:1 and 6.8:1; `--link`
  8.2:1 and 5.3:1.
- Hierarchy: Choose a plan stays the one primary button. Get the Pass is a secondary button
  inside the box, smaller than the footer buttons, so the PixelFx plans come first and the Pass
  is the alternative for people who use more than one Ktechpit app.
- Accessibility: Get the Pass is a real button with a tooltip ("Opens the Ktechpit Pass checkout
  in your browser, with this Account ID") and is in the tab order after Copy and Check again;
  the link keeps its own tooltip.
- Writing: "included in" says what PixelFx is to the pass without selling; no price in the app,
  as before (the checkout shows it in the buyer's currency and with any sale). No "licence" or
  "activate".
- Server: the website counts Pass visits and sales per link (`source`); it learns the place
  `in_app` (KtechpitPass::PLACEMENTS), so the dashboard shows "Inside an app · PixelFx".
- Remove one thing: removed the portal link as the line's main action; it is now the small last
  line for pass owners.
- Owner's review (2026-11-10): approved as is, the "Have one already?" line included. Built the
  same day.
