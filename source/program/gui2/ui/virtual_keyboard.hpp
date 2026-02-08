#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace d3::gui2::ui::virtual_keyboard {

    struct Options {
        size_t           max_len       = 64;
        std::string_view allowed_chars = {};  // empty = allow any ASCII printable
        bool             allow_space   = false;
    };

    // Opens the keyboard popup to edit `*target`. Caller must call Render() every
    // frame while the overlay is active.
    void Open(const char *title, std::string *target, Options options = {});

    // Renders the keyboard popup if open. Returns true if the user accepted the
    // edit and `*target` was updated.
    auto Render() -> bool;

    auto IsOpen() -> bool;

}  // namespace d3::gui2::ui::virtual_keyboard
