# DESIGN: PixelFx 2

Principle: **the photo first, every control one glance away**. The photo fills the middle,
looks sit on the left as previews of that photo, adjustments on the right in the order a
photographer works (light, colour, detail, effects). Nothing is applied on release; the
picture follows the slider. Nothing is ever written to the original.

The design is new; nothing is carried over from v1's layout (owner, 2026-11-04). Every
screen has an HTML mock in `mocks/` (open `mocks/index.html`; the toolbar at the bottom right
switches dark and light, the OS title bar, and the review notes). The editor mocks are
interactive: sliders and look tiles change the photo with CSS stand-ins. No screen or visible
change is coded before its mock is approved; approval is recorded per screen in
`mocks/index.html` and `PROGRESS.md` (CLAUDE.md standing rule).

## 1. Brand and tokens

- Name: PixelFx. Mark: the existing colour-wheel icon (a rainbow rosette); the header shows it
  small. A redrawn vector icon is an open question (PLAN section 9).
- The chrome is neutral grey with no hue, in both themes, so it never biases the user's
  judgement of colour. One accent (blue) marks selection, focus, changed values and the
  primary button; the icon supplies all the other colour.
- `mocks/mock.css` is the source of these tokens until the app's style code exists; the two
  must never disagree.

| Token | Dark | Light | Used for |
|---|---|---|---|
| `accent` | `#6EA8FE` | `#2560CC` | focus ring, selected tab line, changed slider values, links |
| `accentStrong` | `#2F6FE0` | `#2560CC` | primary button fill (white text), checked boxes, switches |
| `accentHover` | `#3B7CF0` | `#1F52B0` | hover on primary |
| `accentSoft` | `#1D2A40` | `#E3ECFC` | selected row, active rail item, focus halo |
| `bg` | `#161616` | `#EDEDED` | window background |
| `canvas` | `#0F0F0F` | `#D6D6D6` | behind the photo (setting: dark grey, middle grey, black, white) |
| `bar` | `#1C1C1C` | `#FAFAFA` | header, rail, filmstrip |
| `panel` | `#1E1E1E` | `#F5F5F5` | side panels, sheets |
| `elevated` | `#282828` | `#FFFFFF` | menus, popups, toasts |
| `hover` | `#2C2C2C` | `#E6E6E6` | hover on neutral controls |
| `input` | `#141414` | `#FFFFFF` | fields, histogram and curve wells |
| `border` | `#303030` | `#D4D4D4` | hairlines |
| `track` | `#696969` | `#8E8E8E` | slider tracks, switch off |
| `text` | `#ECECEC` | `#1A1A1A` | primary text, slider thumbs |
| `muted` | `#A8A8A8` | `#555555` | labels, secondary text |
| `faint` | `#8E8E8E` | `#676767` | hints |
| `success` / `warning` / `danger` | `#5CCB84` / `#F5B14C` / `#FF6B6B` | `#1B7F38` / `#B45309` / `#C01C28` | states |

Slider tracks carry meaning where it helps: Temperature blue to amber, Tint green to magenta,
Saturation from grey through the hues (as v1's gradient tracks did). Type: the system UI
font, 13 px UI, 12 px labels, 11 px section headings in capitals. Shape: 7 px controls, 10 px
cards, 14 px sheets.

## 2. Layout

```
+------------------------------------------------------------------------------+
| (o) [open] sunset-lake.jpg  Edited       [undo][redo][hist] [cmp][eye] Export [=] |  header
+----+------------+----------------------------------------+-------------------+
|Edit| Looks|Pres.|                                        | histogram         |
|Crop| [search]   |                                        | [Auto][Copy][Paste]|
|Btch| tiles of   |              the photo                 | > Light       *   |
|    | the photo  |                                        | > Color       *   |
|    | ...        |            [- Fit + 1:1 []]            | > Color mixer     |
|    | strength   +----------------------------------------+ > Curves ...      |
| [s]|            | [th][th][th][th][th][+]     1 of 6     |                   |
+----+------------+----------------------------------------+-------------------+
```

- Rail: modes Edit (E), Crop (C), Heal (R), Batch (B); Settings at the bottom.
- Left panel (Edit mode): Looks and Presets tabs. Right panel: histogram, quick actions,
  adjustment groups. Crop mode swaps the right panel for crop controls and hides the left.
  Batch is its own page (list of photos, recipe on the right).
- No menu bar on Linux and Windows: the header carries Open, Export and the main menu, and the
  shortcuts sheet lists every action. macOS keeps its native menu bar.
- Dialogs ("sheets") are windows without the system's title bar (owner, 2026-11-05): a
  rounded panel with a 1 px border over the dimmed window, moved by dragging any empty part of
  it, closed by its close button or Escape. Inside: a title row, the body, a footer bar. The
  system's own dialogs (file and colour pickers, short questions) keep the system's look.
  Sheets never stack.
- Minimum 1024 by 640. Below 1280 px wide the left panel collapses to a button in the header;
  Tab hides both panels.

## 3. Screens

| Mock | Shows | Milestone |
|---|---|---|
| `welcome.html` | First run and nothing open: drop zone, Open images, Open folder, Batch edit, Recent | M3 |
| `editor.html` | The editor with the Looks panel, live sliders, filmstrip | M3 |
| `editor-presets.html` | The Presets panel: groups, import, row menu | M4 |
| `editor-compare.html` | Before and after split | M3 |
| `history.html` | History popup with snapshots | M3 |
| `crop.html` | Crop mode: ratios, straighten dial, perspective, overlays | M3 |
| `heal.html` | Heal mode: spot and brush, spots as editable objects, reveal dust | M3 |
| `preset-save.html` | Save as preset: name, group, which groups to include | M4 |
| `filter-editor.html` | New look: step stack, step parameters, LUT import, preview on test images, export, bake to .cube | M4 |
| `apply-to-selected.html` | Filmstrip selection and Paste settings to several photos | M4 |
| `export.html` | Export sheet, with Size and Watermark rows | M3 |
| `export-sizes.html` | Size presets by destination, framing preview, crop to fill or fit inside | M3 |
| `watermark.html` | Watermark editor: text or logo, relative size, position, Auto colour, saved by name | M5 |
| `batch.html`, `batch-running.html`, `batch-done.html` | Batch page: recipe, resize, save as; progress; results with reasons | M5 |
| `batch-compress.html` | Batch in Compress only mode (free): sizes before and after, savings, 100% compare, totals | M5 |
| `menu.html` | Main menu | M3 |
| `settings.html` | Settings sheet | M3 |
| `shortcuts.html` | Keyboard shortcuts sheet (F1, Ctrl+/) | M3 |
| (M6) | About with diagnostics, What's new, Report a bug: the kit's standard sheets | M6 |

## 4. Rules that survive the mocks

- The photo updates while a slider moves; the full-quality render follows within a moment of
  the slider stopping. A render never blocks input.
- A changed slider shows its value in the accent; a changed group shows a dot. Double-click
  resets a slider; the group's arrow resets the group.
- Looks are previewed on the user's own photo, never on a stock image.
- Applying anything with a multi-selection says how many photos it touches before it happens
  (the header pill "Applies to 4 photos").
- Export is the only thing that writes an image; overwriting an original asks once.
- A failed photo shows its reason on its row in plain words; the tooltip alone is never read.
- Dark and light are both first class; screenshots for the owner are taken in both.
- Keyboard: every action has a shortcut shown in its tooltip and in the shortcuts sheet;
  Escape cancels Crop and closes sheets; the focus ring is the accent with a soft halo.
- Every hover-only action (preset row menu, filmstrip remove) has a right-click twin.
- Colour is never the only signal: changed values also turn bold labels, states carry words.
- Nothing in the UI mentions accounts or app licensing (CLAUDE.md).
- Slider ranges, units and defaults follow the established editors, not the mocks' sample
  numbers (the mocks' values were never checked; owner, 2026-11-05). Reference: Lightroom
  and Camera Raw first, then darktable and Snapseed. Exposure is in stops, -5 to +5, shown
  with hundredths ("+0.12"); Contrast, Highlights, Shadows, Whites, Blacks, Temperature and
  Tint (relative), Vibrance, Saturation, Clarity, Dehaze, the colour mixer and Vignette are
  -100 to +100; Sharpen 0 to 150; Noise reduction, Color noise, Grain, Fade, grading amounts
  and look strength 0 to 100; grading hues 0 to 360 degrees; Straighten -45 to +45 degrees;
  Vignette midpoint and Grain size default to 50 and 25. A new control is checked against the
  same references before it is built.

## 5. Design review before the owner looks

Every mock (and later every coded screen, compared with its mock) gets a written review in
`mocks/reviews/<mock>.md` before it is put to the owner. The method is adapted, in our own
words, from the five-lens reviewer in github.com/dickwu/apple-design-skill (owner, 2026-11-04);
that repository has no licence and quotes Apple's guidelines, so nothing from it is copied
here. Apple's Human Interface Guidelines are a reference for principles, not a platform target:
PixelFx follows each desktop's conventions (DESIGN section 2).

**Thesis first.** One sentence: the screen's single job and the one thing it will be remembered
by. For the editor: "make this photo better while watching it"; remembered by looks previewed on
the user's own photo. A screen without a thesis is a finding.

**The lenses, in order, with numbers rather than adjectives:**

1. **Accessibility (failures block approval).** Text contrast 4.5:1 (3:1 at 18 px or bold),
   non-text controls (slider tracks, switches, focus rings, checkbox borders) 3:1, computed from
   the token values, never estimated from a screenshot. Desktop type: 13 px default, 11 px
   minimum. Hit targets at least 24 by 24 px (a 16 px checkbox counts with its label). Nothing by
   colour alone. Every icon-only button has an accessible name and a tooltip. Keyboard-only use
   reaches everything with a visible focus ring. The app honours the system font scale, reduced
   motion and high contrast.
2. **Platform conventions.** Every command is reachable from the main menu (and the macOS menu
   bar), not only from a toolbar or a hover. Standard shortcuts are kept (Ctrl+Z, Ctrl+O,
   Ctrl+, , F11); custom ones are few and listed in the shortcuts sheet. Windows resize down to
   1024 by 640 with the same functions. Right-click menus mirror hover actions. On macOS,
   Settings sits under the app menu.
3. **Visual craft.** One colour means one thing (the accent is selection, focus and "changed";
   nothing else). The chrome stays neutral so the photo carries the colour. Alignment and
   grouping show hierarchy; progressive disclosure (collapsed groups) over density. Icons from
   one set (Lucide), one stroke weight. Template check: "near-black with one accent" is a common
   default; here it is a reasoned choice (a photo editor must not tint the user's colour
   judgement), and the colour-wheel mark is the one place the brand is loud.
4. **Interaction.** Something visible within 100 ms of opening a photo; work can continue while
   anything renders; progress is determinate when it can be. Feedback lives in the window, not
   in alerts; a confirmation only for what undo cannot reverse (overwriting an original,
   deleting a preset file). Sheets have an obvious way out and one short task.
5. **Writing.** Labels say what happens ("Export 4 photos", not "OK") and an action keeps its
   name through the flow (Export, Exporting, Exported). Sentence case everywhere, on every
   platform. Errors say what went wrong and what to do, without apology. Empty states invite the
   next step. Names come from what users know (look, preset, crop), never from the code
   (recipe, pipeline, op).

**Then remove one thing.** Each review names one element that could go without loss, or says
the screen is already lean.

Report shape: Summary (rating, thesis, signature), Blocking, Improvements tagged High / Medium /
Low, What works. Findings carry the token or the measurement.

### Measured token contrast (2026-11-04)

| Pair | Dark | Light | Needs |
|---|---|---|---|
| `text` on `panel` | 14.1 | 16.0 | 4.5 |
| `muted` on `panel` | 7.0 | 6.8 | 4.5 |
| `faint` on `panel` / on `hover` | 5.1 / 4.5 | 5.2 / 4.5 | 4.5 |
| `accent` on `panel` (changed values, links) | 6.9 | 5.3 | 4.5 |
| `accent` on `accentSoft` (selected row text) | 6.0 | 4.9 | 4.5 |
| white on `accentStrong` (primary button) | 4.7 | 5.8 | 4.5 |
| `track` on `panel` (slider, switch off) | 3.0 | 3.0 | 3.0 |

The first pass failed three rows (`faint` 3.3 and 3.0, light `accent` 4.3, `track` 1.5) and the
tokens above are the fixed values.
