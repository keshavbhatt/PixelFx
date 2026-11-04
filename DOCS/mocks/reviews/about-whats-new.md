# Design review: about.html and whats-new.html (About, What's new)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: tell people what they have and what changed, in their words, and give a
bug report everything it needs without a single personal detail. Signature: Diagnostics is
plain text made for pasting, and What's new is the changelog itself, so the two never drift.

## Blocking

None. Measured with the tokens of section 1: the credit line and hints are `muted` on `panel`
(dark 7.0:1, light 6.8:1); the diagnostics block is `text` on `input` (dark 15.6:1, light
17.4:1); the link "The full list of changes" is `accent` on `panel` (6.9:1, 5.3:1). Buttons are
the 32 px `sm` size (24 px minimum met); the tab segments are 32 px tall.

## Improvements

- **Medium.** Diagnostics must stay free of file names, folder paths under the home folder and
  anything about accounts or licences: the build fills it from versions and capabilities only,
  and a test checks the text (as `tst_changelog` does for release text).
- **Medium.** What's new opens over the editor at the first start of a new version; it must not
  open on top of a photo passed on the command line before it is shown (it waits for the first
  paint) and never during `--batch`.
- **Low.** The Version list shows only 2.0.0 until there is a second release; it is hidden while
  it holds one entry.
- **Removed one thing.** A "Check for updates" button: stores and package managers do that, and
  it would need the network.

## What works

- About holds the four ways on (What's new, guide, source, bug report) as plain buttons, not
  links in a paragraph.
- The credit line names both licences in plain words and points to the full notices.
- What's new can be turned off with one check box and is always in the main menu.
