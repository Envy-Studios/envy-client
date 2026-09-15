#include "pch.h"
#include "EntityCounter.h"

#include "client/event/Eventing.h"
#include "client/event/events/TickEvent.h"

EntityCounter::EntityCounter()
    : TextModule("EntityCounter", LocalizeString::get("client.textmodule.entityCounter.name"),
                 LocalizeString::get("client.textmodule.entityCounter.desc"), HUD) {
    addSliderSetting("radius", LocalizeString::get("client.textmodule.entityCounter.radius.name"),
                     LocalizeString::get("client.textmodule.entityCounter.radius.desc"), radiusF, FloatValue(1.f),
                     FloatValue(128.f), FloatValue(1.f));
    addSetting("countPlayers", LocalizeString::get("client.textmodule.entityCounter.countPlayers.name"),
               LocalizeString::get("client.textmodule.entityCounter.countPlayers.desc"), countPlayers);
    addSetting("countItems", LocalizeString::get("client.textmodule.entityCounter.countItems.name"),
               LocalizeString::get("client.textmodule.entityCounter.countItems.desc"), countItems);

    this->prefix = TextValue(L"Entities: ");

    listen<TickEvent>(static_cast<EventListenerFunc>(&EntityCounter::onTick));
}

void EntityCounter::onTick(Event&) {
    count = 0;

    auto clientInstance = SDK::ClientInstance::get();
    if (!clientInstance || !clientInstance->minecraft) return;

    auto level = clientInstance->minecraft->getLevel();
    if (!level) return;

    auto lp = clientInstance->getLocalPlayer();
    if (!lp) return;

    float radius = std::get<FloatValue>(radiusF);
    bool players = std::get<BoolValue>(countPlayers);
    bool items = std::get<BoolValue>(countItems);

    Vec3 myPos = lp->getPos();

    for (auto actor : level->getRuntimeActorList()) {
        if (!actor || actor == lp) continue;
        if (!players && actor->isPlayer()) continue;
        if (!items && actor->getEntityTypeID() == 64) continue; // item entities

        if (actor->getPos().distance(myPos) <= radius) count++;
    }
}

std::wstringstream EntityCounter::text(bool isDefault, bool inEditor) {
    return std::wstringstream() << count;
}
