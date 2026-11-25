# Isaac

Isaac is a spreadsheet rebuilt from the ground up from
[Gnumeric](https://gitlab.gnome.org/GNOME/gnumeric), in modern C++20, on the
Native UI library. It reads and writes Gnumeric's `.gnumeric` files, and is the
sibling of [Writer](https://github.com/wischner/native-abiword), with the same menus, toolbars and status
bar.

It is an early port. Formulas, 389 functions, styles, number formats, merged
cells, multiple sheets, undo, clipboard, autofill, sort and find all work.
Charts, conditional formats and printing do not yet: charts and print settings
survive a save, but cannot be seen or changed. See
[port status](docs/notes/PORT-STATUS.md) before relying on a file that uses them.

## Build and run

```sh
make                         # bin/isaac: desktop
make run                     # build, then open it on your desktop
make headless                # bin/isaac-headless: no display dependency
bin/isaac-headless in.gnumeric --to out.csv
bin/isaac-headless in.gnumeric --report
bin/isaac-headless in.gnumeric --cell Sheet1!B4
make debug                   # bin/isaac-debug with ASan and UBSan
make clean                   # remove build/ and bin/
```

Dependencies: Linux with Docker, GNU make, Python 3 and an X11 or Xwayland
desktop. Every build goal runs inside the `wischner/gcc-x86_64-linux-sdl` image
(GCC 11, CMake, SDL2 and the other development packages), so the host needs no
compiler or headers. Objects go to `build/sdl2/`, binaries to `bin/`. `make run`
and the `SDL2, Local` F5 profile run the binary in the same image, drawing on
your desktop through its X11 socket.
[Native](https://github.com/wischner/native) is taken from the sibling
`../native` checkout when present, consistently for Make, CMake, platform builds
and F5 debugging. Otherwise the `master` branch is cloned
into `build/deps/native`. Explicit `NATIVE_REF` or `NATIVE_URL` select that cached
clone even when a sibling exists; `make update-native` refreshes only the clone.
`NATIVE_ROOT=/path/to/native` overrides the selection.

**VS Code F5** builds and debugs the selected backend. Profiles are named
`<name>, <env>`: `SDL2, Local`, `X11, Xephyr`, `GEM, Rasta` and so on.

Toolbar commands prefer Native's standard icons; GEM reuses TED's artwork.
Application-specific and other-backend fallback graphics are embedded with
Native's resource compiler. Native supplies toolbar dimensions, field heights,
borders and highlights. See [graphics resources](src/resources/README.md) and
[the GEM UI audit](docs/notes/GEM-UI.md).

To build and test GEM against the shared library checkout:

```sh
docker pull wischner/gcc-x86_64-gemix:latest
python3 -B scripts/build/platforms.py linux-gemix-gemd --mode debug
```

Use Native commit `5643515` or a descendant containing its changes. It includes
top-level window icons through `app_wnd::set_icon()`, standard toolbar icons,
toolbar metrics, full-width hosted controls, configurable separator gaps and
optional selection-only tabs. A cached Native clone is not
updated automatically: run `make update-native` to refresh it. Update a sibling
checkout separately; the build leaves its local work untouched.

Native `75cdcd4` includes the dialog compatibility corrections. For an older
checkout pinned to `426a42f`, the same changes are retained in
[`scripts/build/native-dialogs.patch`](scripts/build/native-dialogs.patch).
Apply it only to that older revision (from this repository):

```sh
git -C ../native apply "$PWD/scripts/build/native-dialogs.patch"
```

The build does not patch shared checkouts automatically. See
[the dialog and persistence review](docs/notes/DIALOG-REVIEW.md) for the audit,
validation results and the remaining upstream test limitation.

On OpenLook, Isaac requires Native commit `009ff38` or later. It keeps
canvas-hosted toolbars above their canvas, provides XView canvas scrollbars,
reports canvas resizes, and keeps keyboard focus off those scrollbars. Older
checkouts hide the sheet-tab bar (a blank strip at the bottom), leave an
enlarged grid black, and draw XView's focus arrow over a clicked scrollbar.

Docker supplies the GEM SDK and `gemd`; Native itself is built from the source
selected above. Complete menus require matching GEM client/server SDK **1.5.1
or later**. Pulling a newer image requires rebuilding Isaac and restarting its
GEM session so both sides use the matching protocol. Passing the desktop tests
alone does not prove that the server accepted the menu; check the runtime log
and the visible menu as described in [testing](docs/notes/TESTING.md).

All application UI goes through Native's public API. The default build selects
Native's SDL2 backend; `make platform-test` builds the others. See
[platform status](docs/notes/PLATFORM-STATUS.md).

## Using it

| Action | Keys |
| --- | --- |
| Move, extend | Arrows, Shift+arrows |
| Jump to the edge of the data | Ctrl+arrows |
| Edit the cell | F2, or double-click, or click the formula bar |
| Finish an edit and move | Enter, Tab; Shift reverses |
| Enter one value into the selection | Ctrl+Enter |
| Enter an array formula | Ctrl+Shift+Enter |
| Point at a reference while typing a formula | Arrows or mouse after an operator |
| Next or previous sheet | Ctrl+PageDown, Ctrl+PageUp |

Drag the square at a selection's corner to continue a series. Drag a header edge
to resize, and double-click it to fit the content. Click the cell name in the
formula bar to go to a cell, range or defined name.

## Tests

```sh
make test                    # unit tests under ASan and UBSan, no display
make ui-test                 # the real window, through Native's input hooks
make platform-test           # every backend SDK; PLATFORM=linux-x11 for one
make list-platforms
```

See [testing](docs/notes/TESTING.md). Tests run in Docker, so ASan works even
on hosts that preload a security agent; see [that note](docs/notes/HOST-ASAN.md).

## Desktop icon

Isaac's desktop icon is [`assets/icons/ISAAC.PNG`](assets/icons/ISAAC.PNG): 32x32
pixels, black ink on white paper, transparent outside the icon. The build
copies it next to the executable: `make` writes `bin/ISAAC.PNG`, and every
release zip has `ISAAC.PNG` beside `isaac`.

The GEM artwork and [`assets/icons/openlook-isaac.png`](assets/icons/openlook-isaac.png)
are embedded as variants of `icons.application`. Native's resource lookup
selects the OpenLook artwork on that toolkit and the GEM artwork elsewhere,
without platform checks in application code. The selected image is assigned
to each main window with `app_wnd::set_icon()` before creation; OpenLook shows
it when the window is minimized. `app_wnd::set_icon_text("Isaac")` keeps the
minimized icon label fixed as **Isaac**, independently of the workbook title.
This requires Native commit `33ead43` or later. Native sizes it for each
backend: Windows, SDL2 and X11 desktops use window icons; macOS uses the main
window's icon for the Dock. GTK on Wayland, elementary OS, Haiku and GEM cache
the image without displaying a per-window icon. Their desktop integration
still requires the platform's application icon packaging.

To build a GEMiX image, take `ISAAC.PNG` from the `linux-gemix-gemd` release
zip, convert it with GEM's `resgen` and install it under the lowercase program
name, matching the program entry `/opt/gem/Programs/isaac`:

```sh
resgen icons -o isaac.rsc ISAAC.PNG
install -m 0644 isaac.rsc /opt/gem/share/gem/icons/isaac.rsc
```

GEM Desktop then shows the icon for `isaac` in the Programs folder. Without
the file, Desktop shows the standard program icon. GEM's
[program deployment guide](https://github.com/triglav-os/gem/blob/main/docs/guides/PROGRAM_DEPLOYMENT.md)
explains how Desktop finds icons.

## Releases

Pushing a `vX.Y.Z` tag builds every backend and publishes one zip per
platform on the GitHub release page. See [releasing](docs/notes/RELEASING.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md): layers and design decisions
- [Port status](docs/notes/PORT-STATUS.md): what has moved from Gnumeric, module
  by module
- [Native changes](docs/notes/NATIVE-CHANGES.md): what this project added to Native
- [Licensing](docs/notes/LICENSING.md)
- [Coding standards](docs/standards/)

## License

GNU General Public License, version 2 or later. See [LICENSE](LICENSE),
[third-party notices](THIRD-PARTY-NOTICES.md) and the open question in
[licensing](docs/notes/LICENSING.md).
