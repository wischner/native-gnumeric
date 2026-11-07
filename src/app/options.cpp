//
// Command line and batch conversion, the role of Gnumeric's ssconvert.
//
// Derived from Gnumeric src/ssconvert.c,
// Copyright (C) 2003 Jody Goldberg, 2003-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "app/options.h"

#include <iostream>
#include <map>
#include <set>
#include <string_view>

#include "book/region.h"
#include "book/workbook.h"
#include "impexp/file_format.h"

namespace app {
namespace {

// Report what a workbook holds and what failed to calculate.
void report(book::workbook &source) {
    for (int i = 0; i < source.sheet_count(); ++i) {
        book::sheet &item = source.sheet_at(i);
        long cells = 0;
        long formulas = 0;
        std::map<std::string, long> errors;
        item.for_each_stored([&](book::cell &stored) {
            ++cells;
            if (stored.has_formula())
                ++formulas;
            const val::value &result = item.value_at(stored.pos());
            if (stored.has_formula() && result.is_error())
                ++errors[result.to_text()];
        });
        std::cout << item.name() << ": " << cells << " cells, " << formulas
                  << " formulas";
        for (const auto &[name, count] : errors)
            std::cout << ", " << count << ' ' << name;
        std::cout << '\n';
    }
}

// Print one cell addressed as Sheet!A1 or A1.
int print_cell(book::workbook &source, const std::string &address) {
    const auto bang = address.rfind('!');
    book::sheet *where = &source.sheet_at(0);
    std::string name = address;
    if (bang != std::string::npos) {
        std::string sheet_name = address.substr(0, bang);
        if (sheet_name.size() > 1 && sheet_name.front() == '\'' &&
            sheet_name.back() == '\'')
            sheet_name = sheet_name.substr(1, sheet_name.size() - 2);
        where = static_cast<book::sheet *>(source.find_sheet(sheet_name));
        name = address.substr(bang + 1);
    }
    const auto pos = val::parse_cell_name(name);
    if (!where || !pos) {
        std::cerr << "isaac: no such cell: " << address << '\n';
        return 2;
    }
    std::cout << book::display_text(*where, *pos) << '\n';
    return 0;
}

} // namespace

options parse_options(int argc, char **argv) {
    options settings;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            settings.show_help = true;
        } else if (argument == "--version") {
            settings.show_version = true;
        } else if (argument == "--report") {
            settings.report = true;
        } else if (argument == "--to" || argument == "--cell") {
            if (i + 1 >= argc) {
                settings.error = std::string(argument) + " needs a value";
                break;
            }
            if (argument == "--to")
                settings.convert_to = argv[++i];
            else
                settings.print_cell = argv[++i];
        } else if (!argument.empty() && argument[0] == '-') {
            settings.error = "unknown option " + std::string(argument);
            break;
        } else if (settings.file) {
            settings.error = "only one file can be opened";
            break;
        } else {
            settings.file = std::filesystem::path(argument);
        }
    }
    return settings;
}

std::string usage_text() {
    return "usage: isaac [file]\n"
           "       isaac file --to output     convert by extension\n"
           "       isaac file --report        list contents and errors\n"
           "       isaac file --cell Sheet!A1 print one calculated cell\n"
           "       isaac --help | --version\n";
}

int run_batch(const options &settings) {
    if (!settings.file) {
        std::cerr << "isaac: a file is needed\n";
        return 2;
    }
    book::workbook source;
    const impexp::import_result result =
        impexp::load_file(*settings.file, source);
    if (!result.ok) {
        std::cerr << "isaac: " << settings.file->string() << ": "
                  << result.error << '\n';
        return 1;
    }
    for (const std::string &warning : result.warnings)
        std::cerr << "isaac: warning: " << warning << '\n';
    if (settings.report)
        report(source);
    if (settings.print_cell) {
        const int status = print_cell(source, *settings.print_cell);
        if (status != 0)
            return status;
    }
    if (settings.convert_to) {
        std::string error;
        if (!impexp::save_file(*settings.convert_to, source, error)) {
            std::cerr << "isaac: " << error << '\n';
            return 1;
        }
    }
    return 0;
}

} // namespace app
