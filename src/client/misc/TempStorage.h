#pragma once

#include <filesystem>

namespace EnvyTemp {
    std::filesystem::path resolvePath(std::filesystem::path const& relative);
    void cleanup();
}
