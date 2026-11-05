# Design review: editor-collapse-all.html (Collapse all)

Reviewed 2026-11-05 with the method in `DESIGN.md` section 5, before the owner's review.

## Summary

Rating: Good. Thesis: get from a panel full of open groups back to the one the user needs, in
one click. Signature: Collapse all keeps the changed dots, so a closed panel still says where
the edits are.

## Where it comes from

Lightroom's panel headers: right-click gives Expand all, Collapse all and Solo mode (opening one
panel closes the others), and Alt+click opens only the panel clicked. The mock keeps all three
and adds a visible button, because a right-click menu alone is not found (DESIGN lens 2: hover
and right-click are mirrors, never the only way).

## Behaviour

- **Collapse all** (chevrons-down-up, the last button of the top row) closes every group. It is
  greyed while every group is already closed.
- **A group heading's menu** (right-click or the Menu key): Collapse all, Expand all, One group
  open at a time (checked when on, remembered between sessions), Open only <group>.
- **Alt+click** on a heading opens only that group, in either mode.
- **Presets**: the same button in the search row closes every category; each heading's menu has
  Close all and Open all. The closed categories are already remembered.
- The open groups and the mode are saved with the other panel state; nothing changes in a photo,
  so none of this is a history step.

## Measurements

- The button is a 26 px icon button like Reset all beside it: its icon in `--text`, 14.1:1 dark
  and 16.0:1 light; accessible name and tooltip "Collapse all groups".
- The menu is the shared menu style (DESIGN, menus): 13 px items, the check in the accent
  (6.9:1 and 5.3:1), the Alt+click hint in `--muted` (7.0:1 and 6.8:1).
- The top row keeps its 8 px gaps: Auto shrinks from 98 to 66 px of free width and still shows
  its label.

## Remove one thing

Removed: an "Expand all" button next to it. Opening every group gives a long panel nobody reads
at once; it stays in the heading menu for the rare need.

## Questions for the owner

- None.
