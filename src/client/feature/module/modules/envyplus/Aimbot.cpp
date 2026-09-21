#include "pch.h"
#include "Aimbot.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/ClickEvent.h"
#include "client/event/events/FocusLostEvent.h"
#include "client/localization/LocalizeString.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/client/player/LocalPlayer.h"
#include "mc/common/world/level/Level.h"
#include "mc/common/world/actor/player/Player.h"
#include "util/Crypto.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace {
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

    listen<TickEvent>(static_cast<EventListenerFunc>(&Aimbot::onTick));
    listen<ClickEvent>(static_cast<EventListenerFunc>(&Aimbot::onClick));
    listen<FocusLostEvent>(static_cast<EventListenerFunc>(&Aimbot::onFocusLost));
}

void Aimbot::onEnable() {
    resetTargeting();
}

void Aimbot::onDisable() {
    resetTargeting();
}

void Aimbot::resetTargeting() {
    attackDown = false;
    reactionTimer = 0.f;
    targetRuntimeId = 0;
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
        "iron_axe"_fnv64,       "golden_axe"_fnv64,   "diamond_axe"_fnv64, "netherite_axe"_fnv64,
        "bow"_fnv64,            "crossbow"_fnv64,     "trident"_fnv64,     "mace"_fnv64,
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

        const AABB box = actor->getBoundingBox();
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

void Aimbot::onTick(Event& evGeneric) {
    auto& ev = reinterpret_cast<TickEvent&>(evGeneric);

    auto* ci = SDK::ClientInstance::get();
    if (!ci || !ci->minecraftGame || !ci->minecraftGame->isCursorGrabbed()) {
        resetTargeting();
        return;
    }

    auto* plr = ci->getLocalPlayer();
    if (!plr || !plr->actorRotation || !ev.getLevel()) {
        resetTargeting();
        return;
    }

    if (std::get<BoolValue>(weaponOnly) && !isHoldingWeapon(plr)) {
        resetTargeting();
        return;
    }

    if (std::get<BoolValue>(onlyOnAttack) && !attackDown) {
        resetTargeting();
        return;
    }

    Target target = findTarget(plr, ev.getLevel());
    if (!target.actor) {
        resetTargeting();
        return;
    }

    // Human reactions (Legit): freshly acquired targets get a small randomized
    // delay before the aim starts moving, so it doesn't snap the instant an
    // enemy peeks into the FOV.
    const uint64_t id = target.actor->getRuntimeID();
    if (id != targetRuntimeId) {
        targetRuntimeId = id;
        if (mode.getSelectedKey() == 0) {
            const float delay = std::get<FloatValue>(reactionDelay);
            reactionTimer = delay * randomFactor(0.8f, 1.2f);
        } else {
            reactionTimer = 0.f;
        }
    }

    if (reactionTimer > 0.f) {
        reactionTimer -= 50.f; // one client tick
        return;
    }

    Vec3 eye = plr->getPos();
    eye.y += 1.62f;

    const Vec3 to = target.aimPoint - eye;
    const float horizontal = std::sqrt(to.x * to.x + to.z * to.z);
    if (horizontal < 0.001f && EnvyMath::abs(to.y) < 0.001f) return;

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
        // Legit: exponential smoothing towards the target, X/Y axes tuned separately
        const float sx = std::clamp(std::get<FloatValue>(xSens), 1.f, 100.f) / 100.f;
        const float sy = std::clamp(std::get<FloatValue>(ySens), 1.f, 100.f) / 100.f;
        newYaw = rot.x + wrapDegrees(targetYaw - rot.x) * sx;
        newPitch = rot.y + (targetPitch - rot.y) * sy;
    }

    plr->getRot() = Vec2{wrapDegrees(newYaw), newPitch};
}
