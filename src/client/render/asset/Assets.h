#pragma once
#include "Asset.h"
#include <vector>

class Assets final {
public:
    Assets();
    ~Assets();
    Assets(Assets&) = delete;
    Assets(Assets&&) = delete;

    Asset envyLogo;
    Asset searchIcon;
    Asset arrowIcon;
    Asset xIcon;
    Asset hudEditIcon;
    Asset arrowBackIcon;
    Asset cogIcon;
    Asset checkmarkIcon;
    Asset logoWhite;
    Asset document;
    Asset tabAllIcon;
    Asset tabGameIcon;
    Asset tabHudIcon;
    Asset tabPluginsIcon;

    void loadAll();
    void unloadAll();

private:
    std::vector<Asset*> allAssets {};
};
