#include "pch.h"
#include "PotionHUD.h"

#include "client/event/Eventing.h"
#include "client/event/events/PacketReceiveEvent.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/LeaveGameEvent.h"
#include "mc/common/network/packet/MobEffectPacket.h"

namespace {
    struct EffectInfo {
        const wchar_t* name;
        d2d::Color color;
    };

    // Bedrock effect ids. Colors roughly match the vanilla effect bubbles.
    const std::unordered_map<int, EffectInfo> effectTable = {
        { 1,  { L"Speed",            d2d::Color::RGB(51, 153, 255) } },
        { 2,  { L"Slowness",         d2d::Color::RGB(93, 113, 155) } },
        { 3,  { L"Haste",            d2d::Color::RGB(217, 169, 87) } },
        { 4,  { L"Mining Fatigue",   d2d::Color::RGB(74, 66, 58) } },
        { 5,  { L"Strength",         d2d::Color::RGB(145, 61, 46) } },
        { 6,  { L"Instant Health",   d2d::Color::RGB(181, 255, 165) } },
        { 7,  { L"Instant Damage",   d2d::Color::RGB(67, 10, 9) } },
        { 8,  { L"Jump Boost",       d2d::Color::RGB(120, 255, 63) } },
        { 9,  { L"Nausea",           d2d::Color::RGB(85, 29, 74) } },
        { 10, { L"Regeneration",     d2d::Color::RGB(205, 92, 171) } },
        { 11, { L"Resistance",       d2d::Color::RGB(153, 69, 181) } },
        { 12, { L"Fire Resistance",  d2d::Color::RGB(228, 154, 57) } },
        { 13, { L"Water Breathing",  d2d::Color::RGB(46, 84, 255) } },
        { 14, { L"Invisibility",     d2d::Color::RGB(127, 131, 134) } },
        { 15, { L"Blindness",        d2d::Color::RGB(31, 31, 35) } },
        { 16, { L"Night Vision",     d2d::Color::RGB(31, 31, 163) } },
        { 17, { L"Hunger",           d2d::Color::RGB(88, 66, 18) } },
        { 18, { L"Weakness",         d2d::Color::RGB(72, 77, 72) } },
        { 19, { L"Poison",           d2d::Color::RGB(78, 147, 49) } },
        { 20, { L"Wither",           d2d::Color::RGB(53, 42, 21) } },
        { 21, { L"Health Boost",     d2d::Color::RGB(220, 32, 32) } },
        { 22, { L"Absorption",       d2d::Color::RGB(37, 82, 166) } },
        { 23, { L"Saturation",       d2d::Color::RGB(246, 219, 82) } },
        { 24, { L"Levitation",       d2d::Color::RGB(206, 255, 255) } },
        { 25, { L"Fatal Poison",     d2d::Color::RGB(78, 22, 24) } },
        { 26, { L"Conduit Power",    d2d::Color::RGB(26, 189, 207) } },
        { 27, { L"Dolphin's Grace",  d2d::Color::RGB(136, 200, 225) } },
        { 28, { L"Bad Omen",         d2d::Color::RGB(10, 91, 22) } },
        { 29, { L"Hero of the Village", d2d::Color::RGB(68, 255, 234) } },
        { 30, { L"Darkness",         d2d::Color::RGB(64, 64, 88) } },
        { 31, { L"Trial Omen",       d2d::Color::RGB(110, 148, 99) } },
        { 32, { L"Raid Omen",        d2d::Color::RGB(10, 91, 22) } },
        { 33, { L"Wind Charged",     d2d::Color::RGB(150, 197, 224) } },
        { 34, { L"Weaving",          d2d::Color::RGB(110, 56, 36) } },
        { 35, { L"Oozing",           d2d::Color::RGB(157, 205, 63) } },
        { 36, { L"Infested",         d2d::Color::RGB(69, 61, 54) } },
    };

    bool isPlausibleEvent(int v) {
        return v >= 1 && v <= 3;
    }

    bool isPlausibleEffect(int v) {
        return v >= 1 && v <= 36;
    }
}

PotionHUD::PotionHUD()
    : HUDModule("PotionHUD", LocalizeString::get("client.hudmodule.potionHud.name"),
                LocalizeString::get("client.hudmodule.potionHud.desc"), HUD) {
    addSetting("hideWhenNone", LocalizeString::get("client.hudmodule.potionHud.hideWhenNone.name"),
               LocalizeString::get("client.hudmodule.potionHud.hideWhenNone.desc"), hideWhenNone);
    addSetting("showAmplifier", LocalizeString::get("client.hudmodule.potionHud.showAmplifier.name"),
               LocalizeString::get("client.hudmodule.potionHud.showAmplifier.desc"), showAmplifier);
    addSetting("showDuration", LocalizeString::get("client.hudmodule.potionHud.showDuration.name"),
               LocalizeString::get("client.hudmodule.potionHud.showDuration.desc"), showDuration);

    listen<PacketReceiveEvent>(static_cast<EventListenerFunc>(&PotionHUD::onPacketReceive));
    listen<TickEvent>(static_cast<EventListenerFunc>(&PotionHUD::onTick));
    listen<LeaveGameEvent>(static_cast<EventListenerFunc>(&PotionHUD::onLeaveGame));
}

std::wstring PotionHUD::effectName(int id) {
    auto it = effectTable.find(id);
    if (it != effectTable.end()) return it->second.name;
    return L"Effect #" + std::to_wstring(id);
}

d2d::Color PotionHUD::effectColor(int id) {
    auto it = effectTable.find(id);
    if (it != effectTable.end()) return it->second.color;
    return d2d::Colors::WHITE;
}

std::wstring PotionHUD::amplifierSuffix(int amplifier) {
    if (amplifier <= 0) return L"";
    switch (amplifier) {
        case 1: return L" II";
        case 2: return L" III";
        case 3: return L" IV";
        case 4: return L" V";
        default: return L" +" + std::to_wstring(amplifier);
    }
}

std::wstring PotionHUD::formatDuration(int ticksLeft) {
    if (ticksLeft < 0) return L"";

    int totalSeconds = (ticksLeft + 19) / 20; // round up so "0:01" means still active
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    std::wstring out = std::to_wstring(minutes) + L":";
    if (seconds < 10) out += L"0";
    out += std::to_wstring(seconds);
    return out;
}

bool PotionHUD::resolveEffectFields(int slotA, int slotB, int durationTicks, int& effectId, int& event) {
    bool aEvent = isPlausibleEvent(slotA);
    bool bEvent = isPlausibleEvent(slotB);
    bool aEffect = isPlausibleEffect(slotA);
    bool bEffect = isPlausibleEffect(slotB);

    if (aEvent && !bEvent) {
        // event comes first
        event = slotA;
        effectId = slotB;
        return bEffect;
    }
    if (bEvent && !aEvent) {
        // effect comes first
        effectId = slotA;
        event = slotB;
        return aEffect;
    }
    if (aEvent && bEvent) {
        // both slots look like events: the pair is ambiguous. Remove packets carry a
        // duration of 0, adds/modifies carry a positive one - use that as a tiebreaker.
        if (durationTicks > 0) {
            // it cannot be a Remove; prefer the reading where slotA != Remove
            if (slotA == 3) {
                effectId = slotA;
                event = slotB;
            } else {
                event = slotA;
                effectId = slotB;
            }
        } else {
            // it cannot be an Add with no duration; prefer the reading where slotB != Add
            if (slotB == 1) {
                effectId = slotA;
                event = slotB;
            } else {
                event = slotA;
                effectId = slotB;
            }
        }
        return true;
    }
    return false; // neither slot looks like an event - unsupported layout
}

void PotionHUD::onPacketReceive(Event& evGeneric) {
    auto& ev = reinterpret_cast<PacketReceiveEvent&>(evGeneric);
    auto pkt = ev.getPacket();
    if (!pkt) return;

    if (static_cast<int>(pkt->getID()) != 0x1D) return; // MobEffectPacket

    auto effectPacket = static_cast<SDK::MobEffectPacket*>(pkt);

    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (!lp) return;
    if (effectPacket->runtimeID != lp->getRuntimeID()) return;

    int effectId = 0;
    int event = 0;
    if (!resolveEffectFields(effectPacket->slotA, effectPacket->slotB, effectPacket->durationTicks, effectId, event))
        return;

    switch (event) {
        case 1: // add
        case 2: // modify
            effects[effectId] = { effectPacket->amplifier, effectPacket->durationTicks };
            break;
        case 3: // remove
            effects.erase(effectId);
            break;
        default:
            break;
    }
}

void PotionHUD::onTick(Event&) {
    for (auto it = effects.begin(); it != effects.end();) {
        auto& [id, entry] = *it;
        if (entry.ticksLeft < 0) {
            ++it;
            continue;
        }
        entry.ticksLeft--;
        if (entry.ticksLeft <= 0) {
            it = effects.erase(it);
        } else {
            ++it;
        }
    }
}

void PotionHUD::onLeaveGame(Event&) {
    effects.clear();
}

void PotionHUD::render(DrawUtil& ct, bool isDefault, bool inEditor) {
    if (isDefault) return;

    auto& dc = reinterpret_cast<MCDrawUtil&>(ct);

    if (inEditor && effects.empty()) {
        effects[1] = { 0, 30 * 20 };
        effects[5] = { 1, 90 * 20 };
    }

    if (effects.empty()) {
        this->rect.right = this->rect.left;
        this->rect.bottom = this->rect.top;
        return;
    }

    const float textSize = 22.f;
    const float rowHeight = 26.f;
    const float dotSize = 9.f;
    const float gap = 6.f;
    const float pad = 5.f;

    float maxWidth = 0.f;
    float y = 0.f;

    // deterministic order
    std::vector<int> ids;
    ids.reserve(effects.size());
    for (auto& [id, entry] : effects) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (int id : ids) {
        auto& entry = effects[id];

        std::wstring line = effectName(id);
        if (std::get<BoolValue>(showAmplifier)) line += amplifierSuffix(entry.amplifier);
        if (std::get<BoolValue>(showDuration)) {
            std::wstring dur = formatDuration(entry.ticksLeft);
            if (!dur.empty()) line += L"  " + dur;
        }

        Vec2 txtSize = dc.getTextSize(line, Renderer::FontSelection::PrimaryRegular, textSize);

        d2d::Color col = effectColor(id);
        d2d::Rect dotRect = { 0.f, y + (rowHeight - dotSize) * 0.5f, dotSize,
                              y + (rowHeight - dotSize) * 0.5f + dotSize };
        dc.fillRectangle(dotRect, col);

        d2d::Rect textRect = { dotSize + gap, y, dotSize + gap + txtSize.x, y + rowHeight };
        dc.drawText(textRect, line.c_str(), d2d::Colors::WHITE, Renderer::FontSelection::PrimaryRegular, textSize,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        maxWidth = std::max(maxWidth, dotSize + gap + txtSize.x);
        y += rowHeight;
    }

    this->rect.right = this->rect.left + maxWidth + pad;
    this->rect.bottom = this->rect.top + y + pad;
}
