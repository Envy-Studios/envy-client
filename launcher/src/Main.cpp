#include "Flow.h"

#include <commctrl.h>
#include <windows.h>

#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

    constexpr int IDC_DISCORD = 101;
    constexpr int IDC_KEYEDIT = 102;
    constexpr int IDC_ACTIVATE = 103;
    constexpr int IDC_LAUNCH = 104;

    enum Panel { PanelLogin, PanelAuthed };

    struct Ui {
        HWND discordBtn = nullptr;
        HWND keyEdit = nullptr;
        HWND activateBtn = nullptr;
        HWND launchBtn = nullptr;

        Panel panel = PanelLogin;
        bool showLoginControls = false; // stays hidden while the saved sign in is checked
        bool busy = false;

        std::wstring statusText = L"Checking your saved sign in...";
        int statusColor = 0; // 0 gray, 1 green, 2 red
        std::wstring signedInAs;

        HFONT wordFont = nullptr;
        HFONT subFont = nullptr;
        HFONT captionFont = nullptr;
        HFONT btnFont = nullptr;
        HFONT statusFont = nullptr;
    };

    Ui g;
    UINT g_dpi = 96;
    UINT_PTR g_hotButton = 0;
    bool g_trackedButton = false;

    int S(int v) { return MulDiv(v, (int)g_dpi, 96); }

    void EnableDpiAwareness() {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (auto f = (BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT))GetProcAddress(
                user32, "SetProcessDpiAwarenessContext")) {
            f(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }

    void CreateUiFonts() {
        g.wordFont = CreateFontW(-S(30), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g.subFont = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g.captionFont = CreateFontW(-S(11), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g.btnFont = CreateFontW(-S(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g.statusFont = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    void DestroyUiFonts() {
        for (HFONT f : {g.wordFont, g.subFont, g.captionFont, g.btnFont, g.statusFont}) {
            if (f) DeleteObject(f);
        }
    }

    HWND MakeButton(HWND parent, HINSTANCE inst, wchar_t const* text, int id) {
        return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
                               0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, inst, nullptr);
    }

    void ApplyLayout(HWND hwnd) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;

        bool login = g.panel == PanelLogin && g.showLoginControls;

        ShowWindow(g.discordBtn, login ? SW_SHOW : SW_HIDE);
        ShowWindow(g.keyEdit, login ? SW_SHOW : SW_HIDE);
        ShowWindow(g.activateBtn, login ? SW_SHOW : SW_HIDE);
        ShowWindow(g.launchBtn, g.panel == PanelAuthed ? SW_SHOW : SW_HIDE);

        if (login) {
            SetWindowPos(g.discordBtn, nullptr, S(28), S(104), w - S(56), S(46), SWP_NOZORDER);
            SetWindowPos(g.keyEdit, nullptr, S(28), S(216), w - S(56), S(34), SWP_NOZORDER);
            SetWindowPos(g.activateBtn, nullptr, S(28), S(262), w - S(56), S(42), SWP_NOZORDER);
        } else if (g.panel == PanelAuthed) {
            SetWindowPos(g.launchBtn, nullptr, S(28), S(180), w - S(56), S(46), SWP_NOZORDER);
        }

        InvalidateRect(hwnd, nullptr, TRUE);
    }

    void TakeString(LPVOID lp, std::wstring& out) {
        auto* s = (std::wstring*)lp;
        if (s) {
            out = std::move(*s);
            delete s;
        }
    }

    void Paint(HWND hwnd) {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        int w = rc.right;

        SetBkMode(dc, TRANSPARENT);

        // accent bar + wordmark
        RECT accent{S(28), S(30), S(34), S(66)};
        HBRUSH accentBrush = CreateSolidBrush(RGB(124, 92, 255));
        FillRect(dc, &accent, accentBrush);
        DeleteObject(accentBrush);

        SelectObject(dc, g.wordFont);
        SetTextColor(dc, RGB(235, 237, 240));
        RECT wordRect{S(44), S(26), w - S(28), S(70)};
        DrawTextW(dc, L"Envy", -1, &wordRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(dc, g.subFont);
        SetTextColor(dc, RGB(138, 145, 156));
        wchar_t sub[128];
        if (g.panel == PanelLogin) {
            wcscpy_s(sub, L"Sign in to launch Minecraft.");
        } else {
            _snwprintf_s(sub, _TRUNCATE, L"Signed in as %s", g.signedInAs.c_str());
        }
        RECT subRect{S(44), S(70), w - S(28), S(94)};
        DrawTextW(dc, sub, -1, &subRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        if (g.panel == PanelLogin && g.showLoginControls) {
            // divider with an "or" in the middle
            int y = S(178);
            RECT leftLine{S(28), y, w / 2 - S(24), y + 1};
            RECT rightLine{w / 2 + S(24), y, w - S(28), y + 1};
            HBRUSH lineBrush = CreateSolidBrush(RGB(52, 57, 66));
            FillRect(dc, &leftLine, lineBrush);
            FillRect(dc, &rightLine, lineBrush);
            DeleteObject(lineBrush);

            SetTextColor(dc, RGB(110, 116, 128));
            RECT orRect{w / 2 - S(24), y - S(10), w / 2 + S(24), y + S(12)};
            DrawTextW(dc, L"or", -1, &orRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(dc, g.captionFont);
            SetTextColor(dc, RGB(110, 116, 128));
            RECT capRect{S(28), S(194), w - S(28), S(212)};
            DrawTextW(dc, L"PRODUCT KEY", -1, &capRect, DT_LEFT | DT_SINGLELINE);

            // thin frame around the key field so it reads as an input on the dark bg
            RECT frame{S(28) - 1, S(216) - 1, w - S(28) + 1, S(216) + S(34) + 1};
            FrameRect(dc, &frame, (HBRUSH)GetStockObject(GRAY_BRUSH));
        }

        if (g.panel == PanelAuthed) {
            SelectObject(dc, g.subFont);
            SetTextColor(dc, RGB(138, 145, 156));
            RECT noteRect{S(28), S(110), w - S(28), S(134)};
            DrawTextW(dc, L"Minecraft starts by itself and the client goes in with it.", -1, &noteRect,
                      DT_LEFT | DT_NOPREFIX);
        }

        if (!g.statusText.empty()) {
            SelectObject(dc, g.statusFont);
            COLORREF c = g.statusColor == 1 ? RGB(88, 199, 110)
                         : g.statusColor == 2 ? RGB(232, 108, 108)
                                              : RGB(120, 126, 136);
            SetTextColor(dc, c);
            RECT statusRect{S(28), rc.bottom - S(34), w - S(28), rc.bottom - S(8)};
            DrawTextW(dc, g.statusText.c_str(), -1, &statusRect,
                      DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        EndPaint(hwnd, &ps);
    }

    void DrawButton(DRAWITEMSTRUCT* dis) {
        bool hot = g_hotButton == dis->CtlID && !(dis->itemState & ODS_DISABLED);
        bool down = (dis->itemState & ODS_SELECTED) != 0;

        COLORREF bg = down ? RGB(24, 26, 32) : hot ? RGB(41, 45, 54) : RGB(30, 33, 40);
        COLORREF border = hot ? RGB(124, 92, 255) : RGB(56, 60, 70);
        COLORREF text = (dis->itemState & ODS_DISABLED) ? RGB(110, 116, 128) : RGB(235, 237, 240);
        if (dis->itemState & ODS_DISABLED) bg = RGB(24, 26, 32);

        HBRUSH brush = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldBrush = SelectObject(dis->hDC, brush);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top, dis->rcItem.right, dis->rcItem.bottom,
                  S(14), S(14));
        SelectObject(dis->hDC, oldPen);
        SelectObject(dis->hDC, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);

        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, text);
        SelectObject(dis->hDC, g.btnFont);
        wchar_t label[128]{};
        GetWindowTextW(dis->hwndItem, label, 127);
        DrawTextW(dis->hDC, label, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    LRESULT CALLBACK ButtonSubclassProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id,
                                        DWORD_PTR) {
        switch (msg) {
        case WM_MOUSEMOVE:
            if (!g_trackedButton) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, h, 0};
                TrackMouseEvent(&tme);
                g_trackedButton = true;
            }
            if (g_hotButton != id) {
                g_hotButton = id;
                InvalidateRect(h, nullptr, TRUE);
            }
            break;
        case WM_MOUSELEAVE:
            g_trackedButton = false;
            if (g_hotButton == id) {
                g_hotButton = 0;
                InvalidateRect(h, nullptr, TRUE);
            }
            break;
        default:
            break;
        }
        return DefSubclassProc(h, msg, wp, lp);
    }

    void StartLaunch(HWND hwnd) {
        g.busy = true;
        EnableWindow(g.launchBtn, FALSE);
        g.statusColor = 0;
        g.statusText = L"Preparing the client...";
        InvalidateRect(hwnd, nullptr, TRUE);
        flow::StartLaunch(hwnd);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CREATE: {
            HINSTANCE inst = ((CREATESTRUCTW*)lp)->hInstance;
            CreateUiFonts();

            g.discordBtn = MakeButton(hwnd, inst, L"Sign in with Discord", IDC_DISCORD);
            g.keyEdit = CreateWindowExW(0, L"EDIT", L"",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0,
                                        10, 10, hwnd, (HMENU)(INT_PTR)IDC_KEYEDIT, inst, nullptr);
            SendMessageW(g.keyEdit, EM_SETLIMITTEXT, 64, 0);
            g.activateBtn = MakeButton(hwnd, inst, L"Activate", IDC_ACTIVATE);
            g.launchBtn = MakeButton(hwnd, inst, L"Launch Minecraft", IDC_LAUNCH);

            for (HWND b : {g.discordBtn, g.activateBtn, g.launchBtn}) {
                SetWindowSubclass(b, ButtonSubclassProc, (UINT_PTR)GetDlgCtrlID(b), 0);
            }

            // login controls stay hidden until the restore check says so
            ShowWindow(g.discordBtn, SW_HIDE);
            ShowWindow(g.keyEdit, SW_HIDE);
            ShowWindow(g.activateBtn, SW_HIDE);
            ShowWindow(g.launchBtn, SW_HIDE);
            return 0;
        }

        case WM_PAINT:
            Paint(hwnd);
            return 0;

        case WM_DRAWITEM:
            DrawButton((DRAWITEMSTRUCT*)lp);
            return TRUE;

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetBkColor(dc, RGB(24, 26, 32));
            SetTextColor(dc, RGB(235, 237, 240));
            static HBRUSH editBrush = CreateSolidBrush(RGB(24, 26, 32));
            return (LRESULT)editBrush;
        }

        case WM_COMMAND:
            if (HIWORD(wp) != BN_CLICKED || g.busy) break;
            switch (LOWORD(wp)) {
            case IDC_DISCORD: {
                g.busy = true;
                EnableWindow(g.discordBtn, FALSE);
                g.statusColor = 0;
                g.statusText = L"Waiting for Discord in your browser...";
                InvalidateRect(hwnd, nullptr, TRUE);
                flow::StartDiscordSignIn(hwnd);
                break;
            }
            case IDC_ACTIVATE: {
                wchar_t key[65]{};
                GetWindowTextW(g.keyEdit, key, 64);
                g.busy = true;
                EnableWindow(g.activateBtn, FALSE);
                flow::StartKeyCheck(hwnd, key);
                break;
            }
            case IDC_LAUNCH:
                StartLaunch(hwnd);
                break;
            default:
                break;
            }
            return 0;

        case WM_ENVY_STATUS: {
            TakeString((LPVOID)lp, g.statusText);
            g.statusColor = 0;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_ENVY_AUTH_OK: {
            TakeString((LPVOID)lp, g.signedInAs);
            g.busy = false;
            g.panel = PanelAuthed;
            EnableWindow(g.discordBtn, TRUE);
            EnableWindow(g.activateBtn, TRUE);
            g.statusColor = 0;
            g.statusText = L"Launching Minecraft...";
            ApplyLayout(hwnd);
            flow::StartLaunch(hwnd);
            return 0;
        }

        case WM_ENVY_AUTH_FAIL: {
            std::wstring error;
            TakeString((LPVOID)lp, error);
            g.busy = false;
            g.panel = PanelLogin;
            g.showLoginControls = true;
            EnableWindow(g.discordBtn, TRUE);
            EnableWindow(g.activateBtn, TRUE);
            if (error.empty()) {
                g.statusColor = 0;
                g.statusText = L"Sign in to continue.";
            } else {
                g.statusColor = 2;
                g.statusText = error;
            }
            ApplyLayout(hwnd);
            return 0;
        }

        case WM_ENVY_LAUNCH_OK:
            g.busy = false;
            g.statusColor = 1;
            g.statusText = L"Envy loaded. Have fun.";
            EnableWindow(g.launchBtn, TRUE);
            InvalidateRect(hwnd, nullptr, TRUE);
            SetTimer(hwnd, 1, 3000, nullptr);
            return 0;

        case WM_ENVY_LAUNCH_FAIL: {
            std::wstring error;
            TakeString((LPVOID)lp, error);
            g.busy = false;
            g.statusColor = 2;
            g.statusText = error;
            EnableWindow(g.launchBtn, TRUE);
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        }

        case WM_TIMER:
            if (wp == 1) {
                KillTimer(hwnd, 1);
                DestroyWindow(hwnd);
            }
            return 0;

        case WM_DESTROY:
            DestroyUiFonts();
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    EnableDpiAwareness();

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    HDC screen = GetDC(nullptr);
    g_dpi = (UINT)GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(17, 18, 22));
    wc.lpszClassName = L"EnvyLauncherWnd";
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    RECT rc{0, 0, S(424), S(352)};
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Envy", style,
                                (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                                (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h, nullptr, nullptr, inst,
                                nullptr);
    if (!hwnd) return 1;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    flow::StartRestore(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
