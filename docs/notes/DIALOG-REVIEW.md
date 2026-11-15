# Dialog and persistence review

Reviewed against Native `426a42f` (2026-11-15), especially
`src/vision_edit_form.cpp`, `input_geometry.cpp`, `wnd_focus.cpp`, the theme
metrics and the public grid layout API. The accompanying Native corrections
were published in Native `75cdcd4` and are present in the sibling `../native`
checkout. `scripts/build/native-dialogs.patch` retains those changes for older
checkouts pinned to `426a42f`.

## Inventory and layout

All menu routes, toolbar commands, sheet-tab rename and formula-bar Go To
routes were traced. There are **14 application dialog purposes**, implemented
by **three custom windows, two Native selectors and Native message boxes**.
All **eight custom form purposes** were updated; the two SDL selectors also
received fixes in their shared Native implementation. SDL message boxes were
also corrected after visual inspection exposed clipped multiline content.

| Dialog | Implementation | Review / result |
| --- | --- | --- |
| Go To | `input_window` | Measured prompt, themed field/button heights, grid layout, initial text focus |
| Rename Sheet | `input_window` | Same layout; existing name loaded; accept/cancel preserved |
| Column Width | `input_window` | Current points loaded; finite numeric input required |
| Row Height | `input_window` | Current points loaded; finite numeric input required |
| Custom Number Format | `input_window` | Existing format loaded; accept/cancel preserved |
| Define Name | `input_window` | Full example prompt fits; existing command and undo retained |
| Insert Function | `function_window` | Themed category, eight-row list, wrapped read-only scrollable description, action row |
| Find and Replace | `find_window` | Added field labels; aligned fields, spaced checkboxes and action row |
| Open / import | Native Open selector | Filter wiring reviewed; SDL ordered traversal and initial list focus corrected |
| Save As / export | Native Save selector | Name/extension/filter wiring reviewed; SDL initial filename focus and keyboard actions corrected |
| About | Native message box | SDL multiline text wrapping/scrolling and button traversal corrected |
| Save changes confirmation | Native message box | SDL ordered yes/no/cancel keyboard actions corrected; existing asynchronous Save As behavior retained |
| Could not open | Native message box | Existing error reporting retained |
| Could not save | Native message box | Receives more precise file-operation diagnostics |

All custom forms use `form_layout` metrics from the active Native theme and
control font, Native grid tracks, measured margins/gaps, minimum client sizes
and resize callbacks. They center on their owner and clamp to the display work
area. There is no blanket dialog-size increment. SDL's shared file browser
also clamps its initial size/position and enforces its existing 640×420 logical
pixel content minimum. Displays below that minimum remain unsupported by that
selector.

Tab order is explicit: answer → OK → Cancel; category → list → description →
OK → Cancel; search → replacement → Match case → Whole cell → Find → Replace →
Replace All. Labels are painted, never tab stops. Each opening restores initial
focus. Native supplies reverse traversal, wrapping and exclusion of unavailable
controls. Its single-line Enter-as-Tab behavior is retained.

The Native update consumed spreadsheet Tab before its editor could commit.
The shared fix lets canvases with order zero keep Tab/Shift+Tab; positive-order
canvases still participate in forms. SDL's button/check/radio keyboard actions
and the file browser's separate event loop also needed corrections.
SDL message boxes now wrap paragraphs in a read-only scrollable text control
and provide explicit traversal and activation of their buttons. Those
fixes belong to Native, rather than toolkit branches in Isaac.

## Settings and persistence

This application has **no Preferences dialog, user configuration subsystem,
system configuration, persistent search history, application cache, runtime
state directory or persistent log file**. Command-line `app::options` are
runtime arguments. Search fields and match options are transient query state.
Do not confuse those with preferences that promise to survive restarts.

Workbook settings remain in the user-selected `.gnumeric` document: sheet
names, dimensions, number formats, defined names, grid/formula visibility and
per-sheet zoom. No new format, keys or migration are needed. Existing charts
and print settings continue to round-trip through the existing XML preservation
mechanism; there are no chart or print-settings dialogs in this port.

Corrected defects:

- Zoom was serialized but the view neither loaded nor updated the sheet value.
  It now restores on load/sheet change and saves per sheet. Non-finite values
  cannot enter rendering geometry; invalid saved zoom falls back to 100%.
- Zoom/grid/formula changes did not mark the workbook modified. A separate
  view-change flag preserves that information across cell undo/redo and clears
  only on save/reset, without changing the established cell undo history.
- Selecting a dynamically added zoom percentage indexed past the fixed preset
  array. Selection now reads the displayed item safely.
- Dimension input accepted numeric prefixes and NaN. Full finite values in
  the existing `(0, 1000]` range are now required.
- Single-cell Replace ignored case-insensitive matching, despite Find and
  Replace All honoring the checkbox. The replacement now uses the selected
  matching rule.
- Save used a predictable `<document>.tmp` that could truncate an unrelated
  file or follow a pre-created symlink. It now creates an exclusive random
  temporary beside the destination, flushes/syncs/closes it, then atomically
  replaces the destination. Failures remove only the temporary we created and
  preserve the previous file. Existing permissions are retained; new files
  are private (0600 on Unix). Read failures are also reported.

## Filesystem and logging

Documents and exports stay at paths explicitly selected by the user, not in
XDG data directories. Atomic-save temporaries must be in the destination's
filesystem; they are removed after success/failure. A missing document parent
is reported instead of being created or silently replaced with another path.

There are no application configuration files to move into `$XDG_CONFIG_HOME`,
no application-managed persistent data/cache to migrate, and no reason for
this ordinary desktop application to write `/etc`, `/var/lib` or `/var/log`.
Diagnostics remain on stderr or in the UI. No persistent log retention policy
is needed. Developer build/test logs under `build/` are build artifacts,
not installed application logs. Session framebuffer/socket/viewer files belong
to the existing temporary Docker/debug sessions. The restart test uses an
exclusive temporary directory and removes it on exit.

## Validation

- Full Debug application builds with the established Docker/GCC workflows on
  SDL and GEM, plus the headless executable; ASan and UBSan enabled.
  The headless executable also loaded and reported both sheets of the loan
  workbook fixture successfully.
- `make test`: **1,839 / 1,839** checks passed across all six unit suites.
- `make MODE=debug ui-test`: **940 / 940** desktop checks, plus **7 / 7** in the
  writer process and **20 / 20** in a fresh reader process. The latter verifies
  sheet name, dimensions, number format, defined name, zoom, grid/formula
  visibility, reopening settings, Cancel and switching sheets.
- GEM Debug portable desktop suite: **957 / 957** checks passed.
- Native's dedicated tab-order, modal runtime and real SDL keyboard runtime
  tests passed.
  Its broader window API suite reports one tab-height-reset geometry failure;
  the unchanged upstream test source reproduces the same failure. It is
  outside these dialog changes and remains unresolved.
- Atomic-file tests ran as a non-root user: new private files, preserved
  permissions, a legacy `.tmp` symlink left untouched, missing parent,
  permission-denied directory, failed rename and temporary cleanup.
- Every custom form is opened through its command in the desktop test, checked
  for containment/non-overlap, forward/reverse focus traversal and wrapping.
  Input/search layouts are also checked after resizing. Tests exercise Enter
  traversal, an unavailable control, invalid numeric values, cancel/reopen,
  function accept/cancel, and case/whole-cell replacement behavior.
- Live SDL screenshots were captured with Xvfb, including all eight custom
  form purposes, Open, Save As, About and the save confirmation. External X11
  keyboard events verified
  forward/reverse Tab and Enter actions in Open, Save As, Rename Sheet,
  Insert Function, About and the save confirmation, rather than relying only
  on direct callback tests.
- All three custom form implementations and Save As were additionally inspected
  at 800×600 with 100% scaling and 1280×900 with 200% scaling. Controls and action
  rows remained visible. The SDL selector/message sizing accounts for the
  window frame when fitting to the display.

Generated evidence is in `build/dialog-*.log`, `build/dialog-visual/`,
`build/native-*.log` and `build/platforms/linux-gemix-gemd/debug/`.
This does not establish visual/runtime acceptance on macOS, Windows, Haiku or
all other Linux backends. GEM was exercised through the server and portable
UI checks; SDL received the external-keyboard and screenshot inspection.
