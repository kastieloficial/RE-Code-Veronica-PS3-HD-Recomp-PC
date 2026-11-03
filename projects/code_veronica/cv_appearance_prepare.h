#pragma once
#include <filesystem>
#include <string>
#include "cv_appearance_assets.h"
namespace cv_appearance {
// Shared bounded writer for locally generated archives.
std::vector<uint8_t> encode_archive_resources(const std::vector<Resource>& records);
std::filesystem::path appearance_system_directory(const std::filesystem::path& game_root);
// Read-only source game files; generated archives live only below cache_root.
std::filesystem::path appearance_cache_root(const std::filesystem::path& system);
bool prepare_appearance_asset(const std::filesystem::path& system,
    const std::filesystem::path& cache_root, unsigned choice,
    std::filesystem::path& archive, std::string& error);
}
