//
// The command line and the batch work that needs no window.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef APP_OPTIONS_H
#define APP_OPTIONS_H

#include <filesystem>
#include <optional>
#include <string>

namespace app {

// What the command line asked for.
struct options {
    std::optional<std::filesystem::path> file;
    std::optional<std::filesystem::path> convert_to;
    bool show_help = false;
    bool show_version = false;

    // Report per-sheet statistics of the file on standard output.
    bool report = false;

    // Print one cell of the file after calculation, Sheet!A1 or A1.
    std::optional<std::string> print_cell;

    // What was wrong with the command line.
    std::string error;
};

// Read the command line.
options parse_options(int argc, char **argv);

// Return the usage text.
std::string usage_text();

// Do the work of a batch run and return an exit status.
int run_batch(const options &settings);

} // namespace app

#endif
