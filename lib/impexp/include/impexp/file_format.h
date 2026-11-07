//
// File formats behind one interface, so the window and the batch tool
// know nothing of any particular format. Formats register by extension;
// reading tells every loss through warnings rather than dropping data
// silently.
//
// Derived from Gnumeric src/workbook-view.c and goffice's file openers,
// Copyright (C) 2001-2006 Jody Goldberg.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef IMPEXP_FILE_FORMAT_H
#define IMPEXP_FILE_FORMAT_H

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "book/workbook.h"

namespace impexp {

// What reading a file did.
struct import_result {
    bool ok = false;

    // Why it failed, when it did.
    std::string error;

    // What could not be kept.
    std::vector<std::string> warnings;
};

// One file format.
class file_format {
public:
    virtual ~file_format() = default;

    // Return the name in file dialogs.
    virtual std::string_view name() const = 0;

    // Return the main extension, without a dot.
    virtual std::string_view extension() const = 0;

    // Report whether the format reads files.
    virtual bool can_read() const = 0;

    // Report whether the format writes files.
    virtual bool can_write() const = 0;

    // Report whether bytes look like this format.
    virtual bool recognizes(std::string_view data) const = 0;

    // Fill an empty workbook from bytes.
    virtual import_result read(std::string_view data,
                               book::workbook &target) const = 0;

    // Serialize a workbook.
    virtual std::string write(book::workbook &source) const = 0;
};

// Return every format, native first.
const std::vector<std::unique_ptr<file_format>> &file_formats();

// Return the format for a file name, or null.
const file_format *format_for(const std::filesystem::path &path);

// Read a file into an empty workbook, choosing the format by content
// and extension.
import_result load_file(const std::filesystem::path &path,
                        book::workbook &target);

//
// Write a file.
//
// Notes:
//      Writes a sibling temporary and renames it, so a failed save
//      cannot destroy the previous document.
//
// Returns:
//      Whether the file was written; error says why not.
//
bool save_file(const std::filesystem::path &path, book::workbook &source,
               std::string &error);

// Fill a workbook with a new document's sheets.
void new_workbook(book::workbook &target);

} // namespace impexp

#endif
