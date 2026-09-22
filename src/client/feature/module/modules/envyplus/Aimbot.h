#pragma once

#include "../../Module.h"
#include "util/LMath.h"

#include <chrono>
#include <optional>

class Aimbot : public Module {
public:
    Aimbot();

    void onTurnDelta(Event& event);
    void onUpdate(Event& event);
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
    void reportState(const char* reason);

    // The turn-delta driver counts as alive while natural look input or a
    // recent synthetic injection is flowing through LocalPlayer::applyTurnDelta.
    bool turnDriverAlive();
    // Correction step for the game's look-controls, already packed the way
    // the game wants it ({x = pitch, y = yaw}), gain-limited and rate-budgeted
    // so the feedback loop can never run away into the pitch clamp.
    Vec2 aimCorrection(const Vec2& rot, const Vec2& aim, float dtMs, bool blatant);
    Vec2 budgetStep(Vec2 stepGameOrder, bool blatant);

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

    bool turnDriverSeen = false;
    bool cameraDriverSeen = false;
    bool fallbackLogged = false;
    bool syntheticInject = false;
    std::chrono::steady_clock::time_point lastTurnDelta {};
    std::chrono::steady_clock::time_point lastSyntheticTurn {};
    std::chrono::steady_clock::time_point lastCameraEvent {};
    std::chrono::steady_clock::time_point budgetWindow {};
    Vec2 budgetUsed {};         // game order: x = pitch, y = yaw
    bool pitchPinnedLogged = false;
    const char* lastState = "";
};
