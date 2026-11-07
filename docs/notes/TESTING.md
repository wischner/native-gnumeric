# Testing

UI tests use Native and nothing else, so one test source covers every backend.

| Layer | Runs | How |
| --- | --- | --- |
| `tests/unit/` | `make test` | Headless, with `fixed_font_provider` for exact metrics |
| `tests/ui/test_desktop.cpp` | `make ui-test`, `make platform-test` | The real `main_window` |

## Unit suites

| Suite | Covers |
| --- | --- |
| `test_numfmt` | Number formats, General, dates, entry recognition, gzip |
| `test_engine` | Operators, printing, recalculation, a 65,536-cell chain, cycles, names, arrays, structural edits, autofill, sort, styles, axes |
| `test_functions` | Every family, against documented results |
| `test_impexp` | Round trips, CSV, HTML, failures, Gnumeric's samples, undo through every command class |
| `test_view` | Navigation, editing, pointing, clipboard, fill handle, painting through a recording painter |

`test_undo_fidelity` in `test_view` runs each named command with one cell, a
range and the whole sheet selected, and compares a complete snapshot of the
workbook after undo.

## Gnumeric's samples

`.gnumeric` files store no results for formula cells, so they are not an oracle.
They are a parser and loader corpus: every sample in Gnumeric's `samples/` and
`templates/` is expected to load with no formula left unparsed.

```sh
for f in ../../fork-vault/gnumeric/samples/*.gnumeric; do
    bin/isaac-headless "$f" --report
done
```

`--report` lists each sheet's cells and calculation errors. `#NAME?` there means
a function not yet ported; see [PORT-STATUS.md](PORT-STATUS.md).

`test_functions` is the conformance suite. Its expected values come from the
function documentation in Gnumeric and Excel and from hand calculation. Gnumeric
itself is not installed here, so nothing has been compared cell by cell against
a running Gnumeric.

## The desktop test

The test links the application's window and enters through Native's public
`on_native_*` entry points. It verifies results in the saved `.gnumeric`, the view
and the pixels of the painted back buffer. It reaches private members through
`friend class ::desktop_test`, which keeps test accessors out of the public
interface.

Committed test text is preceded by a fresh unhandled physical key and followed
by release, matching Native's shortcut-text suppression contract.

`test_every_command` runs every menu row on a loaded workbook, undoes them all,
and checks the workbook came back.

**What it does not cover.** Those entry points sit just above each backend, so
the backend's own step from OS event to `on_native_*` is outside this test. That
step belongs to Native and is covered by Native's tests. Passing here on a
backend therefore does not prove a user can type there; see
[PLATFORM-STATUS.md](PLATFORM-STATUS.md).

## Platforms

`make platform-test` runs `scripts/build/platforms.py`, which configures, builds
and runs the desktop test in each image from `scripts/build/platforms.json`.
Logs land in `build/platforms/<name>/<mode>/`.

F5 profiles are named `<name>, <env>`, such as `GEM, Rasta`. Each builds through
the same script and debugs through `scripts/debug/linux.py`.
