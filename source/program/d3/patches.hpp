#pragma once

// Utility hook toggles (override with -D...).
#ifndef D3HACK_ENABLE_UTILITY_DEBUG_HOOKS
#define D3HACK_ENABLE_UTILITY_DEBUG_HOOKS 1
#endif

#ifndef D3HACK_ENABLE_UTILITY_SCRATCH
#define D3HACK_ENABLE_UTILITY_SCRATCH 0
#endif

struct GameParams;

namespace d3 {

    void PatchBuildlocker();
    void PatchVarResLabel();
    void PatchGraphicsPersistentHeapEarly();
    void PatchReleaseFPSLabel();
    void PatchDDMLabels();
    void PatchResolutionTargets();
    void PatchDynamicSeasonal();
    void UpdateDynamicSeasonalForSpawn(const GameParams *tParams);
    void PatchDynamicEvents();
    void PatchInfiniteMp(bool enabled);
    void PatchBase();

}  // namespace d3
