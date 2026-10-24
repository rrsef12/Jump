// Settings center (Manager) window procedure, page builders, and shortcuts.
LRESULT CALLBACK ManagerCheckProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    DWORD_PTR original = feedback;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000)))
        feedback |= 2;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS)
        feedback &= ~DWORD_PTR(2);
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp);
        auto dc = paint.dc();
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, g->brush);
        bool checked = SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
        bool enabled = IsWindowEnabled(h) != FALSE;
        bool hover = enabled && (feedback & 1);
        bool focus = enabled && (feedback & 2) && GetFocus() == h;
        auto fill = CreateSolidBrush(controlFill(g->field, checked, hover, false, false, enabled));
        auto pen = CreatePen(PS_SOLID, 1,
                             focus     ? g->accent
                             : checked ? mixColor(g->selection, g->accent, 50)
                             : hover   ? mixColor(g->line, g->muted, 35)
                                       : RGB(52, 64, 82));
        auto ob = SelectObject(dc, fill), op = SelectObject(dc, pen);
        int top = (r.bottom - px(14)) / 2;
        paintRoundedRect(dc, 0, top, px(14), top + px(14), px(5), px(5));
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen);
        DeleteObject(fill);
        if (checked)
            drawUiIcon(dc, UiIcon::Check, 0, static_cast<float>(top), static_cast<float>(px(14)),
                       enabled ? g->fg : g->muted);
        auto font = SelectObject(dc, g->font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, enabled ? g->fg : g->muted);
        r.left += px(22);
        auto text = windowText(h);
        DrawTextW(dc, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    auto check = msg == BM_SETCHECK ? SendMessageW(h, BM_GETCHECK, 0, 0) : 0;
    auto result = DefSubclassProc(h, msg, wp, lp);
    bool changed = false;
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
        feedback |= 1;
        changed = true;
    }
    if (msg == WM_MOUSELEAVE && (feedback & 1)) {
        feedback &= ~DWORD_PTR(1);
        changed = true;
    }
    if (changed || original != feedback)
        SetWindowSubclass(h, ManagerCheckProc, 1, feedback);
    if ((msg == BM_SETCHECK && check != static_cast<LRESULT>(wp)) || original != feedback || changed)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
LRESULT CALLBACK ManagerTabProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR hovered) {
    DWORD_PTR previous = hovered;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000)))
        hovered |= 256;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS)
        hovered &= ~DWORD_PTR(256);
    if (previous != hovered) {
        SetWindowSubclass(h, ManagerTabProc, 1, hovered);
        InvalidateRect(h, nullptr, FALSE);
    }
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp);
        auto dc = paint.dc();
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, g->brush);
        auto font = SelectObject(dc, g->font);
        SetBkMode(dc, TRANSPARENT);
        int selected = TabCtrl_GetCurSel(h);
        for (int i = 0; i < TabCtrl_GetItemCount(h); ++i) {
            RECT item{};
            TabCtrl_GetItemRect(h, i, &item);
            bool focus = i == selected && (hovered & 256) && GetFocus() == h;
            COLORREF color =
                controlFill(g->bg, i == selected, (hovered & 255) == static_cast<DWORD_PTR>(i + 1), false,
                            false, IsWindowEnabled(h));
            auto fill = CreateSolidBrush(color);
            auto pen = CreatePen(PS_SOLID, 1, focus ? g->accent : color);
            auto ob = SelectObject(dc, fill), op = SelectObject(dc, pen);
            paintRoundedRect(dc, item.left, item.top, item.right, item.bottom, px(8), px(8));
            SelectObject(dc, op);
            SelectObject(dc, ob);
            DeleteObject(pen);
            DeleteObject(fill);
            wchar_t text[80]{};
            TCITEMW value{};
            value.mask = TCIF_TEXT;
            value.pszText = text;
            value.cchTextMax = 80;
            bool advanced =
                i == 2 || i == MANAGER_TEMPLATES || i == MANAGER_BACKUP || i == MANAGER_CONNECTIONS;
            TabCtrl_GetItem(h, i, &value);
            SetTextColor(dc, i == selected || !advanced ? g->fg : g->muted);
            item.left += px(12);
            if (advanced) {
                RECT badge = item;
                badge.left = badge.right - px(30);
                badge.right -= px(4);
                auto previousFont = SelectObject(dc, g->smallFont);
                SetTextColor(dc, g->muted);
                DrawTextW(dc, L"高级", -1, &badge, DT_SINGLELINE | DT_RIGHT | DT_VCENTER);
                SelectObject(dc, previousFont);
                SetTextColor(dc, i == selected ? g->fg : g->muted);
                item.right -= px(31);
            }
            DrawTextW(dc, text, -1, &item, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        }
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    auto result = DefSubclassProc(h, msg, wp, lp);
    bool changed = false;
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        TCHITTESTINFO hit{};
        hit.pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        DWORD_PTR next = msg == WM_MOUSELEAVE ? 0 : static_cast<DWORD_PTR>(TabCtrl_HitTest(h, &hit) + 1);
        if (next != (hovered & 255)) {
            SetWindowSubclass(h, ManagerTabProc, 1, (hovered & ~DWORD_PTR(255)) | next);
            changed = true;
            if (msg == WM_MOUSEMOVE && !(hovered & 255)) {
                TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
                TrackMouseEvent(&tracking);
            }
        }
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_KEYDOWN || changed || msg == TCM_SETCURSEL)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
void resizeManager(int height) {
    RECT frame{0, 0, px(MANAGER_SHELL_WIDTH), px(std::max(430, height))};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(GetWindowLongPtrW(g->manager, GWL_STYLE)), FALSE,
                             WS_EX_APPWINDOW, g->dpi);
    RECT current{};
    GetWindowRect(g->manager, &current);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(g->manager, MONITOR_DEFAULTTONEAREST), &monitor);
    int width = frame.right - frame.left, total = frame.bottom - frame.top;
    total = std::min<int>(total, monitor.rcWork.bottom - monitor.rcWork.top);
    int left = std::clamp<int>(current.left, monitor.rcWork.left,
                               std::max<int>(monitor.rcWork.left, monitor.rcWork.right - width));
    int top = std::clamp<int>(current.top, monitor.rcWork.top,
                              std::max<int>(monitor.rcWork.top, monitor.rcWork.bottom - total));
    if (current.left != left || current.top != top || current.right - current.left != width ||
        current.bottom - current.top != total)
        SetWindowPos(g->manager, nullptr, left, top, width, total, SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(g->manager, nullptr, TRUE);
}
bool managingAffixes() {
    return g->managerTab == MANAGER_AFFIXES;
}

bool managingPrefixes() {
    return g->managerTab == MANAGER_AFFIXES && g->affixCategory == 0;
}
std::vector<std::wstring> &managerAffixes() {
    return managingPrefixes() ? g->cfg.prefixes : g->cfg.suffixes;
}
void updateAffixManagerPreview() {
    if (!g->manager || !managingAffixes())
        return;
    auto value = windowText(GetDlgItem(g->manager, M_VALUE));
    bool prefix = managingPrefixes();
    bool valid = validAffix(value, prefix);
    auto preview = valid ? L"示例：" + (prefix ? expandAffix(value, g->cfg.dateFormat) + L"文件名"
                                               : L"文件名" + expandAffix(value, g->cfg.dateFormat))
                         : L"请输入有效内容，不能包含路径字符。";
    SetDlgItemTextW(g->manager, M_AFFIX_PREVIEW, preview.c_str());
    const auto &values = managerAffixes();
    EnableWindow(GetDlgItem(g->manager, M_SAVE), valid);
    EnableWindow(GetDlgItem(g->manager, M_DELETE), !g->addingAffix && g->editing < values.size());
    EnableWindow(GetDlgItem(g->manager, M_UP),
                 !g->addingAffix && g->editing > 0 && g->editing < values.size());
}
UINT dragMessage = 0;
HWND managerChild(HWND parent, const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x,
                  int y, int w, int h) {
    if (parent == g->manager && id != M_TAB)
        x += MANAGER_CONTENT_LEFT + 60;
    const std::wstring kind(cls);
    bool combo = kind == L"COMBOBOX", edit = kind == L"EDIT", list = kind == L"LISTBOX";
    bool button = kind == L"BUTTON", check = button && (style & BS_TYPEMASK) == BS_AUTOCHECKBOX;
    if (combo)
        style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    if (button && !check)
        style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
    if (edit || list) {
        g->managerFields.push_back({px(x), px(y), px(x + w), px(y + h)});
        style &= ~WS_BORDER;
        if (list)
            style |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT;
        x += edit ? 8 : 3;
        y += 5;
        w -= edit ? 16 : 6;
        h -= 10;
    }
    auto hWnd =
        createChild(parent, cls, text, style, id, x, y, w, h, kind == L"STATIC" ? g->smallFont : g->font);
    if (list)
        SendMessageW(hWnd, LB_SETITEMHEIGHT, 0, px(26));
    if (edit || list)
        SetWindowSubclass(hWnd, InputFeedbackProc, 1, 0);
    if (button)
        SetWindowSubclass(hWnd, check ? ManagerCheckProc : PillProc, 1, 0);
    if (kind == WC_TABCONTROLW) {
        TabCtrl_SetPadding(hWnd, 0, 0);
        TabCtrl_SetItemSize(hWnd, px(120), px(32));
        SetWindowSubclass(hWnd, ManagerTabProc, 1, 0);
    }
    if (combo) {
        SendMessageW(hWnd, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), px(24));
        SendMessageW(hWnd, CB_SETITEMHEIGHT, 0, px(30));
        styleDropdown(hWnd);
    }
    return hWnd;
}
void managerLabel(HWND p, const std::wstring &text, int x, int y, int w = 336) {
    managerChild(p, L"STATIC", text, SS_ENDELLIPSIS, 0, x, y, w, 18);
}
std::wstring pickFolder(HWND owner) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    if (FAILED(dialog->Show(owner)))
        return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item)))
        return {};
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return {};
    std::wstring result = path;
    CoTaskMemFree(path);
    return result;
}
std::filesystem::path startupLink() {
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &path))) {
        result = std::filesystem::path(path) / L"Jump.lnk";
        CoTaskMemFree(path);
    }
    return result;
}
bool enableStartup(bool enable) {
    auto link = startupLink();
    if (link.empty())
        return false;
    if (!enable)
        return !std::filesystem::exists(link) || DeleteFileW(link.c_str()) != 0;
    wchar_t exe[32768]{};
    auto length = GetModuleFileNameW(nullptr, exe, 32768);
    if (!length || length >= 32768)
        return false;
    ComPtr<IShellLinkW> shortcut;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut))))
        return false;
    if (FAILED(shortcut->SetPath(exe)) ||
        FAILED(shortcut->SetWorkingDirectory(std::filesystem::path(exe).parent_path().c_str())) ||
        FAILED(shortcut->SetDescription(L"Jump 后台保存助手")) || FAILED(shortcut->SetShowCmd(SW_HIDE)))
        return false;
    ComPtr<IPersistFile> file;
    shortcut.As(&file);
    auto temporary = link.parent_path() / (L"Jump-" + std::to_wstring(GetCurrentProcessId()) + L".tmp.lnk");
    bool saved =
        file && SUCCEEDED(file->Save(temporary.c_str(), TRUE)) &&
        MoveFileExW(temporary.c_str(), link.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!saved)
        DeleteFileW(temporary.c_str());
    return saved;
}
#include "settings_draft.h"
StructuredNaming managedNamingScheme(size_t row) {
    auto index = SendDlgItemMessageW(g->manager, M_LIST, LB_GETITEMDATA, row, 0);
    const auto &schemes = namingSchemeCatalog(g->cfg);
    return index >= 0 && static_cast<size_t>(index) < schemes.size() ? schemes[index] : StructuredNaming{};
}
size_t namingRecipeIndex(const std::wstring &id) {
    const auto &indices = namingCatalog(g->cfg).recipeIndices;
    auto found = indices.find(id);
    return found == indices.end() ? g->cfg.recipes.size() : found->second;
}
void managerSelect() {
    if (!g->manager)
        return;
    g->managerLoading = true;
    for (int id : {M_NAME, M_VALUE})
        SetDlgItemTextW(g->manager, id, L"");
    auto &cfg = g->cfg;
    int selected = static_cast<int>(SendDlgItemMessageW(g->manager, M_LIST, LB_GETCURSEL, 0, 0));
    if (selected < 0)
        selected = 0;
    g->addingAffix = false;
    g->editing = selected;
    if (g->managerTab == 1) {
        if (g->editing < cfg.favorites.size()) {
            auto &l = cfg.favorites[g->editing];
            SetDlgItemTextW(g->manager, M_NAME, l.name.c_str());
            SetDlgItemTextW(g->manager, M_VALUE, l.path.c_str());
        }
    } else if (g->managerTab == 2) {
        auto index = SendDlgItemMessageW(g->manager, M_LIST, LB_GETITEMDATA, g->editing, 0);
        if (index >= 0 && static_cast<size_t>(index) < cfg.recipes.size()) {
            auto &r = cfg.recipes[index];
            SetDlgItemTextW(g->manager, M_NAME, r.name.c_str());
            SetDlgItemTextW(g->manager, M_VALUE, r.directory.c_str());
        }
    } else if (managingAffixes()) {
        const auto &values = managerAffixes();
        if (g->editing < values.size())
            SetDlgItemTextW(g->manager, M_VALUE, values[g->editing].c_str());
    }
    g->managerLoading = false;
    if (g->managerTab == 0) {
        auto scheme = managedNamingScheme(g->editing);
        bool hasScheme = scheme.enabled;
        bool saved = namingRecipeIndex(scheme.id) < cfg.recipes.size();
        bool canMove = saved && g->editing > 0 &&
                       namingRecipeIndex(managedNamingScheme(g->editing - 1).id) < cfg.recipes.size();
        for (int id : {M_SAVE, M_COPY_SCHEME, M_DELETE, M_UP, M_APPLY_SCHEME, M_SCHEME_DEFAULT})
            EnableWindow(GetDlgItem(g->manager, id),
                         hasScheme && (id != M_APPLY_SCHEME || g->attached && g->dialog.save) &&
                             (id != M_DELETE || saved) && (id != M_UP || canMove));
        SetDlgItemTextW(g->manager, M_DELETE, builtinNamingScheme(scheme.id) ? L"还原初始规则" : L"删除方案");
        std::wstring detail;
        if (hasScheme) {
            std::wstring fields;
            for (const auto &field : scheme.fields) {
                if (!fields.empty())
                    fields += L" · ";
                fields += field.label;
            }
            auto sample = composeStructure(namingSession(scheme, L"Asset"), cfg);
            if (sample.error.empty() && scheme.counter)
                sample.name = numberingExample(sample.name, {scheme.counter, false, L"", L"",
                                                             scheme.dateFormat, scheme.digits, scheme.upper});
            detail = namingDefaultsSummary(scheme.id) + L"\r\n命名部分：" + fields + L"\r\n" +
                     (sample.error.empty() ? L"示例：" + sample.name : L"使用时填写 · " + sample.error);
        }
        if (hasScheme)
            detail += g->attached && g->dialog.save ? L"\r\n使用方案会填入当前保存窗口。"
                                                    : L"\r\n“使用方案”需先在原软件中打开保存窗口。";
        SetDlgItemTextW(g->manager, M_INFO, detail.c_str());
    }
    if (g->managerTab == 1 || g->managerTab == 2) {
        auto count = SendDlgItemMessageW(g->manager, M_LIST, LB_GETCOUNT, 0, 0);
        EnableWindow(GetDlgItem(g->manager, M_DELETE), count > 0);
        EnableWindow(GetDlgItem(g->manager, M_UP), count > 0 && g->editing > 0);
    }
    updateAffixManagerPreview();
    if (g->managerTab != MANAGER_TEMPLATES)
        rememberManagerDraft();
}
void managerList() {
    auto list = GetDlgItem(g->manager, M_LIST);
    if (!list)
        return;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    if (g->managerTab == 1) {
        for (auto &p : g->cfg.favorites)
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    } else if (g->managerTab == 0) {
        const auto &schemes = namingSchemeCatalog(g->cfg);
        for (size_t i = 0; i < schemes.size(); ++i) {
            auto row = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(schemes[i].name.c_str()));
            SendMessageW(list, LB_SETITEMDATA, row, i);
        }
    } else if (g->managerTab == 2) {
        for (size_t i = 0; i < g->cfg.recipes.size(); ++i) {
            const auto &recipe = g->cfg.recipes[i];
            if (recipe.namingOnly)
                continue;
            auto row = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(recipe.name.c_str()));
            SendMessageW(list, LB_SETITEMDATA, row, i);
        }
    } else if (managingAffixes()) {
        for (const auto &value : managerAffixes()) {
            auto name = affixLabel(value);
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
    }
    auto count = static_cast<size_t>(SendMessageW(list, LB_GETCOUNT, 0, 0));
    g->editing = count ? std::min(g->editing, count - 1) : 0;
    SendMessageW(list, LB_SETCURSEL, g->editing, 0);
    managerSelect();
}
int buildTemplateManager(HWND parent) {
    initializeVariableDraft();
    int y = 52, available = MANAGER_WIDTH - 32, cell = (available - 16) / 3;
    auto make = [&](const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int top,
                    int width,
                    int height) { return managerChild(parent, cls, text, style, id, x, top, width, height); };
    auto caption = [&](int x, int top, int width, const std::wstring &text) {
        managerLabel(parent, text, x, top, width);
    };
    caption(16, y, available, L"点选命名部分组合 · 再点移除，新项加在末尾");
    y += 26;
    g->variableTokens = {L"项目", L"name", L"作者", L"folder", L"date"};
    auto selectedKeys = expressionKeys(g->variableTemplateDraft);
    for (const auto &key : selectedKeys)
        if (std::find(g->variableTokens.begin(), g->variableTokens.end(), key) == g->variableTokens.end() &&
            g->variableTokens.size() < 9)
            g->variableTokens.push_back(key);
    for (const auto &[key, value] : variableValues(g->variableValuesDraft))
        if (!key.empty() && !builtInVariable(variableKey(key)) &&
            std::find(g->variableTokens.begin(), g->variableTokens.end(), key) == g->variableTokens.end() &&
            g->variableTokens.size() < 9)
            g->variableTokens.push_back(key);
    for (size_t i = 0; i < g->variableTokens.size(); ++i) {
        const auto &key = g->variableTokens[i];
        auto button =
            make(L"BUTTON", variableCaption(key), WS_TABSTOP, ID_VARIABLE_TOKEN_FIRST + static_cast<int>(i),
                 16 + static_cast<int>(i % 3) * (cell + 8), y + static_cast<int>(i / 3) * 32, cell, 26);
        tip(button, L"点击加入组合，再次点击移除：" + variableCaption(key));
    }
    y += static_cast<int>((g->variableTokens.size() + 2) / 3) * 32;
    g->variableInputKeys.clear();
    if (!g->variableAdvanced) {
        auto values = variableValues(g->variableValuesDraft);
        for (const auto &key : selectedKeys) {
            if (builtInVariable(key) || g->variableInputKeys.size() >= 4)
                continue;
            int i = static_cast<int>(g->variableInputKeys.size()),
                left = 16 + i % 2 * ((available - 8) / 2 + 8);
            int top = y + i / 2 * 54, width = (available - 8) / 2;
            caption(left, top, width, key);
            auto edit = make(L"EDIT", values[key], ES_AUTOHSCROLL | WS_TABSTOP, ID_VARIABLE_FIELD_FIRST + i,
                             left, top + 22, width, 28);
            SetWindowSubclass(edit, VariableEditProc, 1, 0);
            SendMessageW(edit, EM_SETLIMITTEXT, 120, 0);
            auto cue = L"填写" + key;
            SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue.c_str()));
            if (key == L"项目")
                tip(edit, L"首次默认带入当前文件夹名，可修改；保存后复用");
            if (key == L"作者")
                tip(edit, L"首次默认带入系统用户名，可修改；保存后复用");
            g->variableInputKeys.push_back(key);
        }
        y += static_cast<int>((g->variableInputKeys.size() + 1) / 2) * 54;
    }
    make(L"BUTTON", g->variableAdvanced ? L"收起表达式" : L"编辑表达式…", WS_TABSTOP, ID_VARIABLE_ADVANCED,
         16, y, 100, 24);
    y += 28;
    if (g->variableAdvanced) {
        auto expression = make(L"EDIT", g->variableTemplateDraft, ES_AUTOHSCROLL | WS_TABSTOP,
                               ID_VARIABLE_TEMPLATE, 16, y, available, 28);
        SetWindowSubclass(expression, VariableEditProc, 1, 0);
        SendMessageW(expression, EM_SETLIMITTEXT, 160, 0);
        y += 32;
        auto values = make(L"EDIT", g->variableValuesDraft,
                           ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
                           ID_VARIABLE_VALUES, 16, y, available, 62);
        SetWindowSubclass(values, VariableEditProc, 1, 0);
        SendMessageW(values, EM_SETLIMITTEXT, 1200, 0);
        y += 66;
    }
    make(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, ID_VARIABLE_HINT, 16, y, available, 22);
    y += 26;
    make(L"BUTTON", L"撤销修改", WS_TABSTOP, ID_VARIABLE_CANCEL, 16, y, 80, 26);
    make(L"BUTTON", L"保存模板", WS_TABSTOP, ID_VARIABLE_SAVE, MANAGER_WIDTH - 108, y, 92, 26);
    y += 34;
    updateVariableDraftPreview();
    return std::max(MANAGER_HEIGHT, y + 16);
}
#include "settings_pages.h"
void buildManagerPage(bool preserveDraft) {
    HWND parent = g->manager;
    bool visible = IsWindowVisible(parent) != FALSE;
    if (visible)
        SendMessageW(parent, WM_SETREDRAW, FALSE, 0);
    std::vector<HWND> children;
    EnumChildWindows(
        parent,
        [](HWND h, LPARAM data) -> BOOL {
            if (GetDlgCtrlID(h) != M_TAB && GetParent(h) == g->manager)
                reinterpret_cast<std::vector<HWND> *>(data)->push_back(h);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&children));
    for (auto h : children)
        DestroyWindow(h);
    g->managerFields.clear();
    g->managerLoading = true;
    g->addingAffix = false;
    constexpr int left = 12, listWidth = 148, x = 172, width = 336;
    auto button = [&](const wchar_t *text, int id, int bx, int y, int w) {
        return managerChild(parent, L"BUTTON", text, BS_PUSHBUTTON | WS_TABSTOP, id, bx, y, w, 28);
    };
    auto field = [&](const wchar_t *title, int id, int y) {
        managerLabel(parent, title, x, y, width);
        return managerChild(parent, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, id, x, y + 20, width, 28);
    };
    auto actions = [&](int y) {
        button(L"保存修改", M_SAVE, x, y, 96);
        if (g->managerTab != 2)
            button(L"新增", M_ADD, x + 104, y, 72);
        button(L"移除", M_DELETE, x + 184, y, 72);
        button(L"上移", M_UP, x + 264, y, 72);
    };
    auto makeList = [&](int y, int bottom, bool draggable) {
        auto list = managerChild(parent, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, M_LIST, left,
                                 y, listWidth, bottom - y);
        if (draggable)
            MakeDragList(list);
    };
    int height = MANAGER_HEIGHT;
    if (g->managerTab == MANAGER_AFFIXES) {
        auto category = managerChild(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
                                     M_AFFIX_CATEGORY, left, 52, 148, 140);
        for (auto name : {L"前缀预设", L"后缀预设"})
            SendMessageW(category, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        SendMessageW(category, CB_SETCURSEL, g->affixCategory, 0);
        managerLabel(parent, g->affixCategory == 0 ? L"添加在文件名之前" : L"添加在文件名之后、扩展名之前", x,
                     58, width);
    }
    if (g->managerTab >= MANAGER_PROJECTS) {
        height = buildSettingsPage(parent);
    } else if (g->managerTab == MANAGER_TEMPLATES) {
        height = buildTemplateManager(parent);
    } else if (g->managerTab == 3) {
        const std::pair<int, const wchar_t *> checks[] = {
            {M_AUTO, L"检测到保存窗口时自动显示助手"},
            {M_QUICK, L"允许快捷提交保存（Enter / 双击 / Ctrl + 单击）"},
            {M_STARTUP, L"开机后台运行"}};
        int y = 52;
        for (auto [id, text] : checks) {
            managerChild(parent, L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, id, left, y, 496, 26);
            y += 36;
        }
        managerLabel(parent, L"助手位置", left, 170, 496);
        auto combo = managerChild(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, M_LAYOUT, left,
                                  194, 336, 160);
        for (auto text : {L"自动：优先侧边，空间不足时调整", L"优先侧边", L"优先底部"})
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        SendMessageW(combo, CB_SETCURSEL, g->cfg.layout, 0);
        CheckDlgButton(parent, M_AUTO, g->cfg.automatic ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(parent, M_QUICK, g->cfg.quickSave ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(parent, M_STARTUP,
                       std::filesystem::exists(startupLink()) ? BST_CHECKED : BST_UNCHECKED);
        button(L"保存选项", M_OPTIONS_SAVE, left, 242, 96);
        managerLabel(parent, L"后台等待窗口事件，不联网、不安装系统服务。", left, 300, 496);
        managerLabel(parent, L"配置文件", left, 336, 496);
        auto location = managerChild(parent, L"EDIT", g->configFile.wstring(), ES_READONLY | ES_AUTOHSCROLL,
                                     0, left, 358, 496, 28);
        SendMessageW(location, EM_SETSEL, 0, 0);
    } else if (managingAffixes()) {
        bool prefix = managingPrefixes();
        int top = 94;
        makeList(top, 340, true);
        auto value = field(prefix ? L"前缀内容" : L"后缀内容（放在扩展名前）", M_VALUE, top);
        SendMessageW(value, EM_SETLIMITTEXT, 80, 0);
        managerChild(parent, L"STATIC", L"", SS_LEFT, M_AFFIX_PREVIEW, x, top + 62, width, 34);
        managerChild(parent, L"STATIC",
                     L"前 3 项常驻显示，其余点「＋」查找。\n拖动或上移调整顺序；支持 {date}。", SS_LEFT, 0, x,
                     top + 108, width, 44);
        managerChild(parent, L"STATIC", L"选择预设修改，或点击新增。", SS_LEFT, M_AFFIX_STATUS, x, 268, width,
                     32);
        actions(312);
    } else {
        int y = 52;
        if (g->managerTab != 0) {
            field(L"显示名称", M_NAME, y);
            field(L"目录路径", M_VALUE, y + 56);
        }
        if (g->managerTab == 0) {
            makeList(52, 380, false);
            managerChild(parent, L"STATIC",
                         L"命名部分、常用选项、格式、目录推荐和软件关联，统一在方案设置中修改和保存。",
                         SS_LEFT, 0, x, 58, width, 40);
            button(L"方案设置", M_SAVE, x, 108, 156);
            button(L"复制方案", M_COPY_SCHEME, x + 168, 108, 168);
            button(L"新建方案", M_ADD, x, 156, 156);
            button(L"使用方案", M_APPLY_SCHEME, x + 168, 156, 168);
            auto defaults = button(L"软件关联与默认", M_SCHEME_DEFAULT, x, 200, width);
            tip(defaults, L"打开方案设置的“软件关联”，与其他规则一起保存。");
            managerChild(parent, L"STATIC", L"", SS_LEFT, M_INFO, x, 240, width, 96);
            button(L"删除方案", M_DELETE, x, 348, 156);
            button(L"上移", M_UP, x + 168, 348, 168);
        } else if (g->managerTab == 1) {
            makeList(y, 340, false);
            button(L"选择文件夹…", M_FOLDER, x, 180, 160);
            button(L"清空历史位置", M_CLEAR_HISTORY, x + 168, 180, 168);
            managerChild(parent, L"STATIC",
                         L"收藏名称和路径可直接编辑。历史仅表示浏览过的目录，不等同于成功保存。", SS_LEFT, 0,
                         x, 228, width, 56);
            actions(312);
        } else {
            makeList(y, 376, false);
            managerChild(parent, L"STATIC", L"保存目录、命名规则及本次修饰设置。规则在命名方案编辑器中维护。",
                         SS_LEFT, 0, x, 174, width, 64);
            button(L"保存当前设置为新流程", M_CAPTURE, x, 286, width);
            managerLabel(parent, L"流程沿用原软件的保存格式。", x, 322, width);
            actions(348);
        }
    }
    if (g->managerTab == 1 || g->managerTab == 2 || g->managerTab == 3)
        managerChild(parent, L"STATIC", L"", SS_LEFT, M_FEEDBACK, 12,
                     g->managerTab == 3   ? 274
                     : g->managerTab == 2 ? 388
                                          : 356,
                     496, 32);
    for (int id : {M_SAVE, M_OPTIONS_SAVE, ID_VARIABLE_SAVE})
        if (auto save = GetDlgItem(parent, id);
            save && (g->managerTab == 1 || g->managerTab == 2 || g->managerTab == 3 ||
                     g->managerTab == MANAGER_AFFIXES || g->managerTab == MANAGER_TEMPLATES))
            tip(save, L"保存当前修改（Ctrl + S）");
    g->managerLoading = false;
    if (g->managerTab == MANAGER_AFFIXES)
        g->builtAffixCategory = g->affixCategory;
    if (g->managerTab < MANAGER_PROJECTS)
        managerList();
    if (!preserveDraft)
        rememberManagerDraft();
    resizeManager(height);
    if (visible) {
        SendMessageW(parent, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    }
}
void managerMessage(const std::wstring &text) {
    MessageBoxW(g->manager, text.c_str(), L"Jump", MB_OK | MB_ICONINFORMATION);
}
size_t managerRecipeIndex() {
    if (g->managerTab == 0)
        return namingRecipeIndex(managedNamingScheme(g->editing).id);
    auto index = SendDlgItemMessageW(g->manager, M_LIST, LB_GETITEMDATA, g->editing, 0);
    return index < 0 ? g->cfg.recipes.size() : static_cast<size_t>(index);
}
void editManagedScheme(bool create, bool copy, int initialPage = 0) {
    auto draft =
        create ? namingDefinition(basicStructure(L"Asset", &g->cfg)) : managedNamingScheme(g->editing);
    if (create)
        for (const auto &s : namingSchemeCatalog(g->cfg))
            if (s.id == L"asset-v2") {
                draft = s;
                break;
            }
    if (!draft.enabled)
        return;
    auto previous = namingDefinition(draft);
    if (create || copy) {
        draft.id = newNamingIdentity();
        draft.name = create ? L"新命名方案" : draft.name + L" 副本";
    }
    auto applications = namingSchemeApplications(g->cfg, draft.id);
    CompactFormStyle style{g->smallFont, g->inputBrush, g->field,   g->muted,         g->line,
                           g->accent,    g->primary,    g->surface, paintRoundedRect, styleDropdown};
    style.popupMenu = trackPopupMenuWithHelp;
    auto theme = workflowForm(L"命名方案", L"", {});
    theme.compact = &style;
    bool globalDefault = defaultNamingScheme(g->cfg).id == draft.id;
    while (showNamingDesigner(g->manager, draft, g->cfg, theme, g->attached ? schemeSubject() : L"Asset",
                              applications, false, create ? 9 : initialPage, -1, &globalDefault)) {
        if (!storeNamingScheme(draft, false, &applications, &globalDefault))
            continue;
        if (g->attached && !g->structure.free && g->structure.id == draft.id)
            applySchemeRevision(draft, previous, schemeSubject());
        const auto &schemes = namingSchemeCatalog(g->cfg);
        auto selected =
            std::find_if(schemes.begin(), schemes.end(), [&](const auto &s) { return s.id == draft.id; });
        if (selected != schemes.end())
            g->editing = static_cast<size_t>(selected - schemes.begin());
        managerList();
        return;
    }
}
bool templateManagerCommand(HWND hwnd, int id, int code) {
    if (g->managerTab != MANAGER_TEMPLATES)
        return false;
    if ((id == ID_VARIABLE_TEMPLATE || id == ID_VARIABLE_VALUES ||
         (id >= ID_VARIABLE_FIELD_FIRST && id < ID_VARIABLE_FIELD_FIRST + 4)) &&
        code == EN_CHANGE) {
        updateVariableDraftPreview();
        invalidateManagerHeading();
        return true;
    }
    if (code != BN_CLICKED)
        return false;
    if (id >= ID_VARIABLE_TOKEN_FIRST &&
        id < ID_VARIABLE_TOKEN_FIRST + static_cast<int>(g->variableTokens.size())) {
        updateVariableDraftPreview();
        auto key = g->variableTokens[id - ID_VARIABLE_TOKEN_FIRST];
        toggleVariableToken(key);
        buildManagerPage(true);
        return true;
    }
    if (id == ID_VARIABLE_ADVANCED) {
        updateVariableDraftPreview();
        g->variableAdvanced = !g->variableAdvanced;
        buildManagerPage(true);
        return true;
    }
    if (id == ID_VARIABLE_CANCEL) {
        g->variableEditor = false;
        buildManagerPage();
        return true;
    }
    if (id != ID_VARIABLE_SAVE)
        return false;
    updateVariableDraftPreview();
    auto preview = smartNamingPreview(
        g->parts.base.empty() ? L"文件名" : compose(g->parts), g->dialog.directory, g->dialog.extension,
        {0, false, g->variableTemplateDraft, g->variableValuesDraft, g->cfg.dateFormat});
    if (trim(g->variableTemplateDraft).empty() || !preview.error.empty())
        return true;
    if (g->attached && !g->sessionExpression.empty()) {
        if (auto project = activeProject(g->cfg)) {
            auto previous = g->cfg;
            auto found = std::find_if(g->cfg.projects.begin(), g->cfg.projects.end(),
                                      [&](const auto &p) { return p.id == project->id; });
            auto values = variableValues(found->variables), draft = variableValues(g->variableValuesDraft);
            std::wstring updated;
            for (auto &[key, value] : values)
                updated += key + L"=" + (draft.contains(key) ? draft[key] : value) + L"\r\n";
            found->variables = updated;
            if (!persist()) {
                g->cfg = std::move(previous);
                SetDlgItemTextW(hwnd, ID_VARIABLE_HINT, L"项目变量未保存，请检查配置写入权限后重试。");
                return true;
            }
        }
        checkpointNaming();
        g->sessionExpression = trim(g->variableTemplateDraft);
        g->sessionVariables = g->variableValuesDraft;
        rememberManagerDraft();
        updateInlineRule();
        SetDlgItemTextW(hwnd, ID_VARIABLE_HINT, L"已应用到本次命名；可将当前设置另存为方案。");
        return true;
    }
    auto oldTemplate = g->cfg.variableTemplate, oldValues = g->cfg.variableValues;
    g->cfg.variableTemplate = trim(g->variableTemplateDraft);
    g->cfg.variableValues = g->variableValuesDraft;
    if (!persist()) {
        g->cfg.variableTemplate = oldTemplate;
        g->cfg.variableValues = oldValues;
        setTextIfChanged(GetDlgItem(hwnd, ID_VARIABLE_HINT), L"模板未保存，请检查配置写入权限。");
        return true;
    }
    // Configuration does not turn the template on or submit the host's save dialog.
    if (g->attached && g->variablesEnabled)
        updateInlineRule();
    if (g->attached)
        updatePanelContent();
    rememberManagerDraft();
    setTextIfChanged(GetDlgItem(hwnd, ID_VARIABLE_HINT), L"已保存 · 在助手中点击“变量表达式”即可套用");
    return true;
}
LRESULT CALLBACK ManagerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == dragMessage) {
        auto d = reinterpret_cast<DRAGLISTINFO *>(lp);
        static int from = -1;
        switch (d->uNotification) {
        case DL_BEGINDRAG:
            if (!discardManagerDraft())
                return FALSE;
            from = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            return from >= 0;
        case DL_DRAGGING: {
            int to = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            DrawInsert(hwnd, d->hWnd, to);
            return DL_MOVECURSOR;
        }
        case DL_DROPPED: {
            int to = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            if (managingAffixes() && from >= 0 && to >= 0 && from != to) {
                auto &values = managerAffixes();
                if (static_cast<size_t>(from) < values.size() && static_cast<size_t>(to) < values.size()) {
                    auto previous = g->cfg;
                    auto selected = g->editing;
                    auto value = values[from];
                    values.erase(values.begin() + from);
                    values.insert(values.begin() + to, value);
                    g->editing = to;
                    if (!commitManagerConfig(std::move(previous)))
                        g->editing = selected;
                    managerList();
                }
            }
            DrawInsert(hwnd, d->hWnd, -1);
            from = -1;
            return 0;
        }
        case DL_CANCELDRAG:
            DrawInsert(hwnd, d->hWnd, -1);
            from = -1;
            return 0;
        }
    }
    switch (msg) {
    case WM_GETMINMAXINFO: {
        auto info = reinterpret_cast<MINMAXINFO *>(lp);
        RECT minimum{0, 0, px(MANAGER_SHELL_WIDTH), px(380)};
        AdjustWindowRectExForDpi(&minimum, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                                 WS_EX_APPWINDOW, g->dpi);
        info->ptMinTrackSize = {minimum.right - minimum.left, minimum.bottom - minimum.top};
        return 0;
    }
    case WM_SIZE: {
        if (wp == SIZE_MINIMIZED || settingsDialogs.empty())
            break;
        auto host = settingsDialogs.back();
        if (!host->window || !host->contentWidth)
            break;
        RECT client{};
        GetClientRect(hwnd, &client);
        int available = std::max(1L, client.bottom - px(56));
        int height = std::min(host->contentHeight, available),
            maximum = std::max(0, host->contentHeight - height);
        int next = std::clamp(host->scroll, 0, maximum);
        if (next != host->scroll) {
            ScrollWindowEx(host->window, 0, host->scroll - next, nullptr, nullptr, nullptr, nullptr,
                           SW_SCROLLCHILDREN | SW_INVALIDATE);
            host->scroll = next;
        }
        SetWindowLongPtrW(host->window, GWL_STYLE,
                          (GetWindowLongPtrW(host->window, GWL_STYLE) & ~WS_VSCROLL) |
                              (maximum ? WS_VSCROLL : 0));
        SCROLLINFO scroll{sizeof(scroll), SIF_RANGE | SIF_PAGE | SIF_POS};
        scroll.nMax = std::max(0, host->contentHeight - host->footerHeight - 1);
        scroll.nPage = std::max(1, height - host->footerHeight);
        scroll.nPos = next;
        SetScrollInfo(host->window, SB_VERT, &scroll, TRUE);
        int space = std::max(1L, client.right - px(MANAGER_CONTENT_LEFT));
        int width = std::min(space, host->contentWidth +
                                        (maximum ? GetSystemMetricsForDpi(SM_CXVSCROLL, g->dpi) : 0));
        SetWindowPos(host->window, nullptr, px(MANAGER_CONTENT_LEFT) + (space - width) / 2, px(48), width,
                     height, SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
        if (host->footerHeight)
            host->procedure(host->window, WM_SETTINGS_VIEWPORT_CHANGED, 0, 0);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_DPICHANGED: {
        int before = g->dpi;
        g->dpi = HIWORD(wp);
        fonts();
        for (auto child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (settingsDialog(child))
                continue;
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT *>(&r), 2);
            MoveWindow(child, MulDiv(r.left, g->dpi, before), MulDiv(r.top, g->dpi, before),
                       MulDiv(r.right - r.left, g->dpi, before), MulDiv(r.bottom - r.top, g->dpi, before),
                       FALSE);
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), FALSE);
        }
        for (auto &r : g->managerFields) {
            r = {MulDiv(r.left, g->dpi, before), MulDiv(r.top, g->dpi, before),
                 MulDiv(r.right, g->dpi, before), MulDiv(r.bottom, g->dpi, before)};
        }
        auto suggested = reinterpret_cast<RECT *>(lp);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOACTIVATE | SWP_NOZORDER);
        NamingDesignerDpi scale{g->dpi, g->font, g->smallFont};
        for (auto host : settingsDialogs)
            if (host->procedure == namingDesignerProcedure)
                SendMessageW(host->window, WM_NAMING_DESIGNER_DPI, 0, reinterpret_cast<LPARAM>(&scale));
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(hwnd, msg, wp);
        auto dc = paint.dc();
        RECT client{};
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, g->brush);
        auto oldFont = SelectObject(dc, g->boldFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g->fg);
        RECT brand{px(24), px(16), px(136), px(40)};
        DrawTextW(dc, L"设置中心", -1, &brand, DT_SINGLELINE | DT_VCENTER);
        auto title = std::wstring(managerTitles[g->managerTab]);
        if (!settingsDialogs.empty()) {
            auto editor = windowText(settingsDialogs.back()->window);
            title += L"  /  " + (editor == title ? L"编辑" : editor);
        } else if (managerDraftDirty())
            title += L" · 未保存";
        RECT heading{px(MANAGER_CONTENT_LEFT + 20), px(16), client.right - px(20), px(40)};
        DrawTextW(dc, title.c_str(), -1, &heading, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, oldFont);
        if (settingsDialogs.empty())
            for (const auto &r : g->managerFields)
                paintInputFrame(dc, hwnd, r, px(8));
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        bool field = msg != WM_CTLCOLORSTATIC || windowClass(reinterpret_cast<HWND>(lp)) == L"Edit";
        SetTextColor(reinterpret_cast<HDC>(wp), field ? g->fg : g->muted);
        SetBkColor(reinterpret_cast<HDC>(wp), field ? g->field : g->bg);
        return reinterpret_cast<LRESULT>(field ? g->inputBrush : g->brush);
    }
    case WM_DRAWITEM: {
        auto item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (item.CtlType == ODT_COMBOBOX)
            return drawDropdownItem(item);
        bool list = item.CtlType == ODT_LISTBOX;
        if (!list && item.CtlType != ODT_BUTTON)
            return FALSE;
        PaintBuffer buffer(item.hDC, item.rcItem);
        item.hDC = buffer.dc;
        RECT r = item.rcItem;
        FillRect(item.hDC, &r, list ? g->inputBrush : g->brush);
        if (list && item.itemID == static_cast<UINT>(-1))
            return TRUE;
        bool disabled = (item.itemState & ODS_DISABLED) != 0;
        POINT mouse{};
        GetCursorPos(&mouse);
        ScreenToClient(item.hwndItem, &mouse);
        DWORD_PTR feedback = 0;
        GetWindowSubclass(item.hwndItem, PillProc, 1, &feedback);
        bool hover = !disabled && (list ? PtInRect(&r, mouse) : (feedback & 1));
        bool primary = item.CtlID == M_SAVE || item.CtlID == M_OPTIONS_SAVE || item.CtlID == ID_VARIABLE_SAVE;
        bool fold = item.CtlID == ID_VARIABLE_ADVANCED;
        bool expanded = g->variableAdvanced;
        bool templateChip = item.CtlID >= ID_VARIABLE_TOKEN_FIRST &&
                            item.CtlID < ID_VARIABLE_TOKEN_FIRST + g->variableTokens.size();
        bool chosen = false;
        if (templateChip) {
            auto keys = expressionKeys(g->variableTemplateDraft);
            chosen = std::find(keys.begin(), keys.end(),
                               g->variableTokens[item.CtlID - ID_VARIABLE_TOKEN_FIRST]) != keys.end();
        }
        bool keyboardFocus = !disabled && !list && (item.itemState & ODS_FOCUS) && (feedback & 2);
        COLORREF color = list ? (item.itemState & ODS_SELECTED ? g->selection : g->field)
                              : controlFill(fold ? g->bg : g->surface, (fold && expanded) || chosen, hover,
                                            (item.itemState & ODS_SELECTED) != 0, primary, !disabled);
        auto fill = CreateSolidBrush(color);
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : color);
        auto ob = SelectObject(item.hDC, fill), op = SelectObject(item.hDC, pen);
        paintRoundedRect(item.hDC, r.left, r.top, r.right, r.bottom, px(8), px(8));
        SelectObject(item.hDC, op);
        SelectObject(item.hDC, ob);
        DeleteObject(pen);
        DeleteObject(fill);
        std::wstring text;
        if (list) {
            auto length = SendMessageW(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0);
            if (length == LB_ERR)
                return TRUE;
            text.resize(static_cast<size_t>(length) + 1);
            SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
            text.resize(static_cast<size_t>(length));
        } else
            text = windowText(item.hwndItem);
        auto font = SelectObject(item.hDC, primary ? g->boldFont : g->font);
        SetBkMode(item.hDC, TRANSPARENT);
        SetTextColor(item.hDC, disabled || (fold && !expanded && !hover) ? g->muted : g->fg);
        r.left += px(8);
        r.right -= px(8);
        if (fold) {
            int size = px(14);
            drawUiIcon(item.hDC, expanded ? UiIcon::ChevronUp : UiIcon::Chevron,
                       static_cast<float>(r.right - size), (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size), expanded ? g->accent : g->muted);
            r.right -= px(22);
        }
        DrawTextW(item.hDC, text.c_str(), -1, &r,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (list || fold ? DT_LEFT : DT_CENTER));
        SelectObject(item.hDC, font);
        return TRUE;
    }
    case WM_EXITSIZEMOVE:
        rememberManagerPosition();
        return 0;
    case WM_CLOSE:
        if (!settingsDialogs.empty()) {
            requestSettingsNavigation(-1);
            return 0;
        }
        if (!discardManagerDraft())
            return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_SETTINGS_NAVIGATE:
        if (!settingsDialogs.empty())
            return 0;
        if (pendingSettingsClose) {
            pendingSettingsClose = false;
            pendingSettingsPage = -1;
            DestroyWindow(hwnd);
            return 0;
        }
        if (pendingSettingsPage >= 0) {
            g->managerTab = pendingSettingsPage;
            pendingSettingsPage = -1;
            TabCtrl_SetCurSel(GetDlgItem(hwnd, M_TAB), g->managerTab);
            g->editing = 0;
            buildManagerPage();
        }
        return 0;
    case WM_DESTROY:
        g->manager = nullptr;
        g->variableEditor = false;
        if (g->attached) {
            if (g->managerTab == MANAGER_AFFIXES)
                g->namingMode = g->affixCategory + 1;
            g->paths = rankLocations(g->cfg, g->dialog.source, {});
            rebuildPanel();
            if (GetForegroundWindow() == g->dialog.hwnd)
                reposition(true);
        }
        return 0;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR *>(lp)->idFrom == M_TAB &&
            reinterpret_cast<NMHDR *>(lp)->code == TCN_SELCHANGE) {
            int next = TabCtrl_GetCurSel(GetDlgItem(hwnd, M_TAB));
            if (!settingsDialogs.empty()) {
                TabCtrl_SetCurSel(GetDlgItem(hwnd, M_TAB), g->managerTab);
                requestSettingsNavigation(next);
                return 0;
            }
            if (!discardManagerDraft()) {
                TabCtrl_SetCurSel(GetDlgItem(hwnd, M_TAB), g->managerTab);
                return 0;
            }
            if (g->managerTab == MANAGER_TEMPLATES)
                g->variableEditor = false;
            g->managerTab = next;
            g->editing = 0;
            buildManagerPage();
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (g->managerLoading)
            return 0;
        if (settingsDialogs.empty() &&
            (((id == M_NAME || id == M_VALUE) && code == EN_CHANGE) ||
             ((id == M_AUTO || id == M_QUICK || id == M_STARTUP) && code == BN_CLICKED) ||
             (id == M_LAYOUT && code == CBN_SELCHANGE))) {
            managerFeedback(L"");
            invalidateManagerHeading();
        }
        if (settingsPageCommand(id, code))
            return 0;
        if (templateManagerCommand(hwnd, id, code))
            return 0;
        if (id == M_AFFIX_CATEGORY && code == CBN_SELCHANGE) {
            int selected = static_cast<int>(SendDlgItemMessageW(hwnd, id, CB_GETCURSEL, 0, 0));
            if (selected >= 0 && selected <= 1) {
                if (!discardManagerDraft()) {
                    SendDlgItemMessageW(hwnd, id, CB_SETCURSEL, g->affixCategory, 0);
                    return 0;
                }
                g->affixCategory = selected;
                g->editing = 0;
                buildManagerPage();
            }
            return 0;
        }

        if (id == M_LIST && code == LBN_SELCHANGE) {
            if (!discardManagerDraft()) {
                SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, g->editing, 0);
                return 0;
            }
            managerSelect();
            return 0;
        }
        if (id == M_VALUE && code == EN_CHANGE && managingAffixes()) {
            updateAffixManagerPreview();
            SetDlgItemTextW(hwnd, M_AFFIX_STATUS, L"修改后点击「保存修改」。");
            return 0;
        }
        if (code != BN_CLICKED)
            return 0;
        auto text = [&](int control) { return trim(windowText(GetDlgItem(hwnd, control))); };
        auto &cfg = g->cfg;

        if (g->managerTab == 0) {
            auto scheme = managedNamingScheme(g->editing);
            auto index = managerRecipeIndex();
            if (id == M_ADD || id == M_SAVE || id == M_COPY_SCHEME) {
                editManagedScheme(id == M_ADD, id == M_COPY_SCHEME);
                return 0;
            }
            if (id == M_SCHEME_DEFAULT) {
                editManagedScheme(false, false, 5);
                return 0;
            }
            if (id == M_APPLY_SCHEME && g->attached) {
                useNamingScheme(scheme);
                DestroyWindow(hwnd);
                SetForegroundWindow(g->dialog.hwnd);
                reposition(true);
                return 0;
            }
            if (index >= cfg.recipes.size())
                return 0;
            if (id == M_DELETE || id == M_UP) {
                auto previous = cfg;
                if (id == M_DELETE) {
                    if (!builtinNamingScheme(scheme.id))
                        removeNamingSchemeDefaults(cfg, scheme.id);
                    cfg.recipes.erase(cfg.recipes.begin() + index);
                    if (builtinNamingScheme(scheme.id))
                        std::erase_if(cfg.candidateRenames,
                                      [&](const auto &r) { return r.schema == scheme.id; });
                } else if (g->editing > 0) {
                    auto before = namingRecipeIndex(managedNamingScheme(g->editing - 1).id);
                    if (before < cfg.recipes.size()) {
                        std::swap(cfg.recipes[index], cfg.recipes[before]);
                        --g->editing;
                    }
                }
                if (!persist()) {
                    cfg = std::move(previous);
                    managerMessage(L"保存失败，原方案已保留。");
                } else if (id == M_DELETE && builtinNamingScheme(scheme.id) && g->attached &&
                           g->structure.id == scheme.id) {
                    for (const auto &s : namingSchemeCatalog(cfg))
                        if (s.id == scheme.id) {
                            applySchemeRevision(s, scheme, schemeSubject());
                            break;
                        }
                }
                managerList();
                return 0;
            }
            return 0;
        }

        if (managingAffixes()) {
            auto &values = managerAffixes();
            bool prefix = managingPrefixes();
            if (id == M_ADD) {
                if (!discardManagerDraft())
                    return 0;
                if (values.size() >= 100u) {
                    managerMessage(L"最多保存 100 个预设，请先移除不用的内容。");
                    return 0;
                }
                g->addingAffix = true;
                g->editing = values.size();
                SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                SetDlgItemTextW(hwnd, M_VALUE, L"");
                SetDlgItemTextW(hwnd, M_AFFIX_STATUS,
                                prefix ? L"新增前缀：填写内容后保存，即可在主面板点选。"
                                       : L"新增后缀：填写内容后保存，即可在主面板点选。");
                updateAffixManagerPreview();
                rememberManagerDraft();
                if (IsWindowVisible(hwnd))
                    SetFocus(GetDlgItem(hwnd, M_VALUE));
                return 0;
            }
            if (id == M_SAVE) {
                auto value = windowText(GetDlgItem(hwnd, M_VALUE));
                if (!validAffix(value, prefix)) {
                    managerMessage(L"请输入有效内容，不能包含路径字符。");
                    return 0;
                }
                auto duplicate = std::find(values.begin(), values.end(), value);
                if (duplicate != values.end() &&
                    (g->addingAffix || static_cast<size_t>(duplicate - values.begin()) != g->editing)) {
                    managerMessage(L"已有相同内容的预设，请修改内容或直接使用已有预设。");
                    return 0;
                }
                auto previous = cfg;
                bool adding = g->addingAffix || g->editing >= values.size();
                if (adding) {
                    if (values.size() >= 100u)
                        return 0;
                    values.insert(values.begin(), value);
                } else {
                    moveAffixColor(prefix, values[g->editing], value);
                    values[g->editing] = value;
                }
                if (!commitManagerConfig(std::move(previous)))
                    return 0;
                if (adding)
                    g->editing = 0;
                g->addingAffix = false;
                managerList();
                managerFeedback(L"已保存，返回助手即可点选。");
                return 0;
            }
            if (id == M_DELETE || id == M_UP) {
                if (g->addingAffix || g->editing >= values.size())
                    return 0;
                if (!discardManagerDraft())
                    return 0;
                auto previous = cfg;
                auto selected = g->editing;
                if (id == M_DELETE) {
                    moveAffixColor(prefix, values[g->editing], L"");
                    values.erase(values.begin() + g->editing);
                } else if (g->editing > 0) {
                    std::swap(values[g->editing], values[g->editing - 1]);
                    --g->editing;
                }
                bool saved = commitManagerConfig(std::move(previous));
                if (!saved)
                    g->editing = selected;
                managerList();
                managerFeedback(!saved           ? L"保存失败，原预设已保留。"
                                : id == M_DELETE ? L"已移除预设，当前文件名保持不变。"
                                                 : L"已调整预设顺序。");
                return 0;
            }
            return 0;
        }
        if (id == M_ADD) {
            if (g->managerTab != 1) {
                SendMessageW(hwnd, WM_COMMAND, M_CAPTURE, 0);
                return 0;
            }
            if (!discardManagerDraft())
                return 0;
            if (cfg.favorites.size() >= 100) {
                managerFeedback(L"最多收藏 100 个位置，请先移除不用的位置。");
                return 0;
            }
            g->editing = cfg.favorites.size();
            SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
            SetDlgItemTextW(hwnd, M_NAME, L"");
            SetDlgItemTextW(hwnd, M_VALUE, g->dialog.directory.c_str());
            for (int action : {M_DELETE, M_UP})
                EnableWindow(GetDlgItem(hwnd, action), FALSE);
            managerFeedback(L"填写名称和目录后点击「保存修改」。");
            rememberManagerDraft();
            SetFocus(GetDlgItem(hwnd, M_NAME));
            return 0;
        }
        if (id == M_SAVE) {
            auto name = text(M_NAME), value = text(M_VALUE);
            if (name.empty() || (g->managerTab == 1 && value.empty())) {
                managerMessage(L"请输入名称和内容。");
                return 0;
            }
            auto previous = cfg;
            if (g->managerTab == 1) {
                value = pathEditValue(value);
                if (name.size() > 80 || value.empty()) {
                    managerFeedback(L"请填写 80 字以内的名称和完整文件夹路径。");
                    return 0;
                }
                value = std::filesystem::path(value).lexically_normal().wstring();
                for (size_t i = 0; i < cfg.favorites.size(); ++i)
                    if (i != g->editing && normalize(cfg.favorites[i].path) == normalize(value)) {
                        managerFeedback(L"此目录已经收藏，请选择已有位置修改名称。");
                        return 0;
                    }
                if (g->editing < cfg.favorites.size())
                    cfg.favorites[g->editing] = {name, value, L"收藏"};
                else {
                    cfg.favorites.push_back({name, value, L"收藏"});
                }
            } else {
                auto index = managerRecipeIndex();
                if (index >= cfg.recipes.size())
                    return 0;
                for (size_t i = 0; i < cfg.recipes.size(); ++i)
                    if (i != index && lower(cfg.recipes[i].name) == lower(name)) {
                        managerMessage(L"同名方案或流程已存在，请使用其他名称。");
                        return 0;
                    }
                Recipe recipe = cfg.recipes[index];
                recipe.name = name;
                recipe.directory = value;
                if (std::filesystem::path(value).is_absolute())
                    recipe.relativeDirectory = false;
                cfg.recipes[index] = recipe;
            }
            if (!commitManagerConfig(std::move(previous)))
                return 0;
            managerList();
            managerFeedback(L"已保存。");
            return 0;
        }
        if (id == M_DELETE || id == M_UP) {
            if (!discardManagerDraft())
                return 0;
            auto previous = cfg;
            auto selected = g->editing;
            auto index = g->editing;
            if (g->managerTab == 1) {
                auto &p = cfg.favorites;
                if (index < p.size()) {
                    if (id == M_DELETE)
                        p.erase(p.begin() + index);
                    else if (index > 0) {
                        std::swap(p[index], p[index - 1]);
                        --g->editing;
                    }
                }
            } else {
                auto &p = cfg.recipes;
                index = managerRecipeIndex();
                if (index < p.size()) {
                    if (id == M_DELETE)
                        p.erase(p.begin() + index);
                    else if (g->editing > 0) {
                        auto before = SendDlgItemMessageW(hwnd, M_LIST, LB_GETITEMDATA, g->editing - 1, 0);
                        if (before >= 0)
                            std::swap(p[index], p[before]);
                        --g->editing;
                    }
                }
            }
            bool saved = commitManagerConfig(std::move(previous));
            if (!saved)
                g->editing = selected;
            managerList();
            managerFeedback(saved ? (id == M_DELETE ? L"已移除。" : L"已调整顺序。")
                                  : L"保存失败，原设置已保留。");
            return 0;
        }
        if (id == M_FOLDER) {
            auto path = pickFolder(hwnd);
            if (!path.empty())
                SetDlgItemTextW(hwnd, M_VALUE, path.c_str());
            return 0;
        }
        if (id == M_CLEAR_HISTORY) {
            managerMessage(removeHistoryLocation() ? L"历史位置已清空。" : g->status);
            return 0;
        }
        if (id == M_CAPTURE) {
            if (!g->attached || g->dialog.directory.empty() || !validName(g->stemName) ||
                !g->ruleError.empty() || !structuredInputError().empty()) {
                managerMessage(L"请先在保存窗口中选择目录并组成有效名称。");
                return 0;
            }
            if (cfg.recipes.size() >= 100) {
                managerMessage(L"最多保存 100 套方案与流程。");
                return 0;
            }
            auto name = text(M_NAME);
            if (name.empty())
                name = L"保存流程";
            auto base = name;
            int suffix = 2;
            while (std::any_of(cfg.recipes.begin(), cfg.recipes.end(),
                               [&](const auto &r) { return lower(r.name) == lower(name); }))
                name = base + L" " + std::to_wstring(suffix++);
            auto previous = cfg;
            cfg.recipes.push_back(captureRecipe(name));
            if (!persist()) {
                cfg = std::move(previous);
                managerMessage(L"保存失败，原设置已保留。");
                return 0;
            }
            g->editing = std::count_if(cfg.recipes.begin(), cfg.recipes.end(),
                                       [](const auto &r) { return !r.namingOnly; }) -
                         1;
            managerList();
            return 0;
        }
        if (id == M_OPTIONS_SAVE) {
            bool start = IsDlgButtonChecked(hwnd, M_STARTUP) == BST_CHECKED;
            bool previousStart = std::filesystem::exists(startupLink());
            auto previous = cfg;
            cfg.automatic = IsDlgButtonChecked(hwnd, M_AUTO) == BST_CHECKED;
            cfg.quickSave = IsDlgButtonChecked(hwnd, M_QUICK) == BST_CHECKED;
            cfg.layout = static_cast<int>(SendDlgItemMessageW(hwnd, M_LAYOUT, CB_GETCURSEL, 0, 0));
            if (!commitManagerConfig(previous))
                return 0;
            // Re-save an enabled shortcut so moving the portable folder repairs its target.
            if ((start || previousStart) && !enableStartup(start)) {
                cfg = std::move(previous);
                bool restored = persist();
                managerFeedback(restored ? L"开机运行设置失败，原选项已恢复。请检查启动目录写入权限。"
                                         : L"开机运行设置失败，原选项尚未恢复到磁盘，请重试保存。");
                return 0;
            }
            if (!cfg.automatic)
                hidePanel();
            rememberManagerDraft();
            managerFeedback(L"选项已保存。");
            return 0;
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
POINT fitWindowPoint(POINT point, int width, int height, const RECT &work) {
    point.x = std::clamp<LONG>(point.x, work.left, std::max<LONG>(work.left, work.right - width));
    point.y = std::clamp<LONG>(point.y, work.top, std::max<LONG>(work.top, work.bottom - height));
    return point;
}
POINT managerOpenPoint(int width, int height) {
    MONITORINFO monitor{sizeof(monitor)};
    if (g->cfg.managerPositionSaved) {
        RECT saved{g->cfg.managerX, g->cfg.managerY, g->cfg.managerX + width, g->cfg.managerY + height};
        GetMonitorInfoW(MonitorFromRect(&saved, MONITOR_DEFAULTTONEAREST), &monitor);
        return fitWindowPoint({saved.left, saved.top}, width, height, monitor.rcWork);
    }
    HWND anchor = g->attached && IsWindow(g->dialog.hwnd) && !IsIconic(g->dialog.hwnd) ? g->dialog.hwnd
                  : IsWindowVisible(g->panel) && !IsIconic(g->panel)                   ? g->panel
                                                                                       : nullptr;
    POINT mouse{};
    GetCursorPos(&mouse);
    auto screen = anchor ? MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST)
                         : MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONULL);
    if (!screen)
        screen = MonitorFromPoint(mouse, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(screen, &monitor);
    RECT reference = anchor ? visibleWindowRect(anchor) : monitor.rcWork;
    return fitWindowPoint({reference.left + (reference.right - reference.left - width) / 2,
                           reference.top + (reference.bottom - reference.top - height) / 2},
                          width, height, monitor.rcWork);
}
void positionManager(bool opening) {
    RECT current{};
    if (!GetWindowRect(g->manager, &current))
        return;
    int width = current.right - current.left, height = current.bottom - current.top;
    POINT point{current.left, current.top};
    if (opening)
        point = managerOpenPoint(width, height);
    else {
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST), &monitor);
        point = fitWindowPoint(point, width, height, monitor.rcWork);
    }
    if (point.x != current.left || point.y != current.top)
        SetWindowPos(g->manager, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void rememberManagerPosition() {
    RECT current{};
    if (IsIconic(g->manager) || IsZoomed(g->manager) || !GetWindowRect(g->manager, &current))
        return;
    if (g->cfg.managerPositionSaved && g->cfg.managerX == current.left && g->cfg.managerY == current.top)
        return;
    g->cfg.managerPositionSaved = true;
    g->cfg.managerX = current.left;
    g->cfg.managerY = current.top;
    persist();
}
void presentManager() {
    // dev.ps1 and the startup shortcut launch with STARTF_USESHOWWINDOW/SW_HIDE.
    // Windows may override the first ShowWindow(SW_SHOW) with that startup state.
    // An explicit management action must make the window visible on this call.
    ShowWindow(g->manager, IsIconic(g->manager) ? SW_RESTORE : SW_SHOW);
    if (IsIconic(g->manager))
        ShowWindow(g->manager, SW_RESTORE);
    positionManager(false);
    SetWindowPos(g->manager, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!IsWindowVisible(g->manager) || IsIconic(g->manager)) {
        g->status = L"管理窗口未能显示，请重试。";
        if (g->attached)
            updatePanelContent();
        return;
    }
    SetForegroundWindow(g->manager);
    // Hand focus to the visible editor before hiding the clicked helper.
    hidePanel();
}
void showManager(bool present) {
    g->presetEditor = false;
    KillTimer(g->control, T_ACTION);
    if (g->manager) {
        auto tab = GetDlgItem(g->manager, M_TAB);
        if (!settingsDialogs.empty()) {
            int next = g->managerTab;
            g->managerTab = TabCtrl_GetCurSel(tab);
            if (next != g->managerTab)
                requestSettingsNavigation(next);
            if (present)
                presentManager();
            return;
        }
        if (TabCtrl_GetCurSel(tab) != g->managerTab ||
            (g->managerTab == MANAGER_AFFIXES && g->builtAffixCategory != g->affixCategory)) {
            int next = g->managerTab, category = g->affixCategory;
            g->managerTab = TabCtrl_GetCurSel(tab);
            if (g->managerTab == MANAGER_AFFIXES)
                g->affixCategory = g->builtAffixCategory;
            if (!discardManagerDraft()) {
                if (present)
                    presentManager();
                return;
            }
            if (g->managerTab == MANAGER_TEMPLATES)
                g->variableEditor = false;
            g->managerTab = next;
            g->affixCategory = category;
            TabCtrl_SetCurSel(tab, g->managerTab);
            g->editing = 0;
            buildManagerPage();
        }
        if (present)
            presentManager();
        return;
    }
    int w = px(MANAGER_WIDTH), h = px(MANAGER_HEIGHT);
    auto point = managerOpenPoint(w, h);
    g->manager = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_CONTROLPARENT, L"Jump.Manager", L"Jump · 设置中心",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX |
                                     WS_THICKFRAME | WS_CLIPCHILDREN,
                                 point.x, point.y, w, h, nullptr, nullptr, g->instance, nullptr);
    if (!g->manager) {
        g->status = L"无法创建管理窗口，请重试。";
        if (g->attached)
            updatePanelContent();
        return;
    }
    int managerDpi = static_cast<int>(GetDpiForWindow(g->manager));
    if (managerDpi != g->dpi) {
        g->dpi = managerDpi;
        fonts();
    }
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g->manager, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(g->manager, 35, &g->bg, sizeof(g->bg));
    DwmSetWindowAttribute(g->manager, 36, &g->fg, sizeof(g->fg));
    auto tab = managerChild(g->manager, WC_TABCONTROLW, L"",
                            WS_TABSTOP | TCS_FIXEDWIDTH | TCS_BUTTONS | TCS_FLATBUTTONS | TCS_MULTILINE,
                            M_TAB, 12, 54, 124, 340);
    for (auto name : managerTitles) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(name);
        TabCtrl_InsertItem(tab, TabCtrl_GetItemCount(tab), &item);
    }
    // Inserting captions recalculates the native button-tab widths. Apply the
    // compact size after insertion so selecting the final page cannot scroll
    // the first page out of view.
    TabCtrl_SetMinTabWidth(tab, 0);
    TabCtrl_SetPadding(tab, 0, 0);
    TabCtrl_SetItemSize(tab, px(120), px(32));
    TabCtrl_SetCurSel(tab, g->managerTab);
    buildManagerPage();
    positionManager(true);
    if (present)
        presentManager();
}
#include "settings_center.h"
bool handleAppShortcut(const MSG &message) {
    if (message.message != WM_KEYDOWN || !(GetKeyState(VK_CONTROL) & 0x8000) ||
        (GetKeyState(VK_MENU) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000) ||
        GetPropW(GetFocus(), L"Jump.IME") || g->structuredIme)
        return false;
    HWND root = GetAncestor(message.hwnd, GA_ROOT);
    HWND owner = nullptr;
    int id = 0;
    if (root == g->manager && message.wParam == 'S') {
        owner = settingsDialogs.empty() ? g->manager : settingsDialogs.back()->window;
        if (!settingsDialogs.empty())
            id = IDOK;
        else if (g->managerTab == 3)
            id = M_OPTIONS_SAVE;
        else if (g->managerTab == MANAGER_TEMPLATES)
            id = ID_VARIABLE_SAVE;
        else if (g->managerTab == 1 || g->managerTab == 2 || managingAffixes())
            id = M_SAVE;
    } else if (root == g->panel && (message.wParam == 'F' || message.wParam == 'L')) {
        owner = g->panel;
        id = message.wParam == 'F' ? ID_DIRECTORY_SEARCH : ID_DIRECTORY_EDIT;
    } else if (root == g->panel && (message.wParam == 'Z' || message.wParam == 'Y') &&
               windowClass(GetFocus()) != L"Edit") {
        owner = g->panel;
        id = message.wParam == 'Z' ? ID_COMBO_UNDO : ID_COMBO_REDO;
    }
    auto button = id ? GetDlgItem(owner, id) : nullptr;
    if (!button || !IsWindowVisible(button) || !IsWindowEnabled(button))
        return false;
    PostMessageW(owner, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(button));
    return true;
}
void showPresetManager(bool add, size_t index, bool present) {
    if (g->manager && settingsDialogs.empty() && managerDraftDirty()) {
        if (!discardManagerDraft()) {
            if (present)
                presentManager();
            return;
        }
        if (g->managerTab == MANAGER_TEMPLATES)
            g->variableEditor = false;
        buildManagerPage();
    }
    g->managerTab = MANAGER_AFFIXES;
    if (g->namingMode)
        g->affixCategory = g->namingMode - 1;
    showManager(false);
    if (!g->manager || g->managerTab != MANAGER_AFFIXES || !settingsDialogs.empty())
        return;
    g->editing = index;
    managerList();
    if (add)
        SendMessageW(g->manager, WM_COMMAND, M_ADD, reinterpret_cast<LPARAM>(GetDlgItem(g->manager, M_ADD)));
    if (present) {
        showManager();
        SetFocus(GetDlgItem(g->manager, M_VALUE));
    }
}
