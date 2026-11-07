//
// The format registry and the file operations around it.
//
// Derived from Gnumeric src/workbook-view.c,
// Copyright (C) 2001-2006 Jody Goldberg.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "impexp/file_format.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "gnumeric_format.h"
#include "util/gzip.h"
#include "util/text.h"

namespace impexp {
namespace {

// Gnumeric's own format.
class gnumeric_format final : public file_format {
public:
    std::string_view name() const override { return "Gnumeric workbook"; }
    std::string_view extension() const override { return "gnumeric"; }
    bool can_read() const override { return true; }
    bool can_write() const override { return true; }

    bool recognizes(std::string_view data) const override {
        if (util::is_gzip(data))
            return true;
        return data.substr(0, 400).find("gnm:Workbook") !=
                   std::string_view::npos ||
               data.substr(0, 400).find("<Workbook") != std::string_view::npos;
    }

    import_result read(std::string_view data,
                       book::workbook &target) const override {
        return read_gnumeric(data, target);
    }

    std::string write(book::workbook &source) const override {
        return write_gnumeric(source);
    }
};

// Comma-separated values.
class csv_format final : public file_format {
public:
    std::string_view name() const override { return "Comma-separated values"; }
    std::string_view extension() const override { return "csv"; }
    bool can_read() const override { return true; }
    bool can_write() const override { return true; }
    bool recognizes(std::string_view) const override { return false; }

    import_result read(std::string_view data,
                       book::workbook &target) const override {
        return read_csv(data, target);
    }

    std::string write(book::workbook &source) const override {
        return write_csv(source);
    }
};

// A web page.
class html_format final : public file_format {
public:
    std::string_view name() const override { return "Web page"; }
    std::string_view extension() const override { return "html"; }
    bool can_read() const override { return false; }
    bool can_write() const override { return true; }
    bool recognizes(std::string_view) const override { return false; }

    import_result read(std::string_view, book::workbook &) const override {
        import_result result;
        result.error = "HTML cannot be read";
        return result;
    }

    std::string write(book::workbook &source) const override {
        return write_html(source);
    }
};

} // namespace

const std::vector<std::unique_ptr<file_format>> &file_formats() {
    static const auto formats = [] {
        std::vector<std::unique_ptr<file_format>> list;
        list.push_back(std::make_unique<gnumeric_format>());
        list.push_back(std::make_unique<csv_format>());
        list.push_back(std::make_unique<html_format>());
        return list;
    }();
    return formats;
}

const file_format *format_for(const std::filesystem::path &path) {
    std::string extension = path.extension().string();
    if (!extension.empty())
        extension.erase(0, 1);
    for (const auto &format : file_formats())
        if (util::equal_nocase(format->extension(), extension))
            return format.get();
    return nullptr;
}

import_result load_file(const std::filesystem::path &path,
                        book::workbook &target) {
    import_result result;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.error = "The file cannot be opened";
        return result;
    }
    const std::string data((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    const file_format *format = nullptr;
    for (const auto &candidate : file_formats())
        if (candidate->recognizes(data))
            format = candidate.get();
    if (!format)
        format = format_for(path);
    if (!format || !format->can_read()) {
        // Unknown extensions read as text, which is what they mostly are.
        format = file_formats()[1].get();
    }
    return format->read(data, target);
}

bool save_file(const std::filesystem::path &path, book::workbook &source,
               std::string &error) {
    const file_format *format = format_for(path);
    if (!format || !format->can_write())
        format = file_formats()[0].get();
    std::string bytes;
    try {
        bytes = format->write(source);
    } catch (const std::exception &failure) {
        error = failure.what();
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            error = "The file cannot be written";
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code failure;
    std::filesystem::rename(temporary, path, failure);
    if (failure) {
        error = failure.message();
        std::filesystem::remove(temporary, failure);
        return false;
    }
    return true;
}

void new_workbook(book::workbook &target) {
    for (int i = 0; i < 3; ++i)
        target.add_sheet(target.unique_sheet_name());
}

} // namespace impexp
