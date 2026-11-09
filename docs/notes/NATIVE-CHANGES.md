# Native changes made for Isaac

These extend [Native](https://github.com/wischner/native) on its
`fyodor-native-extensions` branch. A contract added to Native has to work on
every backend, so nothing here is SDL2-only.

| Change | Why | Backends |
| --- | --- | --- |
| `native::tab_strip` | Sheet tabs choose what one grid shows. `tab_view` needs a page window per tab, and a canvas per sheet would break GEM's single frame-scrollbar document | All, shared code |

`tab_strip` follows `ruler` and `status_bar`: a shared library-painted
`non_client` strip with protected draw stages, silent setters and signals for
user actions only. Its contract is in Native's `ARCHITECTURE.md` section 17, it is
taught in chapter 22 of Programming Native, and `check_tab_strip` in
`native_ruler_toolbar_tests` covers it on every backend.

## Fixed along the way

- X11 `canvas` destroyed its widget before its children. Outside an Xt dispatch
  that frees the tree at once, so hiding a toolbar with a hosted combo box crashed.
  This was present before Isaac. It stopped `native_ruler_toolbar_tests`, and with
  it the `tab_strip` check, from running on X11.

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

So the bar is a `canvas` that draws the view's one `cell_editor` inside
`theme::draw_text_edit_frame`, and the keyboard stays on the grid, whose text path
every backend already implements. No Native change, no toolkit branch. If
`text_edit` later gains caret access this can be revisited, but a single editor
model is simpler either way.

## Requests for later

- `non_client` has no way to choose a pointer shape, so a header edge shows no
  resize cursor.
- `canvas` has no context-menu signal.
- Native has no static text control; dialogs paint their prompts.
