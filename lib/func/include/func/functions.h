//
// The public entry of the function library.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FUNC_FUNCTIONS_H
#define FUNC_FUNCTIONS_H

#include "expr/function.h"

namespace func {

// Return the table of every built-in function, built once.
const expr::function_table &builtin_functions();

} // namespace func

#endif
