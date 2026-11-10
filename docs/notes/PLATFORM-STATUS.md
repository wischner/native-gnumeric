# Platform status

Updated 2026-11-10. GEM, SDL2 and Motif results include the Native tab migration;
other backend results are from 2026-11-07 and were not rerun for this change.
"End to end" means real input went through the toolkit into the running app.

| Backend | F5 profile | Builds | Latest desktop test | Typing verified by |
| --- | --- | --- | --- | --- |
| SDL2 | SDL2, Local | Yes | 377/377, Debug | Native routing test |
| X11 | X11, Xephyr | Earlier build | 151/151, earlier suite | End to end: click, typing, shifted formula, arrows, Ctrl+B |
| Motif | Motif, Xephyr | Yes | 242/242, tab migration; final toolbar changes not rerun | Not by itself; same input path as Writer's |
| OPEN LOOK | OPEN LOOK, Xephyr | Earlier build | 151/151, earlier suite | Not by itself; same input path as Writer's |
| Window Maker | Window Maker, Xephyr | Earlier build | 151/151, earlier suite | Not by itself; same input path as Writer's |
| GEM (gemd) | GEM, Rasta | Yes | 394/394, Debug | Native's Rasta key packet test |
| Wayland | None | Earlier build | Not run | **Not verified** |
| elementary | None | Earlier build | Not run | **Not verified** |
| Haiku | Haiku, VM | Earlier cross-build | Not run | **Not verified** |
| Windows | None | Earlier cross-build | Not run | **Not verified** |
| macOS | None | Not built | Not run | **Not verified** |

Earlier SDL2, X11 and GEM captures showed the loan template fully calculated.
The final GEM capture also shows the menu after updating to SDK 1.5.1; the
previous missing-menu observation applied to the older GEM runtime.

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

## Initial toolbar audit (2026-11-10)

At the initial audit stage, the sibling Native toolbar metrics and standard-icon
changes passed **249/249** GEM desktop checks. SDL2 Debug passed **232/232**, with Native compiled under AddressSanitizer and UBSan.
Native's existing toolbar/ruler/control regression executable also passes
against the same GEM archive. All 128 words in the eight shared icon masks
match TED v1.0.3 exactly. The different desktop totals reflect GEM's available
standard-icon checks. Build logs
are in `build/platforms/linux-gemix-gemd/release/` and
`build/platforms/linux-sdl2/debug/`.

Private Xvfb/Rasta captures were inspected for Isaac, TED and Gemscape:
`build/ui-review/isaac.png`, `ted.png`, and `gemscape.png`. All show 20-pixel
GEM toolbar bands; Isaac's second row has centered 18-pixel native combo boxes.
Original command artwork stays 16×16. These dimensions are observed results of
the GEM theme, not application constants. Gemscape was opened at `about:blank`.

The tests exercise public input entry points, toolbar actions, combo selection,
keyboard navigation, mouse selection, header resizing, clipboard and editing.
This audit does not add OS-level keyboard/mouse acceptance or verify every
backend. See [GEM UI](GEM-UI.md) for native-control suitability decisions.

Default-build follow-up: the GEM Debug build now resolves `../native` without
an environment override and passes **249/249** checks. Standalone fetching uses
the existing upstream `master` branch; the former extension branch was removed.

## Optional tab-view migration

Isaac now opts into Native's existing `tab_view` with label-only items, hosted
in a toolbar at the canvas's bottom edge. Default page-owning tab views remain
unchanged. GEM Debug passed **259/259** desktop checks; SDL2 Debug and Motif
Debug each passed **242/242**, including selection, double-click rename,
insert/delete and undo. Native's full window API suite (including existing page
lifecycle checks and new optional-mode tests) and collection runtime suite passed
against the same GEM Debug library with ASan/UBSan.

Real pointer input through Xvfb → Rasta → GEM selected Sheet2 and opened the
rename dialog on double-click. The inspected captures are
`build/ui-review/isaac-tabs.png`, `isaac-tabs-selected.png`, and
`isaac-tabs-rename.png`. The first shows the standard GEM sloped-tab rendering.
This does not establish runtime acceptance on macOS, Windows, Haiku, GTK or other
unexecuted backends; their page handling was adapted but those desktops were
not exercised during this change. The earlier GEM runs logged menu-installation
failures; the final run below no longer does.

## Final toolbar validation

GEM Debug passed **394/394** desktop checks and SDL2 Debug passed **377/377**
after joining the combo/formula fields and sizing separators to one square
command slot. Checks cover field containment, alignment, square visible buttons,
separator widths, narrow-window clipping, commands and sheet-tab interactions.
The final GEM capture shows the menu, square Undo button, single field dividers
and button-width group gaps. No menu-installation failure appears in the final
GEM test log. Both builds use the sibling Native checkout inside Docker.
