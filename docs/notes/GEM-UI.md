# GEM UI consistency audit

Audited 2026-11-10 against the local GEM v1.0.3 source at
`d940cf31ee9e85af0264a83abd1526d4475ffb0c` and the latest published
`wischner/gcc-x86_64-gemix:latest` image, digest
`sha256:14c2c1638d545a175a9e16489f32a47a392c2b1ce44e140f1fffba1a96b6b423`.
The image reports the same GEM revision. The application builds against the
sibling Native source to include the accompanying shared changes.

## Authoritative layout and artwork

- GEM `lib/gemctl/gemctl.c`: `gemctl_chrome_layout` derives the band from
  `max(12, character_height + 4, box_height - 2)`. Toolbar painting supplies
  the bottom rule; bitmap drawing centers original artwork in square slots.
- TED `src/apps/ted/toolbar.c`: eight original 16×16 monochrome command masks,
  square band-sized slots, separators and disabled stipple. No partial buttons.
- Gemscape `src/apps/gemscape/gemscape_ui.c`: the same chrome calculation,
  centered bitmap resources and an inset address editor. Its four navigation
  icons are not substitutes for spreadsheet commands.
- Native `lib/native/toolbar.cpp` and `toolkits/gemix/theme.cpp`: shared layout,
  input dispatch, native field metrics, GEM borders and icon feedback.

## Changes

Isaac previously fixed all three strips at 30 pixels and specified manual
positions and 24-pixel combo heights. Native also forced its generic 24-pixel
single-line height, so reducing a strip alone could make the fields overflow.

Isaac now requests the toolkit toolbar height, allowing the native input height
and toolbar insets to establish a minimum. Both command and formatting rows
use the same calculation, as does the formula strip. Combo positions and
heights are owned by Native; Isaac only supplies meaningful field widths.
GEM's shared theme uses the chrome band and an inset native field. No application
code guesses GEM dimensions or paints toolbar borders.

Formatting and zoom combos use Native's existing per-control
`set_border_sides(right)` option and opt into `tool_item::control_fill_width`.
Adjacent fields touch and draw one vertical divider. The toolbar supplies the
horizontal rules, avoiding doubled outlines. Dialog controls retain their
default borders; Native's new width option defaults to false, preserving
existing hosted-control layout.

Tool-group separators request one square button's width via `tool_item::length`,
measured from Native's actual command slot. GEM draws the separator at the
gap's trailing edge, so the gap cannot look like part of Undo or another
following button. This does not add gaps between the adjoining combo fields.

The formula strip also fills its hosted slot width. Its name and formula
areas touch, use Native's content surface, and draw only the name area's
right divider. The toolbar supplies the horizontal rules. Formula hit testing
and caret placement use the same adjoining text-area geometry.

New, Open, Save, Undo, Cut, Copy and Paste resolve to Native's shared TED masks.
Save As is mapped too, without adding a toolbar button. Native exposes semantic
IDs for Close, Redo, Print and Find but returns no stock image when GEM has none.
Isaac retains its existing fallback for Redo and Find. Spreadsheet formatting,
sorting, autosum and other domain artwork remain 16×16 embedded resources.
Fallbacks for other backends are retained; no standard GEM bitmap is vendored
in this application. The shared Native catalog preserves TED's GPL attribution.

## Controls reviewed

| Purpose | Native control / decision |
| --- | --- |
| Hierarchy | `tree_view` exists with expansion, stable IDs, navigation and scrolling. Isaac has no hierarchical view to replace. |
| Tabular records | `table_view` / `table_model` exist with row selection, virtualization, sorting, scrolling and column resizing. No ordinary record table is custom-drawn in Isaac. |
| Spreadsheet | Retain `grid_canvas` on `native::canvas`: the row-oriented table lacks rectangular cell selection, merged cells, per-cell styles, variable row heights, formula/reference editing and autofill handles. A substitution would remove working features. |
| Formula editor | Retain the canvas using Native's content surface, border primitive and stock font. `text_edit` lacks public caret positioning/insertion and submit/cancel integration required by the shared spreadsheet editor. |
| Row/column headers | Retain the `native::ruler` specialization for spreadsheet labels, axis resizing and selection. |
| Formatting/zoom | Existing `combo_box` hosted by `toolbar`; native sizing, selection and popup handling. |
| Function chooser | Existing `list` and `combo_box`; no custom list replacement needed. |
| Other controls | Existing `button`, `check`, `text_edit`, file dialogs, `tab_view` in optional selection-only mode, status bar, canvas scrollbars and modal/modeless windows. Native also provides `split_view`, `tab_view` and `icon_view`; Isaac does not need new instances for this task. |

No custom tree or generic table was found, and no replacement with reduced
functionality was made. Toolbar actions, command IDs and shortcuts are unchanged.

## Validation

The desktop suite checks standard-icon identity and 16×16 dimensions, toolbar
height consistency, native field containment and vertical centering, including
window narrowing and restoration. Its existing command and interaction checks
are retained. GEM passed 249/249 checks; SDL2 Debug passed 232/232. Native's existing
toolbar/ruler regression executable also passed against the GEM archive. Captures of Isaac, TED
and Gemscape show 20-pixel bands and centered 18-pixel Isaac combo boxes.
Test results and remaining runtime limits are recorded in
[platform status](PLATFORM-STATUS.md). OS-level input and manual visual acceptance
are distinct from these public-entry-point tests.

## Optional native sheet tabs

Isaac now uses `native::tab_view` with `set_selection_only(true)`, hosted in a
Native toolbar. The option defaults to false, so existing tab views keep their
borrowed page windows. This mode supplies labels without application pages and
uses the same native peers and tab drawing as ordinary tabs. It preserves one
spreadsheet canvas and its GEM frame scrollbars. The previous separately painted
`tab_strip` is no longer used by Isaac. The tab band's height comes from the
created tab control, with the theme's hosted-control insets.

Tests cover sheet selection, double-click renaming, insert/delete/undo, and the
optional API's disabled items, overflow, resizing and ownership restrictions.
The old page lifecycle remains covered by Native's existing API suite.
