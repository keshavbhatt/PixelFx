# DOCS: how this project is tracked

Everything about the rewrite lives here. If it is not in `DOCS/`, it did not happen.

| File | What it is |
|---|---|
| `PLAN.md` | Architecture and scope; OPEN questions for the owner in section 9. |
| `FEATURES.md` | Feature list with the decision per row. The scope contract. |
| `DECISIONS.md` | Architecture Decision Records, append-only. |
| `DESIGN.md` | Brand, tokens, layout, every screen, the design review method. |
| `presets.md` | File formats: recipe sidecars, presets, looks, batch recipes; pipeline order; imports. |
| `CODING_STANDARDS.md` | Binding rules for the app and the library. |
| `ROADMAP.md` | Milestones with exit criteria. |
| `PROGRESS.md` | Session log and milestone status, newest first. |
| `LESSONS.md` | Mistakes in v1 and the rule each became. |
| `mocks/` | HTML mocks (open `mocks/index.html`) and `mocks/reviews/`. |
| `research/image-libraries.md` | Library and approach research behind ADR-003. |
| `research/runtime-contents.md` | Imaging library versions in the Flathub KDE runtime and the KDE neon snaps. |
| `private/` | Not published (git-ignored): licensing decisions and store keyword research. |
| `reference/analysis-pixelfx-v1.md` | Full technical audit of the old app (frozen). |

## Workflow

1. Before coding a feature: its row in `FEATURES.md` says `KEEP`, and if it is visible, its
   mock is reviewed and approved.
2. While coding: follow `CODING_STANDARDS.md`; new architectural choices get an ADR.
3. After coding: update the row's status, add a `PROGRESS.md` entry, run the tests.
4. Commit style: `area: plain sentence`, no trailers.
