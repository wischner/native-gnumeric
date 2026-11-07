# Isaac graphics resources

`isaac.json` declares 26 toolbar icons under `icons.toolbar.<command>`. Each has
an unqualified color PNG and a `color: monochrome` PNG. Native's
`resources::get()` selects the latter automatically on GEMix and X11.

Seventeen are Writer's, byte for byte, so the two applications share their
toolbars. Nine are Isaac's own: `autoSum`, `sortAscending`, `sortDescending`,
`mergeCells`, `formatPercent`, `formatCurrency`, `decimalsMore`, `decimalsLess`
and `borderOutline`, drawn in the same hand. All are original project artwork,
copyright 2026 Tomaz Stih, GPL-2.0-or-later, 16×16, not copies of Gnumeric's.
`export_icons.cpp` holds the drawing instructions.

Monochrome pixels are black with either zero or full alpha, so Native can tint
the mask white on a selected button.

## Module transfer

Gnumeric's `src/pixmaps` and GResource bundle are replaced by this manifest and
the public Native resource API.

| Entry point | Responsibility |
| --- | --- |
| `ui::graphics_resources()` | Decode the embedded bytes once into an immutable `native::resources` |
| `ui::tool_icon(command)` | Resolve `icons.toolbar.<command>` and decode the selected PNG; unknown commands return null |
| `export_icons` | Reproduce both asset sets from the drawing functions, outside normal builds |
| Native `rc` | Validate the manifest and generate the pack and embedded translation unit |

## Build and deployment

`make` builds Native's `rc`, compiles the manifest and links the generated C++
into Isaac. `make resources` produces just the pack and generated source.

CMake SDK builds generate `build/platforms/<backend>/<mode>/resources/`. The
platform driver prepares resources in the Linux SDL2 SDK before Windows or Haiku
cross-compilation and passes `ISAAC_RESOURCE_CPP` to the destination build.

## Regenerate the artwork

From the repository root, after a normal Make build:

```sh
cmake --build build/native-release --target export_icons -j8
build/native-release/export_icons src/resources/icons
make resources
```
