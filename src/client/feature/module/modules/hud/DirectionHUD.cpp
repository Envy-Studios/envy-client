#include "pch.h"
#include "DirectionHUD.h"

DirectionHUD::DirectionHUD()
    : HUDModule("DirectionHUD", LocalizeString::get("client.hudmodule.directionHud.name"),
                LocalizeString::get("client.hudmodule.directionHud.desc"), HUD) {
    addSliderSetting("stripWidth", LocalizeString::get("client.hudmodule.directionHud.stripWidth.name"),
                     LocalizeString::get("client.hudmodule.directionHud.stripWidth.desc"), stripWidth,
                     FloatValue(80.f), FloatValue(400.f), FloatValue(10.f));
    addSliderSetting("visibleDegrees", LocalizeString::get("client.hudmodule.directionHud.visibleDegrees.name"),
                     LocalizeString::get("client.hudmodule.directionHud.visibleDegrees.desc"), visibleDegrees,
                     FloatValue(60.f), FloatValue(360.f), FloatValue(15.f));
    addSetting("showTicks", LocalizeString::get("client.hudmodule.directionHud.showTicks.name"),
               LocalizeString::get("client.hudmodule.directionHud.showTicks.desc"), showTicks);
    addSetting("showReadout", LocalizeString::get("client.hudmodule.directionHud.showReadout.name"),
               LocalizeString::get("client.hudmodule.directionHud.showReadout.desc"), showReadout);
    addSetting("fillCol", LocalizeString::get("client.hudmodule.directionHud.fillColor.name"), L"", fillColor);
}

float DirectionHUD::wrapDegrees(float deg) {
    // normalize into [-180, 180)
    deg = std::fmod(deg + 180.f, 360.f);
    if (deg < 0.f) deg += 360.f;
    return deg - 180.f;
}

std::wstring DirectionHUD::cardinalFor(float yaw) {
    static const wchar_t* cardinals[8] = { L"S", L"SW", L"W", L"NW", L"N", L"NE", L"E", L"SE" };
    // mc yaw: 0 = south, 90 = west, 180 = north, 270 = east (increases clockwise)
    float normalized = std::fmod(yaw, 360.f);
    if (normalized < 0.f) normalized += 360.f;
    int index = static_cast<int>(std::lround(normalized / 45.f)) % 8;
    return cardinals[index];
}

void DirectionHUD::render(DrawUtil& ct, bool isDefault, bool inEditor) {
    if (isDefault) return;

    auto& dc = reinterpret_cast<MCDrawUtil&>(ct);

    float width = std::get<FloatValue>(stripWidth);
    float visible = std::get<FloatValue>(visibleDegrees);
    bool ticks = std::get<BoolValue>(showTicks);
    bool readout = std::get<BoolValue>(showReadout);

    float yaw = 0.f;
    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (lp) yaw = lp->getRot().x;

    float stripHeight = 24.f;
    float lineHeight = 3.f;

    d2d::Rect strip = { 0.f, 0.f, width, stripHeight };
    dc.fillRoundedRectangle(strip, std::get<ColorValue>(fillColor).getMainColor(), 6.f);

    float center = width * 0.5f;
    float pxPerDeg = width / visible;

    // cardinal marks every 45 degrees, minor ticks every 15 degrees
    for (int deg = 0; deg < 360; deg += 15) {
        float diff = wrapDegrees(static_cast<float>(deg) - yaw);
        float px = center + diff * pxPerDeg;
        if (px < 8.f || px > width - 8.f) continue;

        float alpha = 1.f - std::min(1.f, std::abs(px - center) / (width * 0.5f)) * 0.65f;

        if (deg % 45 == 0) {
            static const wchar_t* labels[8] = { L"S", L"SW", L"W", L"NW", L"N", L"NE", L"E", L"SE" };
            std::wstring label = labels[(deg / 45) % 8];

            Vec2 txtSize = dc.getTextSize(label, Renderer::FontSelection::PrimaryRegular, 18.f);
            d2d::Rect textRect = { px - txtSize.x * 0.5f, 1.f, px + txtSize.x * 0.5f, stripHeight - 1.f };
            dc.drawText(textRect, label.c_str(), d2d::Colors::WHITE.asAlpha(alpha),
                        Renderer::FontSelection::PrimaryRegular, 18.f, DWRITE_TEXT_ALIGNMENT_LEADING,
                        DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
        } else if (ticks) {
            d2d::Rect tick = { px - 1.f, stripHeight * 0.35f, px + 1.f, stripHeight - 2.f };
            dc.fillRectangle(tick, d2d::Colors::WHITE.asAlpha(alpha * 0.8f));
        }
    }

    // center marker
    d2d::Rect marker = { center - 1.f, stripHeight, center + 1.f, stripHeight + lineHeight };
    dc.fillRectangle(marker, d2d::Colors::WHITE);

    float totalHeight = stripHeight + lineHeight;

    if (readout) {
        float normalized = std::fmod(yaw, 360.f);
        if (normalized < 0.f) normalized += 360.f;

        int degInt = static_cast<int>(normalized + 0.5f) % 360;
        std::wstring text = cardinalFor(yaw) + L" " + std::to_wstring(degInt) + L"\u00b0";

        Vec2 txtSize = dc.getTextSize(text, Renderer::FontSelection::PrimaryRegular, 16.f);
        d2d::Rect textRect = { center - txtSize.x * 0.5f, totalHeight, center + txtSize.x * 0.5f,
                               totalHeight + 18.f };
        dc.drawText(textRect, text.c_str(), d2d::Colors::WHITE, Renderer::FontSelection::PrimaryRegular, 16.f,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

        totalHeight += 18.f;
    }

    this->rect.right = this->rect.left + width;
    this->rect.bottom = this->rect.top + totalHeight;
}
