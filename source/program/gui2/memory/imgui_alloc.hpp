#pragma once

namespace d3::gui2::memory::imgui_alloc {
    // Configure ImGui allocator functions.
    //
    // Prefers the game's system allocator (when available). Falls back to resolving
    // libc malloc/free via nn::ro and using those.
    //
    // Returns true once ImGui allocator functions are configured.
    auto TryConfigureImGuiAllocators() -> bool;
}  // namespace d3::gui2::memory::imgui_alloc
