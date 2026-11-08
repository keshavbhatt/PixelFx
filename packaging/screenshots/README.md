# Store screenshots

| File | Caption |
|---|---|
| 1-looks | One-click looks shown on your own photo, with a strength slider |
| 2-controls | Every light and colour control you expect, with a live histogram |
| 3-compare | Before and after on one photo, with a divider you can drag |
| 4-batch | Edit a whole folder of photos at once, in the background |
| 5-presets | Hundreds of presets, including your Lightroom and Camera Raw presets |

- `flathub/`: the window alone with its KDE Breeze title bar and shadow on a transparent
  background, at 2x (window 2048x1400; 1024x700 is the narrowest the app allows), as Flathub's
  quality guidelines ask. The metainfo links to them by commit.
- `snapstore/`: 3840x2160 JPEGs with a background and the caption, each under the store's 2 MB,
  uploaded by hand on the snap's listing page.
- `snapstore-banner.jpg`: the listing's featured banner, 4320x1440 (3:1, the largest the store
  takes), with the icon, the name, the summary and two of the screenshots' windows.

Captured from a dev build in a nested `kwin_wayland --virtual` session with default Breeze
settings and a scratch profile, through the `PIXELFX_DEBUG_*` hooks (`app/src/main.cpp`).

The photos shown are wallpapers from Wallhaven (wallhaven.cc/w/<id>): forest snail 49ekw8,
dahlia vmglj8, hot air balloons 43r85d, autumn portrait 2e18x9; in the batch list also 0pq7zm,
4892ly, 423dxn, 4g6k2d, 0wx5x4, nrqlq4, nkx7km, nrk9w4, w8lv96 and 4lvgeq.
