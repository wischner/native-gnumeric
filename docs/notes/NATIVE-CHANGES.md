# Native changes made for Isaac

These changes are part of [Native](https://github.com/wischner/native) on
`master`; local development uses the sibling checkout. Selection-only tabs and
full-width controls were published in `abdfcc7`; configurable separator widths
and GEM separator painting followed in `4e6f805`. Shared API defaults preserve
existing callers. Runtime coverage is listed in [platform status](PLATFORM-STATUS.md);
portable APIs alone do not establish acceptance on every backend.

| Change | Why | Backends |
| --- | --- | --- |
| `tab_view::set_selection_only(true)` | Sheet tabs choose what one independently owned grid shows, using the existing tab peer and painter | Shared model, guarded native page handling on all backends |

The option is disabled by default. Configure it before creating or populating
the tab view, then use `add_item(title)` without a page. Existing page-owning
`tab_view` callers keep the same lifecycle. Isaac hosts the tab view in a Native
toolbar at the canvas's bottom edge; it sizes the host from `get_tab_extent()`
and the theme insets. Selection, double-click rename, sheet insertion/deletion
and undo use the existing workbook handlers.

The earlier `tab_strip` remains in Native for compatibility with other callers,
but Isaac no longer uses that separate tab painter. The optional API is covered
by Native's `native_window_api_tests`, including the unchanged page-owning path.

## Fixed along the way

- X11 `canvas` destroyed its widget before its children. Outside an Xt dispatch
  that frees the tree at once, so hiding a toolbar with a hosted combo box crashed.
  This was present before Isaac. It stopped `native_ruler_toolbar_tests`, and with
  it the `tab_strip` check, from running on X11 (the historical implementation).

## Shared GEM toolbar infrastructure

The GEM UI audit uses Native's `theme::metrics::toolbar_height`,
`toolbar_padding`, `toolbar_control_padding`, and `standard_icon()` catalog.
GEM obtains its band from `gemctl_chrome_layout`, as TED and Gemscape do,
and fits single-line controls inside it. Shared toolbar placement and painting
own the insets, slot separators, clipping and native state feedback. Isaac
contains no toolkit headers or duplicated standard GEM bitmap assets.
See [the audit](GEM-UI.md) for reference sources and retained custom controls.

`tool_item::control_fill_width` optionally removes the horizontal control
insets. Isaac enables it for formatting and zoom combos, using only their
right border so adjacent fields touch with one dividing line. The option
defaults to false and leaves vertical sizing and centering unchanged.
Native's toolbar regression covers opting in and restoring the default.

Separator items also accept a positive `tool_item::length`; Isaac requests
one square button's width between tool groups. Unspecified lengths retain Native's
eight-pixel default.
GEM's `theme::draw_toolbar_separator` places the divider at the trailing edge
of the gap, keeping the following button visibly square. Other backends use
the existing separator painter by default; ordinary separators are unchanged.

## What did not need a change

Everything else is Native as it stood, which is the point of the port:

| Gnumeric used | Isaac uses |
| --- | --- |
| `GnmItemGrid` on a canvas | `native::canvas`, painting through a back-buffer `img` |
| `GnmItemBar` | `native::ruler`, derived |
| Toolbars and format bar | `native::toolbar` with `tool_kind::control` combo boxes |
| Formula bar | `native::canvas` in `tool_kind::control`; see below |
| Status bar | `native::status_bar(owner, canvas)` |
| Selection | `theme::draw_selection` |
| File chooser | `native::open_file_dialog`, `save_file_dialog` |
| Dialogs | `native::modal_wnd`, `modeless_wnd`, `message_box`, `combo_box`, `list`, `text_edit`, `check`, `button` |
| Clipboard | `native::clipboard` |
| Pango fonts | `native::font_t` |
| Resources | Native `rc` and `native::resources` |

## The formula bar is a canvas, not a text_edit

`text_edit` routes keys differently per backend and offers no submit signal,
caret position or insertion at the caret. A formula entry needs all three, to
commit on Enter and to write a pointed-at reference at the insertion point.
Adding them means changing the toolkit edit widget on about ten backends.

So the bar is a `canvas` that draws the view's one `cell_editor` on
Native's content surface, with a single divider after the cell name. The
keyboard stays on the grid, whose text path
every backend already implements. No Native change, no toolkit branch. If
`text_edit` later gains caret access this can be revisited, but a single editor
model is simpler either way.

## Requests for later

- `non_client` has no way to choose a pointer shape, so a header edge shows no
  resize cursor.
- `canvas` has no context-menu signal.
- Native has no static text control; dialogs paint their prompts.
