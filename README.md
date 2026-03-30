# Game of Life

## Overview
An SFML simulation that combines a Game-of-Life-style neighborhood rule with an energy and nutrient field (diffusion + regrowth), so patterns grow, compete, collapse, and migrate.

This version pushes beyond “random mutation” by making mutations **heritable traits** (multipliers over metabolism and reproduction), plus a small chance to mutate the **neighbor-rule window**. The environment then *selects* which traits survive and reproduce.

## Tech Stack
- C++17
- SFML
- CMake
- vcpkg

## Features
- Trait mutation and selection pressure
- Two baseline species seeds
- Nutrient injection and field diffusion
- Camera pan and zoom controls
- Simulation speed controls and presets

## Screenshots
![Game of Life simulation screenshot](assets/gameoflife.png)

## Run Locally
- Build steps: see `BUILDING.md`
- Smoke test:
  ```sh
  gameoflife.exe --smoke
  ```

## Controls
- Click `START` to begin
- `Space`: pause/resume
- `R`: reset (new nutrient field + seed)
- `C`: clear all life (keeps nutrients)
- `N`: toggle nutrient background
- `T`: toggle trails
- `M`: toggle movement (food-seeking)
- Left click: place a small life seed (species 0 baseline)
- Shift + left click: place a small life seed (species 1 baseline)
- Right click: add a nutrient patch
- `Esc`: back to menu
- `+` / `-`: speed up / slow down the simulation ticks
- Arrow keys: pan the viewport
- Mouse wheel: zoom in and out (clamped between 0.5x and 4x)
- `1`, `2`, `3`: spawn preset clusters and reset view

## Notes
- The grid is intentionally very fine now (`kCellSize = 2`, i.e. `1280x720` cells) for very small squares.

## Roadmap
- [ ] Add deterministic scenario presets for benchmarking
- [ ] Export simulation stats to JSON
- [ ] Add short demo GIF to README
