#pragma once
#include "../../HUDModule.h"

class TotemCounter : public HUDModule {
public:
    TotemCounter();

    void render(DrawUtil& ctx, bool isDefault, bool inEditor) override;

    [[nodiscard]] bool forceMinecraftRenderer() override { return true; }

private:
    ValueType alwaysShow = BoolValue(false);
    ValueType hotbarOnly = BoolValue(false);

    int countTotems(bool hotbar);
};
