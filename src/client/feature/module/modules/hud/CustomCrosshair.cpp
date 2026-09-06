#include "pch.h"
#include "CustomCrosshair.h"

#include <client/Envy.h>
#include <mc/common/client/gui/ScreenView.h>
#include <mc/common/client/gui/controls/VisualTree.h>
#include <mc/common/client/gui/controls/UIControl.h>
#include <commdlg.h>
#include "mc/common/client/renderer/MaterialPtr.h"
#include "mc/common/client/renderer/MeshUtils.h"
#include "mc/common/client/renderer/Tessellator.h"
#include <glm/gtc/matrix_transform.hpp>

namespace {
    // RAII helper that rotates the active renderer's transform around a point
    // while a chevron arm is being drawn.
    struct RotateGuard {
        DrawUtil& dc;
        bool active = false;
        D2D1::Matrix3x2F oldTransform = D2D1::Matrix3x2F::Identity();

        RotateGuard(DrawUtil& dc, float cx, float cy, float degrees)
            : dc(dc) {
            if (dc.isMinecraft()) {
                auto& mc = static_cast<MCDrawUtil&>(dc);
                if (!mc.scn || !mc.scn->matrix || mc.scn->matrix->matrixStack.empty()) return;
                glm::mat4 top = mc.scn->matrix->matrixStack.top();
                glm::mat4 rotation =
                    glm::rotate(glm::mat4(1.f), EnvyMath::deg2rad(degrees), glm::vec3(0.f, 0.f, 1.f));
                mc.scn->matrix->matrixStack.push(top * rotation);
                active = true;
            } else {
                auto* d2d = static_cast<D2DUtil*>(&dc);
                d2d->ctx->GetTransform(&oldTransform);
                d2d->ctx->SetTransform(D2D1::Matrix3x2F::Rotation(degrees, D2D1::Point2F(cx, cy)) * oldTransform);
                active = true;
            }
        }

        ~RotateGuard() {
            if (!active) return;
            if (dc.isMinecraft()) {
                auto& mc = static_cast<MCDrawUtil&>(dc);
                if (!mc.scn || !mc.scn->matrix || mc.scn->matrix->matrixStack.empty()) return;
                mc.scn->matrix->matrixStack.pop();
            } else {
                auto* d2d = static_cast<D2DUtil*>(&dc);
                d2d->ctx->SetTransform(oldTransform);
            }
        }
    };
} // namespace

CustomCrosshair::CustomCrosshair()
    : HUDModule("CustomCrosshair", LocalizeString::get("client.hudmodule.customCrosshair.name"),
                LocalizeString::get("client.hudmodule.customCrosshair.desc"), HUD) {
    using Cond = Setting::Condition;

    Cond presetCond("mode", Cond::EQUALS, { Mode::Preset });
    Cond imageCond("mode", Cond::EQUALS, { Mode::Image });

    mode.addEntry(EnumEntry{ Mode::Preset, LocalizeString::get("client.hudmodule.customCrosshair.mode.preset.name") });
    mode.addEntry(EnumEntry{ Mode::Image, LocalizeString::get("client.hudmodule.customCrosshair.mode.image.name") });
    addEnumSetting("mode", LocalizeString::get("client.hudmodule.customCrosshair.mode.name"),
                   LocalizeString::get("client.hudmodule.customCrosshair.mode.desc"), mode);

    style.addEntry(EnumEntry{ Style::Cross, LocalizeString::get("client.hudmodule.customCrosshair.style.cross.name") });
    style.addEntry(EnumEntry{ Style::Dot, LocalizeString::get("client.hudmodule.customCrosshair.style.dot.name") });
    style.addEntry(EnumEntry{ Style::Circle, LocalizeString::get("client.hudmodule.customCrosshair.style.circle.name") });
    style.addEntry(EnumEntry{ Style::CrossDot, LocalizeString::get("client.hudmodule.customCrosshair.style.crossDot.name") });
    style.addEntry(EnumEntry{ Style::CircleDot, LocalizeString::get("client.hudmodule.customCrosshair.style.circleDot.name") });
    style.addEntry(EnumEntry{ Style::TShape, LocalizeString::get("client.hudmodule.customCrosshair.style.tShape.name") });
    style.addEntry(EnumEntry{ Style::Chevron, LocalizeString::get("client.hudmodule.customCrosshair.style.chevron.name") });
    style.addEntry(EnumEntry{ Style::Square, LocalizeString::get("client.hudmodule.customCrosshair.style.square.name") });
    addEnumSetting("style", LocalizeString::get("client.hudmodule.customCrosshair.style.name"),
                   LocalizeString::get("client.hudmodule.customCrosshair.style.desc"), style, presetCond);

    addSetting("color", LocalizeString::get("client.hudmodule.customCrosshair.color.name"),
               LocalizeString::get("client.hudmodule.customCrosshair.color.desc"), color, presetCond);
    addSliderSetting("size", LocalizeString::get("client.hudmodule.customCrosshair.size.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.size.desc"), size, FloatValue(2.f),
                     FloatValue(20.f), FloatValue(1.f), presetCond);
    addSliderSetting("thickness", LocalizeString::get("client.hudmodule.customCrosshair.thickness.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.thickness.desc"), thickness,
                     FloatValue(1.f), FloatValue(10.f), FloatValue(0.5f), presetCond);
    addSliderSetting("gap", LocalizeString::get("client.hudmodule.customCrosshair.gap.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.gap.desc"), gap, FloatValue(0.f),
                     FloatValue(10.f), FloatValue(1.f), presetCond);
    addSetting("outline", LocalizeString::get("client.hudmodule.customCrosshair.outline.name"),
               LocalizeString::get("client.hudmodule.customCrosshair.outline.desc"), outline, presetCond);
    addSetting("outlineColor", LocalizeString::get("client.hudmodule.customCrosshair.outlineColor.name"),
               LocalizeString::get("client.hudmodule.customCrosshair.outlineColor.desc"), outlineColor,
               "outline"_istrue);
    addSliderSetting("opacity", LocalizeString::get("client.hudmodule.customCrosshair.opacity.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.opacity.desc"), opacity,
                     FloatValue(0.05f), FloatValue(1.f), FloatValue(0.05f));

    addSetting("imagePath", LocalizeString::get("client.hudmodule.customCrosshair.imagePath.name"),
               LocalizeString::get("client.hudmodule.customCrosshair.imagePath.desc"), imagePath, imageCond);
    addActionSetting("browseImage", LocalizeString::get("client.hudmodule.customCrosshair.browseImage.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.browseImage.desc"),
                     [this] { browseForImage(); }, imageCond);
    addSliderSetting("imageScale", LocalizeString::get("client.hudmodule.customCrosshair.imageScale.name"),
                     LocalizeString::get("client.hudmodule.customCrosshair.imageScale.desc"), imageScale,
                     FloatValue(0.25f), FloatValue(4.f), FloatValue(0.25f), imageCond);

    // Anchor the crosshair at the center of the screen by default.
    std::get<Vec2Value>(storedPos) = { 0.5f, 0.5f };

    // Move the vanilla crosshair out of the way while this module is enabled.
    // Priority 10 overpowers the HUD renderer, same as the Movable modules.
    listen<RenderLayerEvent>(static_cast<EventListenerFunc>(&CustomCrosshair::onRenderLayer), true, 10);
}

CustomCrosshair::~CustomCrosshair() = default;

d2d::Rect CustomCrosshair::getRect() {
    // Grow/shrink the box equally in every direction around its center. The
    // renderer transforms local points as Scale(s) * Translation(box topLeft),
    // so with a centered box the crosshair's local center (boundingBox / 2)
    // always lands on the same screen point while the shapes scale around it.
    float grow = (boundingBox / 2.f) * (getScale() - 1.f);
    return { rect.left - grow, rect.top - grow, rect.left + boundingBox + grow,
             rect.top + boundingBox + grow };
}

void CustomCrosshair::onRenderLayer(Event& evGeneric) {
    auto& ev = reinterpret_cast<RenderLayerEvent&>(evGeneric);

    if (!isEnabled()) {
        restoreVanillaCrosshair();
        return;
    }

    auto* screenView = ev.getScreenView();
    if (!screenView || !screenView->visualTree || !screenView->visualTree->rootControl) return;
    auto* root = screenView->visualTree->rootControl;
    if (root->name != "hud_screen") return;

    // Locate the vanilla crosshair control. The exact name differs between
    // game versions, so try the known candidates.
    std::shared_ptr<SDK::UIControl> found;
    root->getDescendants([&](std::shared_ptr<SDK::UIControl> const& control) {
        if (found) return;
        if (control->name == "crosshair" || control->name == "crosshair_renderer" ||
            control->name == "crosshair_image") {
            found = control;
        }
    });

    if (!found) {
        vanillaCrosshair = nullptr;
        return;
    }

    if (found != vanillaCrosshair) {
        // A fresh control instance appeared; remember its untouched position.
        vanillaCrosshair = found;
        vanillaOriginalPos = found->position;
    } else if (found->position.x != 9999.f || found->position.y != 9999.f) {
        // The game re-laid-out the control; refresh the restore point.
        vanillaOriginalPos = found->position;
    }

    // Park the vanilla crosshair far off-screen (the same trick the Movable
    // modules use) and propagate the new position to its children.
    found->position = { 9999.f, 9999.f };
    found->getDescendants([](std::shared_ptr<SDK::UIControl> const& control) { control->updatePos(); });
}

void CustomCrosshair::restoreVanillaCrosshair() {
    if (!vanillaCrosshair) return;

    vanillaCrosshair->position = vanillaOriginalPos;
    vanillaCrosshair->getDescendants([](std::shared_ptr<SDK::UIControl> const& control) { control->updatePos(); });
    vanillaCrosshair = nullptr;
}

void CustomCrosshair::render(DrawUtil& dc, bool isDefault, bool inEditor) {
    // Keep a fixed-size bounding box; everything is drawn centered inside it.
    rect = { rect.left, rect.top, rect.left + boundingBox, rect.top + boundingBox };

    if (!initialCentering) {
        initialCentering = true;
        auto& pos = std::get<Vec2Value>(storedPos);
        if (pos.x == 0.5f && pos.y == 0.5f) {
            // The default anchor is the screen center; place the box so the
            // crosshair's center (not its top-left corner) sits on that point.
            // Runs even when no config entry was loaded (fresh installs).
            auto& ss = SDK::ClientInstance::get()->getGuiData()->screenSize;
            float ox = pos.x * ss.x, oy = pos.y * ss.y;
            rect = { ox - boundingBox / 2.f, oy - boundingBox / 2.f,
                     ox + boundingBox / 2.f, oy + boundingBox / 2.f };
        }
    }

    float op = std::clamp(std::get<FloatValue>(opacity).value, 0.05f, 1.f);
    float cx = boundingBox / 2.f;
    float cy = boundingBox / 2.f;

    if (mode.getSelectedKey() == Mode::Image) {
        drawImageCrosshair(dc, cx, cy, inEditor);
        return;
    }

    d2d::Color col = std::get<ColorValue>(color).getMainColor();
    col.a *= op;
    drawPreset(dc, cx, cy, col, inEditor);
}

void CustomCrosshair::drawOutlinedRects(DrawUtil& dc, std::vector<d2d::Rect> const& shapes, d2d::Color const& col,
                                        d2d::Color const& outlineCol, float outlineWidth) {
    if (outlineWidth > 0.f) {
        for (auto const& s : shapes) {
            dc.fillRectangle({ s.left - outlineWidth, s.top - outlineWidth, s.right + outlineWidth,
                               s.bottom + outlineWidth },
                             outlineCol);
        }
    }

    for (auto const& s : shapes) {
        dc.fillRectangle(s, col);
    }
}

void CustomCrosshair::drawRing(DrawUtil& dc, float cx, float cy, float radius, float thickness, d2d::Color const& col,
                               d2d::Color const& outlineCol, float outlineWidth) {
    float half = (std::max)(0.5f, thickness / 2.f);

    if (dc.isMinecraft()) {
        auto& mc = static_cast<MCDrawUtil&>(dc);
        if (!mc.scn || !mc.scn->tess) return;

        auto drawMcRing = [&](float rOuter, float rInner, d2d::Color const& ringCol) {
            auto tess = mc.scn->tess;
            *mc.scn->shaderColor = { 1.f, 1.f, 1.f, 1.f };
            tess->color(ringCol);

            constexpr int sides = 28;
            tess->begin(SDK::Primitive::Trianglestrip, sides * 2);
            float step = (2.f * pi_f) / static_cast<float>(sides);
            for (int i = 0; i <= sides; ++i) {
                float ang = step * static_cast<float>(i);
                float cosA = std::cos(ang);
                float sinA = std::sin(ang);
                tess->vertex(cx + rOuter * cosA, cy + rOuter * sinA);
                tess->vertex(cx + rInner * cosA, cy + rInner * sinA);
            }
            SDK::MeshHelpers::renderMeshImmediately(mc.scn, tess, SDK::MaterialPtr::getUIColor());
        };

        if (outlineWidth > 0.f) {
            drawMcRing(radius + half + outlineWidth, (std::max)(0.f, radius - half - outlineWidth), outlineCol);
        }
        drawMcRing(radius + half, (std::max)(0.f, radius - half), col);
        return;
    }

    auto* d2d = static_cast<D2DUtil*>(&dc);
    if (!d2d->ctx || !d2d->brush) return;
    D2D1_POINT_2F center = D2D1::Point2F(cx, cy);

    if (outlineWidth > 0.f) {
        d2d->brush->SetColor(outlineCol.get());
        d2d->ctx->DrawEllipse(D2D1::Ellipse(center, radius, radius), d2d->brush, thickness + outlineWidth * 2.f,
                              nullptr);
    }

    d2d->brush->SetColor(col.get());
    d2d->ctx->DrawEllipse(D2D1::Ellipse(center, radius, radius), d2d->brush, thickness, nullptr);
}

void CustomCrosshair::drawChevron(DrawUtil& dc, float cx, float cy, float armLength, float thickness, float gap,
                                  d2d::Color const& col, d2d::Color const& outlineCol, float outlineWidth) {
    // The two arms of an upward chevron are a vertical arm rotated +-45 degrees.
    for (float angle : { -45.f, 45.f }) {
        RotateGuard guard(dc, cx, cy, angle);
        std::vector<d2d::Rect> shapes = { { cx - thickness / 2.f, cy - gap - armLength, cx + thickness / 2.f,
                                            cy - gap } };
        drawOutlinedRects(dc, shapes, col, outlineCol, outlineWidth);
    }
}

void CustomCrosshair::drawPreset(DrawUtil& dc, float cx, float cy, d2d::Color const& col, bool inEditor) {
    float sz = (std::max)(1.f, std::get<FloatValue>(size).value);
    float th = (std::max)(1.f, std::get<FloatValue>(thickness).value);
    float gp = (std::max)(0.f, std::get<FloatValue>(gap).value);
    bool useOutline = std::get<BoolValue>(outline);
    float outlineWidth = useOutline ? (std::max)(1.f, th * 0.4f) : 0.f;
    d2d::Color outCol = std::get<ColorValue>(outlineColor).getMainColor();
    outCol.a = col.a;
    d2d::Color noOutline = { 0.f, 0.f, 0.f, 0.f };
    d2d::Color& outlineRef = useOutline ? outCol : noOutline;

    auto styleKey = static_cast<Style>(style.getSelectedKey());
    switch (styleKey) {
    case Style::Cross:
    case Style::CrossDot: {
        std::vector<d2d::Rect> shapes = {
            { cx - th / 2.f, cy - gp - sz, cx + th / 2.f, cy - gp }, // top
            { cx - th / 2.f, cy + gp, cx + th / 2.f, cy + gp + sz }, // bottom
            { cx - gp - sz, cy - th / 2.f, cx - gp, cy + th / 2.f }, // left
            { cx + gp, cy - th / 2.f, cx + gp + sz, cy + th / 2.f }, // right
        };
        if (styleKey == Style::CrossDot) {
            shapes.push_back({ cx - th / 2.f, cy - th / 2.f, cx + th / 2.f, cy + th / 2.f });
        }
        drawOutlinedRects(dc, shapes, col, outlineRef, outlineWidth);
        break;
    }
    case Style::Dot:
        drawOutlinedRects(dc, { { cx - th / 2.f, cy - th / 2.f, cx + th / 2.f, cy + th / 2.f } }, col, outlineRef,
                          outlineWidth);
        break;
    case Style::TShape: {
        std::vector<d2d::Rect> shapes = {
            { cx - th / 2.f, cy + gp, cx + th / 2.f, cy + gp + sz },  // bottom only
            { cx - gp - sz, cy - th / 2.f, cx - gp, cy + th / 2.f },  // left
            { cx + gp, cy - th / 2.f, cx + gp + sz, cy + th / 2.f },  // right
        };
        drawOutlinedRects(dc, shapes, col, outlineRef, outlineWidth);
        break;
    }
    case Style::Square: {
        std::vector<d2d::Rect> shapes = {
            { cx - sz, cy - sz, cx + sz, cy - sz + th }, // top edge
            { cx - sz, cy + sz - th, cx + sz, cy + sz }, // bottom edge
            { cx - sz, cy - sz, cx - sz + th, cy + sz }, // left edge
            { cx + sz - th, cy - sz, cx + sz, cy + sz }, // right edge
        };
        drawOutlinedRects(dc, shapes, col, outlineRef, outlineWidth);
        break;
    }
    case Style::Circle:
    case Style::CircleDot:
        drawRing(dc, cx, cy, sz, th, col, outlineRef, outlineWidth);
        if (styleKey == Style::CircleDot) {
            drawOutlinedRects(dc, { { cx - th / 2.f, cy - th / 2.f, cx + th / 2.f, cy + th / 2.f } }, col, outlineRef,
                              outlineWidth);
        }
        break;
    case Style::Chevron:
        drawChevron(dc, cx, cy, sz, th, gp, col, outlineRef, outlineWidth);
        break;
    }
}

void CustomCrosshair::tryLoadImage() {
    bitmap = nullptr;
    imageLoadFailed = false;

    std::wstring path = std::get<TextValue>(imagePath).str;
    loadedImage = path;
    if (path.empty()) return;

    auto factory = Envy::getRenderer().getImagingFactory();
    auto deviceCtx = Envy::getRenderer().getDeviceContext();
    if (!factory || !deviceCtx) {
        // Renderer is not ready yet; try again on a later frame.
        loadedImage.clear();
        return;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                  decoder.GetAddressOf()))) {
        imageLoadFailed = true;
        return;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.GetAddressOf()))) {
        imageLoadFailed = true;
        return;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.GetAddressOf()))) {
        imageLoadFailed = true;
        return;
    }

    if (FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.f,
                                     WICBitmapPaletteTypeCustom))) {
        imageLoadFailed = true;
        return;
    }

    ID2D1Bitmap* newBitmap = nullptr;
    if (FAILED(deviceCtx->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &newBitmap))) {
        imageLoadFailed = true;
        return;
    }

    bitmap.Attach(newBitmap);
}

void CustomCrosshair::browseForImage() {
    std::vector<wchar_t> selectedFile(32768, L'\0');
    constexpr wchar_t filter[] =
        L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = selectedFile.data();
    dialog.nMaxFile = static_cast<DWORD>(selectedFile.size());
    dialog.lpstrTitle = L"Choose a crosshair image";
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_DONTADDTORECENT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR |
                   OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&dialog)) {
        std::wstring chosen(selectedFile.data());
        if (chosen.empty()) return;
        std::get<TextValue>(imagePath).str = chosen;
        bitmap = nullptr;
        imageLoadFailed = false;
        loadedImage.clear();
    }
}

void CustomCrosshair::drawImageCrosshair(DrawUtil& dc, float cx, float cy, bool inEditor) {
    if (dc.isMinecraft()) {
        // Arbitrary bitmaps can only be drawn through D2D; fall back to the preset.
        d2d::Color col = std::get<ColorValue>(color).getMainColor();
        col.a *= std::clamp(std::get<FloatValue>(opacity).value, 0.05f, 1.f);
        drawPreset(dc, cx, cy, col, inEditor);
        return;
    }

    std::wstring const& path = std::get<TextValue>(imagePath).str;
    if (!path.empty() && path != loadedImage && !imageLoadFailed) {
        tryLoadImage();
    }

    bool valid = bitmap && path == loadedImage;
    if (valid) {
        float scale = std::clamp(std::get<FloatValue>(imageScale).value, 0.1f, 8.f);
        D2D1_SIZE_F size = bitmap->GetSize();
        float w = size.width * scale;
        float h = size.height * scale;
        if (w >= 1.f && h >= 1.f) {
            float imgOpacity = std::clamp(std::get<FloatValue>(opacity).value, 0.05f, 1.f);
            static_cast<D2DUtil&>(dc).drawBitmapRotated(
                bitmap.Get(), { cx - w / 2.f, cy - h / 2.f, cx + w / 2.f, cy + h / 2.f }, 0.f, imgOpacity);
            return;
        }
        valid = false;
    }

    if (inEditor) {
        // Show a placeholder so an empty/invalid image is visible in the HUD editor.
        d2d::Color red = d2d::Colors::RED;
        red.a = 0.8f;
        dc.drawRectangle({ cx - 20.f, cy - 20.f, cx + 20.f, cy + 20.f }, red, 2.f);
    }
}
