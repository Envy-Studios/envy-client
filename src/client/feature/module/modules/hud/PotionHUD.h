#pragma once
#include "../../HUDModule.h"
#include <unordered_map>

class PotionHUD : public HUDModule {
public:
    PotionHUD();

    void render(DrawUtil& ctx, bool isDefault, bool inEditor) override;

    void onTick(Event& ev);
    void onPacketReceive(Event& ev);
    void onLeaveGame(Event& ev);

private:
    struct TrackedEffect {
        int amplifier = 0;
        int ticksLeft = 0; // < 0 = no expiry tracked
    };

    static std::wstring effectName(int id);
    static d2d::Color effectColor(int id);
    static std::wstring amplifierSuffix(int amplifier);
    static std::wstring formatDuration(int ticksLeft);

    // Returns { effectId, event } resolved from the packet's two ambiguous slots.
    static bool resolveEffectFields(int slotA, int slotB, int durationTicks, int& effectId, int& event);

    ValueType hideWhenNone = BoolValue(true);
    ValueType showAmplifier = BoolValue(true);
    ValueType showDuration = BoolValue(true);

    std::unordered_map<int, TrackedEffect> effects;
};
