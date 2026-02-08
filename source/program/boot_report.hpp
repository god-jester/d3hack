#pragma once

namespace d3::boot_report {

    void RecordHook(const char *name);
    void RecordPatch(const char *name);

    // Prints a one-time boot report (best-effort).
    void PrintOnce();

}  // namespace d3::boot_report
