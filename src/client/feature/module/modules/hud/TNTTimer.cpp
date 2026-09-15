#include "pch.h"
#include "TNTTimer.h"

#include "client/event/Eventing.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/RenderLayerEvent.h"
#include "client/event/events/LeaveGameEvent.h"
#include "util/WorldToScreen.h"

TNTTimer::TNTTimer()
    : Module("TNTTimer", LocalizeString::get("client.hudmodule.tntTimer.name"),
             LocalizeString::get("client.hudmodule.tntTimer.desc"), HUD, nokeybind) {
    addSliderSetting("fontSize", LocalizeString::get("client.hudmodule.tntTimer.fontSize.name"),
                     LocalizeString::get("client.hudmodule.tntTimer.fontSize.desc"), fontSize, FloatValue(10.f),
                     FloatValue(60.f), FloatValue(2.f));
    addSetting("showTicks", LocalizeString::get("client.hudmodule.tntTimer.showTicks.name"),
               LocalizeString::get("client.hudmodule.tntTimer.showTicks.desc"), showTicks);
    addSetting("textCol", LocalizeString::get("client.hudmodule.tntTimer.textColor.name"), L"", textColor);

    listen<TickEvent>(static_cast<EventListenerFunc>(&TNTTimer::onTick));
    listen<RenderLayerEvent>(static_cast<EventListenerFunc>(&TNTTimer::onRenderLayer));
    listen<LeaveGameEvent>(static_cast<EventListenerFunc>(&TNTTimer::onLeaveGame));
}

void TNTTimer::onTick(Event&) {
    auto clientInstance = SDK::ClientInstance::get();
    if (!clientInstance || !clientInstance->minecraft) {
        tracked.clear();
        return;
    }

    auto level = clientInstance->minecraft->getLevel();
    if (!level) {
        tracked.clear();
        return;
    }

    std::unordered_map<uint64_t, TrackedTNT> next;

    for (auto actor : level->getRuntimeActorList()) {
        if (!actor) continue;

        auto typeName = actor->getEntityTypeName();
        if (typeName != "tnt" && typeName != "minecraft:tnt") continue;

        uint64_t runtimeId = actor->getRuntimeID();

        auto it = tracked.find(runtimeId);
        if (it != tracked.end()) {
            TrackedTNT entry = it->second;
            entry.pos = actor->getPos();
            entry.ticksLeft--;
            if (entry.ticksLeft > 0) next[runtimeId] = entry;
        } else {
            // freshly primed tnt: the vanilla fuse is 80 ticks
            next[runtimeId] = { actor->getPos(), 80 };
        }
    }

    tracked = std::move(next);
}

void TNTTimer::onRenderLayer(Event& evGeneric) {
    auto& ev = reinterpret_cast<RenderLayerEvent&>(evGeneric);
    auto screenView = ev.getScreenView();
    if (!screenView || !screenView->visualTree || !screenView->visualTree->rootControl) return;
    if (screenView->visualTree->rootControl->name != "hud_screen") return;

    if (tracked.empty()) return;

    auto clientInstance = SDK::ClientInstance::get();
    if (!clientInstance || !clientInstance->minecraftGame) return;

    auto font = clientInstance->minecraftGame->getFontRepository()->getMinecraftFont();
    if (!font) return;

    MCDrawUtil dc { ev.getUIRenderContext(), font };

    float size = std::get<FloatValue>(fontSize);
    d2d::Color col = std::get<ColorValue>(textColor).getMainColor();

    for (auto& [runtimeId, entry] : tracked) {
        Vec3 worldPos = entry.pos + Vec3(0.f, 1.6f, 0.f);

        auto screenPos = WorldToScreen::convert(worldPos);
        if (!screenPos.has_value()) continue;

        std::wstring text;
        if (std::get<BoolValue>(showTicks)) {
            text = std::to_wstring(entry.ticksLeft) + L"t";
        } else {
            int tenth = (entry.ticksLeft * 10 + 19) / 20; // seconds * 10, rounded up
            text = std::to_wstring(tenth / 10) + L"." + std::to_wstring(tenth % 10) + L"s";
        }

        Vec2 txtSize = dc.getTextSize(text, Renderer::FontSelection::PrimaryRegular, size);

        d2d::Rect textRect = { screenPos->x - txtSize.x * 0.5f, screenPos->y - txtSize.y * 0.5f,
                               screenPos->x + txtSize.x * 0.5f, screenPos->y + txtSize.y * 0.5f };
        dc.drawText(textRect, text.c_str(), col, Renderer::FontSelection::PrimaryRegular, size,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
    }

    dc.flush();
}

void TNTTimer::onLeaveGame(Event&) {
    tracked.clear();
}
