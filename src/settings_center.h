// The settings editor stays in the same top-level window, including nested
// option editors. Native file pickers and confirmation dialogs remain native.
void openSettingsPage(int page) {
    g->managerTab = std::clamp(page, 0, MANAGER_PAGES - 1);
    showManager();
}
void refreshSettingsPage() {
    if (g->manager && settingsDialogs.empty() && !managerDraftDirty())
        buildManagerPage();
}
bool requestSettingsNavigation(int page) {
    if (settingsDialogs.empty() || page == g->managerTab)
        return false;
    if (pendingSettingsClose || pendingSettingsPage >= 0)
        return true;
    bool dirty = std::any_of(settingsDialogs.begin(), settingsDialogs.end(),
                             [](const auto host) { return host->unsavedChanges; });
    if (dirty && MessageBoxW(g->manager, L"编辑内容尚未保存。放弃本次编辑并离开？\n选择“否”可继续编辑。",
                             L"未保存的编辑", MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return true;
    pendingSettingsClose = page < 0;
    pendingSettingsPage = page;
    finishSettingsDialog(settingsDialogs.back()->window, IDCANCEL);
    return true;
}
void scrollSettingsDialog(SettingsDialog &host, int position) {
    RECT client{};
    GetClientRect(host.window, &client);
    int next = std::clamp(position, 0, std::max<int>(0, host.contentHeight - client.bottom));
    if (next == host.scroll)
        return;
    ScrollWindowEx(host.window, 0, host.scroll - next, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    host.scroll = next;
    SetScrollPos(host.window, SB_VERT, next, TRUE);
    if (host.footerHeight)
        host.procedure(host.window, WM_SETTINGS_VIEWPORT_CHANGED, 0, 0);
}
bool layoutSettingsDialog(HWND window, int width, int height, int dpi) {
    auto host = settingsDialog(window);
    if (!host)
        return false;
    int content = MulDiv(height, dpi, 96);
    host->contentWidth = MulDiv(width, dpi, 96);
    host->contentHeight = content;
    resizeManager(std::max(430, height + 60));
    RECT client{};
    GetClientRect(g->manager, &client);
    int available = std::max(1L, client.bottom - px(56));
    int w = MulDiv(width, dpi, 96), h = std::min(content, available);
    host->contentHeight = content;
    SetWindowLongPtrW(window, GWL_STYLE,
                      GetWindowLongPtrW(window, GWL_STYLE) | (content > h ? WS_VSCROLL : 0));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMax = std::max(0, content - host->footerHeight - 1);
    info.nPage = std::max(1, h - host->footerHeight);
    info.nPos = 0;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
    if (content > h)
        w += GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);
    int availableWidth = std::max(1L, client.right - px(MANAGER_CONTENT_LEFT));
    w = std::min(w, availableWidth);
    int x = px(MANAGER_CONTENT_LEFT) + std::max(0, (availableWidth - w) / 2);
    SetWindowPos(window, nullptr, x, px(48), w, h, SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
    return true;
}
INT_PTR CALLBACK settingsDialogProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto host = settingsDialog(window);
    if (message == WM_INITDIALOG) {
        host = reinterpret_cast<SettingsDialog *>(lp);
        host->window = window;
        SetPropW(window, SETTINGS_DIALOG_PROPERTY, host);
        auto result = host->procedure(window, message, wp, host->data);
        if (host->procedure == formProcedure && !reinterpret_cast<FormData *>(host->data)->compact) {
            for (auto child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
                SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
                if (windowClass(child) == L"Button") {
                    SetWindowLongPtrW(child, GWL_STYLE,
                                      (GetWindowLongPtrW(child, GWL_STYLE) & ~BS_TYPEMASK) | BS_OWNERDRAW);
                    SetWindowSubclass(child, compactFormControl, 1, 0);
                } else if (windowClass(child) == L"Edit") {
                    SetWindowLongPtrW(child, GWL_EXSTYLE,
                                      GetWindowLongPtrW(child, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
                    SetWindowPos(child, nullptr, 0, 0, 0, 0,
                                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
                }
            }
        }
        return result;
    }
    if (!host)
        return FALSE;
    if (message == WM_MOUSEWHEEL || message == WM_VSCROLL) {
        int position = host->scroll, step = px(28);
        if (message == WM_MOUSEWHEEL)
            position -= GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * step * 3;
        else {
            SCROLLINFO info{sizeof(info), SIF_ALL};
            GetScrollInfo(window, SB_VERT, &info);
            switch (LOWORD(wp)) {
            case SB_LINEUP:
                position -= step;
                break;
            case SB_LINEDOWN:
                position += step;
                break;
            case SB_PAGEUP:
                position -= info.nPage;
                break;
            case SB_PAGEDOWN:
                position += info.nPage;
                break;
            case SB_THUMBTRACK:
                position = info.nTrackPos;
                break;
            case SB_TOP:
                position = 0;
                break;
            case SB_BOTTOM:
                position = host->contentHeight;
                break;
            }
        }
        scrollSettingsDialog(*host, position);
        return TRUE;
    }
    bool form = host->procedure == formProcedure && !reinterpret_cast<FormData *>(host->data)->compact;
    if (form) {
        if (message == WM_CTLCOLORDLG || message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
            bool input = message == WM_CTLCOLOREDIT;
            SetTextColor(reinterpret_cast<HDC>(wp), input ? g->fg : g->muted);
            SetBkColor(reinterpret_cast<HDC>(wp), input ? g->field : g->bg);
            return reinterpret_cast<INT_PTR>(input ? g->inputBrush : g->brush);
        }
        if (message == WM_DRAWITEM) {
            auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
            if (item.CtlType == ODT_BUTTON) {
                CompactFormStyle style{g->smallFont,     g->inputBrush, g->field,   g->muted,
                                       g->line,          g->accent,     g->primary, g->surface,
                                       paintRoundedRect, styleDropdown};
                auto theme = workflowForm(L"", L"", {});
                theme.compact = &style;
                theme.dpi = g->dpi;
                drawCompactFormButton(item, theme, item.CtlID == IDOK);
                return TRUE;
            }
        }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{};
            auto dc = BeginPaint(window, &paint);
            FillRect(dc, &paint.rcPaint, g->brush);
            for (auto child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
                if (windowClass(child) == L"Edit") {
                    RECT r{};
                    GetWindowRect(child, &r);
                    MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
                    InflateRect(&r, px(3), px(3));
                    paintInputFrame(dc, window, r, px(6));
                }
            EndPaint(window, &paint);
            return TRUE;
        }
    }
    if (message == WM_NCDESTROY) {
        host->done = true;
        host->window = nullptr;
        RemovePropW(window, SETTINGS_DIALOG_PROPERTY);
        return FALSE;
    }
    return host->procedure(window, message, wp, lp);
}
INT_PTR hostSettingsDialog(HWND, DLGPROC procedure, LPARAM data, const wchar_t *title) {
    if (settingsDialogs.empty()) {
        if (g->manager && managerDraftDirty()) {
            if (!discardManagerDraft())
                return IDCANCEL;
            if (g->managerTab == MANAGER_TEMPLATES)
                g->variableEditor = false;
            buildManagerPage();
        }
        if (procedure == namingDesignerProcedure)
            openSettingsPage(0);
        else
            showManager();
    }
    if (!g->manager)
        return IDCANCEL;
    RECT shell{};
    GetWindowRect(g->manager, &shell);
    std::vector<HWND> hidden;
    for (auto child = GetWindow(g->manager, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        if (GetDlgCtrlID(child) != M_TAB && IsWindowVisible(child)) {
            hidden.push_back(child);
            ShowWindow(child, SW_HIDE);
        }
    HWND previousFocus = GetFocus();
    SettingsDialog host;
    host.procedure = procedure;
    host.data = data;
    settingsDialogs.push_back(&host);
    struct Template {
        DLGTEMPLATE dialog;
        WORD menu, windowClass, title;
    } layout{};
    layout.dialog.style = WS_CHILD | DS_CONTROL | WS_CLIPCHILDREN;
    layout.dialog.dwExtendedStyle = WS_EX_CONTROLPARENT;
    auto window = CreateDialogIndirectParamW(g->instance, &layout.dialog, g->manager, settingsDialogProcedure,
                                             reinterpret_cast<LPARAM>(&host));
    if (window) {
        SetWindowTheme(window, L"DarkMode_Explorer", nullptr);
        if (windowText(window).empty())
            SetWindowTextW(window, title);
        ShowWindow(window, SW_SHOW);
        SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        InvalidateRect(g->manager, nullptr, TRUE);
        auto initial = host.initialFocus ? GetDlgItem(window, host.initialFocus) : nullptr;
        SetFocus(initial ? initial : GetNextDlgTabItem(window, nullptr, FALSE));
        MSG message{};
        while (!host.done && IsWindow(g->manager)) {
            int result = GetMessageW(&message, nullptr, 0, 0);
            if (result <= 0) {
                if (!result)
                    PostQuitMessage(static_cast<int>(message.wParam));
                break;
            }
            if (handleAppShortcut(message))
                continue;
            if (!IsDialogMessageW(window, &message) && !IsDialogMessageW(g->manager, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (message.message == WM_KEYDOWN && message.wParam == VK_TAB && IsChild(window, GetFocus())) {
                RECT r{}, client = settingsBodyRect(window);
                GetWindowRect(GetFocus(), &r);
                MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
                int id = GetDlgCtrlID(GetFocus());
                if (host.footerControl && host.footerControl(id))
                    continue;
                if (r.top < 0)
                    scrollSettingsDialog(host, host.scroll + r.top - px(8));
                else if (r.bottom > client.bottom)
                    scrollSettingsDialog(host, host.scroll + r.bottom - client.bottom + px(8));
            }
        }
        if (IsWindow(window))
            DestroyWindow(window);
    }
    settingsDialogs.pop_back();
    if (IsWindow(g->manager)) {
        for (auto child : hidden)
            if (IsWindow(child))
                ShowWindow(child, SW_SHOW);
        SetWindowPos(g->manager, nullptr, 0, 0, shell.right - shell.left, shell.bottom - shell.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        positionManager(false);
        if (!settingsDialogs.empty()) {
            auto parent = settingsDialogs.back();
            if (pendingSettingsClose || pendingSettingsPage >= 0)
                finishSettingsDialog(parent->window, IDCANCEL);
        } else {
            if (pendingSettingsClose || pendingSettingsPage >= 0)
                PostMessageW(g->manager, WM_SETTINGS_NAVIGATE, 0, 0);
        }
        InvalidateRect(g->manager, nullptr, TRUE);
        if (IsWindow(previousFocus) && IsWindowVisible(previousFocus))
            SetFocus(previousFocus);
    }
    return host.result;
}
