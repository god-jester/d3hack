#pragma once

#include <string>
#include <vector>

namespace d3::romfs {

// Reads a file from the game's layered RomFS via nn::fs.
// Expects paths like: "romfs:/d3gui/imgui.bin".
bool ReadFileToBytes(const char* path, std::vector<unsigned char>& out, size_t max_size = 4 * 1024 * 1024);

bool ReadFileToString(const char* path, std::string& out, size_t max_size = 4 * 1024 * 1024);

}  // namespace d3::romfs
