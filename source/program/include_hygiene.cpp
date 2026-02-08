// Compile-only TU to catch missing includes and brittle include ordering.

#include "d3/_util.hpp"
#include "d3/hooks/debug.hpp"
#include "d3/hooks/resolution.hpp"
#include "d3/hooks/util.hpp"
#include "d3/patches.hpp"
#include "d3/resolution_util.hpp"
#include "program/config.hpp"
#include "program/config_schema.hpp"
#include "program/exception_handler.hpp"
#include "program/fs_util.hpp"
#include "program/gui2/imgui_overlay.hpp"
#include "program/gui2/input_util.hpp"
#include "program/gui2/ui/overlay.hpp"
#include "program/gui2/ui/virtual_keyboard.hpp"
#include "program/log_once.hpp"
#include "program/logging.hpp"
#include "program/runtime_apply.hpp"
#include "program/tagnx.hpp"

namespace {
    [[maybe_unused]] void IncludeHygieneDummy() {}
}  // namespace
