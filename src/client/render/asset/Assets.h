#pragma once
#include "Asset.h"
#include <vector>

class Assets final {
public:
    Assets();
    ~Assets();
    Assets(Assets&) = delete;
    Assets(Assets&&) = delete;

    Asset searchIcon;
    Asset arrowIcon;
    Asset xIcon;
    Asset hudEditIcon;
    Asset arrowBackIcon;
    Asset cogIcon;
    Asset checkmarkIcon;
    Asset document;

    void loadAll();
    void unloadAll();

private:
    std::vector<Asset*> allAssets {};
};
