# Licensing

Isaac is licensed under the GNU General Public License, version 2 or (at your
option) any later version. `LICENSE` is the GPL v2 text. Source headers read
`GPL 2.0 or later (see: LICENSE)`.

That header is the one deliberate difference from
`docs/standards/CPP-CODING-STYLE.md`, whose sample says MIT and which is kept as
copied.

Native is MIT, which is compatible, and is linked statically. "Gnumeric" is the
GNOME project's name, so the product is called Isaac and only the file format
keeps the Gnumeric name.

## Files derived from Gnumeric

Files derived from a specific Gnumeric file name it and keep its copyright line.
The GPL's notice requirement needs that, so keep those lines when moving code.
Files written from scratch carry only the project line.

## Open question: upstream's terms are not uniformly "or later"

Gnumeric's `COPYING` offers "GPL version 2" or "GPL version 3", and its AppData
states `GPL-2.0 or GPL-3.0`. Its source files are mixed:

| Upstream wording | Example files |
| --- | --- |
| "version 2 ... or (at your option) version 3" | `src/expr.c`, `src/sheet.c`, `src/dependent.c`, `src/commands.c` |
| "version 2 ... or (at your option) any later version" | `plugins/fn-math/functions.c` and most others |
| "version 3 ... or any later version" | The `dpois` block of `src/mathfunc.c`, imported from R |
| No notice in the file | `src/value.c`, `src/cell.c`, `src/func.c`, `src/number-match.c` |

The project's chosen licence is GPL-2.0-or-later. Code derived from a
"version 2 or version 3" file cannot gain "or later" unless its copyright
holders agree, because upstream did not grant it. Those files can be used here
under GPL 2 alone, so a port of them is not free to adopt GPL 4. This needs the
maintainer's decision, not a code change.

The one block that is GPL 3 or later must not be ported: it is incompatible with
GPL 2. `lib/func` computes the Poisson density directly from its definition
instead. See [PORT-STATUS.md](PORT-STATUS.md).
