//
// Prefers shared toolkit command icons, then the embedded resource pack.
// Backend selection and all visual feedback belong to Native.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "tool_icons.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace native_embedded {
// Defined by Native rc in the generated resource translation unit.
std::span<const std::uint8_t> resource_data();
}

namespace ui {

const native::resources &graphics_resources() {
    static const auto pack =
        native::resources::decode(native_embedded::resource_data());
    return pack;
}

std::shared_ptr<const native::img> tool_icon(std::string_view command) {
    using native::standard_icon_id;
    static constexpr std::pair<std::string_view, standard_icon_id> standard[] = {
        {"fileNew", standard_icon_id::file_new},
        {"fileOpen", standard_icon_id::file_open},
        {"fileSave", standard_icon_id::file_save},
        {"fileSaveAs", standard_icon_id::file_save_as},
        {"fileClose", standard_icon_id::file_close},
        {"cut", standard_icon_id::cut}, {"copy", standard_icon_id::copy},
        {"paste", standard_icon_id::paste}, {"undo", standard_icon_id::undo},
        {"redo", standard_icon_id::redo}, {"filePrint", standard_icon_id::print},
        {"find", standard_icon_id::find},
    };
    for (const auto &[id, icon] : standard)
        if (command == id)
            if (auto image = native::standard_icon(icon))
                return image;
    const auto &pack = graphics_resources();
    const native::resource_variant *entry = nullptr;
    try {
        entry = &pack.get("icons.toolbar." + std::string(command));
    } catch (const std::out_of_range &) {
        return nullptr;
    }
    if (entry->type != "icon")
        throw std::runtime_error("Toolbar resource is not an icon.");
    // img is immovable; direct initialization uses guaranteed copy elision.
    return std::shared_ptr<const native::img>(new native::img(
        native::img::decode(entry->data.data(), entry->data.size())));
}

} // namespace ui
