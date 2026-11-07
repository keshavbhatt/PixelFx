# Design review: report-bug.html and crash-notice.html (Report a bug, after a crash)

Reviewed 2026-11-07 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: a crash or a bug becomes a useful report in under a minute, and nothing
leaves the computer without the person seeing it. Signature: Show beside each attachment,
the exact text that would go.

## Where it comes from

- FEATURES S7: crash handler, log file, diagnostics copy, Report a bug sheet. Built: the log
  file (`LogSink`), Diagnostics in About with Copy, Report a bug opening the issue tracker. Not
  built: anything that notices a crash, and a sheet that puts a report together.
- Desktop practice: KDE's crash reporter and Firefox's crash reporter tell on the next start, not
  at the moment of the crash (a crashing program cannot safely draw); both show what is sent.
- The built pieces it joins: the sheet layout (`Sheet`, `footButtons`), Diagnostics
  (`core::diagnosticsText`), the log (`LogSink`), `core::issuesUrl`, the main menu's Report a bug
  and About's.

## Measurements

- After a crash: a narrow sheet (440 px), title 16 px semibold, two short paragraphs 13 px,
  the first `--text`, the second `--muted` (7.0:1 dark, 6.8:1 light); footer Not now and Report
  the problem (primary, the bug glyph). No close button: Not now is the way out, Escape too.
- Report a bug: the standard sheet (560 px). After a crash only, a 36 px line on `--hover` with
  the alert glyph in `--warning` and 12 px text. "What happened" 12 px `--muted` over a field 92
  px tall (four lines). "Attached" a 13 px heading, then one row per attachment: a 16 px check,
  the name 13 px `--text`, a 12 px `--muted` line under it, Show as a 12 px link at the right
  end. A 12 px `--muted` line under them says what happens next. Footer: Copy report at the
  left, Cancel, then Open the issue tracker (primary, the external glyph).
- Contrast: `--muted` on `--panel` 7.0:1 and 6.8:1; on `--hover` 6.6:1 and 6.0:1; `--link`
  6.9:1 and 5.3:1; white on `--accentStrong` 4.7:1 and 5.8:1.

## Accessibility

- Every control has a name; each Show is "Show what is attached: Diagnostics" and so on for
  screen readers. The checks are 16 px with their labels as the target.
- The field takes the focus when the sheet opens; Tab goes through the checks, their Show, then
  the footer. Escape closes the sheet (and is Not now on the crash notice).
- Nothing by colour alone: the crash line has the alert glyph and words.

## Platform conventions

- Report a bug stays in the main menu (Help part) and About; the sheet replaces opening the
  tracker straight away. Copy report is the usual clipboard copy.
- The browser opens through the desktop (QDesktopServices), as the online guide does.

## Interaction

- The crash notice shows once, on the start after a crash, before anything else asks; Not now
  forgets that crash.
- Opening the issue tracker fills in a new issue: the first line of What happened as its title,
  the rest and the ticked attachments as its body. A body too long for a web address (about 6000
  characters) goes on the clipboard instead, and the issue's body says to paste it.
- Show opens the attachment's text in a read-only view inside the sheet; Hide puts it away.
- The crash attachment: the kind of fault (a crash, an abort), where in PixelFx it happened
  (the function names the system can give), the version, and the log lines just before. The
  photo's name is never in it, only its kind.
- Edits are written as they are made, so after a crash only the last few moments can be lost;
  with Remember edits off the notice says the photo's edits could not be kept.

## Writing

- "PixelFx closed unexpectedly", not "crashed" or "fatal error"; "Your edits up to the last
  change were kept" says what matters first.
- "Report the problem", "Report a bug", "Open the issue tracker", "Copy report": each says what
  it does. "PixelFx sends nothing itself" answers the privacy question before it is asked.

## What building it takes

- A crash handler: on Linux and macOS, signal handlers for faults and aborts (and a terminate
  handler) that write only with calls safe at that moment, into a file opened at start: the
  signal, the call stack the system gives, the time. On Windows an unhandled exception filter
  writes the same. The photo's name, kept ready in a fixed buffer when a photo opens, so the
  notice can name it; it never goes into the report.
- On start: the file is read, the notice shown, the file moved aside so it shows once.
- The report: `core::bugReport` (pure, tested): the text, the attachments, file and folder
  names in the log replaced by their kind, the issue's web address, the length limit.
- The sheet, the notice, the main menu's and About's Report a bug opening the sheet.

## Remove one thing

Removed: a "Steps to reproduce" field of its own; one field with a placeholder that asks for
what was done, expected and seen gets the same, with less to fill in.

## Questions for the owner

- Approve Report a bug and the crash notice as drawn.
- The report opens a filled-in issue on GitHub (the person reads and sends it there). Keep
  GitHub issues as the one place reports go (recommended, no server of our own), or also offer
  an email address?
