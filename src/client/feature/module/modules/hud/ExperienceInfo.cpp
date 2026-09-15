#include "pch.h"
#include "ExperienceInfo.h"

#include "client/event/Eventing.h"
#include "client/event/events/TickEvent.h"
#include "mc/common/entity/component/AttributesComponent.h"

namespace {
    // Bedrock attribute ids for xp. If the running game disagrees, the resolver below
    // re-discovers them by the shape of the attribute instances (level's max is huge,
    // progress' max is exactly 1).
    constexpr unsigned int defaultLevelId = 16;    // minecraft:player.level
    constexpr unsigned int defaultProgressId = 17; // minecraft:player.experience

    unsigned int levelId = defaultLevelId;
    unsigned int progressId = defaultProgressId;

    constexpr float levelMinMax = 20000.f; // player.level caps at 24791

    bool looksLikeLevel(SDK::AttributeInstance* inst) {
        return inst && inst->maxValue >= levelMinMax;
    }

    bool looksLikeProgress(SDK::AttributeInstance* inst) {
        return inst && inst->maxValue > 0.f && inst->maxValue <= 1.001f;
    }

    void resolveAttributeIds(SDK::BaseAttributeMap& base) {
        if (looksLikeLevel(base.getInstance(levelId)) && looksLikeProgress(base.getInstance(progressId)) &&
            levelId != progressId)
            return;

        unsigned int bestLevel = 0;
        float bestLevelMax = 0.f;
        unsigned int bestProgress = 0;

        for (unsigned int id = 0; id <= 40; ++id) {
            auto inst = base.getInstance(id);
            if (!inst) continue;

            if (inst->maxValue >= levelMinMax && inst->maxValue > bestLevelMax) {
                bestLevel = id;
                bestLevelMax = inst->maxValue;
            }
        }

        for (unsigned int id = 0; id <= 40; ++id) {
            if (id == bestLevel) continue;
            auto inst = base.getInstance(id);
            if (!inst) continue;

            if (looksLikeProgress(inst)) {
                bestProgress = id;
                break;
            }
        }

        if (bestLevel > 0) levelId = bestLevel;
        if (bestProgress > 0) progressId = bestProgress;
    }
}

ExperienceInfo::ExperienceInfo()
    : TextModule("ExperienceInfo", LocalizeString::get("client.textmodule.experienceInfo.name"),
                 LocalizeString::get("client.textmodule.experienceInfo.desc"), HUD) {
    addSetting("showPercent", LocalizeString::get("client.textmodule.experienceInfo.showPercent.name"),
               LocalizeString::get("client.textmodule.experienceInfo.showPercent.desc"), showPercent);

    this->prefix = TextValue(L"XP: ");

    listen<TickEvent>(static_cast<EventListenerFunc>(&ExperienceInfo::onTick));
}

void ExperienceInfo::onTick(Event&) {
    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (!lp) {
        level = -1.f;
        progress = -1.f;
        return;
    }

    auto component = lp->getAttributesComponent();
    if (!component) {
        level = -1.f;
        progress = -1.f;
        return;
    }

    resolveAttributeIds(component->baseAttributes);

    auto levelInst = component->baseAttributes.getInstance(levelId);
    auto progressInst = component->baseAttributes.getInstance(progressId);

    level = looksLikeLevel(levelInst) ? levelInst->value : -1.f;
    progress = looksLikeProgress(progressInst) ? progressInst->value : -1.f;
}

std::wstringstream ExperienceInfo::text(bool isDefault, bool inEditor) {
    float shownLevel = level;
    float shownProgress = progress;

    if (inEditor && shownLevel < 0.f) {
        shownLevel = 27.f;
        shownProgress = 0.42f;
    }

    std::wstring out;

    if (shownLevel >= 0.f) out = L"Lv. " + std::to_wstring(static_cast<long long>(shownLevel));

    if (shownProgress >= 0.f && std::get<BoolValue>(showPercent)) {
        int percent = static_cast<int>(shownProgress * 100.f + 0.5f);
        if (percent > 100) percent = 100;
        if (!out.empty()) out += L" ";
        out += std::to_wstring(percent) + L"%";
    }

    return std::wstringstream() << out;
}
