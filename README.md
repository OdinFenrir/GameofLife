# Game of Life

An SFML toy that combines a Game-of-Life-style neighborhood rule with an energy + nutrient field (diffusion + regrowth), so patterns grow, compete, collapse, and migrate.

This version includes **two species** with slightly different survival/reproduction parameters, plus a small mutation chance on birth, which tends to create longer-lived “succession” instead of settling into a single stable mush.

## Controls
- Click `START` to begin
- `Space`: pause/resume
- `R`: reset (new nutrient field + seed)
- `C`: clear all life (keep nutrients)
- `N`: toggle nutrient background
- `T`: toggle trails
- Left click: place a small life seed (species 0)
- Shift + left click: place a small life seed (species 1)
- Right click: add a nutrient patch
- `Esc`: back to menu

## Build
See `BUILDING.md`.

## Smoke test
Run `gameoflife.exe --smoke` to auto-close after a couple seconds.