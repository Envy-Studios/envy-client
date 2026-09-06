#pragma once
#include "../HUDModule.h"

#include <string>
#include <vector>

class CustomCrosshair : public HUDModule {
public:
    enum Mode : int {
        Preset = 0,
        Image = 1,
    };

    enum Style : int {
        Cross = 0,
        Dot,
        Circle,
        CrossDot,
        CircleDot,
        TShape,
        Chevron,
        Square,
    };

    // The crosshair draws centered inside this fixed-size box, which is what
    // the HUD editor and the ClickGUI preview use for dragging / layout.
    static constexpr float boundingBox = 50.f;

    CustomCrosshair();
    ~CustomCrosshair() override;

    void render(DrawUtil& dc, bool isDefault, bool inEditor) override;

private:
    void drawPreset(DrawUtil& dc, float cx, float cy, d2d::Color const& col, bool inEditor);
    void drawImageCrosshair(DrawUtil& dc, float cx, float cy, bool inEditor);
    void drawOutlinedRects(DrawUtil& dc, std::vector<d2d::Rect> const& shapes, d2d::Color const& col,
                           d2d::Color const& outlineCol, float outlineWidth);
    void drawRing(DrawUtil& dc, float cx, float cy, float radius, float thickness, d2d::Color const& col,
                  d2d::Color const& outlineCol, float outlineWidth);
    void drawChevron(DrawUtil& dc, float cx, float cy, float armLength, float thickness, float gap,
                     d2d::Color const& col, d2d::Color const& outlineCol, float outlineWidth);
    void tryLoadImage();
    void browseForImage();

    EnumData mode;
    EnumData style;

    ValueType color = ColorValue(1.f, 1.f, 1.f, 1.f);
    ValueType opacity = FloatValue(1.f);
    ValueType size = FloatValue(10.f);
    ValueType thickness = FloatValue(2.f);
    ValueType gap = FloatValue(0.f);
    ValueType outline = BoolValue(true);
    ValueType outlineColor = ColorValue(0.f, 0.f, 0.f, 1.f);
    ValueType imagePath = TextValue(L"");
    ValueType imageScale = FloatValue(1.f);

    ComPtr<ID2D1Bitmap> bitmap;
    std::wstring loadedImage;
    bool imageLoadFailed = false;
    bool initialCentering = false;
};
