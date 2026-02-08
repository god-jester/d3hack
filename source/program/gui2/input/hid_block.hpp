#pragma once

namespace d3::gui2::input::hid_block {
    // Installs nn::hid hooks used to disconnect-style block input while the GUI is focused.
    void InstallHidHooks();

    void SetBlockGamepadInputToGame(bool block);
    void SetAllowLeftStickPassthrough(bool allow);

    // RAII: while held, the calling thread is allowed to query hid state even if input is blocked.
    // Used by the overlay when sampling gamepad state for swipe/toggle detection.
    struct ScopedHidPassthroughForOverlay {
        ScopedHidPassthroughForOverlay();
        ~ScopedHidPassthroughForOverlay();

        ScopedHidPassthroughForOverlay(const ScopedHidPassthroughForOverlay &)            = delete;
        ScopedHidPassthroughForOverlay &operator=(const ScopedHidPassthroughForOverlay &) = delete;
    };
}  // namespace d3::gui2::input::hid_block
