#include "pch.h"
#include "DiscordLogin.h"

#include "client/Envy.h"
#include "client/auth/DiscordAuth.h"
#include "client/config/ConfigManager.h"
#include "client/event/Eventing.h"
#include "client/event/events/RenderOverlayEvent.h"
#include "client/localization/LocalizeString.h"
#include "client/render/Renderer.h"
#include "mc/common/client/game/ClientInstance.h"
#include "util/DrawContext.h"
#include "util/Util.h"

using FontSelection = Renderer::FontSelection;

DiscordLogin::DiscordLogin() {
    Eventing::get().listen<RenderOverlayEvent>(this, (EventListenerFunc)&DiscordLogin::onRender, 1, true);
}

void DiscordLogin::onEnable(bool ignoreAnims) {
    fade = 0.f;
    finishedLoading = false;
    DiscordAuth::get().startRestore();
}

void DiscordLogin::onDisable() {
    DiscordAuth::get().cancelSignIn();
}

void DiscordLogin::onRender(Event&) {
    // the listener runs even when the screen is closed, only draw while active
    if (!isActive()) return;

    auto& auth = DiscordAuth::get();
    auto authState = auth.getState();

    float dt = Envy::getRenderer().getDeltaTime();
    fade += (1.f - fade) * std::min(dt * 0.5f, 1.f);
    dotTimer += dt;

    auto const& rend = Envy::getRenderer();
    auto ss = rend.getScreenSize();
    adaptedScale = ss.width / 1920.f;

    D2DUtil dc;
    dc.ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    Vec2 const& cursorPos = SDK::ClientInstance::get()->cursorPos;

    // dim the game behind the sign in
    dc.fillRectangle({0.f, 0.f, ss.width, ss.height}, d2d::Color(0.f, 0.f, 0.f, 0.6f * fade));

    float boxWidth = 430.f * adaptedScale;
    float boxHeight = 250.f * adaptedScale;
    d2d::Rect box{(ss.width - boxWidth) / 2.f, (ss.height - boxHeight) / 2.f, (ss.width + boxWidth) / 2.f,
                  (ss.height + boxHeight) / 2.f};

    // the name goes above the sign in box
    d2d::Rect titleRect{box.left, box.top - 64.f * adaptedScale, box.right, box.top - 12.f * adaptedScale};
    dc.drawText(titleRect, L"Envy Client", d2d::Color(1.f, 1.f, 1.f, fade), FontSelection::PrimaryLight,
                34.f * adaptedScale, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

    // same window look as the clickgui
    dc.fillRoundedRectangle(box, d2d::Color::RGB(0x7, 0x7, 0x7).asAlpha(0.92f * fade), 19.f * adaptedScale);
    dc.drawRoundedRectangle(box, d2d::Color(0.f, 0.f, 0.f, 0.28f * fade), 19.f * adaptedScale,
                            4.f * adaptedScale, DrawUtil::OutlinePosition::Outside);

    // headline + sub text for the current state
    std::wstring headline;
    std::wstring sub;
    bool busy = false;

    switch (authState) {
    case DiscordAuth::State::Restoring:
        headline = L"Checking saved session";
        busy = true;
        break;
    case DiscordAuth::State::BrowserWaiting:
        headline = L"Waiting for Discord";
        busy = true;
        sub = L"Finish the sign in inside your browser.";
        break;
    case DiscordAuth::State::Verifying:
        headline = L"Verifying session";
        busy = true;
        break;
    case DiscordAuth::State::Failed:
        headline = L"Sign in failed";
        sub = auth.getError();
        if (sub.empty()) sub = L"Something went wrong, try again.";
        break;
    case DiscordAuth::State::SignedIn:
        headline = L"Signed in as " + util::StrToWStr(auth.session().displayName);
        break;
    default:
        headline = L"Sign in to continue";
        sub = L"You need a Discord account to use Envy.";
        break;
    }

    if (busy) {
        int dots = (int)(dotTimer * 2.f) % 4;
        headline += std::wstring((size_t)dots, L'.');
    }

    d2d::Rect headlineRect{box.left + 20.f * adaptedScale, box.top + 22.f * adaptedScale,
                           box.right - 20.f * adaptedScale, box.top + 62.f * adaptedScale};
    dc.drawText(headlineRect, headline, d2d::Color(1.f, 1.f, 1.f, 0.95f * fade), FontSelection::PrimaryRegular,
                20.f * adaptedScale, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

    if (!sub.empty()) {
        d2d::Rect subRect{box.left + 24.f * adaptedScale, box.top + 66.f * adaptedScale,
                          box.right - 24.f * adaptedScale, box.top + 96.f * adaptedScale};
        dc.drawText(subRect, sub, d2d::Color(1.f, 1.f, 1.f, 0.55f * fade), FontSelection::PrimaryRegular,
                    14.f * adaptedScale, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                    false);
    }

    bool click = justClicked[0];

    if (authState == DiscordAuth::State::Idle || authState == DiscordAuth::State::Failed) {
        d2d::Rect button{box.left + boxWidth * 0.14f, box.top + boxHeight * 0.52f,
                         box.right - boxWidth * 0.14f, box.top + boxHeight * 0.52f + 44.f * adaptedScale};
        bool hover = shouldSelect(button, cursorPos);

        d2d::Color fill = hover ? d2d::Color::RGB(0x47, 0x52, 0xC4).asAlpha(0.95f * fade)
                                : d2d::Color::RGB(0x58, 0x65, 0xF2).asAlpha(0.95f * fade);
        dc.fillRoundedRectangle(button, fill, 8.f * adaptedScale);
        dc.drawText(button, L"Sign in with Discord", d2d::Color(1.f, 1.f, 1.f, fade),
                    FontSelection::PrimaryRegular, 18.f * adaptedScale, DWRITE_TEXT_ALIGNMENT_CENTER,
                    DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

        if (click && hover) {
            playClickSound();
            auth.beginSignIn();
        }
    } else if (authState == DiscordAuth::State::BrowserWaiting) {
        d2d::Rect button{box.left + boxWidth * 0.3f, box.top + boxHeight * 0.52f,
                         box.right - boxWidth * 0.3f, box.top + boxHeight * 0.52f + 38.f * adaptedScale};
        bool hover = shouldSelect(button, cursorPos);

        dc.fillRoundedRectangle(button,
                                d2d::Color(1.f, 1.f, 1.f, (hover ? 0.12f : 0.07f) * fade),
                                8.f * adaptedScale);
        dc.drawText(button, L"Cancel", d2d::Color(1.f, 1.f, 1.f, 0.8f * fade), FontSelection::PrimaryRegular,
                    16.f * adaptedScale, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                    false);

        if (click && hover) {
            playClickSound();
            auth.cancelSignIn();
        }
    }

    // legal line at the bottom of the box
    d2d::Rect legalRect{box.left + 12.f * adaptedScale, box.bottom - 34.f * adaptedScale,
                        box.right - 12.f * adaptedScale, box.bottom - 10.f * adaptedScale};
    dc.drawText(legalRect, L"Not affiliated with Mojang or Microsoft.",
                d2d::Color(1.f, 1.f, 1.f, 0.45f * fade), FontSelection::PrimaryRegular, 12.f * adaptedScale,
                DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

    // hand over to the game thread: load the modules, then close the screen
    if (authState == DiscordAuth::State::SignedIn && !finishedLoading) {
        finishedLoading = true;
        Envy::queueForClientThread([] {
            Envy::getConfigManager().applyModuleConfig();
            Envy::getNotifications().push(LocalizeString::get("client.intro.welcome"));
            Envy::getNotifications().push(util::FormatWString(
                LocalizeString::get("client.intro.menubutton"),
                {util::StrToWStr(util::KeyToString(Envy::get().getMenuKey().value))}));
        });
        this->close();
    }
}
