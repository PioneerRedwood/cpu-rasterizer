#pragma once

#include <filesystem>

static bool FileExist(const std::filesystem::path& filepath) {
  std::error_code ec;
  return std::filesystem::exists(filepath, ec) &&
          std::filesystem::is_regular_file(filepath, ec);
}