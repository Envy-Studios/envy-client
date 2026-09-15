#pragma once
#include "../../Module.h"
#include <unordered_map>

class TNTTimer : public Module {
public:
    TNTTimer();

    void onTick(Event& ev);
    void onRenderLayer(Event& ev);
    void onLeaveGame(Event& ev);

private:
    struct TrackedTNT {
        Vec3 pos;
        int ticksLeft = 80;
    };

    ValueType fontSize = FloatValue(22.f);
    ValueType showTicks = BoolValue(false);
    ValueType textColor = ColorValue(1.f, 0.35f, 0.35f, 1.f);

    std::unordered_map<uint64_t, TrackedTNT> tracked;
};
