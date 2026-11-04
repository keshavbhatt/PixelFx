# Changelog

All notable changes to PixelFx. The format follows Keep a Changelog; the What's new sheet shows
the section of the running version.

## [2.0.0] - unreleased

PixelFx 2 is rebuilt from scratch: a new editor, a new image engine, and a new look.

### Added

- Open a photo from the window, the command line, or by dropping it on the window; it shows in
  true colour right away.
- Opening PixelFx a second time brings the open window forward and passes it the photos.
- Paste a picture from the clipboard (Ctrl+V) to edit it, a screenshot for example.
- Crop and rotate (C): crop freely or to a ratio with thirds, grid, golden or diagonal guides,
  straighten with the dial, the slider or a drawn level line, fix leaning buildings with the
  perspective sliders, turn and flip. The crop stays editable later.
- White balance: As shot, Auto, or click something grey with the picker (W).
- Open many photos at once: they line up in a strip under the photo; Left and Right step
  through them, Ctrl or Shift click (or Ctrl+A) selects several, and a preset, pasted settings or
  Export then go to all of them.
- Batch (B): add photos or whole folders, pick a preset (and a second on top), Auto enhance if
  you like, a size and a format, and process them all in the background; each photo says whether
  it was saved, and why not when it was not. Save the page as a batch recipe to run it again, here
  or from the command line: `pixelfx --batch recipe.pfxbatch photos...`.
- Presets: save the whole edit or only parts of it, apply with one click, preview by hovering,
  stack a colour preset on a light one; import and export them as files to share. A few built-in
  presets, Auto enhance among them, to start from.
- Bring your Lightroom and Camera Raw presets (.xmp): light, colour, curves, detail and effects
  come across, and PixelFx says which settings it left out.
- Color mixer: hue, saturation and luminance for eight colour ranges.
- Dehaze clears distance haze from landscapes and city views, or adds a little mist.
- Effects: vignette, film grain and fade.
- The camera's settings (ISO, aperture, shutter speed, focal length) under the histogram.
- The start page lists recent photos as they were left; click one to carry on.
- Export leaves out where a photo was taken unless you tick Keep location.
- Watermarks on export and in Batch: your name, the year or the camera, or a logo; placed in a
  corner or anywhere you drag it, or repeated across the photo for proofs. The colour can follow
  the photo, light on dark areas and dark on light ones, and the size follows the photo, so a
  whole batch gets the same-looking mark. The editor view never shows it.
- Export sizes named for where the photo is going: Instagram portrait, square and story, X,
  web, email, a 4K wallpaper, prints at 300 dpi; crop to fill or fit inside with a border.
- A sheet of every keyboard shortcut (F1), and a main menu (F10) that reaches every command.
- Tab hides the panels to see only the photo; F11 for full screen.
- Settings (Ctrl+,): light or dark theme, the colour around the photo, whether edits are
  remembered, and clipping warnings (J) that show blown highlights in red and blocked shadows
  in blue.
- Snapshots: keep a named version of an edit from History (H) and go back to it any time.
- Copy a photo's settings and paste them onto another (Ctrl+Shift+C, Ctrl+Shift+V); each
  photo keeps its own crop.
- Zoom in to check detail: the scroll wheel, the zoom bar, Ctrl+0 to fit and Ctrl+1 for 100%,
  drag to look around. At 100% and beyond the photo is shown at its full resolution.
