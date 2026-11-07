# Platform status

Recorded 2026-11-07. "End to end" means real input went through the toolkit into
the running application.

| Backend | F5 profile | Builds | Desktop test | Typing verified by |
| --- | --- | --- | --- | --- |
| SDL2 | SDL2, Local | Yes | 151/151, also in Debug | Native routing test |
| X11 | X11, Xephyr | Yes | 151/151 | End to end: click, typing, shifted formula, arrows, Ctrl+B |
| Motif | Motif, Xephyr | Yes | 151/151 | Not by itself; same input path as Writer's |
| OPEN LOOK | OPEN LOOK, Xephyr | Yes | 151/151 | Not by itself; same input path as Writer's |
| Window Maker | Window Maker, Xephyr | Yes | 151/151 | Not by itself; same input path as Writer's |
| GEM (gemd) | GEM, Rasta | Yes | 151/151 | Native's Rasta key packet test |
| Wayland | None | Yes | Not run | **Not verified** |
| elementary | None | Yes | Not run | **Not verified** |
| Haiku | Haiku, VM | Cross-build | Not run | **Not verified** |
| Windows | None | Cross-build | Not run | **Not verified** |
| macOS | None | Not built | Not run | **Not verified** |

Screens were captured and inspected on SDL2 and X11 under Xvfb, and on GEM from
the Rasta framebuffer, each showing Gnumeric's loan template fully calculated.
Writer and Isaac match on all three. Neither shows a menu bar in a GEM framebuffer
capture.

The end-to-end X11 run used a throwaway XTest probe, which is not in the
repository. `tests/ui/test_desktop.cpp` remains the portable suite and enters above
the backend, so see [TESTING.md](TESTING.md) for what it cannot show.

Unit tests pass 1,801 of 1,801 under ASan and UBSan in
`wischner/gcc-x86_64-linux-sdl`. The host runs UBSan only; see
[HOST-ASAN.md](HOST-ASAN.md).

## Known limits

- **GEM** types ASCII only, because its key packets carry nothing more.
- **Monochrome backends:** cell shading dithers, so dark fills can make small text
  hard to read. A workbook's colors are not changed.
- **Windows, Wayland, elementary, Haiku, macOS:** text routing is written in
  Native, and all but macOS compile here, but Isaac has not been run on them.
