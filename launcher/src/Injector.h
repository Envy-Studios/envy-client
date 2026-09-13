#pragma once

#include <functional>
#include <string>

struct LaunchOutcome {
    bool ok = false;
    std::wstring error;
};

// writes the baked-in dll to disk, starts Minecraft if it is not running yet
// and injects into it. reports progress through the status callback.
LaunchOutcome ExtractAndInject(std::function<void(std::wstring const&)> const& status);
