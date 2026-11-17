# Design review: export.html, Save to (issue #4)

Reviewed 2026-11-17 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: choose where the photo goes in one click, including back beside the
original. The report (GitHub issue #4, PixelFx 2.0.2 snap): once a folder was chosen there was no
way back to "Next to the photo", and clicking the path did nothing.

## The change

- Save to was a path with Change...; it becomes a list, as Size, Color space and If it exists
  already are: Next to the photo; Pictures › PixelFx (the default); the last folder chosen, when
  it is another; a line; Choose a folder..., which opens the folder picker. Cancel in the picker
  keeps the choice before. A folder is shown as its parent › name; the tooltip has the full path.
- Settings, Export uses the same list, so the two always show the same choice.
- The library already saves beside the photo when no folder is given; nothing changes in the
  file names, If it exists or the overwrite warning.

## Lenses

1. **Accessibility.** A standard drop-down: keyboard reachable, named "Save to", 30 px tall.
2. **Platform.** A folder list with "Choose a folder..." at the end is the common pattern in
   save and export windows.
3. **Visual craft.** Same control and width as the rows above and below it.
4. **Interaction.** Two clicks back to beside the photo; the picker only when asked for.
5. **Writing.** "Next to the photo" as before; "Choose a folder..." says what follows.

## Then remove one thing

The Change... button goes: the list's last item does its work.
