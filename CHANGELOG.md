# Changelog

All notable changes to PixelFx. The format follows Keep a Changelog; the What's new sheet shows
the section of the running version.

## [2.0.0] - unreleased

PixelFx 2 is rebuilt from scratch

A new editor, a new image engine, and a new look.

### Added

- Looks: one click styles your photo (Vivid, Warm, Film, Golden hour, Teal and orange, Noir and
  more), each tile showing your own photo, with a strength slider. Bring your own .cube LUTs
  and HaldCLUT pictures as looks, or make your own look from steps and LUTs (Sepia, Colorize, Threshold, Invert, Blur,
  Pixelate, Emboss and Edges among them), share it as a file,
  or bake it to a .cube for other apps.
- Crop and rotate (C): crop freely or to a ratio with thirds, grid, golden or diagonal guides,
  straighten with the dial, the slider or a drawn level line, fix leaning buildings with the
  perspective sliders, turn and flip. The crop stays editable later.
- Presets: save the whole edit or only parts of it, apply with one click, preview by hovering,
  stack a colour preset on a light one; import and export them as files to share. A few built-in
  presets, Auto enhance among them, to start from.
- Batch (B): add photos or whole folders, pick a preset (and a second on top), Auto enhance if
  you like, a size and a format, and process them all in the background; each photo says whether
  it was saved, and why not when it was not. Save the page as a batch recipe to run it again, here
  or from the command line: `pixelfx --batch recipe.pfxbatch photos...`. A recipe carries your
  own looks, the size and the watermark inside it, so it works on another computer too.
  Batch can also take the settings of the photo open in the editor, name files after the
  recipe with {preset}, enlarge small photos when you ask, and show the photos as a grid.
- Compress only, in Batch: make a pile of JPEG, PNG, GIF, TIFF and WebP files smaller without
  editing them. JPEGs lose nothing by default, PNGs can drop to 256 colours above a quality you
  set, and the location and embedded previews can go while camera details and copyright stay.
  Each file shows its size before and after and what it saved, any file can be checked at 100%
  next to its original, and Run again tries other settings on the same files.
- Quick jobs on the welcome page: Convert and resize, and Compress photos, each one click
  into Batch ready for the job.
- Batch names: number the photos with {n}, use the day they were taken or the camera, and see
  the first names as they will be saved while you type; Batch warns when every photo would get
  the same name.
- Choose how many photos Batch works on at once in Settings.
- Camera RAW: open RAW files from cameras and phones, including the JPEG XL DNG of Samsung Expert
  RAW. A RAW shows its preview at once while it develops, starts on the Camera profile so it looks
  as it did on the camera, and gives white balance in Kelvin with Daylight, Cloudy, Shade and
  more. A RAW PixelFx cannot read yet can be opened as the preview inside it.
- Watermarks on export and in Batch: your name, the year or the camera, or a logo; placed in a
  corner or anywhere you drag it, or repeated across the photo for proofs. The colour can follow
  the photo, light on dark areas and dark on light ones, and the size follows the photo, so a
  whole batch gets the same-looking mark. The editor view never shows it.
- Open a photo from the window, the command line, or by dropping it on the window; it shows in
  true colour right away.
- Opening PixelFx a second time brings the open window forward and passes it the photos.
- Paste a picture from the clipboard (Ctrl+V) to edit it, a screenshot for example.
- White balance: As shot, Auto, or click something grey with the picker (W).
- Open many photos at once: they line up in a strip under the photo; Left and Right step
  through them, Ctrl or Shift click (or Ctrl+A) selects several, and a preset, pasted settings or
  Export then go to all of them. Close a photo with Ctrl+W or the × on its picture in the strip.
- Photo details (I, or click the file name): the size, format and colour profile, the camera,
  lens, exposure and the day it was taken, with Copy details. Where it was taken shows only when
  you ask, with a line saying whether Export keeps it.
- Lens corrections: Use lens profile in Optics straightens your lens's distortion, removes its
  colour fringes and lifts its dark corners, for lenses in the Lensfun database; the panel names
  the lens it found, and Distortion and Vignetting set how much.
- Bring your Lightroom and Camera Raw presets (.xmp): every setting of the main panels comes
  across, black and white, colour grading, Texture, Calibration and the finer detail and effects
  settings included, and lens corrections for lenses in the Lensfun database; PixelFx says if
  it left anything out.
- Over 400 built-in presets, as pictures of your own photo: film, black and white, slide film,
  cinematic, vintage processes, portrait, landscape, food, seasons and festivals, places and
  more. Keep favourites, find the recent ones, and set how strongly a preset applies with
  Amount, from none to twice as strong.
- Color mixer: hue, saturation and luminance for eight colour ranges. Pick a colour on the
  photo (T) and drag up or down to change it, a sky or a dress, without guessing its range.
- Black and white (V), with a B&W mix that makes each colour lighter or darker in grey: darken
  a blue sky, brighten skin.
- Spot heal (R): click a blemish, a dust spot or a small object and it is gone, a matching
  patch picked for you; paint along a wire or a scratch with the brush. Every spot stays
  editable: move it, pick another patch, change its size, or take it off. Reveal dust (D)
  shows specks on skies. Spots stay with their photo when you crop later.
- Color grading: colour for the shadows, midtones, highlights and the whole photo, each with
  its own brightness, to give a photo a colour mood, cool shadows and warm light for example.
- Texture brings out skin, foliage and fabric; Curves can also lift or lower four ranges of
  tones; Optics removes colour fringes along edges; Calibration fine-tunes the colours of a
  camera. Each group keeps its finer settings under More.
- Dehaze clears distance haze from landscapes and city views, or adds a little mist.
- Effects: vignette, film grain and fade.
- The camera's settings (ISO, aperture, shutter speed, focal length) under the histogram.
- The start page lists recent photos as they were left; click one to carry on.
- Export leaves out where a photo was taken unless you tick Keep location.
- Copy to clipboard in Export: the finished photo, at the chosen size, ready to paste into a
  chat or a document.
- Export sizes named for where the photo is going: Instagram portrait, square and story, X,
  web, email, a 4K wallpaper, prints at 300 dpi; crop to fill or fit inside with a border.
- Export sharpens for the screen or for print, saves in sRGB, Display P3, Adobe RGB or the
  photo's own colour space, and tells you about how big the file will be.
- A sheet of every keyboard shortcut (F1), and a main menu (F10) that reaches every command.
- What's new opens once after an update (it can be turned off), and About shows the version,
  the open-source components with their notices, and diagnostics to paste into a bug report.
- Tab hides the panels to see only the photo; F11 for full screen.
- Settings (Ctrl+,): light or dark theme, the colour around the photo, whether edits are
  remembered, and clipping warnings (J) that show blown highlights in red and blocked shadows
  in blue.
- Snapshots: keep a named version of an edit from History (H) and go back to it any time.
- Copy a photo's settings and paste them onto another (Ctrl+Shift+C, Ctrl+Shift+V); each
  photo keeps its own crop.
- Zoom in to check detail: the scroll wheel, the zoom bar, Ctrl+0 to fit and Ctrl+1 for 100%,
  drag to look around. At 100% and beyond the photo is shown at its full resolution, and a
  small sign beside the zoom bar says when the sharp picture or a preview is still being made.
