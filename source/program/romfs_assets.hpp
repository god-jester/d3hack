#pragma once

#include <string>
#include <vector>

namespace d3::romfs {

// Reads a file from the game's layered RomFS via nn::fs.
// Expects paths like: "romfs:/d3gui/imgui.bin".
bool ReadFileToBytes(const char* path, std::vector<unsigned char>& out, size_t max_size = 4 * 1024 * 1024);

bool ReadFileToString(const char* path, std::string& out, size_t max_size = 4 * 1024 * 1024);

// Lists a directory and returns the first entry whose name ends with `suffix` (case-insensitive),
// returning the full path in `out_path` (dir + "/" + name). Returns false when not found.
// Example: FindFirstFileWithSuffix("romfs:/d3gui", ".ttf", out_path).
bool FindFirstFileWithSuffix(const char* dir_path, const char* suffix, std::string& out_path);

}  // namespace d3::romfs
