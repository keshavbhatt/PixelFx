# Design review: close-photo.html (Close photo)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: a photo you are done with leaves the editor in one step, and nothing
about it is lost. Signature: the × on a filmstrip tile, where the photo already is.

## Where it comes from

- The owner asked how to close an open image (2026-11-06). Today there is no way: opening
  another photo replaces it, and the strip only grows.
- Desktop convention: Ctrl+W closes the document on show (`QKeySequence::Close`; Cmd+W on
  macOS). Browser and editor tabs show their × under the pointer.
- Lightroom's filmstrip removes photos from its right-click menu; photo viewers (Gwenview,
  Photos) go back to their start page when the last photo closes.
- The built pieces it joins: the main menu (`menu.html`), the filmstrip and its selection
  (`apply-to-selected.html`), the welcome page and its Recent list (`welcome.html`).

## Measurements

- Main menu: "Close photo" with its shortcut sits after Paste image, in the group of ways to
  open photos; a row like every menu row (13 px `--text`, the shortcut 12 px `--muted`). The menu
  grows by one 31 px row.
- The ×: a 20 px circle, black at 60% over the photo, the × 12 px white with a 2.5 stroke; its
  target is 24 by 24 px, 2 px in from the tile's top right corner of 92 by 64. It never meets the
  edited mark (bottom right) or the selection box (top left).
- Contrast, computed for the circle over a white, a mid grey (#808080) and a black photo: the
  white × on the circle is 5.7:1, 12.6:1 and 21:1; the circle against the photo is 5.7:1, 3.2:1
  and 1:1, where the × itself carries it at 21:1.
- The tile's menu is the standard context menu, 220 px wide.

## Accessibility

- The × has a tooltip ("Close photo", with "(Ctrl+W)" on the photo on show) and the accessible
  name "Close harbour.jpg". Its 24 px target meets the minimum.
- The tiles take no keyboard focus today (Left and Right step through the strip), so the × is a
  pointer shortcut only: from the keyboard, Left or Right goes to the photo and Ctrl+W, or the
  main menu, closes it. Nothing needs the pointer.
- Nothing is told by colour: the menu items and the tooltip say what happens.

## Platform conventions

- Every way to close is in the main menu (and on macOS in the menu bar), Ctrl+W is the
  standard shortcut and is free in PixelFx (W alone is the white balance picker), and the
  right-click menu repeats what the × does. Ctrl+W joins the Files section of the Keyboard
  shortcuts sheet.

## Interaction

- No confirmation: closing writes nothing and deletes nothing. The file is untouched, the edit
  is kept with Remember edits on (the default), and the photo is first under Recent to open
  again. DESIGN section 5 keeps confirmations for what cannot be undone.
- What opens next is predictable: the photo after the closed one, or the one before when it was
  last, as closing a tab does. The strip counts again at once.
- Close photo is greyed out in Crop and Heal, where Done or Cancel must come first; a running
  batch is not touched.

## Writing

- One name throughout: "Close photo" in the menu, the tooltip and the tile's menu; "Close 3
  photos" counts a selection, and "Close all photos" says what it does. Sentence case.

## Remove one thing

Removed: "Close other photos" (a tab bar's "close others") and a × beside the file name in the
header. Close all and then opening one does the first; the header × would be a second control
for what Ctrl+W, the menu and the tile already do.

## Questions for the owner

- Approve Close photo as drawn: Ctrl+W and the main menu, the × on a filmstrip tile, the tile's
  menu, the next photo or the welcome page after.
- With Remember edits switched off, closing an edited photo loses its edit, as switching to
  another photo already does. Close without asking (recommended, the same as switching and
  quitting today), or ask once before closing an edited photo?

## Decided by the owner (2026-11-06)

Approved. With Remember edits off, closing an edited photo asks first: "Close without keeping
the edit?", Close photo (or Close N photos) and Cancel, Cancel the default. Drawn as 7 in the
mock.

