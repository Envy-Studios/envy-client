#pragma once
#include "../../HUDModule.h"

class DirectionHUD : public HUDModule {
public:
    DirectionHUD();

    void render(DrawUtil& ctx, bool isDefault, bool inEditor) override;

private:
    ValueType stripWidth = FloatValue(220.f);
    ValueType visibleDegrees = FloatValue(120.f);
    ValueType showTicks = BoolValue(true);
    ValueType showReadout = BoolValue(true);
    ValueType fillColor = ColorValue(0.f, 0.f, 0.f, 0.5f);

    static float wrapDegrees(float deg);
    static std::wstring cardinalFor(float yaw);
};
