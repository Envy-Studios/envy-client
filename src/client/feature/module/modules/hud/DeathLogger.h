#pragma once
#include "../../Module.h"

class DeathLogger : public Module {
public:
    DeathLogger();

    void onTick(Event& ev);

private:
    ValueType sendMessage = BoolValue(true);
    ValueType showCount = BoolValue(false);
    ValueType customMessage = TextValue(L"You died at {x}, {y}, {z}");

    float prevHealth = -1.f;
    int deathCount = 0;
};
