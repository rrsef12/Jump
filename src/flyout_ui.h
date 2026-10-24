// Location search flyout, inline preset flyout, and panel popup menu.
void closeFlyout(bool focusPanel) {
    auto popup = g->flyout;
    g->flyout = nullptr;
    if (popup)
        DestroyWindow(popup);
    if (focusPanel && IsWindowVisible(g->panel)) {
        SetForegroundWindow(g->panel);
        if (g->attached && g->dirtyName && g->localNamePending && !g->busy)
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    }
}
bool isFavoritePath(const std::wstring &path) {
    return std::any_of(g->cfg.favorites.begin(), g->cfg.favorites.end(),
                       [&](const auto &p) { return normalize(p.path) == normalize(path); });
}
void toggleFavoriteLocation(const Location &location) {
    auto previous = g->cfg;
    bool favorite = isFavoritePath(location.path);
    if (favorite)
        std::erase_if(g->cfg.favorites,
                      [&](const auto &p) { return normalize(p.path) == normalize(location.path); });
    else
        g->cfg.favorites.push_back({location.name, location.path, L"收藏"});
    if (!persist()) {
        g->cfg = std::move(previous);
        g->status = L"收藏未保存，请检查配置写入权限";
    } else
        g->status = favorite ? L"已取消收藏" : L"已收藏位置";
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
    if (g->flyout)
        InvalidateRect(GetDlgItem(g->flyout, ID_FOLDER_LIST), nullptr, FALSE);
}
void updateLocationResults();
bool removeHistoryLocation(const std::wstring &path = L"") {
    auto previous = g->cfg.history;
    if (path.empty())
        g->cfg.history.clear();
    else
        std::erase_if(g->cfg.history,
                      [&](const auto &entry) { return normalize(entry.path) == normalize(path); });
    if (g->cfg.history.size() == previous.size())
        return true;
    bool saved = persist();
    if (!saved) {
        g->cfg.history = std::move(previous);
        g->status = L"历史清理未保存，请检查配置写入权限。";
        g->statusError = true;
    } else {
        g->status =
            path.empty() ? L"历史位置已清空；收藏和最近保存保留。" : L"已从历史移除；收藏和保存记录保留。";
        g->statusError = false;
        if (path.empty()) {
            g->locationPage = 0;
            if (g->attached)
                g->lastHistoryDirectory = g->dialog.directory;
        }
        if (g->flyoutKind == State::Flyout::Locations && g->historyOnly) {
            std::erase_if(g->flyoutLocations, [&](const auto &entry) {
                return path.empty() || normalize(entry.path) == normalize(path);
            });
            if (g->flyout)
                updateLocationResults();
        }
    }
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
    return saved;
}
void removeRecentLocations(const std::wstring &path = L"") {
    auto previous = g->cfg.recentSaves;
    if (path.empty())
        g->cfg.recentSaves.clear();
    else
        std::erase_if(g->cfg.recentSaves,
                      [&](const auto &entry) { return normalize(entry.path) == normalize(path); });
    if (g->cfg.recentSaves.size() == previous.size())
        return;
    if (!persist()) {
        g->cfg.recentSaves = std::move(previous);
        g->status = L"最近记录清理未保存，请检查配置写入权限。";
        g->statusError = true;
    } else {
        g->status = path.empty() ? L"最近保存记录已清空" : L"已从最近保存移除";
        g->statusError = false;
        if (path.empty())
            g->locationPage = 0;
    }
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
}
HWND flyoutChild(const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y, int width,
                 int height) {
    bool edit = std::wstring(cls) == L"EDIT";
    auto h = createChild(g->flyout, cls, text, style, id, x + (edit ? 8 : 0), y + (edit ? 6 : 0),
                         width - (edit ? 16 : 0), height - (edit ? 12 : 0), g->font);
    if (std::wstring(cls) == L"BUTTON")
        SetWindowSubclass(h, PillProc, 1, 0);
    if (edit)
        SetWindowSubclass(h, InputFeedbackProc, 1, 0);
    return h;
}
LRESULT CALLBACK FlyoutEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        SendMessageW(GetParent(h), WM_COMMAND, wp == VK_RETURN ? IDOK : IDCANCEL, 0);
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE))
        return 0;
    if (msg == WM_KEYDOWN && wp == VK_DOWN && GetDlgCtrlID(h) == ID_FOLDER_SEARCH) {
        SetFocus(GetDlgItem(GetParent(h), ID_FOLDER_LIST));
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void createFlyout(State::Flyout kind, int width, int height, HWND anchor) {
    g->flyoutKind = kind;
    RECT r{};
    if (!IsWindow(anchor) || !IsChild(g->panel, anchor) || !GetWindowRect(anchor, &r) || IsRectEmpty(&r))
        GetWindowRect(g->panel, &r);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
    int x = std::clamp<int>(r.right - px(width), mi.rcWork.left,
                            std::max<int>(mi.rcWork.left, mi.rcWork.right - px(width)));
    int y = r.bottom + px(4);
    if (y + px(height) > mi.rcWork.bottom)
        y = std::max<int>(mi.rcWork.top, r.top - px(height) - px(4));
    y = std::clamp<int>(y, mi.rcWork.top, std::max<int>(mi.rcWork.top, mi.rcWork.bottom - px(height)));
    g->flyout = CreateWindowExW(WS_EX_TOOLWINDOW, L"Jump.Flyout", L"Jump · 快速选择",
                                WS_POPUP | WS_BORDER | WS_CLIPCHILDREN, x, y, px(width), px(height), g->panel,
                                nullptr, g->instance, nullptr);
}
void revealFlyout(int focusId) {
    if (!IsWindowVisible(g->panel))
        return;
    ShowWindow(g->flyout, SW_SHOW);
    SetForegroundWindow(g->flyout);
    SetFocus(GetDlgItem(g->flyout, focusId));
}
void updateLocationResults() {
    auto query = lower(trim(windowText(GetDlgItem(g->flyout, ID_FOLDER_SEARCH))));
    auto list = GetDlgItem(g->flyout, ID_FOLDER_LIST);
    std::vector<Location> filtered;
    for (const auto &p : g->flyoutLocations)
        if (query.empty() || lower(p.name + L" " + p.path).find(query) != std::wstring::npos)
            filtered.push_back(p);
    bool same = filtered.size() == g->filteredLocations.size() &&
                std::equal(filtered.begin(), filtered.end(), g->filteredLocations.begin(),
                           [](const Location &a, const Location &b) {
                               return a.name == b.name && a.path == b.path && a.sources == b.sources;
                           });
    if (same && SendMessageW(list, LB_GETCOUNT, 0, 0) > 0)
        return;
    int oldSelection = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    std::wstring selectedPath =
        oldSelection >= 0 && static_cast<size_t>(oldSelection) < g->filteredLocations.size()
            ? g->filteredLocations[oldSelection].path
            : g->dialog.directory;
    bool visible = IsWindowVisible(list) != FALSE;
    if (visible)
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    g->filteredLocations = std::move(filtered);
    int selected = 0;
    for (size_t i = 0; i < g->filteredLocations.size(); ++i) {
        const auto &p = g->filteredLocations[i];
        if (normalize(p.path) == normalize(selectedPath))
            selected = static_cast<int>(i);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    }
    if (g->filteredLocations.empty())
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"没有匹配的位置"));
    SendMessageW(list, LB_SETCURSEL, selected, 0);
    if (visible)
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, FALSE);
}
LRESULT CALLBACK LocationListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_CONTEXTMENU && g->historyOnly) {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int index = static_cast<int>(SendMessageW(h, LB_GETCURSEL, 0, 0));
        if (point.x == -1 && point.y == -1) {
            RECT row{};
            SendMessageW(h, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&row));
            point = {row.left, row.bottom};
            ClientToScreen(h, &point);
        } else {
            POINT client = point;
            ScreenToClient(h, &client);
            auto hit = SendMessageW(h, LB_ITEMFROMPOINT, 0, MAKELPARAM(client.x, client.y));
            if (HIWORD(hit))
                return 0;
            index = LOWORD(hit);
        }
        if (index < 0 || static_cast<size_t>(index) >= g->filteredLocations.size())
            return 0;
        auto location = g->filteredLocations[index];
        SendMessageW(h, LB_SETCURSEL, index, 0);
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, isFavoritePath(location.path) ? L"取消收藏" : L"收藏文件夹");
        AppendMenuW(menu, MF_STRING, 2, L"从历史移除");
        int selected = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, g->flyout);
        DestroyMenu(menu);
        if (selected == 1)
            toggleFavoriteLocation(location);
        else if (selected == 2)
            removeHistoryLocation(location.path);
        return 0;
    }
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN &&
            (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE || key->wParam == VK_SPACE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_SPACE)) {
        SendMessageW(g->flyout, WM_COMMAND,
                     wp == VK_ESCAPE  ? IDCANCEL
                     : wp == VK_SPACE ? ID_FOLDER_STAR
                                      : ID_FOLDER_SELECT,
                     0);
        return 0;
    }
    if (msg == WM_LBUTTONUP) {
        auto result = DefSubclassProc(h, msg, wp, lp);
        DWORD item = static_cast<DWORD>(SendMessageW(h, LB_ITEMFROMPOINT, 0, lp));
        if (!HIWORD(item)) {
            SendMessageW(h, LB_SETCURSEL, LOWORD(item), 0);
            RECT r{};
            GetClientRect(h, &r);
            SendMessageW(g->flyout, WM_COMMAND,
                         GET_X_LPARAM(lp) >= r.right - px(32) ? ID_FOLDER_STAR : ID_FOLDER_SELECT, 0);
        }
        return result;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void showLocationFlyout(bool historyOnly) {
    if (g->flyout) {
        closeFlyout(true);
        return;
    }
    g->historyOnly = historyOnly;
    g->flyoutLocations.clear();
    std::set<std::wstring> seen;
    auto add = [&](const Location &p) {
        if (!p.path.empty() && seen.insert(normalize(p.path)).second)
            g->flyoutLocations.push_back(p);
    };
    if (!historyOnly) {
        for (const auto &p : g->cfg.favorites)
            add(p);
        for (const auto &p : g->cfg.recentSaves)
            add(p);
        for (const auto &p : g->paths)
            add(p);
        for (const auto &p : g->openedLocations)
            add(p);
        for (const auto &p : g->cfg.history)
            add(p);
        add({L"当前位置", g->dialog.directory, L"当前"});
        const std::pair<const KNOWNFOLDERID *, const wchar_t *> common[] = {{&FOLDERID_Pictures, L"图片"},
                                                                            {&FOLDERID_Downloads, L"下载"},
                                                                            {&FOLDERID_Desktop, L"桌面"},
                                                                            {&FOLDERID_Documents, L"文档"}};
        for (const auto &[id, name] : common) {
            PWSTR path = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DONT_VERIFY, nullptr, &path))) {
                add({name, path, L"系统位置"});
                CoTaskMemFree(path);
            }
        }
    }
    if (historyOnly)
        for (const auto &p : g->cfg.history)
            add(p);
    int width = std::min(g->width - 32, 420);
    createFlyout(State::Flyout::Locations, width, 306, GetDlgItem(g->panel, ID_DIRECTORY));
    auto search =
        flyoutChild(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_FOLDER_SEARCH, 12, 12, width - 24, 32);
    SetWindowSubclass(search, FlyoutEditProc, 1, 0);
    SendMessageW(search, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(historyOnly ? L"搜索历史名称或路径…" : L"搜索文件夹名称或路径…"));
    auto list = flyoutChild(L"LISTBOX", L"",
                            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP |
                                LBS_NOINTEGRALHEIGHT,
                            ID_FOLDER_LIST, 4, 52, width - 8, 210);
    SendMessageW(list, LB_SETITEMHEIGHT, 0, px(42));
    SetWindowSubclass(list, LocationListProc, 1, 0);
    flyoutChild(L"BUTTON", L"管理位置…", BS_OWNERDRAW | WS_TABSTOP, ID_FLYOUT_MANAGE, 12, 270, width - 24,
                24);
    updateLocationResults();
    revealFlyout(ID_FOLDER_SEARCH);
}
void updatePresetChoices() {
    if (!g->flyout || g->flyoutKind != State::Flyout::Presets)
        return;
    g->flyoutActions.clear();
    RECT r{};
    GetClientRect(g->flyout, &r);
    int width = MulDiv(r.right, 96, g->dpi), cell = (width - 40) / 3;
    auto query = lower(trim(g->presetDraft));
    size_t total = g->namingMode == 1 ? g->cfg.prefixes.size() : g->cfg.suffixes.size();
    int shown = 0;
    for (size_t i = 0; i < total && shown < 6; ++i) {
        auto value = (g->namingMode == 1 ? g->cfg.prefixes : g->cfg.suffixes)[i];
        auto content = value + L" " + affixLabel(value);
        if (!query.empty() && lower(content).find(query) == std::wstring::npos)
            continue;
        int id = 2000 + shown;
        auto button = GetDlgItem(g->flyout, id);
        if (!button)
            button = flyoutChild(L"BUTTON", affixLabel(value), BS_OWNERDRAW | WS_TABSTOP, id,
                                 12 + (shown % 3) * (cell + 8), 76 + (shown / 3) * 32, cell, 24);
        else
            setTextIfChanged(button, affixLabel(value));
        showIfChanged(button, SW_SHOWNOACTIVATE);
        InvalidateRect(button, nullptr, FALSE);
        g->flyoutActions[id] = {g->namingMode == 1 ? Action::Prefix : Action::Suffix, i};
        ++shown;
    }
    for (int i = shown; i < 6; ++i)
        showIfChanged(GetDlgItem(g->flyout, 2000 + i), SW_HIDE);
    setTextIfChanged(GetDlgItem(g->flyout, ID_PRESET_HINT), !query.empty() && !shown
                                                                ? L"没有匹配的预设，可直接添加"
                                                                : L"搜索前后缀预设 · Enter 添加 / Esc 取消");
}
void showPresetFlyout(HWND anchor) {
    int width = std::min(g->width - 32, 420);
    // Anchor to the actual source control, never the removed legacy affix button.
    auto usable = [](HWND h) {
        return IsWindow(h) && IsChild(g->panel, h) && (GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE);
    };
    if (!usable(anchor))
        anchor = GetDlgItem(g->panel, g->namingMode == 1 ? ID_ADD_PREFIX : ID_ADD_SUFFIX);

    createFlyout(State::Flyout::Presets, width, 190, anchor);
    auto edit = flyoutChild(L"EDIT", g->presetDraft, ES_AUTOHSCROLL | WS_TABSTOP, ID_PRESET_VALUE, 12, 12,
                            width - 100, 32);
    SetWindowSubclass(edit, FlyoutEditProc, 1, 0);
    SendMessageW(edit, EM_SETLIMITTEXT, 80, 0);
    SendMessageW(edit, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(g->namingMode == 1 ? L"搜索或添加前缀…" : L"搜索或添加后缀…"));
    flyoutChild(L"BUTTON", g->presetIndex == NEW_PRESET ? L"添加" : L"保存", BS_OWNERDRAW | WS_TABSTOP,
                ID_PRESET_SAVE, width - 80, 12, 68, 32);
    auto hint =
        flyoutChild(L"STATIC", L"Enter 保存预设 · Esc 取消", SS_LEFT, ID_PRESET_HINT, 12, 50, width - 24, 20);
    SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
    updatePresetChoices();
    flyoutChild(L"BUTTON", g->namingMode == 1 ? L"管理前缀预设…" : L"管理后缀预设…",
                BS_OWNERDRAW | WS_TABSTOP, ID_FLYOUT_MANAGE, 12, 148, width - 24, 28);
    revealFlyout(ID_PRESET_VALUE);
    if (IsWindowVisible(g->flyout))
        SendMessageW(edit, EM_SETSEL, 0, -1);
}
LRESULT CALLBACK FlyoutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && reinterpret_cast<HWND>(lp) != g->panel)
            closeFlyout();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(hwnd, msg, wp);
        HDC dc = paint.dc();
        RECT r{};
        GetClientRect(hwnd, &r);
        FillRect(dc, &r, g->brush);
        int width = MulDiv(r.right, 96, g->dpi);
        RECT field{px(12), px(12), px(width - (g->flyoutKind == State::Flyout::Presets ? 88 : 12)), px(44)};
        paintInputFrame(dc, hwnd, field, px(8));
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->field);
        return reinterpret_cast<LRESULT>(g->inputBrush);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wp), g->muted);
        SetBkColor(reinterpret_cast<HDC>(wp), g->bg);
        return reinterpret_cast<LRESULT>(g->brush);
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (d->CtlID != ID_FOLDER_LIST)
            return SendMessageW(g->panel, msg, wp, lp);
        DRAWITEMSTRUCT item = *d;
        PaintBuffer buffer(item.hDC, item.rcItem);
        item.hDC = buffer.dc;
        d = &item;
        RECT r = d->rcItem;
        FillRect(d->hDC, &r, g->brush);
        if (d->itemID == static_cast<UINT>(-1))
            return TRUE;
        auto font = SelectObject(d->hDC, g->font);
        SetBkMode(d->hDC, TRANSPARENT);
        if (d->itemID >= g->filteredLocations.size()) {
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, L"没有匹配的位置", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            const auto &p = g->filteredLocations[d->itemID];
            if (d->itemState & ODS_SELECTED) {
                auto b = CreateSolidBrush(g->selection);
                FillRect(d->hDC, &r, b);
                DeleteObject(b);
            }
            RECT name{r.left + px(12), r.top + px(4), r.right - px(40), r.top + px(22)};
            bool recent =
                std::any_of(g->cfg.recentSaves.begin(), g->cfg.recentSaves.end(),
                            [&](const auto &saved) { return normalize(saved.path) == normalize(p.path); });
            auto caption = p.name + (recent ? L" · 最近" : L"");
            SetTextColor(d->hDC, g->fg);
            DrawTextW(d->hDC, caption.c_str(), -1, &name, DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(d->hDC, g->smallFont);
            name.top += px(18);
            name.bottom += px(18);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, p.path.c_str(), -1, &name, DT_SINGLELINE | DT_PATH_ELLIPSIS);
            RECT star{r.right - px(32), r.top, r.right, r.bottom};
            SelectObject(d->hDC, g->font);
            bool pinned = isFavoritePath(p.path);
            SetTextColor(d->hDC, pinned ? RGB(230, 184, 72) : g->muted);
            DrawTextW(d->hDC, pinned ? L"★" : L"☆", -1, &star, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(d->hDC, font);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == IDOK && code == BN_CLICKED) {
            int focused = GetDlgCtrlID(GetFocus());
            if (focused == ID_FLYOUT_MANAGE || g->flyoutActions.contains(focused))
                id = focused;
        }
        if (id == ID_FOLDER_SEARCH && code == EN_CHANGE) {
            updateLocationResults();
            return 0;
        }
        if (id == ID_PRESET_VALUE && code == EN_CHANGE) {
            g->presetDraft = windowText(GetDlgItem(hwnd, id));
            updatePresetChoices();
            return 0;
        }
        if (code != BN_CLICKED)
            return 0;
        if (id == IDCANCEL || id == ID_PRESET_CANCEL) {
            closeFlyout(true);
            return 0;
        }
        if (id == ID_PRESET_SAVE || (id == IDOK && g->flyoutKind == State::Flyout::Presets)) {
            saveInlinePreset();
            return 0;
        }
        if (id == ID_FLYOUT_MANAGE) {
            bool locations = g->flyoutKind == State::Flyout::Locations;
            size_t index = g->presetIndex == NEW_PRESET ? 0 : g->presetIndex;
            closeFlyout(true);
            if (locations)
                SendMessageW(g->panel, WM_COMMAND, ID_MANAGER, 0);
            else
                showPresetManager(false, index);
            return 0;
        }
        if (id == ID_FOLDER_SELECT || id == ID_FOLDER_STAR || id == IDOK) {
            int selected = static_cast<int>(SendDlgItemMessageW(hwnd, ID_FOLDER_LIST, LB_GETCURSEL, 0, 0));
            if (selected < 0 || static_cast<size_t>(selected) >= g->filteredLocations.size())
                return 0;
            auto location = g->filteredLocations[selected];
            if (id == ID_FOLDER_STAR)
                toggleFavoriteLocation(location);
            else {
                closeFlyout(true);
                invalidateNaming();
                if (g->numberMode || g->folderName || g->variablesEnabled)
                    g->stemName = composedName(&location.path);
                apply(location.path, g->stemName, false);
            }
            return 0;
        }
        auto found = g->flyoutActions.find(id);
        if (found != g->flyoutActions.end()) {
            auto action = found->second;
            closeFlyout(true);
            perform(action, false);
        }
        return 0;
    }
    case WM_CLOSE:
        closeFlyout(true);
        return 0;
    case WM_DESTROY:
        g->flyout = nullptr;
        g->flyoutKind = State::Flyout::None;
        g->presetEditor = false;
        g->flyoutActions.clear();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
HMENU buildPanelMenu(int id) {
    auto menu = CreatePopupMenu();
    if (id == ID_PANEL_OPTIONS) {
        AppendMenuW(menu, MF_STRING, ID_RECORDS, L"已核验的保存记录…");
        AppendMenuW(menu, MF_STRING, ID_EXPLAIN, L"查看名称组成…");
        AppendMenuW(menu, MF_STRING, ID_VARIABLE_EDIT, L"配置变量表达式…");
        AppendMenuW(menu, MF_STRING, ID_MANAGER, L"管理收藏位置…");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_BIND_CONTROL, L"指定文件名输入框…");
        AppendMenuW(menu, MF_STRING, ID_TRANSFER, L"迁移、备份与恢复…");
        AppendMenuW(menu, MF_STRING, ID_DIAGNOSTICS, L"当前接入与诊断…");
        AppendMenuW(menu, MF_STRING, ID_COPY_NAME, L"复制当前完整文件名");
        if (g->dialog.generic)
            AppendMenuW(menu, MF_STRING | (g->dialog.genericSubmit ? 0 : MF_GRAYED), ID_GENERIC_SUBMIT,
                        L"填入并调用原窗口保存");
        AppendMenuW(menu, MF_STRING, ID_PANEL_SETTINGS, L"选项…");
    }
    return menu;
}
