#pragma once

#include "../Screen.h"

// the sign in gate. shows up right after injection and stays on top of the
// game until the user authenticated with discord. modules don't load before
// that.
class DiscordLogin : public Screen {
public:
    DiscordLogin();

    void onRender(Event& ev);

    std::string getName() override { return "DiscordLogin"; }
    bool canCloseByUser() override { return false; }

protected:
    void onEnable(bool ignoreAnims) override;
    void onDisable() override;

private:
    float adaptedScale = 0.f;
    float fade = 0.f;
    float dotTimer = 0.f;
    bool finishedLoading = false;
};
