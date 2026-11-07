# Port status

What has moved from Gnumeric `9a738f938` and what has not, module by module.
**Ported** means the behavior exists and is tested. **Partial** lists its gap.

## Core

| Gnumeric | Isaac | Status |
| --- | --- | --- |
| `value`, `value-sheet`, `go-val` | `lib/val/value` | Ported. Values carry no format; see `auto-format` |
| `position`, `ranges` | `lib/val/coords` | Ported |
| `parse-util` | `lib/val/coords`, `lib/expr/parser` | Partial: A1 only, no R1C1 or other locale conventions |
| `parser.y` | `lib/expr/parser` | Partial: no 3-D references, no `A1:INDEX()` range constructors |
| `expr` | `lib/expr/nodes` | Ported |
| `expr-name` | `lib/expr/node`, `lib/book/workbook` | Ported |
| `expr-deriv` | none | Not ported |
| `func`, `func-builtin` | `lib/expr/function`, `lib/func` | Ported; no function help documents |
| `auto-format` | `expr::suggest_format` | Partial: date, time, percent and money by function, not through operators |
| `collect`, `rangefunc`, `rangefunc-strings` | `lib/func/collect`, `fn_stat` | Ported |
| `criteria` | `lib/func/collect` | Ported; no regular expressions |
| `mathfunc`, `sf-*`, `complex` | `lib/func/mathfunc` | Rewritten from published algorithms, not ported; see [licensing](LICENSING.md). Bessel functions absent |
| `gnm-random` | `lib/func/fn_math` | `std::mt19937_64` replaces it |
| `number-match` | `lib/numfmt/match` | Partial: US month-first dates only |
| `gnm-format`, goffice `go-format` | `lib/numfmt/format` | Partial: no `*x` fill expansion, no conditional colors beyond the section's |
| `gnm-datetime` | `lib/numfmt/date` | Partial: 1900 system; no 1904 workbooks |
| `gutils` | `lib/util` | What was used |

## Workbook

| Gnumeric | Isaac | Status |
| --- | --- | --- |
| `cell` | `lib/book/cell` | Ported |
| `sheet`, `sheet-utils` | `lib/book/sheet`, `shift` | Ported; no freeze panes |
| `workbook`, `workbook-view` | `lib/book/workbook`, `lib/view` | Ported |
| `dependent` | `lib/book/dependents`, `workbook` | Ported; explicit stack in place of recursion |
| `colrow` | `lib/book/axis` | Partial: no outline groups |
| `mstyle`, `style`, `style-color`, `style-border` | `lib/book/style` | Partial: no hyperlinks, validation, input messages or conditions in styles |
| `sheet-style` | `lib/book/style_map` | Rewritten as interval maps |
| `sheet-merge` | `lib/book/sheet` | Ported |
| `clipboard` | `lib/book/region` | Ported |
| `sheet-autofill` | `lib/book/fill` | Partial: numbers, names, trailing numbers; no monthly date series |
| `sort` | `lib/book/fill` | Ported; sort dialog not ported |
| `search` | `lib/book/fill` | Ported; no regular expressions |
| `commands`, `undo`, `cmd-edit` | `lib/cmd` | Ported for the commands listed below |
| `sheet-conditions`, `style-conditions` | none | Not ported; warned on load |
| `sheet-filter` | none | Not ported; kept on save |
| `sheet-object*`, `gnm-so-*`, `graph` | none | Not ported; kept on save |
| `hlink`, `input-msg`, `validation` | none | Not ported; warned on load |
| `consolidate`, `format-template`, `file-autoft`, `sheet-diff` | none | Not ported |
| `go-data-*`, `gnm-sheet-slicer*`, `gnm-data-cache-source`, `gnm-commands-slicer` | none | Not ported: no pivot tables |
| `print`, `print-cell`, `print-info` | none | Not ported; print settings kept on save |

## File formats

| Gnumeric | Isaac | Status |
| --- | --- | --- |
| `xml-sax-read` | `lib/impexp/gnumeric_read` | Ported. Unmodeled sheet parts preserved verbatim |
| `xml-sax-write` | `lib/impexp/gnumeric_write` | Ported |
| `stf`, `stf-parse`, `stf-export` | `lib/impexp/text_formats` | Partial: CSV with a guessed separator; no import dialog |
| `plugins/html` | `lib/impexp/text_formats` | Export only |
| `ssconvert` | `src/app/options` | `--to`, `--report`, `--cell` |
| `plugins/excel`, `openoffice`, others | none | Not ported |

All 107 `.gnumeric` files in Gnumeric's `samples/` and `templates/` load with no
formula left unparsed.

## Interface

| Gnumeric | Isaac | Status |
| --- | --- | --- |
| `sheet-view`, `selection` | `lib/view/grid_view` | Ported |
| `item-grid`, `cell-draw`, `cellspan`, `rendered-value` | `lib/view/grid_draw` | Partial: no rotated text, shrink-to-fit or pattern shades |
| `item-edit`, `wbc-gtk-edit` | `lib/view/editor`, `grid_edit` | Ported |
| `widgets/gnm-expr-entry` | `lib/view/editor`, `src/ui/formula_bar` | Partial: no reference coloring or function tooltips |
| `item-cursor` | `lib/view/grid_draw`, `src/ui/grid_canvas` | Ported, with the fill handle |
| `item-bar` | `src/ui/grid_header` | Partial: no outline buttons |
| `gnm-pane`, `sheet-control-gui` | `src/ui/grid_canvas` | Ported; no context menus |
| `wbc-gtk`, `wbc-gtk-actions` | `src/ui/main_window`, `lib/view/commands` | Writer's shell in place of Gnumeric's |
| `widgets/gnm-notebook` | `native::tab_strip` | Ported; no drag to reorder |
| `gui-clipboard` | `native::clipboard`, `src/ui` | Text; rich paste only inside one window |
| `gui-file` | `native::open_file_dialog`, `save_file_dialog` | Ported |
| `dialogs/dialog-search*` | `src/ui/find_window` | Ported |
| `dialogs/dialog-function-select` | `src/ui/dialogs` | Ported |
| `dialogs/dialog-goto-cell`, `dialog-define-names`, `dialog-sheet-rename`, `dialog-col-width`, `dialog-row-height` | `src/ui/dialogs` | One question dialog in their place |
| `dialogs/dialog-cell-format` | format bar, `customFormat` | Not as a dialog |
| Other `dialogs/` (63 files), `tools/` (30), `widgets/` (15) | none | Not ported: analysis tools, solver, goal seek, simulation, scenarios, data shuffling and the like |
| `application`, `session`, `gnumeric-conf`, `history` | none | Not ported: no preferences, recent files or autosave |
| `gnm-plugin`, `plugins/python-loader`, `perl-loader` | none | Not ported: functions are compiled in |
| `sstest`, `ssdiff`, `ssgrep`, `ssindex` | none | Not ported |

## Functions

389 of Gnumeric's functions are ported, by family. Of the 63,183 function calls
in Gnumeric's sample workbooks, 95.4% resolve; the rest are the specialist families
below, led by Bessel functions, Erlang traffic and the `R.*` distributions.

| Family | Plugin | Ported | Not ported |
| --- | --- | --- | --- |
| Mathematics | `fn-math` | 94 | `AGM ARABIC CHOLESKY COTPI DIGAMMA EIGEN IGAMMA ILOG LAMBERTW LINSOLVE LOGMDETERM MPSEUDOINVERSE PERCENTOF REDUCEPI TANPI` |
| Statistics | `fn-stat` | 86 | `ADTEST BERNOULLI BETA.DIST BINOM.DIST.RANGE CAUCHY CONFIDENCE.T CVMTEST EXPPOWDIST GAUSS GEOMDIST LANDAU LAPLACE LEVERAGE LKSTEST LOGFIT LOGISTIC LOGREG OWENT PARETO PERCENTILE.EXC PERCENTRANK.EXC PERMUTATIONA PHI QUARTILE.EXC` and others |
| String | `fn-string` | 29 | `ASC ENCODEURL FINDB JIS LEFTB MIDB REGEXEXTRACT REGEXREPLACE REGEXTEST REPLACEB RIGHTB SEARCHB TEXTAFTER TEXTBEFORE TEXTSPLIT` |
| Logic | `fn-logical`, built-ins | 11 | |
| Lookup | `fn-lookup` | 18 | `COLUMNNUMBER FLIP SHEET SHEETS SORT UNIQUE XMATCH` |
| Date and time | `fn-date` | 23 | `DATE2UNIX ISOYEAR NETWORKDAYS.INTL ODF.TIME UNIX2DATE WORKDAY.INTL` |
| Financial | `fn-financial` | 25 | The bond family: `ACCRINT COUPDAYS DURATION PRICE YIELD` and 28 others |
| Information | `fn-info` | 17 | `CELL EXPRESSION GET.FORMULA GET.LINK GETENV INFO` |
| Database | `fn-database` | 12 | `GETPIVOTDATA` |
| Engineering | `fn-eng` | 21 | `BASE BESSELI BESSELJ BESSELK BESSELY CONVERT DECIMAL HEXREP` |
| Complex | `fn-complex` | 41 | `IMFACT IMGAMMA IMIGAMMA IMINV` |
| Number theory | `fn-numtheory` | 12 | `NT_RADICAL PARTITIONS` |

Not ported at all: `fn-derivatives` (29), `fn-r` (59), `fn-random` (31),
`fn-erlang` (4), `fn-tsa` (4), `fn-flt` (4), `fn-christian-date` (5),
`fn-hebrew-date` (9). A formula calling one keeps its text, shows `#NAME?` and
saves back unchanged.

`SUBTOTAL` does not yet skip hidden rows or nested subtotals.

## Other limits

- A caret that does not blink: a blink would repaint the back buffer twice a
  second, and on GEM that cost is real.
- Fonts come in whole pixel sizes.
