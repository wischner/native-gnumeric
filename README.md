# Isaac

Isaac is a spreadsheet rebuilt from the ground up from
[Gnumeric](https://gitlab.gnome.org/GNOME/gnumeric), in modern C++20, on the
Native UI library. It reads and writes Gnumeric's `.gnumeric` files, and is the
sibling of [Writer](../native-abiword), with the same menus, toolbars and status
bar.

It is an early port. Formulas, 389 functions, styles, number formats, merged
cells, multiple sheets, undo, clipboard, autofill, sort and find all work.
Charts, conditional formats and printing do not yet: charts and print settings
survive a save, but cannot be seen or changed. See
[port status](docs/notes/PORT-STATUS.md) before relying on a file that uses them.

## Build and run

```sh
make -j8                     # bin/isaac: desktop
bin/isaac workbook.gnumeric
make headless -j8            # bin/isaac-headless: no display dependency
bin/isaac-headless in.gnumeric --to out.csv
bin/isaac-headless in.gnumeric --report
bin/isaac-headless in.gnumeric --cell Sheet1!B4
make debug -j8               # bin/isaac-debug with sanitizers
make clean                   # remove build/ and bin/
```

Dependencies: Linux, GCC with C++20, GNU make 4.3+, `flock`, CMake 3.20 and Python 3.
Native comes from the sibling `../native` checkout; override with
`NATIVE_ROOT=/path/to/native`. On Ubuntu without development headers the build
fetches them into `build/sdk/` and installs nothing.

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
make test                    # unit tests, no display
make ui-test                 # the real window, through Native's input hooks
make docker-test             # unit tests under ASan and UBSan
make platform-test           # every backend SDK; PLATFORM=linux-x11 for one
make list-platforms
```

See [testing](docs/notes/TESTING.md). ASan cannot start on hosts that preload a
security agent; see [that note](docs/notes/HOST-ASAN.md).

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
