# Design review: editor-lens-profile.html (Lens profile)

Reviewed 2026-11-06 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: one box corrects the lens the photo was taken with, and says which lens
that is. Signature: the line under the box names the profile found, or says plainly why there
is none.

## Where it comes from

- Lightroom and Camera Raw: "Enable Profile Corrections" in Lens Corrections, the lens found
  from the photo's metadata and named under it, Distortion and Vignetting amounts from 0 to 200
  with 100 as measured; off by default. A preset saves "on" and each photo finds its own lens
  ("LensDefaults").
- darktable and RawTherapee: the same Lensfun database (ADR-013), the lens shown by name.
- The built Optics group (`editor-develop.html`, approved 2026-11-05): Remove chromatic
  aberration with its description line, Defringe under a caption, More.

## Measurements

- The new row reuses the Remove chromatic aberration row exactly: a 16 px box, the label in
  `--text` 13 px, the line under it 12 px `--muted` indented 24 px to the label (7.0:1 dark,
  6.8:1 light). Nothing new is styled.
- Distortion and Vignetting are the standard slider rows (label and value 12 px, 24 px track),
  0 to 200, default 100 at the middle of the track; the value reads in `--accent` once changed,
  as every slider.
- A 1 px `--border` divider with 10 px above and 4 px below separates the profile from the
  corrections measured on the photo; the group gains 104 px when on (two slider rows and the
  name line), 40 px when off.
- The longest lens name in the database (79 characters, a Sigma zoom with a Kenko extender)
  wraps to three lines at the panel's 300 px; nothing is cut. Most names fit on one.

## Accessibility

- The box is a check box named "Use lens profile"; the lens line is its description (read
  after the name). The sliders keep their accessible names and keyboard steps.
- Every state says itself in words: the lens's name, "No profile for this lens yet: ..." or "The
  photo does not say which lens took it". No colour carries meaning.

## Remove one thing

Removed: a lens picker (choose the maker and the model by hand, as Lightroom offers). It serves
the rare photo whose Exif names no lens, costs a two-level menu of a thousand entries, and a
preset could never carry it. The line says when a photo names no lens.

## Questions for the owner

- Approve the Optics row as drawn: Use lens profile at the top of Optics, the lens named, the
  two amounts while on.
- The two EmulsionPresets lens tools (left out of the catalog, ADR-009) could join it once this
  is built: add them under their own names, or keep them out?
