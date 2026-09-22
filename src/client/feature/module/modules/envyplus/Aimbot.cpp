#include "pch.h"
#include "Aimbot.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/ClickEvent.h"
#include "client/event/events/FocusLostEvent.h"
#include "client/event/events/TurnDeltaEvent.h"
#include "client/event/events/UpdateEvent.h"
#include "client/event/events/UpdatePlayerCameraEvent.h"
#include "client/localization/LocalizeString.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/client/player/LocalPlayer.h"
#include "mc/common/world/Minecraft.h"
#include "mc/common/world/level/Level.h"
#include "mc/common/world/actor/player/Player.h"
#include "util/Crypto.h"
#include "util/Logger.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <random>

namespace {
    using Clock = std::chrono::steady_clock;

    float wrapDegrees(float a) {
        a = std::fmod(a + 180.f, 360.f);
        if (a < 0.f) a += 360.f;
        return a - 180.f;
    }

    float randomFactor(float low, float high) {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(low, high);
        return dist(rng);
    }

    float millisSince(Clock::time_point tp) {
        return static_cast<float>(
            std::chrono::duration<double, std::milli>(Clock::now() - tp).count());
    }
}

Aimbot::Aimbot()
    : Module("Aimbot", LocalizeString::get("client.module.aimbot.name"),
             LocalizeString::get("client.module.aimbot.desc"), ENVYPLUS, nokeybind) {
    mode.addEntry(EnumEntry{0, LocalizeString::get("client.module.aimbot.mode.legit.name"),
                            LocalizeString::get("client.module.aimbot.mode.legit.desc")});
    mode.addEntry(EnumEntry{1, LocalizeString::get("client.module.aimbot.mode.blatant.name"),
                            LocalizeString::get("client.module.aimbot.mode.blatant.desc")});

    priority.addEntry(EnumEntry{0, LocalizeString::get("client.module.aimbot.priority.angle.name"),
                                LocalizeString::get("client.module.aimbot.priority.angle.desc")});
    priority.addEntry(EnumEntry{1, LocalizeString::get("client.module.aimbot.priority.distance.name"),
                                LocalizeString::get("client.module.aimbot.priority.distance.desc")});
    priority.addEntry(EnumEntry{2, LocalizeString::get("client.module.aimbot.priority.health.name"),
                                LocalizeString::get("client.module.aimbot.priority.health.desc")});

    Setting::Condition legitOnly("mode", Setting::Condition::EQUALS, {0});

    addEnumSetting("mode", LocalizeString::get("client.module.aimbot.mode.name"),
                   LocalizeString::get("client.module.aimbot.mode.desc"), this->mode);
    addSetting("weaponOnly", LocalizeString::get("client.module.aimbot.weaponOnly.name"),
               LocalizeString::get("client.module.aimbot.weaponOnly.desc"), this->weaponOnly);
    addSliderSetting("xSens", LocalizeString::get("client.module.aimbot.xSens.name"),
                     LocalizeString::get("client.module.aimbot.xSens.desc"), this->xSens, FloatValue(1.f),
                     FloatValue(100.f), FloatValue(1.f), legitOnly);
    addSliderSetting("ySens", LocalizeString::get("client.module.aimbot.ySens.name"),
                     LocalizeString::get("client.module.aimbot.ySens.desc"), this->ySens, FloatValue(1.f),
                     FloatValue(100.f), FloatValue(1.f), legitOnly);
    addSliderSetting("reactionDelay", LocalizeString::get("client.module.aimbot.reactionDelay.name"),
                     LocalizeString::get("client.module.aimbot.reactionDelay.desc"), this->reactionDelay,
                     FloatValue(0.f), FloatValue(1000.f), FloatValue(10.f), legitOnly);
    addSliderSetting("fov", LocalizeString::get("client.module.aimbot.fov.name"),
                     LocalizeString::get("client.module.aimbot.fov.desc"), this->fov, FloatValue(1.f),
                     FloatValue(180.f), FloatValue(1.f));
    addSliderSetting("range", LocalizeString::get("client.module.aimbot.range.name"),
                     LocalizeString::get("client.module.aimbot.range.desc"), this->range, FloatValue(3.f),
                     FloatValue(64.f), FloatValue(1.f));
    addEnumSetting("priority", LocalizeString::get("client.module.aimbot.priority.name"),
                   LocalizeString::get("client.module.aimbot.priority.desc"), this->priority);
    addSetting("onlyOnAttack", LocalizeString::get("client.module.aimbot.onlyOnAttack.name"),
               LocalizeString::get("client.module.aimbot.onlyOnAttack.desc"), this->onlyOnAttack);
    addSetting("playersOnly", LocalizeString::get("client.module.aimbot.playersOnly.name"),
               LocalizeString::get("client.module.aimbot.playersOnly.desc"), this->playersOnly);

    // Primary driver: inject aim as turn deltas through the game's own
    // look-controls (LocalPlayer::applyTurnDelta). A delta moves the real
    // camera and the player rotation together - the same path the mouse and
    // the Gyro module use. The delta is packed {x = pitch, y = yaw}.
    listen<TurnDeltaEvent>(static_cast<EventListenerFunc>(&Aimbot::onTurnDelta));
    // Keeps the lock alive whenever no look input is arriving (mouse held
    // still) by feeding the same look-controls a synthetic turn.
    listen<UpdateEvent>(static_cast<EventListenerFunc>(&Aimbot::onUpdate));
    // Fallback for game versions where the turn-delta hook does not fire.
    listen<UpdatePlayerCameraEvent>(static_cast<EventListenerFunc>(&Aimbot::onCameraUpdate));
    listen<TickEvent>(static_cast<EventListenerFunc>(&Aimbot::onTick));
    listen<ClickEvent>(static_cast<EventListenerFunc>(&Aimbot::onClick));
    listen<FocusLostEvent>(static_cast<EventListenerFunc>(&Aimbot::onFocusLost));
}

void Aimbot::onEnable() {
    fullReset();
}

void Aimbot::onDisable() {
    fullReset();
}

void Aimbot::fullReset() {
    attackDown = false;
    resetTargeting();
    lastState = "";
    syntheticInject = false;
    budgetUsed = {};
    budgetWindow = {};
}

void Aimbot::resetTargeting() {
    targetRuntimeId = 0;
    reacting = false;
    pitchPinnedLogged = false;
}

void Aimbot::reportState(const char* reason) {
    if (lastState == reason) return;
    lastState = reason;
    if (reason && *reason) {
        Logger::Info("[Aimbot] {}", reason);
    }
}

void Aimbot::onClick(Event& evGeneric) {
    auto& ev = reinterpret_cast<ClickEvent&>(evGeneric);
    if (ev.getClickType() == ClickEvent::ClickType::Left) {
        attackDown = ev.isDown() != 0;
    }
}

void Aimbot::onFocusLost(Event&) {
    attackDown = false;
}

bool Aimbot::isHoldingWeapon(SDK::Player* player) {
    auto* supplies = player->supplies;
    if (!supplies || !supplies->inventory) return false;

    auto* stack = supplies->inventory->getItem(supplies->selectedSlot);
    if (!stack) return false;
    auto* item = stack->getItem();
    if (!item) return false;

    static constexpr std::array<uint64_t, 16> weapons = {
        "wooden_sword"_fnv64,   "stone_sword"_fnv64,  "iron_sword"_fnv64,  "golden_sword"_fnv64,
        "diamond_sword"_fnv64,  "netherite_sword"_fnv64, "wooden_axe"_fnv64,  "stone_axe"_fnv64,
        "iron_axe"_fnv64,       "golden_axe"_fnv64,   "diamond_axe"_fnv64,  "netherite_axe"_fnv64,
        "bow"_fnv64,            "crossbow"_fnv64,     "trident"_fnv64,      "mace"_fnv64,
    };

    const uint64_t idHash = static_cast<uint64_t>(item->id.hash);
    return std::find(weapons.begin(), weapons.end(), idHash) != weapons.end();
}

Aimbot::Target Aimbot::findTarget(SDK::Player* self, SDK::Level* level) {
    Target best;

    const float fovDeg = std::get<FloatValue>(fov);
    const float maxRange = std::get<FloatValue>(range);
    const int prio = priority.getSelectedKey();
    const bool players = std::get<BoolValue>(playersOnly);

    Vec3 eye = self->getPos();
    eye.y += 1.62f; // standing eye height

    const Vec2 rot = self->getRot();
    const float yaw = EnvyMath::deg2rad(rot.x);
    const float pitch = EnvyMath::deg2rad(rot.y);
    const Vec3 forward{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};

    for (auto* actor : level->getRuntimeActorList()) {
        if (!actor || actor == self) continue;
        if (players && !actor->isPlayer()) continue;
        if (!actor->aabbShape) continue;

        auto health = actor->getHealth();
        if (!health || *health <= 0.f) continue;
        if (actor->isInvisible()) continue;

        AABB box = actor->getBoundingBox();
        const Vec3 center = box.getCenter();

        const float dist = eye.distance(center);
        if (dist > maxRange || dist < 0.1f) continue;

        const Vec3 to = center - eye;
        const float len = to.magnitude();
        if (len < 0.001f) continue;
        const Vec3 dir = to * (1.f / len);

        const float dot =
            std::clamp(forward.x * dir.x + forward.y * dir.y + forward.z * dir.z, -1.f, 1.f);
        const float angle = std::acos(dot) * (180.f / pi_f);
        if (angle > fovDeg) continue;

        float score = angle;
        if (prio == 1) score = dist;
        else if (prio == 2) score = health.value();

        if (!best.actor || score < best.score) {
            best = Target{actor, score, center, dist};
        }
    }

    return best;
}

bool Aimbot::turnDriverAlive() {
    if (!turnDriverSeen) return false;
    return millisSince(lastTurnDelta) < 250.f || millisSince(lastSyntheticTurn) < 250.f;
}

Vec2 Aimbot::budgetStep(Vec2 step, bool blatant) {
    if (millisSince(budgetWindow) > 15.f) {
        budgetWindow = Clock::now();
        budgetUsed = {};
    }

    // Rate budget per ~16 ms window, per axis (game order: x = pitch, y = yaw).
    // Without this, a burst of applyTurnDelta calls in one frame could apply
    // the correction several times over and drive the view into the clamp.
    float capX;
    float capY;
    if (blatant) {
        capX = capY = 100.f;
    } else {
        const float sens = std::clamp(float(std::get<FloatValue>(xSens)), 1.f, 100.f);
        capX = capY = 5.f + 0.4f * sens;
    }

    const float roomX = std::max(capX - std::abs(budgetUsed.x), 0.f);
    const float roomY = std::max(capY - std::abs(budgetUsed.y), 0.f);
    step.x = std::clamp(step.x, -roomX, roomX);
    step.y = std::clamp(step.y, -roomY, roomY);
    budgetUsed.x += step.x;
    budgetUsed.y += step.y;
    return step;
}

Vec2 Aimbot::aimCorrection(const Vec2& rot, const Vec2& aim, float dtMs, bool blatant) {
    const float yawErr = wrapDegrees(aim.x - rot.x);
    const float pitchErr = std::clamp(aim.y - rot.y, -89.9f, 89.9f);

    float gainPitch;
    float gainYaw;
    if (blatant) {
        // Half the remaining error per call: locks within a few frames yet
        // stays stable even if the game batches look input.
        gainPitch = gainYaw = 0.5f;
    } else {
        // Exponential smoothing, X/Y axes tuned separately. Slider semantics
        // are "per client tick"; converted to real elapsed time so smoothing
        // feels the same at any FPS or delta rate.
        const float sp = std::clamp(float(std::get<FloatValue>(ySens)), 1.f, 100.f) / 100.f;
        const float sw = std::clamp(float(std::get<FloatValue>(xSens)), 1.f, 100.f) / 100.f;
        const float steps = std::max(dtMs, 1.f) / 50.f;
        gainPitch = std::min(1.f - std::pow(1.f - sp, steps), 0.35f);
        gainYaw = std::min(1.f - std::pow(1.f - sw, steps), 0.35f);
    }

    return budgetStep(Vec2{pitchErr * gainPitch, yawErr * gainYaw}, blatant);
}

void Aimbot::onTurnDelta(Event& evGeneric) {
    auto& ev = reinterpret_cast<TurnDeltaEvent&>(evGeneric);

    // Synthetic injections (onUpdate) already carry the exact step we want;
    // let them pass through untouched.
    if (syntheticInject) {
        syntheticInject = false;
        return;
    }

    if (!turnDriverSeen) {
        turnDriverSeen = true;
        Logger::Info("[Aimbot] turn delta driver active");
    }

    const float dtMs = std::clamp(millisSince(lastTurnDelta), 1.f, 100.f);
    lastTurnDelta = Clock::now();

    const auto aim = aimStep(dtMs);
    if (!aim) return;

    auto* plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    // Aim by steering the game's own look-controls. The delta packing here is
    // {x = pitch (vertical), y = yaw (horizontal)} - the same packing the Gyro
    // module and the camera stick use - while the actor rotation we aim with
    // is {x = yaw, y = pitch}, so the axes cross at this boundary. Feeding
    // yaw into the pitch axis drives the view straight into the pitch clamp
    // (staring at the ground).
    const Vec2 rot = plr->getRot();
    const bool blatant = mode.getSelectedKey() == 1;
    const Vec2 step = aimCorrection(rot, *aim, dtMs, blatant);

    if (blatant) {
        // Hard lock: while a target is held the crosshair belongs to the
        // aimbot, so replace this call's look input entirely.
        ev.setDelta(step);
    } else {
        // Legit: keep the player's own look input and glide towards the
        // target on top of it.
        const Vec2 d = ev.getDelta();
        ev.setDelta(Vec2{d.x + step.x, d.y + step.y});
    }
}

void Aimbot::onUpdate(Event&) {
    // Only after a real TurnDeltaEvent proved the applyTurnDelta hook works:
    // the synthetic call re-enters that hook, and calling through a dead
    // signature would dereference a null pointer.
    if (!turnDriverSeen) return;
    // Natural look input is flowing - onTurnDelta already steers it.
    if (millisSince(lastTurnDelta) < 50.f) return;

    // The mouse is resting, so the game is not calling applyTurnDelta on its
    // own. Feed the same look-controls a synthetic turn so Blatant keeps
    // pinning the crosshair and Legit keeps gliding while the mouse is still.
    // The direct call passes back through our TurnDeltaEvent handler (the
    // hook is a detour on the same function), where syntheticInject lets it
    // through unmodified.
    syntheticInject = false;

    const auto aim = aimStep(16.f);
    if (!aim) return;

    auto* plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    const Vec2 rot = plr->getRot();
    const bool blatant = mode.getSelectedKey() == 1;
    const Vec2 step = aimCorrection(rot, *aim, 16.f, blatant);

    if (!pitchPinnedLogged && targetRuntimeId != 0 && std::abs(rot.y) > 89.5f) {
        pitchPinnedLogged = true;
        Logger::Warn("[Aimbot] view is pinned at the pitch clamp while a target is locked");
    }

    syntheticInject = true;
    lastSyntheticTurn = Clock::now();
    plr->applyTurnDelta(step);
}

void Aimbot::onCameraUpdate(Event& evGeneric) {
    auto& ev = reinterpret_cast<UpdatePlayerCameraEvent&>(evGeneric);

    // The turn-delta driver (natural + synthetic) owns aiming once its hook
    // has proven alive.
    if (turnDriverSeen) return;

    if (!cameraDriverSeen) {
        cameraDriverSeen = true;
        Logger::Warn("[Aimbot] turn delta hook silent, using camera override fallback (view may not move)");
    }

    const float dtMs = std::clamp(millisSince(lastCameraEvent), 1.f, 100.f);
    lastCameraEvent = Clock::now();

    const auto aim = aimStep(dtMs);
    if (!aim) return;

    ev.setViewAngles(*aim);

    // Keep the actor rotation component in sync with the camera override so
    // the server and the target math see the same rotation the player sees.
    auto* plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (plr && plr->actorRotation) {
        plr->actorRotation->rotationOld = plr->actorRotation->rotation;
        plr->getRot() = *aim;
    }
}

void Aimbot::onTick(Event&) {
    // Last-resort fallback: no turn delta (natural or synthetic) and no
    // camera events arrived.
    const bool turnFresh = turnDriverAlive();
    const bool cameraFresh = cameraDriverSeen && millisSince(lastCameraEvent) < 250.f;
    if (turnFresh || cameraFresh) return;

    if (!fallbackLogged) {
        fallbackLogged = true;
        Logger::Warn("[Aimbot] no turn delta or camera driver, using per-tick fallback (view may not move)");
    }

    if (const auto aim = aimStep(50.f)) {
        auto* plr = SDK::ClientInstance::get()->getLocalPlayer();
        if (plr && plr->actorRotation) {
            plr->actorRotation->rotationOld = plr->actorRotation->rotation;
            plr->getRot() = *aim;
        }
    }
}

std::optional<Vec2> Aimbot::aimStep(float dtMs) {
    auto* ci = SDK::ClientInstance::get();
    if (!ci || !ci->minecraftGame || !ci->minecraftGame->isCursorGrabbed()) {
        resetTargeting();
        reportState("waiting: not in gameplay (cursor not grabbed)");
        return std::nullopt;
    }

    auto* plr = ci->getLocalPlayer();
    if (!plr || !plr->actorRotation) {
        resetTargeting();
        reportState("waiting: no local player");
        return std::nullopt;
    }

    SDK::Level* level = ci->minecraft ? ci->minecraft->getLevel() : nullptr;
    if (!level) {
        resetTargeting();
        reportState("waiting: no level loaded");
        return std::nullopt;
    }

    if (std::get<BoolValue>(weaponOnly) && !isHoldingWeapon(plr)) {
        resetTargeting();
        reportState("waiting for a weapon (weapon only is on)");
        return std::nullopt;
    }

    if (std::get<BoolValue>(onlyOnAttack) && !attackDown) {
        resetTargeting();
        reportState("waiting for attack (only while attacking is on)");
        return std::nullopt;
    }

    const Target target = findTarget(plr, level);
    if (!target.actor) {
        resetTargeting();
        reportState("searching: no target inside range/FOV");
        return std::nullopt;
    }

    reportState("");

    // Human reactions (Legit): freshly acquired targets get a small randomized
    // delay before the aim starts moving, so it doesn't snap the instant an
    // enemy peeks into the FOV.
    const uint64_t id = target.actor->getRuntimeID();
    if (id != targetRuntimeId) {
        targetRuntimeId = id;
        Logger::Info("[Aimbot] target acquired at {:.1f} m", target.distance);
        if (mode.getSelectedKey() == 0) {
            const float delay = std::get<FloatValue>(reactionDelay);
            reactionDeadline = Clock::now() + std::chrono::milliseconds(
                static_cast<long long>(delay * randomFactor(0.8f, 1.2f)));
            reacting = true;
        } else {
            reacting = false;
        }
    }

    if (reacting) {
        if (Clock::now() < reactionDeadline) return std::nullopt;
        reacting = false;
    }

    Vec3 eye = plr->getPos();
    eye.y += 1.62f;

    const Vec3 to = target.aimPoint - eye;
    const float horizontal = std::sqrt(to.x * to.x + to.z * to.z);
    if (horizontal < 0.001f && EnvyMath::abs(to.y) < 0.001f) return std::nullopt;

    // Bedrock: rotation.x = yaw (0 = +Z, clockwise), rotation.y = pitch (positive = down)
    const float targetYaw = std::atan2(-to.x, to.z) * (180.f / pi_f);
    const float targetPitch =
        std::clamp(std::atan2(-to.y, horizontal) * (180.f / pi_f), -89.9f, 89.9f);

    const Vec2 rot = plr->getRot();
    float newYaw;
    float newPitch;

    if (mode.getSelectedKey() == 1) {
        // Blatant: hard lock onto the target
        newYaw = targetYaw;
        newPitch = targetPitch;
    } else {
        // Legit: exponential smoothing towards the target, X/Y axes tuned
        // separately. Slider semantics are "per client tick"; convert to the
        // real frame time so smoothing feels the same at any FPS.
        const float sx = std::clamp(float(std::get<FloatValue>(xSens)), 1.f, 100.f) / 100.f;
        const float sy = std::clamp(float(std::get<FloatValue>(ySens)), 1.f, 100.f) / 100.f;
        const float steps = std::max(dtMs, 1.f) / 50.f;
        const float fx = 1.f - std::pow(1.f - sx, steps);
        const float fy = 1.f - std::pow(1.f - sy, steps);
        newYaw = rot.x + wrapDegrees(targetYaw - rot.x) * fx;
        newPitch = rot.y + (targetPitch - rot.y) * fy;
    }

    return Vec2{wrapDegrees(newYaw), newPitch};
}
