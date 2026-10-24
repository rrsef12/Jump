#pragma once

void updatePanelContent() {
    g->building = true;
    if (g->dialog.generic) {
        auto directory = GetDlgItem(g->panel, ID_GENERIC_DIRECTORY);
        setTextIfChanged(directory, g->dialog.directory.empty() ? L"目录未确认 · 自动编号不可用"
                                                                : L"目录：" + g->dialog.directory);
        if (directory)
            tip(directory, g->dialog.directory.empty() ? L"请在原软件选择目录；Jump 不猜测保存位置"
                                                       : g->dialog.directory + L"\n切换目录请在原软件操作");
    }
    auto preview = GetDlgItem(g->panel, ID_PREVIEW);
    setTextIfChanged(preview, g->stemName + g->dialog.extension);
    if (preview)
        tip(preview, L"最终文件名：" + g->stemName + g->dialog.extension +
                         L"\n结果预览；修改名称请使用上方命名部分或“命名设置”。");
    setTextIfChanged(GetDlgItem(g->panel, ID_FREE_NAME_INPUT), g->stemName);
    setTextIfChanged(GetDlgItem(g->panel, ID_DIRECTORY),
                     g->dialog.directory.empty()
                         ? (g->dialog.genericNavigate ? L"选择快捷位置或输入目录" : L"使用原软件当前目录")
                         : g->dialog.directory.c_str());
    if (auto directory = GetDlgItem(g->panel, ID_DIRECTORY))
        tip(directory, g->dialog.directory.empty() ? L"使用原软件当前目录" : g->dialog.directory);
    showIfChanged(GetDlgItem(g->panel, ID_DIRECTORY), g->editingPath ? SW_HIDE : SW_SHOWNOACTIVATE);
    auto pathInput = GetDlgItem(g->panel, ID_DIRECTORY_INPUT);
    if (pathInput && windowText(pathInput) != g->pathDraft)
        setTextIfChanged(pathInput, g->pathDraft.c_str());
    showIfChanged(pathInput, g->editingPath ? SW_SHOWNOACTIVATE : SW_HIDE);
    setTextIfChanged(GetDlgItem(g->panel, ID_DIRECTORY_EDIT), L"编辑路径");
    if (auto edit = GetDlgItem(g->panel, ID_DIRECTORY_EDIT)) {
        tip(edit, g->editingPath ? L"Enter 切换目录；点击别处或 Esc 退出编辑" : L"输入或粘贴其他文件夹路径");
        enableIfChanged(edit, !g->busy);
    }
    enableIfChanged(GetDlgItem(g->panel, ID_DIRECTORY_SEARCH), !g->busy && !g->pastePending);
    InvalidateRect(g->panel, &g->pathEditRect, FALSE);
    bool saveError = g->settingsDirty && !g->configPending;
    auto status = g->status + (saveError ? L" · 配置未保存" : L"");
    setTextIfChanged(GetDlgItem(g->panel, ID_STATUS), status.c_str());
    showIfChanged(GetDlgItem(g->panel, ID_STATUS),
                  g->statusError || saveError || g->namingPending || g->pastePending ? SW_SHOWNOACTIVATE
                                                                                     : SW_HIDE);
    if (auto control = GetDlgItem(g->panel, ID_STATUS))
        tip(control, saveError
                         ? g->status + L"\n预设和最近记录尚未写入配置；恢复权限后会在下一次操作重试。\n" +
                               g->configFile.wstring()
                         : g->status);
    bool favorite = std::any_of(g->cfg.favorites.begin(), g->cfg.favorites.end(), [](const auto &p) {
        return normalize(p.path) == normalize(g->dialog.directory);
    });
    setTextIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), favorite ? L"已收藏" : L"＋收藏");
    enableIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), !g->busy && !g->dialog.directory.empty());
    if (auto control = GetDlgItem(g->panel, ID_FAVORITE_CURRENT))
        tip(control, favorite ? L"取消收藏当前目录" : L"收藏当前目录");
    updateLocationShortcuts();
    updateInlineControls();
    updateStructuredContent();
    updateExperienceContent();
    updateSaveAssist();
    updatePanelStatusLayout();
    enableIfChanged(GetDlgItem(g->panel, ID_SUBMIT),
                    (!g->busy || (!g->busySubmitting && !g->dialog.generic)) && !g->pastePending &&
                        !g->structuredIme && structuredInputError().empty() && g->ruleError.empty() &&
                        validName(g->stemName));
    enableIfChanged(GetDlgItem(g->panel, ID_UNDO), !g->busy && g->dirtyName);
    if (auto submit = GetDlgItem(g->panel, ID_SUBMIT))
        tip(submit, g->status + (g->dialog.generic ? L"\n填入名称后，在原软件中确认保存"
                                                   : L"\n确认文件名与位置后，提交原软件保存"));
    setTextIfChanged(g->panel, (L"Jump 保存助手 · " + std::wstring(status)).c_str());
    g->building = false;
    for (auto h : g->children)
        if (windowClass(h) == L"Button")
            InvalidateRect(h, nullptr, FALSE);
}
RECT visibleWindowRect(HWND hwnd) {
    RECT rect{};
    GetWindowRect(hwnd, &rect);
    RECT frame{};
    // GetWindowRect includes invisible resize borders on DWM windows.
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame))) &&
        frame.right > frame.left && frame.bottom > frame.top)
        return frame;
    return rect;
}
void reposition(bool show = false) {
    if (!g->attached || !IsWindow(g->dialog.hwnd))
        return;
    RECT r = visibleWindowRect(g->dialog.hwnd);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(g->dialog.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    int width = px(g->width), height = px(g->height), x = r.right + px(PANEL_GAP), y = r.top + px(32);
    if (g->bottom || x + width > mi.rcWork.right) {
        if (!g->bottom && r.left - width - px(PANEL_GAP) >= mi.rcWork.left)
            x = r.left - width - px(PANEL_GAP);
        else {
            x = r.left;
            y = r.bottom + px(PANEL_GAP);
        }
    }
    x = std::clamp<int>(x, mi.rcWork.left, std::max<int>(mi.rcWork.left, mi.rcWork.right - width));
    y = std::clamp<int>(y, mi.rcWork.top, std::max<int>(mi.rcWork.top, mi.rcWork.bottom - height));
    RECT current{};
    GetWindowRect(g->panel, &current);
    bool moved = current.left != x || current.top != y;
    bool resized = current.right - current.left != width || current.bottom - current.top != height;
    bool topmost = (GetWindowLongPtrW(g->panel, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (moved || resized || !topmost || (show && !IsWindowVisible(g->panel)))
        SetWindowPos(g->panel, HWND_TOPMOST, x, y, width, height,
                     SWP_NOACTIVATE | (moved ? 0 : SWP_NOMOVE) | (resized ? 0 : SWP_NOSIZE) |
                         (show ? SWP_SHOWWINDOW : 0));
}
void updateInlineControls() {
    if (g->inlineTool == 2) {
        for (int id : {ID_AFFIX_PREFIX, ID_AFFIX_SUFFIX}) {
            auto input = GetDlgItem(g->panel, id);
            const auto &value = id == ID_AFFIX_PREFIX ? g->parts.prefix : g->parts.suffix;
            auto draft = windowText(input);
            if (input && GetFocus() != input && draft != value &&
                (draft.empty() || validAffix(draft, id == ID_AFFIX_PREFIX)))
                SetWindowTextW(input, value.c_str());
        }
    }
    if (g->inlineTool != 3)
        return;
    auto sourceText = compose(g->parts);
    g->rulePreview = renamePreview(sourceText, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex);
    auto source = GetDlgItem(g->panel, ID_RULE_SOURCE);
    setHighlightedText(source, sourceText, g->rulePreview.matches);
    SetDlgItemTextW(g->panel, ID_RULE_ENABLE, g->ruleEnabled ? L"已启用" : L"启用");
    auto message = !g->rulePreview.error.empty()
                       ? g->rulePreview.error
                       : L"匹配 " + std::to_wstring(g->rulePreview.matches.size()) + L" 处 · " +
                             (g->ruleEnabled ? L"规则已启用，收起后保留" : L"规则未启用");
    SetDlgItemTextW(g->panel, ID_RULE_HINT, message.c_str());
    showIfChanged(GetDlgItem(g->panel, ID_RULE_HINT),
                  g->rulePreview.error.empty() ? SW_SHOWNOACTIVATE : SW_HIDE);
    SetDlgItemTextW(g->panel, ID_RULE_RESULT, (L"→ " + g->stemName + g->dialog.extension).c_str());
}
void buildInlineTools(int &y, int available) {
    g->dividers.push_back(px(y - 4));
    int cell = (available - 16) / 3;
    if (g->inlineTool == 2) {
        for (bool prefix : {true, false}) {
            label(16, y + 4, 36, prefix ? L"前缀" : L"后缀");
            auto input =
                child(L"EDIT", prefix ? g->parts.prefix : g->parts.suffix, ES_AUTOHSCROLL | WS_TABSTOP,
                      prefix ? ID_AFFIX_PREFIX : ID_AFFIX_SUFFIX, 52, y, available - 72, 32);
            SetWindowSubclass(input, InlineEditProc, 1, 0);
            SendMessageW(input, EM_SETLIMITTEXT, 80, 0);
            SendMessageW(input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"自由输入，留空停用"));
            auto add = child(L"BUTTON", L"＋", WS_TABSTOP, prefix ? ID_ADD_PREFIX : ID_ADD_SUFFIX,
                             g->width - 44, y, 28, 28);
            tip(add, L"搜索全部或添加预设");
            y += 36;
            buildNamingHint(prefix ? ID_PREFIX_ERROR : ID_SUFFIX_ERROR, 52, y, available - 36);
            int x = 16, row = y;
            const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
            for (size_t i = 0; i < values.size() && i < 3; ++i)
                pill(affixLabel(values[i]), prefix ? Action::Prefix : Action::Suffix, i, x, row, available,
                     L"单击启用，再点取消；右键编辑或换色", 16, cell);
            y += 40;
        }
    } else if (g->inlineTool == 3) {
        auto combo = child(L"COMBOBOX", L"",
                           CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL,
                           ID_RULE_MODE, 16, y, available - 124, 240);
        SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), px(24));
        SendMessageW(combo, CB_SETITEMHEIGHT, 0, px(28));
        SendMessageW(combo, CB_SETDROPPEDWIDTH, px(240), 0);
        styleDropdown(combo);
        for (size_t i = 1; i < renameRules().size(); ++i)
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(renameRules()[i].name.c_str()));
        SendMessageW(combo, CB_SETCURSEL, g->ruleIndex - 1, 0);
        child(L"BUTTON", L"启用", WS_TABSTOP, ID_RULE_ENABLE, g->width - 132, y, 56, 28);
        child(L"BUTTON", L"清空", WS_TABSTOP, ID_RULE_CLEAR, g->width - 72, y, 56, 28);
        y += 36;
        auto pattern = child(L"EDIT", g->rulePattern, ES_AUTOHSCROLL | WS_TABSTOP, ID_RULE_PATTERN, 16, y,
                             (available - 8) / 2, 32);
        auto replacement = child(L"EDIT", g->ruleReplacement, ES_AUTOHSCROLL | WS_TABSTOP,
                                 ID_RULE_REPLACEMENT, 24 + (available - 8) / 2, y, (available - 8) / 2, 32);
        SendMessageW(pattern, EM_SETLIMITTEXT, 160, 0);
        SendMessageW(replacement, EM_SETLIMITTEXT, 240, 0);
        SetWindowSubclass(pattern, InlineEditProc, 1, 0);
        SetWindowSubclass(replacement, InlineEditProc, 1, 0);
        SendMessageW(pattern, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"查找文字 / 正则表达式"));
        SendMessageW(replacement, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"替换为，留空即删除"));
        y += 40;
        buildNamingHint(ID_RULE_ERROR, 16, y, available);
        LoadLibraryW(L"Msftedit.dll");
        child(MSFTEDIT_CLASS, L"", ES_READONLY | ES_AUTOHSCROLL, ID_RULE_SOURCE, 16, y, available, 28);
        y += 32;
        for (int id : {ID_RULE_HINT, ID_RULE_RESULT}) {
            auto text = child(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, id, 16, y, available, 22);
            SendMessageW(text, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
            y += 24;
        }
        y += 4;
    }
}
#include "structured_controls.h"
#include "subject_group.h"
void rebuildPanel() {
    if (!g->panel)
        return;
    g->rebuildPending = false;
    resetEditorDismiss();
    bool wasVisible = IsWindowVisible(g->panel) != 0;
    g->building = true;
    int focusId = 0;
    DWORD selectionStart = 0, selectionEnd = 0;
    HWND focused = GetFocus();
    if (GetForegroundWindow() == g->panel && focused && GetAncestor(focused, GA_ROOT) == g->panel) {
        focusId = GetDlgCtrlID(focused);
        if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT + 8) || focusId == ID_STRUCT_INPUT ||
            focusId == ID_FREE_NAME_INPUT || focusId == ID_DIRECTORY_INPUT || focusId == ID_RULE_PATTERN ||
            focusId == ID_RULE_REPLACEMENT || focusId == ID_VARIABLE_TEMPLATE ||
            (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
            SendMessageW(focused, EM_GETSEL, reinterpret_cast<WPARAM>(&selectionStart),
                         reinterpret_cast<LPARAM>(&selectionEnd));
    }
    RECT target{}, work{};
    if (g->attached && GetWindowRect(g->dialog.hwnd, &target)) {
        target = visibleWindowRect(g->dialog.hwnd);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(g->dialog.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        work = mi.rcWork;
        g->bottom = g->cfg.layout == 2 ||
                    (g->cfg.layout == 0 && target.right + px(PANEL_WIDTH + PANEL_GAP) > work.right &&
                     target.left - px(PANEL_WIDTH + PANEL_GAP) < work.left);
        g->width =
            g->bottom ? std::clamp(MulDiv(target.right - target.left, 96, g->dpi), 648, 760) : PANEL_WIDTH;
    }
    SetWindowTextW(g->panel, (L"Jump 保存助手 · " + g->status).c_str());
    SendMessageW(g->panel, WM_SETREDRAW, FALSE, 0);
    if (g->tooltip)
        DestroyWindow(g->tooltip);
    g->tipStrings.clear();
    for (auto h : g->children) {
        g->highlights.erase(h);
        DestroyWindow(h);
    }
    g->children.clear();
    g->actions.clear();
    g->labels.clear();
    g->fields.clear();
    g->dividers.clear();
    g->locationTabsRect = {};
    g->nextAction = 1000;
    g->tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0,
                                 0, g->panel, nullptr, g->instance, nullptr);
    SendMessageW(g->tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
    SendMessageW(g->tooltip, TTM_SETMAXTIPWIDTH, 0, px(420));
    const int m = PANEL_MARGIN;
    int w = g->width, avail = w - m * 2, y = 32;
    label(m, 6, w - 144, g->dialog.save ? L"Jump · 保存助手" : L"Jump · 打开助手", true);
    auto close = child(L"BUTTON", L"×", WS_TABSTOP, ID_CLOSE, w - 32, 6, 24, 22);
    tip(close, L"在本次文件窗口中隐藏助手");
    if (!g->dialog.generic || g->dialog.genericNavigate) {
        auto clearRecent = child(L"BUTTON", L"清空最近", WS_TABSTOP, ID_CLEAR_RECENT, w - 112, 6, 76, 22);
        tip(clearRecent, L"清空最近保存的位置记录；不删除文件夹，不影响收藏和浏览历史");
        auto clearHistory = child(L"BUTTON", L"清空历史", WS_TABSTOP, ID_CLEAR_HISTORY, w - 112, 6, 76, 22);
        tip(clearHistory, L"清空浏览过的目录记录；不删除文件夹，不影响收藏和最近保存");
        const std::pair<int, const wchar_t *> tabs[] = {{ID_LOC_RECOMMENDED, L"推荐"},
                                                        {ID_LOC_FAVORITES, L"收藏"},
                                                        {ID_LOC_RECENT, L"最近"},
                                                        {ID_LOC_HISTORY, L"历史"}};
        constexpr int tabCount = static_cast<int>(std::size(tabs));
        g->locationTabsRect = {px(m), px(y), px(m + avail), px(y + 28)};
        for (int i = 0; i < tabCount; ++i) {
            int left = m + 2 + (avail - 4) * i / tabCount;
            int right = m + 2 + (avail - 4) * (i + 1) / tabCount;
            auto tab = child(L"BUTTON", tabs[i].second, WS_TABSTOP, tabs[i].first, left + 1, y + 2,
                             right - left - 2, 24);
            if (tabs[i].first == ID_LOC_RECOMMENDED)
                tip(tab, L"推荐位置与资源管理器中已打开的文件夹；相同路径只显示一次");
            if (tabs[i].first == ID_LOC_RECENT)
                tip(tab, L"已确认文件保存成功的位置；仅浏览或取消保存不会计入");
            if (tabs[i].first == ID_LOC_HISTORY)
                tip(tab, L"在文件窗口浏览或跳转过的目录；未保存或取消保存也会记录");
        }
        y = 68;
        g->folderTop = y;
        size_t locationCount = locationChoices().size();
        int columns = locationColumnCount(locationCount);
        g->folderRows = locationRowCount(locationCount);
        for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
            int left = m + (avail + 8) * (i % columns) / columns,
                right = m + (avail + 8) * (i % columns + 1) / columns - 8;
            child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_FIRST + i, left, y + i / columns * FOLDER_ROW_STEP,
                  right - left, FOLDER_BUTTON_HEIGHT);
        }
        child(L"SCROLLBAR", L"", SBS_VERT | WS_TABSTOP, ID_FOLDER_SCROLL, w - m - 16, y, 16,
              g->folderRows * FOLDER_ROW_STEP - 8);
        auto historyMore =
            child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_PAGE, m, y, avail, FOLDER_BUTTON_HEIGHT);
        tip(historyMore, L"按名称或完整路径搜索浏览历史；记录按最近访问排序");
        auto empty = child(L"STATIC", L"", SS_LEFT, ID_LOCATION_EMPTY, m, y, avail, FOLDER_BUTTON_HEIGHT);
        SendMessageW(empty, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
        y += g->folderRows * FOLDER_ROW_STEP + 2;
        auto directory = child(L"BUTTON", L"", WS_TABSTOP, ID_DIRECTORY, m, y, avail - 82, 24);
        tip(directory, L"当前路径；点击编辑（Ctrl + L）");
        g->pathEditRect = {px(m), px(y), px(m + avail - 82), px(y + 24)};
        auto input = CreateWindowExW(0, L"EDIT", g->pathDraft.c_str(), WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
                                     px(m + 4), px(y + 2), px(avail - 90), px(20), g->panel,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIRECTORY_INPUT)),
                                     g->instance, nullptr);
        g->children.push_back(input);
        SetWindowSubclass(input, TransientEditProc, 4, 0);
        SetWindowSubclass(input, InputFeedbackProc, 1, 0);
        configureMaterialEditor(input);
        SendMessageW(input, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
        SendMessageW(input, EM_SETLIMITTEXT, 32767, 0);
        SendMessageW(input, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"输入或粘贴文件夹路径，Enter 切换"));
        SetWindowSubclass(input, PathEditProc, 1, 0);
        auto search = child(L"BUTTON", L"搜索位置", WS_TABSTOP, ID_DIRECTORY_SEARCH, w - m - 76, y, 24, 24);
        tip(search, L"搜索收藏、最近保存和浏览历史（Ctrl + F）");
        child(L"BUTTON", L"编辑路径", WS_TABSTOP, ID_DIRECTORY_EDIT, w - m - 50, y, 24, 24);
        auto favorite = child(L"BUTTON", L"☆", WS_TABSTOP, ID_FAVORITE_CURRENT, w - m - 24, y, 24, 24);
        tip(favorite, L"收藏或取消收藏当前位置");
        y += 34;
    } else {
        label(m, y, avail - 88, L"快速命名 · 在原窗口确认保存");
        child(L"BUTTON", L"重新指定", WS_TABSTOP, ID_BIND_CONTROL, w - 94, y, 80, 24);
        y += 28;
        child(L"STATIC", L"", SS_PATHELLIPSIS, ID_GENERIC_DIRECTORY, m, y, avail, 22);
        y += 28;
    }
    if (g->dialog.save && (!g->dialog.generic || g->dialog.genericNavigate)) {
        auto route = child(L"BUTTON", L"", WS_TABSTOP, ID_ROUTE_SUGGEST, m, y, avail, 0);
        showIfChanged(route, SW_HIDE);
    }
    g->dividers.push_back(px(y));
    y += 8;
    if (g->dialog.save) {
        if (!g->structure.enabled) {
            g->structure = basicStructure(g->stemName, &g->cfg);
            g->structure.literal = true;
            g->structure.literalName = g->stemName;
        }
        buildStructuredControls(y, avail);
        buildNamingActions(y, avail);
        g->dividers.push_back(px(y + 2));
        y += 10;
        auto finalLabel = child(L"STATIC", L"最终文件名", SS_LEFT, ID_FINAL_NAME_LABEL, m, y, avail, 18);
        SendMessageW(finalLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
        y += 20;
        g->structuredFullHeight = structuredPreviewHeight(g->stemName + g->dialog.extension, avail);
        child(L"STATIC", g->stemName + g->dialog.extension, SS_LEFT, ID_PREVIEW, m, y, avail,
              g->structuredFullHeight);
        y += g->structuredFullHeight + 6;
        auto targetInfo = child(L"BUTTON", L"", WS_TABSTOP, ID_TARGET_INFO, m, y, avail, 0);
        showIfChanged(targetInfo, SW_HIDE);
        auto status = child(L"STATIC", g->status, SS_LEFT | SS_ENDELLIPSIS, ID_STATUS, m, y + 3, avail, 16);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
    } else
        y += 8;
    auto capabilities = child(L"BUTTON", L"", WS_TABSTOP, ID_CAPABILITIES, m, y, avail, 24);
    tip(capabilities, L"查看当前窗口可以操作的内容、限制及下一步。");
    y += 28;
    g->height = y;
    g->building = false;
    updatePanelContent();
    SendMessageW(g->panel, WM_SETREDRAW, TRUE, 0);
    if (!wasVisible)
        ShowWindow(g->panel, SW_HIDE);
    // WM_SETREDRAW suppresses child painting during reconstruction too. Refresh
    // the complete subtree so old field highlights and candidate captions cannot remain.
    RedrawWindow(g->panel, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    reposition();
    if (focusId) {
        auto edit = GetDlgItem(g->panel, focusId);
        if (edit) {
            SetFocus(edit);
            if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT + 8) || focusId == ID_STRUCT_INPUT ||
                focusId == ID_FREE_NAME_INPUT || focusId == ID_PRESET_VALUE ||
                focusId == ID_VARIABLE_TEMPLATE ||
                (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
                SendMessageW(edit, EM_SETSEL, selectionStart, selectionEnd);
        }
    }
}
bool persist() {
    g->settingsDirty =
        !(g->configWriter ? g->configWriter->saveNow(g->cfg) : jump::save(g->cfg, g->configFile));
    g->configPending = false;
    return !g->settingsDirty;
}
void persistDeferred() {
    if (!g->configWriter) {
        persist();
        return;
    }
    g->settingsDirty = true;
    g->configPending = true;
    g->configWriter->request(g->cfg);
}
bool flushPersistence() {
    if (g->settingsDirty)
        return persist();
    return !g->configWriter || g->configWriter->flush();
}
bool setBubbleColor(const ButtonAction &action, int color) {
    if (color < 0 || color > PRESET_COLOR_COUNT)
        return false;
    auto previous = g->cfg;
    {
        auto entry = bubbleColorEntry(action);
        if (!entry.colors)
            return false;
        if (color)
            (*entry.colors)[entry.key] = color;
        else
            entry.colors->erase(entry.key);
    }
    bool saved = persist();
    if (!saved)
        g->cfg = std::move(previous);
    g->status = !saved ? L"颜色未保存，请检查配置写入权限" : color ? L"已设置颜色标记" : L"已清除颜色标记";
    rebuildPanel();
    return saved;
}
void openInlinePreset(size_t index = NEW_PRESET, HWND anchor = nullptr) {
    closeFlyout();
    std::wstring value;
    if (index != NEW_PRESET) {
        const auto &values = g->namingMode == 1 ? g->cfg.prefixes : g->cfg.suffixes;
        if (index >= values.size())
            return;
        value = values[index];
    }
    g->presetEditor = true;
    g->presetIndex = index;
    g->presetDraft = value;
    KillTimer(g->control, T_ACTION);
    showPresetFlyout(anchor);
}
void saveInlinePreset() {
    if (!g->presetEditor)
        return;
    auto value = g->presetDraft;
    auto error = [&](const std::wstring &message) {
        auto hint = GetDlgItem(g->flyout, ID_PRESET_HINT);
        SetWindowTextW(hint, message.c_str());
    };
    if (!(validAffix(value, g->namingMode == 1))) {
        error(L"请输入有效内容，不能含路径字符");
        return;
    }
    bool added = g->presetIndex == NEW_PRESET;
    // Keep a failed write retryable without adding the same draft twice.
    auto previous = g->cfg;
    {
        auto &values = g->namingMode == 1 ? g->cfg.prefixes : g->cfg.suffixes;
        auto duplicate = std::find(values.begin(), values.end(), value);
        if (duplicate != values.end() &&
            (added || static_cast<size_t>(duplicate - values.begin()) != g->presetIndex)) {
            error(L"已有相同预设");
            return;
        }
        if (added) {
            if (values.size() >= 100) {
                error(L"最多 100 个预设");
                return;
            }
            values.insert(values.begin(), value);
        } else {
            if (g->presetIndex >= values.size()) {
                error(L"预设已变化，请重新选择");
                return;
            }
            moveAffixColor(g->namingMode == 1, values[g->presetIndex], value);
            values[g->presetIndex] = value;
        }
    }
    if (!persist()) {
        g->cfg = std::move(previous);
        error(L"写入失败，请重试");
        updatePanelContent();
        return;
    }
    closeFlyout(true);

    g->status = added ? L"已新增预设，点选即可使用" : L"已修改预设，当前文件名保持不变";
    rebuildPanel();
    if (IsWindowVisible(g->panel))
        SetFocus(GetDlgItem(g->panel, g->namingMode == 1 ? ID_ADD_PREFIX : ID_ADD_SUFFIX));
}
// Exercise the actual panel commands with a hidden window and isolated settings.
// This does not create a host dialog, start the worker, or change foreground focus.
bool hiddenCheckFailure(const char *name, int line) {
    std::fprintf(stderr, "Hidden check failed: %s, main.cpp:%d\n", name, line);
    auto message = std::string(name) + ", main.cpp:" + std::to_string(line);
    auto file = CreateFileW((g->configFile.parent_path() / L"failure.txt").c_str(), GENERIC_WRITE,
                            FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written{};
        WriteFile(file, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
        CloseHandle(file);
    }
    return false;
}

void rememberCurrentDirectory() {
    const auto &directory = g->dialog.directory;
    if (directory.empty())
        return;
    // Polling the same directory is not a new visit, especially after removal.
    if (normalize(g->lastHistoryDirectory) == normalize(directory))
        return;
    g->lastHistoryDirectory = directory;
    if (!g->cfg.history.empty() && normalize(g->cfg.history.front().path) == normalize(directory)) {
        if (g->settingsDirty)
            persistDeferred();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberLocation(g->cfg, {name.empty() ? directory : name, directory, L"历史"});
    persistDeferred();
}
void syncSaveProbe(const std::wstring *targetDirectory = nullptr, const std::wstring *targetStem = nullptr) {
    if (!g->attached || !g->dialog.save)
        return;
    DialogData snapshot = g->dialog;
    if (targetDirectory && !targetDirectory->empty())
        snapshot.directory = *targetDirectory;
    auto stem = targetStem ? *targetStem : g->stemName;
    if (!stem.empty())
        snapshot.name = stem;
    auto current = saveProbeFor(snapshot);
    if (current.file.empty())
        return;
    current.namingStructure = savedStructure();
    current.namingProject = g->cfg.activeProject;
    current.namingKnown = true;
    g->saveProbe = preserveSaveBaseline(g->saveProbe, std::move(current));
}
void syncSaveProbeFromNativeControls() {
    if (!g->attached || !g->dialog.save || g->dialog.generic || !IsWindow(g->dialog.hwnd))
        return;
    DialogData live = g->dialog;
    if (auto dir = directoryFromToolbar(g->dialog.hwnd); !dir.empty())
        live.directory = dir;
    if (nativeValueControl(g->dialog.nameControl, g->dialog.hwnd, L"Edit")) {
        if (auto text = trim(windowText(g->dialog.nameControl));
            !text.empty() && !text.ends_with(L"\\") && !text.ends_with(L"/"))
            live.name = text;
    }
    auto current = saveProbeFor(live);
    if (!current.file.empty())
        g->saveProbe = preserveSaveBaseline(g->saveProbe, std::move(current));
    syncSaveProbe(&live.directory);
}
void verifySavedFile(const SaveProbe &probe) {
    if (probe.file.empty())
        return;
    Job job;
    job.type = JobType::VerifySave;
    job.probe = probe;
    enqueue(std::move(job));
}
void armSaveChecks() {
    KillTimer(g->control, T_SAVE_VERIFY);
    if (g->saveChecks.empty())
        return;
    auto earliest =
        std::min_element(g->saveChecks.begin(), g->saveChecks.end(), [](const Job &a, const Job &b) {
            return a.verifyAt < b.verifyAt;
        })->verifyAt;
    auto now = GetTickCount64();
    SetTimer(g->control, T_SAVE_VERIFY, static_cast<UINT>(earliest > now ? earliest - now : 1), nullptr);
}
void rememberSavedDirectory(const std::wstring &directory) {
    if (directory.empty())
        return;
    if (!g->cfg.recentSaves.empty() && normalize(g->cfg.recentSaves.front().path) == normalize(directory)) {
        if (g->settingsDirty)
            persistDeferred();
        if (g->attached)
            updatePanelContent();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberSavedLocation(g->cfg, {name.empty() ? directory : name, directory, L"最近保存"});
    persistDeferred();
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    if (g->attached && !g->busy && GetForegroundWindow() != g->panel)
        rebuildPanel();
}
void apply(const std::wstring &path, const std::wstring &name, bool submit, bool explicitGeneric = false) {
    if (g->structuredIme || !structuredInputError().empty())
        return;
    if (g->pastePending)
        return;
    // Generic label clicks and Enter remain fill-only. Invoking Save requires its explicit menu command.
    if (g->dialog.generic && !explicitGeneric)
        submit = false;
    if (!g->attached)
        return;
    syncSaveProbe(&path, &name);
    if (g->busy) {
        // Explicit Save during a name fill is serialized behind that fill.
        // Further edits invalidate this intent; background fills never create it.
        if (submit && !g->busySubmitting && g->ruleError.empty() && validName(name))
            g->saveAfterFill =
                Job{JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path, name, true};
        return;
    }
    if (g->numberMode && g->namingWorker && !g->numberingApproved) {
        if (submit) {
            auto displayed = g->stemName;
            invalidateNaming();
            composedName(&path);
            if (!g->ruleError.empty()) {
                g->status = g->ruleError;
                g->statusError = true;
                updatePanelContent();
                return;
            }
            g->namingRequest.submit = true;
            g->namingRequest.displayed = displayed;
            g->namingId = g->namingWorker->request(g->namingRequest);
            g->namingPending = g->namingSubmit = true;
            g->status = L"保存前重新核验编号…修改命名会取消本次提交";
            updatePanelContent();
            return;
        }
        if (g->namingPending) {
            updatePanelContent();
            return;
        }
    }
    if (!g->ruleError.empty()) {
        g->status = g->ruleError;
        updatePanelContent();
        return;
    }
    if (!sameTarget({JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load()})) {
        g->status = L"原对话框已失效，未执行操作。";
        updatePanelContent();
        return;
    }
    if (g->dialog.save && !validName(name) && !(g->dialog.generic && name.empty() && !submit)) {
        g->status = L"请输入有效的文件名，不要包含路径字符。";
        updatePanelContent();
        return;
    }
    g->busy = true;
    g->busySubmitting = submit;
    g->localNamePending = true;
    g->applyFocus = GetForegroundWindow() == g->panel ? GetFocus() : nullptr;
    g->status = submit ? L"正在核验并提交保存…" : L"正在填入…";
    updatePanelContent();
    Job job{JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path, name, submit};
    job.namingStructure = savedStructure();
    job.namingProject = g->cfg.activeProject;
    // Transfer focus from the foreground UI thread, before the UIA worker needs
    // to press Enter in a custom address editor.
    if (g->dialog.genericAddress && !path.empty() && normalize(path) != normalize(g->dialog.directory) &&
        focusAllowsAction(job))
        SetForegroundWindow(job.hwnd);
    enqueue(job);
    for (auto h : g->children)
        if ((windowClass(h) == L"Button" && GetDlgCtrlID(h) != ID_CLOSE &&
             !liveStructuredControl(GetDlgCtrlID(h)) &&
             !((GetDlgCtrlID(h) >= ID_LOC_RECOMMENDED && GetDlgCtrlID(h) <= ID_LOC_HISTORY) ||
               GetDlgCtrlID(h) == ID_LOC_RECENT) &&
             !(GetDlgCtrlID(h) == ID_SUBMIT && !submit && !g->dialog.generic) &&
             !g->actions.contains(GetDlgCtrlID(h))))
            enableIfChanged(h, FALSE);
}
std::wstring pathEditValue(std::wstring value) {
    value = trim(value);
    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"')
        value = value.substr(1, value.size() - 2);
    std::replace(value.begin(), value.end(), L'/', L'\\');
    bool drive = value.size() > 2 && iswalpha(value[0]) && value[1] == L':' && value[2] == L'\\';
    bool network = value.starts_with(L"\\\\") && value.find(L'\\', 2) != value.npos;
    if ((!drive && !network) || value.find_first_of(L"\r\n\t\"<>|*?") != value.npos)
        return {};
    return value;
}
void beginPathEdit() {
    if (g->busy)
        return;
    invalidateNaming();
    closeFlyout();
    KillTimer(g->control, T_ACTION);
    g->pathDraft = g->dialog.directory;
    g->editingPath = true;
    g->status = L"Enter 切换目录 · 点击别处或 Esc 退出编辑";
    updatePanelContent();
    if (IsWindowVisible(g->panel)) {
        SetForegroundWindow(g->panel);
        auto input = GetDlgItem(g->panel, ID_DIRECTORY_INPUT);
        SetFocus(input);
        SendMessageW(input, EM_SETSEL, 0, -1);
    }
}
void cancelPathEdit(bool restoreFocus) {
    if (!g->editingPath)
        return;
    if (g->pastePending)
        invalidateNaming();
    g->editingPath = false;
    g->pathDraft.clear();
    g->status = L"已取消路径编辑";
    updatePanelContent();
    if (g->attached && !g->busy && g->dirtyName && g->localNamePending)
        SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    if (restoreFocus && IsWindowVisible(g->panel) && GetForegroundWindow() == g->panel)
        SetFocus(GetDlgItem(g->panel, ID_DIRECTORY));
}
void commitPathEdit() {
    if (!g->editingPath || g->busy)
        return;
    g->pathDraft = windowText(GetDlgItem(g->panel, ID_DIRECTORY_INPUT));
    auto path = pathEditValue(g->pathDraft);
    if (path.empty()) {
        g->status = L"请输入完整文件夹路径，例如 C:\\Users\\用户名\\Downloads";
        updatePanelContent();
        return;
    }
    g->editingPath = false;
    if (normalize(path) == normalize(g->dialog.directory)) {
        g->status = L"已在该目录";
        updatePanelContent();
        if (g->attached && g->dirtyName && g->localNamePending)
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
        return;
    }
    if (g->numberMode || g->folderName || g->variablesEnabled) {
        g->stemName = composedName(&path);
        if (!g->ruleError.empty()) {
            g->status = g->ruleError;
            g->statusError = true;
            updatePanelContent();
            return;
        }
    }
    apply(path, g->stemName, false);
    g->pathPending = g->busy;
    g->applyFocus = nullptr;
    // A counter scan continues this navigation asynchronously. Reopening the
    // editor here races its queued blur and can replace the result with "cancelled".
    if (!g->busy && !g->namingPending) {
        g->editingPath = true;
        updatePanelContent();
    }
}
#include "paste_target_ui.h"
NamingSnapshot captureNaming() {
    auto options = namingOptions();
    auto parts = g->parts;
    // Derived output is recomputed on restore and must not create phantom undo steps.
    if (g->structure.enabled && !g->structure.free) {
        parts.base.clear();
        parts.stage.clear();
        parts.date.clear();
    }
    return {parts,
            options.expression,
            options.variables,
            g->rulePattern,
            g->ruleReplacement,
            g->numberMode,
            options.digits,
            options.dateFormat,
            g->ruleIndex,
            g->folderName,
            g->variablesEnabled,
            options.upperVersion,
            g->ruleEnabled,
            g->ruleRegex,
            g->structure,
            g->freeRecovery ? std::make_shared<NamingSnapshot>(*g->freeRecovery) : nullptr};
}
void checkpointNaming() {
    g->namingHistory.checkpoint(captureNaming());
    g->structuredCheckpoint = false;
}
void moveNamingHistory(bool redo) {
    if (!g->attached || g->busy || composingNaming())
        return;
    auto value = g->namingHistory.move(captureNaming(), redo);
    if (!value)
        return;
    restoreNamingState(*value);
    g->stemName = composedName();
    g->dirtyName = g->localNamePending = true;
    g->statusError = !g->ruleError.empty();
    g->status = g->statusError ? g->ruleError : redo ? L"已重做命名组合" : L"已撤销命名组合";
    rebuildPanel();
    if (g->ruleError.empty())
        apply(g->dialog.directory, g->stemName, false);
}
Recipe captureRecipe(const std::wstring &name) {
    Recipe recipe{name, g->dialog.directory, g->parts.base, false, !g->parts.date.empty()};
    recipe.complete = true;
    if (g->structure.enabled && !g->structure.free) {
        auto structure = g->structure;
        structure.counter = g->numberMode;
        structure.digits = namingOptions().digits;
        structure.dateFormat = namingOptions().dateFormat;
        structure.upper = namingOptions().upperVersion;
        recipe.structure = serializeStructure(structure);
    }
    recipe.prefix = g->parts.prefix;
    recipe.suffix = g->parts.suffix;
    recipe.stageValue = g->parts.stage;
    for (const auto &preset : g->cfg.prefixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.prefix) {
            recipe.prefix = preset;
            break;
        }
    for (const auto &preset : g->cfg.suffixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.suffix) {
            recipe.suffix = preset;
            break;
        }
    auto options = namingOptions();
    recipe.counter = options.counter;
    recipe.folder = options.folder;
    recipe.variables = g->variablesEnabled;
    recipe.expression = options.expression;
    // Project values stay with their project. A recipe stores only the session/default values.
    recipe.variableValues = g->sessionExpression.empty() ? g->cfg.variableValues : g->sessionVariables;
    recipe.digits = options.digits;
    recipe.upperVersion = options.upperVersion;
    recipe.dateFormat = options.dateFormat;
    recipe.ruleEnabled = g->ruleEnabled;
    recipe.ruleRegex = g->ruleRegex;
    recipe.pattern = g->rulePattern;
    recipe.replacement = g->ruleReplacement;
    if (const auto *project = activeProject(g->cfg)) {
        auto relative = std::filesystem::path(recipe.directory).lexically_relative(project->root).wstring();
        if (!relative.empty() && !projectPath(*project, relative).empty()) {
            recipe.directory = relative;
            recipe.relativeDirectory = true;
        }
    }
    return recipe;
}
void perform(ButtonAction action, bool quick) {
    if (!g->attached)
        return;
    bool affix = action.kind == Action::Prefix || action.kind == Action::Suffix;
    bool smart = action.kind == Action::Folder || action.kind == Action::Variables;
    if (affix || smart)
        quick = false;
    bool localNaming = affix || smart;
    if (g->busy && (quick || !localNaming)) {
        if (quick && action.kind == g->lastAction.kind && action.index == g->lastAction.index)
            g->pendingQuick = true;
        return;
    }
    // Compose local clicks even while an older fill is in flight. WM_RESULT
    // already keeps the newer stem and sends the latest name after that fill.
    if (g->busy && localNaming)
        g->pendingQuick = false;
    checkpointNaming();
    invalidateNaming();
    g->lastAction = action;
    g->editingPath = false;
    std::wstring path = g->dialog.directory, name = g->stemName;
    bool updateName = false;
    switch (action.kind) {
    case Action::Location:
        if (action.index >= g->displayPaths.size())
            return;
        path = g->displayPaths[action.index].path;
        updateName = g->numberMode || g->folderName || g->variablesEnabled;
        break;
    case Action::Folder:
        g->folderName = !g->folderName;
        updateName = true;
        quick = false;
        break;
    case Action::Variables:
        if (!g->variablesEnabled &&
            (trim(g->cfg.variableTemplate).empty() ||
             !smartNamingPreview(
                  compose(g->parts), path, g->dialog.extension,
                  {0, false, g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression,
                   effectiveVariables(), g->cfg.dateFormat})
                  .error.empty())) {
            openVariableEditor();
            return;
        }
        g->variablesEnabled = !g->variablesEnabled;
        updateName = true;
        quick = false;
        break;
    case Action::Prefix:
    case Action::Suffix: {
        bool prefix = action.kind == Action::Prefix;
        const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
        if (action.index >= values.size())
            return;
        auto value = expandAffix(values[action.index], namingOptions().dateFormat);
        if (!prefix)
            if (auto field = structuredSuffixField(g->structure, g->cfg, value)) {
                auto next = namingFieldCanOmit(g->structure,
                                               static_cast<size_t>(field - g->structure.fields.data())) &&
                                    fieldValue(*field, g->cfg, g->structure.dateFormat) == value.substr(1)
                                ? L""
                                : value.substr(1);
                field->value = next;
                field->source = 0;
                field->omitted = next.empty();
                g->parts.suffix.clear();
                if (g->structuredField == static_cast<int>(field - g->structure.fields.data()))
                    resetStructuredInput();
                updateName = true;
                g->dirtyName = true;
                break;
            }
        auto &selected = prefix ? g->parts.prefix : g->parts.suffix;
        if (selected == value)
            selected.clear();
        else {
            // Adopt an identical literal affix already in the name rather than duplicating it.
            auto body = composeBody(g->parts);
            if (selected.empty() && (prefix ? body.starts_with(value) : body.ends_with(value))) {
                if (prefix)
                    body.erase(0, value.size());
                else
                    body.resize(body.size() - value.size());
                auto oldPrefix = g->parts.prefix, oldSuffix = g->parts.suffix;
                g->parts = splitName(body, g->cfg.modifiers);
                g->parts.prefix = oldPrefix;
                g->parts.suffix = oldSuffix;
            }
            if (!prefix && std::any_of(g->cfg.modifiers.begin(), g->cfg.modifiers.end(),
                                       [&](const auto &m) { return value == L"_" + m; }))
                g->parts.stage.clear();
            if (!prefix && value == L"_" + g->parts.date)
                g->parts.date.clear();
            (prefix ? g->parts.prefix : g->parts.suffix) = value;
        }
        name = compose(g->parts);
        g->dirtyName = true;
        updateName = true;
        // A pill is always a fill action; affix double-clicks must not accidentally save.
        quick = false;
        break;
    }
    case Action::Recipe: {
        if (action.index >= g->cfg.recipes.size())
            return;
        auto &r = g->cfg.recipes[action.index];
        if (r.namingOnly) {
            useNamingScheme(deserializeStructure(r.structure));
            return;
        }

        path = r.directory.empty() ? g->dialog.directory : r.directory;
        if (r.relativeDirectory) {
            const auto *project = activeProject(g->cfg);
            path = project ? projectPath(*project, r.directory) : L"";
            if (path.empty()) {
                g->status = L"此方案使用项目相对目录，请先选择项目并设置根目录。";
                g->statusError = true;
                updatePanelContent();
                return;
            }
        }
        resetStructuredInput();
        g->inlineTool = 0;
        g->structure = {};
        g->structuredField = -1;
        g->freeRecovery.reset();
        if (r.complete) {
            g->structure = deserializeStructure(r.structure);
            g->structuredField = -1;
            g->freeRecovery.reset();
            g->parts = {r.value, r.stageValue, r.date ? today(r.dateFormat) : L"",
                        expandAffix(r.prefix, r.dateFormat), expandAffix(r.suffix, r.dateFormat)};
            g->numberMode = r.counter;
            g->folderName = r.folder;
            g->variablesEnabled = r.variables;
            g->sessionExpression = r.expression;
            g->sessionVariables = r.variableValues;
            g->sessionDigits = r.digits;
            g->sessionDateFormat = r.dateFormat;
            g->sessionUpper = r.upperVersion;
            g->ruleEnabled = r.ruleEnabled;
            g->ruleRegex = r.ruleRegex;
            g->rulePattern = r.pattern;
            g->ruleReplacement = r.replacement;
            name = compose(g->parts);
        } else if (r.stage) {
            g->parts.stage = r.value;
            if (r.date)
                g->parts.date = today(namingOptions().dateFormat);
            name = compose(g->parts);
        } else {
            g->parts = splitName(r.value, g->cfg.modifiers);
            if (r.date)
                g->parts.date = today(namingOptions().dateFormat);
            name = compose(g->parts);
        }
        updateName = true;
        g->rebuildPending = true;
        PostMessageW(g->control, WM_PANEL_REFRESH, 0, 0);
        break;
    }
    }
    if (updateName) {
        name = composedName(&path);
        g->stemName = name;
        g->dirtyName = true;
        g->statusError = !g->ruleError.empty();
        if (g->statusError)
            g->status = g->ruleError;
        g->localNamePending = true;
        g->building = true;
        setTextIfChanged(GetDlgItem(g->panel, ID_PREVIEW), name + g->dialog.extension);
        g->building = false;
        updatePanelContent();
    }
    if (g->ruleError.empty()) {
        syncSaveProbe(&path, &name);
        apply(path, name, quick && g->cfg.quickSave && g->dialog.save);
    }
}
void updateInlineRule(bool immediate) {
    invalidateNaming();
    KillTimer(g->control, T_ACTION);
    g->stemName = composedName();
    syncSaveProbe();
    g->dirtyName = true;
    g->localNamePending = true;
    g->status = g->ruleError.empty() ? L"命名预览已更新，规则收起后继续生效" : g->ruleError;
    g->statusError = !g->ruleError.empty();
    updatePanelContent();
    if (g->ruleError.empty()) {
        if (immediate && g->attached && !g->editingPath &&
            focusAllowsAction({JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load()}))
            apply(g->dialog.directory, g->stemName, false);
        else
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    }
}
void updateVariableDraftPreview() {
    if (!g->manager || g->managerTab != MANAGER_TEMPLATES)
        return;
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_TEMPLATE))
        g->variableTemplateDraft = windowText(edit);
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_VALUES))
        g->variableValuesDraft = windowText(edit);
    if (!g->variableAdvanced) {
        for (size_t i = 0; i < g->variableInputKeys.size(); ++i)
            if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_FIELD_FIRST + static_cast<int>(i)))
                setVariableDraftValue(g->variableInputKeys[i], trim(windowText(edit)));
    }
    auto preview = smartNamingPreview(
        g->parts.base.empty() ? L"文件名" : compose(g->parts), g->dialog.directory, g->dialog.extension,
        {0, false, g->variableTemplateDraft, g->variableValuesDraft, g->cfg.dateFormat});
    if (trim(g->variableTemplateDraft).empty())
        preview.error = L"至少点选一个命名部分来组成文件名";
    auto hint = GetDlgItem(g->manager, ID_VARIABLE_HINT);
    auto text = preview.error.empty() ? L"预览：" + preview.name : preview.error;
    setTextIfChanged(hint, text);
    enableIfChanged(GetDlgItem(g->manager, ID_VARIABLE_SAVE), preview.error.empty());
}
void initializeVariableDraft() {
    if (!g->variableEditor) {
        g->variableTemplateDraft =
            g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression;
        g->variableValuesDraft = effectiveVariables();
        g->variableAdvanced = false;
        auto values = variableValues(g->variableValuesDraft);
        if (!values.contains(L"项目")) {
            auto folder = g->dialog.directory;
            while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/'))
                folder.pop_back();
            folder = std::filesystem::path(folder).filename().wstring();
            if (validName(folder))
                setVariableDraftValue(L"项目", folder);
        }
        if (!values.contains(L"作者")) {
            wchar_t user[256]{};
            DWORD length = static_cast<DWORD>(std::size(user));
            if (GetUserNameW(user, &length) && validName(user))
                setVariableDraftValue(L"作者", user);
        }
    }
    g->variableEditor = true;
}
void openVariableEditor(bool present) {
    initializeVariableDraft();
    g->managerTab = MANAGER_TEMPLATES;
    showManager(present);
    if (present && IsWindowVisible(g->manager)) {
        auto field = GetDlgItem(g->manager, ID_VARIABLE_FIELD_FIRST);
        SetFocus(field ? field : GetDlgItem(g->manager, ID_VARIABLE_SAVE));
    }
}

void hidePanel() {
    closeFlyout();
    ShowWindow(g->panel, SW_HIDE);
}
bool attachedDialogAlive() {
    DWORD pid = 0;
    return IsWindow(g->dialog.hwnd) && GetWindowThreadProcessId(g->dialog.hwnd, &pid) && pid == g->dialog.pid;
}
void detach() {
    rememberNamingDraft();
    g->nativeSubjectPending.reset();
    invalidateNaming();
    g->namingHistory.clear();
    if (g->attached && (!attachedDialogAlive() || !IsWindowVisible(g->dialog.hwnd)))
        verifySavedFile(g->saveProbe);
    g->saveProbe = {};
    g->attached = false;
    g->editingPath = g->pathPending = false;
    g->pathDraft.clear();
    g->rebuildPending = false;
    g->busy = false;
    ++g->generation;
    hidePanel();
    for (auto hook : g->targetHooks)
        UnhookWinEvent(hook);
    g->targetHooks.clear();
    KillTimer(g->control, T_REFRESH);
    KillTimer(g->control, T_ACTION);
    KillTimer(g->control, T_SEARCH);
    KillTimer(g->control, T_GENERIC_REFRESH);
    g->pendingQuick = false;
    g->presetEditor = false;
}
void CALLBACK winEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG object, LONG child, DWORD, DWORD) {
    if (!g || !hwnd)
        return;
    bool value = event == EVENT_OBJECT_VALUECHANGE || event == EVENT_OBJECT_NAMECHANGE;
    bool windowName = event == EVENT_OBJECT_NAMECHANGE && object == OBJID_WINDOW && child == CHILDID_SELF &&
                      GetAncestor(hwnd, GA_ROOT) == hwnd;
    if (event != EVENT_SYSTEM_FOREGROUND && !value && (object != OBJID_WINDOW || child != CHILDID_SELF))
        return;
    if (value && !windowName && (!g->attached || GetAncestor(hwnd, GA_ROOT) != g->dialog.hwnd))
        return;
    if ((event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_DESTROY) && hwnd != g->dialog.hwnd)
        return;
    if (event == EVENT_OBJECT_SHOW &&
        (GetAncestor(hwnd, GA_ROOT) != hwnd ||
         (windowClass(hwnd) != L"#32770" && !windowClass(hwnd).starts_with(L"GHOST_WindowClass") &&
          !houdiniBrowserWindow(hwnd) && !genericCandidate(hwnd))))
        return;
    PostMessageW(g->control, WM_EVENT, event, reinterpret_cast<LPARAM>(hwnd));
}
void targetHooks() {
    for (auto hook : g->targetHooks)
        UnhookWinEvent(hook);
    g->targetHooks.clear();
    for (auto event : {EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_NAMECHANGE, EVENT_OBJECT_VALUECHANGE}) {
        auto hook = SetWinEventHook(event, event, nullptr, winEvent, g->dialog.pid, 0,
                                    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (hook)
            g->targetHooks.push_back(hook);
    }
}
void inspectForeground(bool requested = false) {
    HWND foreground = GetForegroundWindow();
    if (!foreground || (!requested && (g->paused || !g->cfg.automatic)))
        return;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (pid == g->pid)
        return;
    if (g->testTargetPid && pid != g->testTargetPid) {
        if (g->attached)
            hidePanel();
        return;
    }
    if (!g->testTargetImage.empty() && processImage(foreground) != g->testTargetImage) {
        if (g->attached)
            hidePanel();
        return;
    }
    if (requested) {
        if (!IsWindowVisible(foreground) || !IsWindowEnabled(foreground) || IsHungAppWindow(foreground))
            return;
        g->suppressed = nullptr;
        g->inspectAttempts = 0;
    }
    if (g->attached && (!attachedDialogAlive() || !IsWindowVisible(g->dialog.hwnd)))
        detach();
    if (g->attached && foreground == g->dialog.hwnd && pid == g->dialog.pid) {
        if (IsWindowEnabled(foreground) && g->suppressed != foreground) {
            reposition(true);
        }
        return;
    }
    if (g->attached)
        hidePanel();
    if ((!requested && !looksLikeFileDialog(foreground)) || g->suppressed == foreground)
        return;
    if (g->bindingHint)
        return;
    if (g->inspectCandidate != foreground) {
        g->inspectCandidate = foreground;
        g->inspectAttempts = 0;
    }
    if (g->inspectAttempts++ >= 4)
        return;
    if (g->attached)
        detach();
    ULONGLONG gen = ++g->generation;
    g->dialog.hwnd = foreground;
    GetWindowThreadProcessId(foreground, &g->dialog.pid);
    Job job{JobType::Inspect, foreground, g->dialog.pid, gen};
    job.requestedInspect = requested;
    enqueue(std::move(job));
}
