#pragma once
#include "../../TextModule.h"

class EntityCounter : public TextModule {
public:
    EntityCounter();

    void onTick(Event& ev);

private:
    ValueType radiusF = FloatValue(16.f);
    ValueType countPlayers = BoolValue(true);
    ValueType countItems = BoolValue(false);

    int count = 0;

protected:
    std::wstringstream text(bool isDefault, bool inEditor) override;
};
