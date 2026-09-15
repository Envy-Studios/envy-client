#include "pch.h"
#include "TotemCounter.h"

TotemCounter::TotemCounter()
    : HUDModule("TotemCounter", LocalizeString::get("client.hudmodule.totemCounter.name"),
                LocalizeString::get("client.hudmodule.totemCounter.desc"), HUD) {
    addSetting("alwaysShow", LocalizeString::get("client.hudmodule.totemCounter.alwaysShow.name"),
               LocalizeString::get("client.hudmodule.totemCounter.alwaysShow.desc"), alwaysShow);
    addSetting("hotbarOnly", LocalizeString::get("client.hudmodule.totemCounter.hotbarOnly.name"),
               LocalizeString::get("client.hudmodule.totemCounter.hotbarOnly.desc"), hotbarOnly);
}

int TotemCounter::countTotems(bool hotbar) {
    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (!lp || !lp->supplies) return 0;

    auto inv = lp->supplies->inventory;
    if (!inv) return 0;

    int slots = hotbar ? 9 : 36;
    int count = 0;
    for (int i = 0; i < slots; i++) {
        auto stack = inv->getItem(i);
        if (auto it = stack->item) {
            auto tName = (*it)->id.getString();
            if (tName.find("totem") != std::string::npos) count += stack->itemCount;
        }
    }
    return count;
}

void TotemCounter::render(DrawUtil& ct, bool isDefault, bool inEditor) {
    if (isDefault) return;

    auto& dc = reinterpret_cast<MCDrawUtil&>(ct);
    auto ctx = dc.renderCtx;

    const float iconSize = 48.f;

    int count = inEditor ? 3 : countTotems(std::get<BoolValue>(hotbarOnly));

    if (count <= 0 && !std::get<BoolValue>(alwaysShow)) {
        this->rect.right = this->rect.left;
        this->rect.bottom = this->rect.top;
        return;
    }

    this->rect.right = this->rect.left + iconSize;
    this->rect.bottom = this->rect.top + iconSize;

    SDK::TexturePtr texture {};
    ctx->getTexture(&texture, SDK::ResourceLocation("textures/items/totem", 0), false);
    dc.drawImage(texture, { 0.f, 0.f }, { iconSize, iconSize }, d2d::Colors::WHITE);

    std::wstring txt = std::to_wstring(count);
    Vec2 txtSize = dc.getTextSize(txt, Renderer::FontSelection::PrimaryRegular, 30.f);

    d2d::Rect textRect = { iconSize + 3.f, 0.f, iconSize + 3.f + txtSize.x, iconSize };
    dc.drawText(textRect, txt.c_str(), d2d::Colors::WHITE, Renderer::FontSelection::PrimaryRegular, 30.f,
                DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    this->rect.right = this->rect.left + iconSize + 3.f + txtSize.x;
}
