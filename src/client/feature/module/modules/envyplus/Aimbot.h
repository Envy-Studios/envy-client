#pragma once

#include "../../Module.h"
#include "util/LMath.h"

#include <chrono>
#include <optional>

class Aimbot : public Module {
public:
    Aimbot();

    void onCameraUpdate(Event& event);
    void onTick(Event& event);
    void onClick(Event& event);
    void onFocusLost(Event& event);

    void onEnable() override;
    void onDisable() override;

private:
    struct Target {
        SDK::Actor* actor = nullptr;
        float score = 0.f;      // smaller is better (angle deg / distance / health)
        Vec3 aimPoint {};
        float distance = 0.f;
    };

    bool isHoldingWeapon(SDK::Player* player);
    Target findTarget(SDK::Player* player, SDK::Level* level);
    void resetTargeting();
    void fullReset();
    std::optional<Vec2> aimStep(float dtMs);
    void reportState(const char* reason, bool toast);

    EnumData mode;              // 0 = Legit, 1 = Blatant
    EnumData priority;          // 0 = closest to crosshair, 1 = closest distance, 2 = lowest health
    ValueType weaponOnly = BoolValue(true);
    ValueType xSens = FloatValue(45.f);
    ValueType ySens = FloatValue(45.f);
    ValueType fov = FloatValue(60.f);
    ValueType range = FloatValue(24.f);
    ValueType reactionDelay = FloatValue(200.f);
    ValueType onlyOnAttack = BoolValue(false);
    ValueType playersOnly = BoolValue(true);

    bool attackDown = false;
    uint64_t targetRuntimeId = 0;
    bool reacting = false;
    std::chrono::steady_clock::time_point reactionDeadline {};

    bool cameraDriverSeen = false;
    bool fallbackLogged = false;
    std::chrono::steady_clock::time_point lastCameraEvent {};
    const char* lastState = "";
};
