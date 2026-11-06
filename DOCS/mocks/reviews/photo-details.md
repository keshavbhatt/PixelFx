# Design review: photo-details.html (Photo details)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review. Three
files show its states: `photo-details.html` (a camera photo, the location hidden),
`photo-details-plain.html` (a file without Exif) and `photo-details-location.html` (the
location shown).

## Summary

Rating: Good. Thesis: what this file is and what the camera wrote, one click from its name.
Signature: the location kept behind a click, with a line saying whether Export would pass it on.

## Where it comes from

- FEATURES O7: size, format, bit depth, colour profile, camera, lens, exposure, date, GPS behind
  a click; "a panel, not a dialog". Today the header shows size, format and file size, and the
  exposure line under the histogram has camera and lens in its tooltip; nothing shows the
  colour profile, the date taken, the folder or the location.
- Desktop convention: file managers and photo apps show a file's details from its name or with
  Get Info (Cmd+I on macOS, Properties on Windows and KDE); Lightroom shows camera data with I.
- The built pieces it joins: the header's file name (`editor.html`), the main menu's View
  section (`menu.html`), Export's Keep location (`export.html`), the elevated surface and
  section headings of the menus.

## Measurements

- The panel: 340 px wide, `--elevated` with a 1 px `--border`, 10 px corners and the menus'
  shadow; its left edge on the header's file name (86 px from the window's left), 6 px under the
  header. 482 px tall with a camera photo, 358 px without Exif; at a 1024 by 640 window it fits
  under the header with 100 px to spare.
- Head: the file name 14 px semibold `--text`, cut in the middle when long, the whole name in
  its tooltip; the folder 12 px `--muted` with the home folder as "~", then Show in folder in
  `--link`. The × is a 26 px flat small button, 10 px from the right edge.
- Sections: headings as the side panels' (11 px bold capitals `--muted`, 10 px above, 4 px
  below); rows 25 px apart, the label column 96 px plus a 10 px gap, labels 13 px `--muted`,
  values 13 px `--text`, 16 px from the panel's edges.
- Location: the Show and Hide buttons are the 26 px small buttons; the note under them 12 px
  `--muted`.
- Footer: a 1 px `--border` line, 10 px above and below Copy details (a 26 px small button).
- The header's name becomes a button: 40 px tall, 8 px corners, `--hover` while hovered or
  open, a 14 px chevron in `--muted` 6 px after the text. It grows by 20 px.
- Contrast, computed from the tokens: on `--elevated`, `--text` 12.5:1 dark and 17.4:1 light,
  `--muted` 6.2:1 and 7.5:1, `--link` 7.3:1 and 5.8:1; the open name on `--hover`, `--text`
  11.8:1 and 13.9:1, `--muted` 5.9:1 and 6.0:1.

## Accessibility

- The name button has the accessible name "Photo details for sunset-lake.jpg" and the tooltip
  "Photo details (I)"; the × "Close (I, Esc)"; Show "Show where it was taken"; the copy button
  "Copy the coordinates". Every target is at least 26 px tall.
- From the keyboard: I opens the panel and puts the focus on its first control, Tab walks Show
  in folder, the ×, Show and Copy details, Esc closes it and gives the focus back. Nothing needs
  the pointer.
- The panel is announced as "Photo details"; each row reads as its label and value.
- Nothing is told by colour: a missing part says so in words ("No location in this file.").

## Platform conventions

- In the main menu under View, after Clipping warnings: "Photo details  I"; on macOS in the
  View menu of the menu bar. I is free in PixelFx and is the key Lightroom users know for photo
  information; the Keyboard shortcuts sheet gains it under View.
- Show in folder opens the system file manager on the file, as Batch's Show in folder does.

## Interaction

- A panel, not a sheet: it stays open while stepping through photos (Left, Right, the
  filmstrip) and follows the photo on screen, so comparing two photos' settings is two key
  presses. Clicking the photo does not close it; I, Esc and the × do.
- Reading the details costs nothing: they come from the file's header (`pfx::probe`,
  `pfx::readMetadata`), already read when the photo opened.
- The location is hidden each time the panel opens and for every next photo: showing it is a
  choice made each time, so a screen share or a screenshot does not give it away.
- The note under the location says what Export does with it, from Export's Keep location (off by
  default): "Export leaves it out: Keep location is off." or "Export keeps it: Keep location is
  on." It changes when the setting does.

## Writing

- One name: "Photo details" in the menu, the tooltip and the panel's accessible name.
- Values in plain words: "JPEG, 8 bits", "PNG, 8 bits, transparency", "Display P3, embedded",
  "None, read as sRGB", "1/250 s · f/8 · ISO 100", "3 October 2026, 18:42" (the date and time
  in the app's language). Megapixels to one decimal below 10 ("2.8 MP"), whole above ("24 MP").
- A missing part says so once instead of listing empty rows: "No camera details in this file.",
  "No location in this file."
- The camera is named as people say it: "Fujifilm X-T4", not "FUJIFILM X-T4" or "Canon Canon EOS
  R5" (the make in its usual case, not repeated when the model starts with it).

## What building it takes

- libpixelfx: `ExifSummary` gains the GPS latitude and longitude (the GPS block is already found,
  to strip it), and the embedded profile's name (Little CMS reads the description tag), each
  tested. A RAW's format says "Camera RAW (RAF)", without a bit depth that would be the decoder's.
- The app: the panel, the header's name as a button, the View menu entry, the I shortcut and its
  row in the Keyboard shortcuts sheet, and their mocks (`menu.html`, `shortcuts.html`) updated.

## Remove one thing

Removed: the file's Modified date (the file system's, usually the day it was copied, which
reads as a second and wrong date beside Taken) and an Open in map link, which would send the
location to a web site; copying the coordinates lets the person choose where they go.

## Questions for the owner

- Approve Photo details as drawn: the name in the header opens it, I and View > Photo details
  too; the File, Camera and Location sections; the location behind Show; Copy details.
- The shortcut: I (recommended; Lightroom's key for photo information, one key as J, H and V
  are) or Ctrl+I (Get Info on macOS)?
