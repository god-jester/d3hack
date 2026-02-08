#pragma once

namespace d3 {

    void PatchBuildlocker();
    void PatchVarResLabel();
    void PatchGraphicsPersistentHeapEarly();
    void PatchReleaseFPSLabel();
    void PatchDDMLabels();
    void PatchResolutionTargets();
    void PatchDynamicSeasonal();
    void PatchDynamicEvents();
    void PatchBase();

}  // namespace d3
