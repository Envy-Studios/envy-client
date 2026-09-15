#pragma once
#include "../../TextModule.h"

class ExperienceInfo : public TextModule {
public:
    ExperienceInfo();

    void onTick(Event& ev);

private:
    ValueType showPercent = BoolValue(true);

    float level = -1.f;
    float progress = -1.f;

protected:
    std::wstringstream text(bool isDefault, bool inEditor) override;
};
