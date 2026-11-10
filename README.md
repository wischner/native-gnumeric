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
[Native](https://github.com/wischner/native) (branch
`fyodor-native-extensions`) is cloned into `build/deps/native` on first build;
`make update-native` moves it to the branch's newest commit. To build against
your own checkout, pass `NATIVE_ROOT=/path/to/native`.

**VS Code F5** builds and debugs the selected backend. Profiles are named
`<name>, <env>`: `SDL2, Local`, `X11, Xephyr`, `GEM, Rasta` and so on.

Toolbar graphics are embedded with Native's resource compiler. GEM and X11
automatically select black-and-white icons; Native supplies menu and toolbar
highlights. See [graphics resources](src/resources/README.md).

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
