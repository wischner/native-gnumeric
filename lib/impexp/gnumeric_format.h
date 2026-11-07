//
// The private tables shared by the .gnumeric reader and writer.
//
// Derived from Gnumeric src/xml-sax-write.c and src/style.h,
// Copyright (C) 2003-2007 Jody Goldberg, 2002-2021 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef IMPEXP_GNUMERIC_FORMAT_H
#define IMPEXP_GNUMERIC_FORMAT_H

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "impexp/file_format.h"

namespace impexp {

// The spelled alignments.
inline constexpr std::array<std::pair<std::string_view, book::h_align>, 8>
    gnumeric_h_aligns{{
        {"GNM_HALIGN_GENERAL", book::h_align::general},
        {"GNM_HALIGN_LEFT", book::h_align::left},
        {"GNM_HALIGN_RIGHT", book::h_align::right},
        {"GNM_HALIGN_CENTER", book::h_align::center},
        {"GNM_HALIGN_FILL", book::h_align::fill},
        {"GNM_HALIGN_JUSTIFY", book::h_align::justify},
        {"GNM_HALIGN_CENTER_ACROSS_SELECTION", book::h_align::center_across},
        {"GNM_HALIGN_DISTRIBUTED", book::h_align::distributed},
    }};

inline constexpr std::array<std::pair<std::string_view, book::v_align>, 5>
    gnumeric_v_aligns{{
        {"GNM_VALIGN_TOP", book::v_align::top},
        {"GNM_VALIGN_BOTTOM", book::v_align::bottom},
        {"GNM_VALIGN_CENTER", book::v_align::center},
        {"GNM_VALIGN_JUSTIFY", book::v_align::justify},
        {"GNM_VALIGN_DISTRIBUTED", book::v_align::distributed},
    }};

// The border child elements, in side order.
inline constexpr std::array<std::string_view, book::border_side_count>
    gnumeric_border_names{"Top",   "Bottom",       "Left",
                          "Right", "Rev-Diagonal", "Diagonal"};

// Read a .gnumeric file.
import_result read_gnumeric(std::string_view data, book::workbook &target);

// Write a .gnumeric file, compressed.
std::string write_gnumeric(book::workbook &source);

// Read comma-separated text into a new sheet.
import_result read_csv(std::string_view data, book::workbook &target);

// Write the first sheet as comma-separated text.
std::string write_csv(book::workbook &source);

// Write every sheet as an HTML document.
std::string write_html(book::workbook &source);

} // namespace impexp

#endif
