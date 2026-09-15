#include "pch.h"
#include "DeathLogger.h"

#include "client/event/Eventing.h"
#include "client/event/events/TickEvent.h"
#include "client/misc/ClientMessageQueue.h"
#include "util/Logger.h"
#include "util/Util.h"

namespace {
    void replacePlaceholder(std::wstring& text, std::wstring const& token, std::wstring const& value) {
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::wstring::npos) {
            text.replace(pos, token.size(), value);
            pos += value.size();
        }
    }
}

DeathLogger::DeathLogger()
    : Module("DeathLogger", LocalizeString::get("client.hudmodule.deathLogger.name"),
             LocalizeString::get("client.hudmodule.deathLogger.desc"), HUD, nokeybind) {
    addSetting("sendMessage", LocalizeString::get("client.hudmodule.deathLogger.sendMessage.name"),
               LocalizeString::get("client.hudmodule.deathLogger.sendMessage.desc"), sendMessage);
    addSetting("showCount", LocalizeString::get("client.hudmodule.deathLogger.showCount.name"),
               LocalizeString::get("client.hudmodule.deathLogger.showCount.desc"), showCount);
    addSetting("customMessage", LocalizeString::get("client.hudmodule.deathLogger.customMessage.name"),
               LocalizeString::get("client.hudmodule.deathLogger.customMessage.desc"), customMessage);

    listen<TickEvent>(static_cast<EventListenerFunc>(&DeathLogger::onTick));
}

void DeathLogger::onTick(Event&) {
    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (!lp) {
        prevHealth = -1.f;
        return;
    }

    auto health = lp->getHealth();
    if (!health.has_value()) {
        prevHealth = -1.f;
        return;
    }

    float current = *health;
    bool died = prevHealth > 0.f && current <= 0.f;
    prevHealth = current;

    if (!died) return;

    deathCount++;

    Vec3 pos = lp->getPos();

    int x = static_cast<int>(std::floor(pos.x));
    int y = static_cast<int>(std::floor(pos.y));
    int z = static_cast<int>(std::floor(pos.z));

    std::wstring message = std::get<TextValue>(customMessage).str;
    if (message.empty()) {
        message = L"You died at {x}, {y}, {z}";
    }

    replacePlaceholder(message, L"{x}", std::to_wstring(x));
    replacePlaceholder(message, L"{y}", std::to_wstring(y));
    replacePlaceholder(message, L"{z}", std::to_wstring(z));
    replacePlaceholder(message, L"{n}", std::to_wstring(deathCount));

    std::wstring formatted = L"\u00a7c[Envy]\u00a7r " + message;
    if (std::get<BoolValue>(showCount)) {
        formatted += L" \u00a77(#" + std::to_wstring(deathCount) + L")";
    }

    if (std::get<BoolValue>(sendMessage)) {
        Envy::getClientMessageQueue().push(formatted);
    }

    Logger::Warn(util::WStrToStr(formatted));
}
