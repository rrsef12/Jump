#pragma once

void showRecipeMenu() {
    if (!IsWindowVisible(g->panel))
        return;
    auto menu = CreatePopupMenu();
    for (size_t i = 0; i < g->cfg.recipes.size(); ++i)
        if (!g->cfg.recipes[i].namingOnly)
            AppendMenuW(menu, MF_STRING, 4000 + i, g->cfg.recipes[i].name.c_str());
    if (GetMenuItemCount(menu))
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_CAPTURE_RECIPE, L"将当前整套设置保存为流程…");
    AppendMenuW(menu, MF_STRING, 2, L"添加可填写变量的示例方案");
    AppendMenuW(menu, MF_STRING, 1, L"管理保存流程…");
    auto anchor = GetDlgItem(g->panel, g->structure.enabled ? ID_STRUCT_SCHEMES : ID_NAMING_MENU);
    RECT rect{};
    GetWindowRect(anchor ? anchor : g->panel, &rect);
    SetForegroundWindow(g->panel);
    int selected = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, rect.left, rect.bottom, g->panel);
    DestroyMenu(menu);
    if (selected == 2)
        addExampleRecipes();
    else if (selected == ID_CAPTURE_RECIPE)
        SendMessageW(g->panel, WM_COMMAND, ID_CAPTURE_RECIPE, 0);
    else if (selected == 1) {
        g->managerTab = 2;
        showManager();
    } else if (selected >= 4000 && static_cast<size_t>(selected - 4000) < g->cfg.recipes.size())
        SendMessageW(g->panel, WM_COMMAND, selected, 0);
}
LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g && !g->testTargetImage.empty()) {
        static LRESULT paintSerial = 0, accessibilityQueries = 0;
        if (msg == WM_APP + 200)
            return wp ? accessibilityQueries : paintSerial;
        if (msg == WM_PAINT || msg == WM_DRAWITEM)
            ++paintSerial;
        if (msg == WM_GETOBJECT)
            ++accessibilityQueries;
    }
    switch (msg) {
    case WM_VSCROLL:
        if (reinterpret_cast<HWND>(lp) == GetDlgItem(hwnd, ID_FOLDER_SCROLL) && !g->busy) {
            SCROLLINFO info{sizeof(info), SIF_TRACKPOS};
            GetScrollInfo(reinterpret_cast<HWND>(lp), SB_CTL, &info);
            int code = LOWORD(wp);
            if (code == SB_LINEUP)
                --g->locationPage;
            else if (code == SB_LINEDOWN)
                ++g->locationPage;
            else if (code == SB_PAGEUP)
                g->locationPage -= 3;
            else if (code == SB_PAGEDOWN)
                g->locationPage += 3;
            else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION)
                g->locationPage = info.nTrackPos;
            else if (code == SB_TOP)
                g->locationPage = 0;
            else if (code == SB_BOTTOM)
                g->locationPage = INT_MAX;
            updatePanelContent();
            return 0;
        }
        break;
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd, &point);
        RECT folders{px(PANEL_MARGIN), px(g->folderTop), px(g->width - PANEL_MARGIN),
                     px(g->folderTop + g->folderRows * FOLDER_ROW_STEP - 8)};
        if (PtInRect(&folders, point) && !g->busy) {
            g->folderWheel += GET_WHEEL_DELTA_WPARAM(wp);
            int rows = g->folderWheel / WHEEL_DELTA;
            g->folderWheel %= WHEEL_DELTA;
            if (rows) {
                g->locationPage -= rows;
                updatePanelContent();
            }
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN:
        cancelPathEdit(false);
        closeFlyout();
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        requestEditorDismiss(true);
        return 0;
    case WM_EDITOR_DISMISS:
        if ((static_cast<ULONG_PTR>(lp) >> 1) == g->editorRevision)
            finishEditorDismiss(reinterpret_cast<HWND>(wp), (lp & 1) != 0);
        return 0;
    case WM_TIMER:
        if (wp == T_COPY_FEEDBACK) {
            KillTimer(hwnd, T_COPY_FEEDBACK);
            setTextIfChanged(GetDlgItem(hwnd, ID_FINAL_NAME_LABEL), L"最终文件名");
            return 0;
        }
        if (wp == T_EDITOR_DISMISS) {
            KillTimer(hwnd, T_EDITOR_DISMISS);
            finishEditorDismiss(g->pendingEditorDismiss, g->explicitEditorDismiss);
            return 0;
        }
        break;
    case WM_PATH_BLUR:
        if (g->editingPath &&
            (GetForegroundWindow() != hwnd || (GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_INPUT) &&
                                               GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_EDIT))))
            cancelPathEdit(false);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g->editingPath)
            PostMessageW(hwnd, WM_PATH_BLUR, 0, 0);
        if (LOWORD(wp) != WA_INACTIVE)
            rememberNativeSubject();
        if (LOWORD(wp) == WA_INACTIVE)
            requestEditorDismiss((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
                                 (GetAsyncKeyState(VK_RBUTTON) & 0x8000));
        break;
    case WM_CONTEXTMENU: {
        closeFlyout();
        auto h = reinterpret_cast<HWND>(wp);
        if (GetParent(h) == hwnd && GetDlgCtrlID(h) == ID_PREVIEW) {
            bool copyable = g->ruleError.empty() && !g->namingPending && structuredInputError().empty() &&
                            validName(g->stemName);
            auto menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (copyable ? 0 : MF_GRAYED), ID_COPY_NAME, L"复制完整文件名");
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (point.x == -1 && point.y == -1) {
                RECT r{};
                GetWindowRect(h, &r);
                point = {r.left, r.bottom};
            }
            SetForegroundWindow(hwnd);
            int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
            DestroyMenu(menu);
            if (command)
                SendMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        if (g->busy)
            return 0;
        if (renameStructuredCandidate(h))
            return 0;
        if (GetDlgCtrlID(h) == ID_DIRECTORY) {
            auto menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_EDIT, L"编辑路径");
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_SEARCH, L"搜索位置…");
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (point.x == -1 && point.y == -1) {
                RECT r{};
                GetWindowRect(h, &r);
                point = {r.left, r.bottom};
            }
            SetForegroundWindow(hwnd);
            int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
            DestroyMenu(menu);
            if (command)
                SendMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        auto found = g->actions.find(GetDlgCtrlID(h));
        if (GetParent(h) != hwnd || found == g->actions.end())
            return 0;
        auto action = found->second;
        if (action.kind == Action::Variables) {
            openVariableEditor();
            return 0;
        }
        bool folder = action.kind == Action::Location;
        if (!folder && action.kind != Action::Prefix && action.kind != Action::Suffix)
            return 0;
        if (folder && action.index >= g->displayPaths.size())
            return 0;
        HMENU menu = CreatePopupMenu();
        if (!folder)
            AppendMenuW(menu, MF_STRING, 3, L"修改预设");
        HMENU colors = CreatePopupMenu();
        int marked = bubbleColorIndex(action);
        for (int i = 0; i <= PRESET_COLOR_COUNT; ++i)
            AppendMenuW(colors, MF_STRING | (i == marked ? MF_CHECKED : 0), 100 + i, bubbleColors[i].name);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colors), L"颜色标记");
        if (folder)
            AppendMenuW(menu, MF_STRING, 4,
                        isFavoritePath(g->displayPaths[action.index].path) ? L"取消收藏" : L"收藏文件夹");
        else
            AppendMenuW(menu, MF_STRING, 1, L"移除此预设");
        if (folder && g->locationTab == 2)
            AppendMenuW(menu, MF_STRING, 6, L"从历史移除");
        if (folder && g->locationTab == 4)
            AppendMenuW(menu, MF_STRING, 7, L"从最近移除");
        AppendMenuW(menu, MF_STRING, 2,
                    folder                          ? L"管理收藏…"
                    : action.kind == Action::Prefix ? L"管理前缀预设…"
                    : action.kind == Action::Suffix ? L"管理后缀预设…"
                                                    : L"管理命名预设…");
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (point.x == -1 && point.y == -1) {
            RECT r{};
            GetWindowRect(h, &r);
            point = {r.left, r.bottom};
        }
        SetForegroundWindow(hwnd);
        int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
        DestroyMenu(menu);
        if (command == 1) {
            auto &values = action.kind == Action::Prefix ? g->cfg.prefixes : g->cfg.suffixes;
            if (action.index < values.size()) {
                auto previous = g->cfg;
                moveAffixColor(action.kind == Action::Prefix, values[action.index]);
                values.erase(values.begin() + action.index);
                bool saved = persist();
                if (!saved)
                    g->cfg = std::move(previous);
                g->status = saved ? L"已移除预设，当前文件名保持不变" : L"移除未保存，原预设已保留";
                g->statusError = !saved;
                rebuildPanel();
            }
        } else if (command == 2) {
            if (folder) {
                g->managerTab = 1;
                showManager();
            } else {
                g->namingMode = action.kind == Action::Prefix ? 1 : 2;
                showPresetManager(false, action.index);
            }
        } else if (command == 3) {
            g->namingMode = action.kind == Action::Prefix ? 1 : 2;
            openInlinePreset(action.index, h);

        } else if (command == 4 && folder) {
            auto location = g->displayPaths[action.index];
            toggleFavoriteLocation(location);
        } else if (command == 6 && folder && g->locationTab == 2) {
            auto path = g->displayPaths[action.index].path;
            removeHistoryLocation(path);
        } else if (command == 7 && folder && g->locationTab == 4) {
            auto path = g->displayPaths[action.index].path;
            removeRecentLocations(path);
        } else if (command >= 100 && command <= 100 + PRESET_COLOR_COUNT) {
            setBubbleColor(action, command - 100);
        }
        return 0;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_CLOSE:
        invalidateNaming();
        g->suppressed = g->dialog.hwnd;
        ++g->generation;
        g->busy = false;
        g->pendingQuick = false;
        KillTimer(g->control, T_ACTION);
        hidePanel();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(hwnd, msg, wp);
        HDC dc = paint.dc();
        RECT r;
        GetClientRect(hwnd, &r);
        FillRect(dc, &r, g->brush);
        SetBkMode(dc, TRANSPARENT);
        auto old = SelectObject(dc, g->smallFont);
        auto pen = CreatePen(PS_SOLID, 1, g->line);
        auto op = SelectObject(dc, pen);
        // A single fine edge defines the floating surface; section separation
        // comes from spacing and typography rather than stacked card borders.
        auto edgeBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(14), px(14));
        SelectObject(dc, edgeBrush);
        if (!IsRectEmpty(&g->locationTabsRect)) {
            auto track = CreateSolidBrush(g->field);
            auto oldBrush = SelectObject(dc, track), oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            const auto &tabs = g->locationTabsRect;
            paintRoundedRect(dc, tabs.left, tabs.top, tabs.right, tabs.bottom, px(16), px(16));
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(track);
        }
        for (int y : g->dividers) {
            MoveToEx(dc, px(PANEL_MARGIN), y, nullptr);
            LineTo(dc, r.right - px(PANEL_MARGIN), y);
        }
        if (g->editingPath) {
            paintInputFrame(dc, hwnd, g->pathEditRect, px(8));
        }
        for (const auto &field : g->fields)
            paintInputFrame(dc, hwnd, field, px(10));
        SelectObject(dc, op);
        DeleteObject(pen);
        for (auto &[rect, text, heading] : g->labels) {
            auto copy = rect;
            SelectObject(dc, heading ? g->boldFont : g->smallFont);
            SetTextColor(dc, heading ? g->fg : g->muted);
            auto separator = text.find(L" · ");
            if (heading && rect.top < px(12) && separator != text.npos) {
                auto brand = text.substr(0, separator), caption = text.substr(separator + 3);
                DrawTextW(dc, brand.c_str(), -1, &copy, DT_SINGLELINE | DT_VCENTER);
                SIZE size{};
                GetTextExtentPoint32W(dc, brand.c_str(), static_cast<int>(brand.size()), &size);
                copy.left += size.cx + px(10);
                SelectObject(dc, g->smallFont);
                SetTextColor(dc, g->muted);
                DrawTextW(dc, caption.c_str(), -1, &copy, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                continue;
            }
            DrawTextW(dc, text.c_str(), -1, &copy, DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
        }
        SelectObject(dc, old);
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->field);
        return reinterpret_cast<LRESULT>(g->inputBrush);
    case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->field);
        return reinterpret_cast<LRESULT>(g->inputBrush);
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wp), g->muted);
        if (windowClass(reinterpret_cast<HWND>(lp)) == L"Edit") {
            SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
            SetBkColor(reinterpret_cast<HDC>(wp), g->field);
            return reinterpret_cast<LRESULT>(g->inputBrush);
        }
        if (GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == ID_STATUS && g->statusError)
            SetTextColor(reinterpret_cast<HDC>(wp), RGB(226, 122, 122));
        if (GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == ID_RULE_HINT && !g->rulePreview.error.empty())
            SetTextColor(reinterpret_cast<HDC>(wp), RGB(226, 122, 122));
        SetBkColor(reinterpret_cast<HDC>(wp), g->bg);
        return reinterpret_cast<LRESULT>(g->brush);
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (d->CtlType == ODT_COMBOBOX)
            return drawDropdownItem(*d);
        if (d->CtlType == ODT_STATIC) {
            DWORD_PTR textStyle = 0;
            if (!GetWindowSubclass(d->hwndItem, MaterialControlProc, 2, &textStyle))
                return FALSE;
            PaintBuffer buffer(d->hDC, d->rcItem, panelComposited(d->hwndItem) && IsWindowVisible(g->panel));
            paintMaterialText(d->hwndItem, buffer.dc, d->rcItem, static_cast<DWORD>(textStyle));
            return TRUE;
        }
        if (d->CtlType != ODT_BUTTON)
            return FALSE;
        DRAWITEMSTRUCT item = *d;
        PaintBuffer buffer(item.hDC, item.rcItem,
                           panelComposited(item.hwndItem) && IsWindowVisible(g->panel));
        item.hDC = buffer.dc;
        d = &item;
        if (drawSubjectGroup(*d))
            return TRUE;
        auto text = windowText(d->hwndItem);
        const auto &actions = GetParent(d->hwndItem) == g->flyout ? g->flyoutActions : g->actions;
        bool isPill = actions.contains(static_cast<int>(d->CtlID));
        bool variableChip = GetParent(d->hwndItem) == g->panel && g->variableEditor &&
                            d->CtlID >= ID_VARIABLE_TOKEN_FIRST &&
                            d->CtlID < ID_VARIABLE_TOKEN_FIRST + g->variableTokens.size();
        RECT r = d->rcItem;
        FillRect(d->hDC, &r, g->brush);
        DWORD_PTR dropdownFeedback = 0;
        GetWindowSubclass(d->hwndItem, PillProc, 1, &dropdownFeedback);
        bool hover = (dropdownFeedback & 1) != 0;
        bool selected = false;
        bool namingView = d->CtlID == ID_STRUCT_QUICK_VIEW || d->CtlID == ID_STRUCT_FIELDS_VIEW;
        if (namingView)
            selected = d->CtlID == ID_STRUCT_QUICK_VIEW ? quickNamingView() : !quickNamingView();
        bool quickPick = d->CtlID >= ID_QUICK_PICK && d->CtlID < ID_QUICK_PICK + 48;
        if (quickPick) {
            int index = (d->CtlID - ID_QUICK_PICK) / 6, slot = (d->CtlID - ID_QUICK_PICK) % 6;
            if (static_cast<size_t>(index) < g->structure.fields.size() && g->quickPicks.contains(index) &&
                static_cast<size_t>(slot) < g->quickPicks[index].size())
                selected = g->quickPicks[index][slot].value ==
                           fieldValue(g->structure.fields[index], g->cfg, g->structure.dateFormat);
        }
        bool structuredField =
            d->CtlID >= ID_STRUCT_FIELD && d->CtlID < ID_STRUCT_FIELD + g->structure.fields.size();
        if (structuredField)
            selected = static_cast<int>(d->CtlID) - ID_STRUCT_FIELD == g->structuredField;
        if (d->CtlID >= ID_STRUCT_CHOICE && d->CtlID < ID_STRUCT_CHOICE + g->structuredChoices.size() &&
            g->structuredField >= 0 && static_cast<size_t>(g->structuredField) < g->structure.fields.size())
            selected = g->structuredChoices[d->CtlID - ID_STRUCT_CHOICE].value ==
                       fieldValue(g->structure.fields[g->structuredField], g->cfg, g->structure.dateFormat);
        if (isPill) {
            auto action = actions.at(static_cast<int>(d->CtlID));
            if (action.kind == Action::Location)
                selected = action.index < g->displayPaths.size() &&
                           normalize(g->displayPaths[action.index].path) == normalize(g->dialog.directory);
            else if (action.kind == Action::Folder)
                selected = g->folderName;
            else if (action.kind == Action::Variables)
                selected = g->variablesEnabled;
            else if (action.kind == Action::Prefix)
                selected =
                    g->parts.prefix == expandAffix(g->cfg.prefixes[action.index], namingOptions().dateFormat);
            else if (action.kind == Action::Suffix) {
                selected =
                    g->parts.suffix == expandAffix(g->cfg.suffixes[action.index], namingOptions().dateFormat);
                auto value = expandAffix(g->cfg.suffixes[action.index], namingOptions().dateFormat);
                if (auto field = structuredSuffixField(g->structure, g->cfg, value))
                    selected =
                        selected || fieldValue(*field, g->cfg, g->structure.dateFormat) == value.substr(1);
            }
        }
        if (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY)
            selected = static_cast<int>(d->CtlID) - ID_LOC_RECOMMENDED == g->locationTab;
        if (d->CtlID == ID_LOC_RECENT)
            selected = g->locationTab == 4;
        bool affixTool = d->CtlID == ID_NAMING_MENU || d->CtlID == ID_AFFIX_SUMMARY;
        bool replaceTool = d->CtlID == ID_RENAME || d->CtlID == ID_REPLACE_SUMMARY;
        bool tool = affixTool || replaceTool;
        bool active = affixTool     ? !g->parts.prefix.empty() || !g->parts.suffix.empty()
                      : replaceTool ? g->ruleEnabled && !g->rulePattern.empty()
                                    : false;
        int toolIndex = affixTool ? 2 : 3;
        if (tool)
            selected = g->inlineTool == toolIndex || active;
        if (d->CtlID == ID_RULE_ENABLE)
            selected = g->ruleEnabled;
        if (variableChip) {
            auto keys = expressionKeys(g->variableTemplateDraft);
            selected = std::find(keys.begin(), keys.end(),
                                 g->variableTokens[d->CtlID - ID_VARIABLE_TOKEN_FIRST]) != keys.end();
        }
        if (d->CtlID == ID_VARIABLE_ADVANCED)
            selected = g->variableAdvanced;
        bool primary = d->CtlID == ID_SUBMIT || d->CtlID == ID_PRESET_SAVE || d->CtlID == ID_VARIABLE_SAVE;
        bool disabled = (d->itemState & ODS_DISABLED) != 0;
        hover = hover && !disabled;
        bool locationTab =
            (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY) || d->CtlID == ID_LOC_RECENT;
        bool panelButton = GetParent(d->hwndItem) == g->panel;
        bool locationButton = isPill && actions.at(static_cast<int>(d->CtlID)).kind == Action::Location;
        bool subjectButton = d->CtlID >= ID_QUICK_EDIT && d->CtlID < ID_QUICK_EDIT + 8;
        bool framed =
            namingView || quickPick || subjectButton || structuredField || d->CtlID == ID_NAME_MANAGER;
        bool quiet = !isPill && !variableChip && !primary;
        bool menuOpen = g->openPanelMenu == static_cast<int>(d->CtlID);
        bool keyboardFocus = !disabled && (dropdownFeedback & 2) && (d->itemState & ODS_FOCUS);
        if (d->CtlID == ID_CAPABILITIES) {
            auto font = SelectObject(d->hDC, g->smallFont);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, RGB(124, 130, 140));
            RECT caption = r;
            caption.left += px(2);
            DrawTextW(d->hDC, text.c_str(), -1, &caption,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (keyboardFocus) {
                RECT focus = r;
                InflateRect(&focus, -px(1), -px(1));
                DrawFocusRect(d->hDC, &focus);
            }
            SelectObject(d->hDC, font);
            return TRUE;
        }
        int panelHover = panelButton && !disabled ? hoverAmount(d->hwndItem, hover) : 0;
        auto appearance = panelButton ? panelAppearance(static_cast<int>(d->CtlID), menuOpen, !disabled,
                                                        panelHover, (d->itemState & ODS_SELECTED) != 0)
                                      : std::nullopt;
        COLORREF fill =
            controlFill(primary                  ? g->surface
                        : framed                 ? g->field
                        : isPill || variableChip ? g->surface
                                                 : g->bg,
                        selected || menuOpen, hover, (d->itemState & ODS_SELECTED) != 0, primary, !disabled);
        if (panelButton) {
            COLORREF base =
                quickPick || structuredField || subjectButton || locationButton ? RGB(29, 35, 45) : g->bg;
            fill = controlFill(base, selected || menuOpen, false, (d->itemState & ODS_SELECTED) != 0, primary,
                               !disabled);
            if (quickPick && selected && !disabled)
                fill = controlFill(RGB(39, 64, 99), false, false, (d->itemState & ODS_SELECTED) != 0, false,
                                   true);
            if (namingView)
                fill = controlFill(selected ? RGB(43, 51, 64) : g->bg, false, false,
                                   (d->itemState & ODS_SELECTED) != 0, false, !disabled);
            if (locationTab) {
                auto track = CreateSolidBrush(g->field);
                FillRect(d->hDC, &r, track);
                DeleteObject(track);
                fill = selected ? g->surface : g->field;
            }
            if (panelHover)
                fill = mixColor(fill, RGB(255, 255, 255), panelHover * (selected || primary ? 5 : 7) / 100);
        }
        int color = isPill ? bubbleColorIndex(actions.at(static_cast<int>(d->CtlID))) : 0;
        COLORREF mark = bubbleColors[color].value;
        if (color && !selected && !hover && !disabled)
            fill = RGB((GetRValue(fill) * 9 + GetRValue(mark)) / 10,
                       (GetGValue(fill) * 9 + GetGValue(mark)) / 10,
                       (GetBValue(fill) * 9 + GetBValue(mark)) / 10);
        if (appearance)
            fill = appearance->fill;
        auto brush = CreateSolidBrush(fill);
        COLORREF edge = fill;
        bool nameSelected = !disabled && isPill && selected &&
                            actions.at(static_cast<int>(d->CtlID)).kind != Action::Location;
        if (panelButton && !disabled && ((quickPick && selected) || (namingView && selected)))
            edge = namingView ? RGB(61, 71, 87) : mixColor(g->selection, g->accent, 30);
        if (panelButton && locationTab && selected && !disabled)
            edge = mixColor(g->surface, g->muted, 18);
        if (panelButton && !disabled) {
            if (quickPick || structuredField)
                edge = selected ? RGB(82, 126, 187) : RGB(46, 56, 71);
            if (subjectButton)
                edge = RGB(59, 74, 94);
            if (locationButton && !selected)
                edge = RGB(43, 52, 66);
        }
        if (appearance)
            edge = appearance->edge;
        auto pen = CreatePen(PS_SOLID, 1,
                             keyboardFocus  ? g->accent
                             : nameSelected ? mixColor(g->selection, g->accent, 50)
                                            : edge);
        auto ob = SelectObject(d->hDC, brush), op = SelectObject(d->hDC, pen);
        int radius = px(panelButton && locationTab ? 12 : panelButton ? 10 : 12);
        paintRoundedRect(d->hDC, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(d->hDC, op);
        SelectObject(d->hDC, ob);
        DeleteObject(pen);
        DeleteObject(brush);
        if (d->CtlID == ID_DIRECTORY) {
            auto font = SelectObject(d->hDC, g->smallFont);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, hover || (d->itemState & ODS_FOCUS) ? g->fg : g->muted);
            RECT textRect = r;
            textRect.right -= px(4);
            auto caption = trailingPath(d->hDC, text, textRect.right - textRect.left);
            DrawTextW(d->hDC, caption.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, font);
            return TRUE;
        }
        if (d->CtlID == ID_PANEL_OPTIONS || d->CtlID == ID_DIRECTORY_EDIT ||
            d->CtlID == ID_DIRECTORY_SEARCH || d->CtlID == ID_CLOSE) {
            UiIcon icon = d->CtlID == ID_PANEL_OPTIONS      ? UiIcon::Settings
                          : d->CtlID == ID_DIRECTORY_EDIT   ? UiIcon::Edit
                          : d->CtlID == ID_DIRECTORY_SEARCH ? UiIcon::Search
                          : d->CtlID == ID_CLOSE            ? UiIcon::Close
                                                            : UiIcon::Plus;
            int size = px(d->CtlID == ID_CLOSE ? 13 : 14);
            drawUiIcon(d->hDC, icon, (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size),
                       disabled                                                           ? RGB(96, 106, 122)
                       : hover                                                            ? g->fg
                       : d->CtlID == ID_DIRECTORY_EDIT || d->CtlID == ID_DIRECTORY_SEARCH ? RGB(143, 178, 225)
                                                                                          : g->muted);
            return TRUE;
        }
        if (d->CtlID == ID_FAVORITE_CURRENT) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Star, (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size), RGB(230, 184, 72), text == L"已收藏");
            return TRUE;
        }
        bool mainBody = d->hwndItem && GetParent(d->hwndItem) == g->panel && d->CtlID != ID_CLOSE;
        auto of = SelectObject(d->hDC, primary || subjectButton ? g->boldFont
                                       : panelButton &&
                                               (locationTab || namingView || locationButton ||
                                                d->CtlID == ID_STRUCT_SCHEMES || d->CtlID == ID_STRUCT_NAME ||
                                                d->CtlID >= ID_NUMBER_MENU && d->CtlID <= ID_RULES_SUMMARY ||
                                                d->CtlID == ID_COMBO_UNDO || d->CtlID == ID_COMBO_REDO ||
                                                d->CtlID == ID_CLEAR_RECENT || d->CtlID == ID_CLEAR_HISTORY)
                                           ? g->smallFont
                                       : tool || d->CtlID == ID_CLOSE ? g->font
                                       : mainBody                     ? g->bodyFont
                                       : primary                      ? g->boldFont
                                       : framed                       ? g->font
                                       : quiet                        ? g->smallFont
                                                                      : g->font);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, disabled                     ? g->muted
                             : primary                    ? RGB(255, 255, 255)
                             : selected                   ? g->fg
                             : tool || (quiet && !framed) ? g->muted
                                                          : g->fg);
        if (d->CtlID == ID_FAVORITE_CURRENT)
            SetTextColor(d->hDC, RGB(230, 184, 72));
        if (locationTab && !selected)
            SetTextColor(d->hDC, g->muted);
        if (appearance)
            SetTextColor(d->hDC, appearance->ink);
        RECT t = r;
        t.left += px(6);
        t.right -= px(6);
        if (d->CtlID == ID_TARGET_INFO) {
            auto split = text.find(L'\n');
            auto title = text.substr(0, split), detail = split == text.npos ? L"" : text.substr(split + 1);
            RECT top = t, bottom = t;
            top.top += px(5);
            top.bottom = top.top + px(21);
            bottom.top = top.bottom;
            bottom.bottom -= px(5);
            SelectObject(d->hDC, g->smallFont);
            SetTextColor(d->hDC, disabled ? g->muted : RGB(203, 182, 145));
            DrawTextW(d->hDC, title.c_str(), -1, &top,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, detail.c_str(), -1, &bottom,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (appearance) {
            if (d->CtlID == ID_COMBO_UNDO || d->CtlID == ID_COMBO_REDO) {
                int size = px(13);
                drawUiIcon(d->hDC, appearance->icon, (r.left + r.right - size) / 2.f,
                           (r.top + r.bottom - size) / 2.f, static_cast<float>(size), appearance->iconInk);
                SelectObject(d->hDC, of);
                return TRUE;
            }
            bool compact = d->CtlID == ID_STRUCT_SCHEMES || d->CtlID == ID_RULES_SUMMARY;
            int size = px(compact ? 12 : 14);
            drawUiIcon(d->hDC, appearance->icon, static_cast<float>(r.left + px(compact ? 7 : 8)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), appearance->iconInk);
            t.left = r.left + px(compact ? 21 : 28);
            if (appearance->dropdown) {
                if (!text.empty() && text.back() == L'▾')
                    text = trim(text.substr(0, text.size() - 1));
                if (d->CtlID == ID_STRUCT_SCHEMES)
                    text = L"命名设置";
                drawUiIcon(d->hDC, menuOpen ? UiIcon::ChevronUp : UiIcon::Chevron,
                           static_cast<float>(r.right - px(compact ? 15 : 19)),
                           (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)),
                           appearance->iconInk);
                t.right = r.right - px(compact ? 17 : 23);
            }
        }
        if (d->CtlID == ID_CLOSE)
            t = r;
        if (d->CtlID >= ID_QUICK_CHOICE && d->CtlID < ID_QUICK_CHOICE + 8 &&
            primarySubject(d->CtlID - ID_QUICK_CHOICE)) {
            int size = px(14);
            drawUiIcon(d->hDC, menuOpen ? UiIcon::ChevronUp : UiIcon::Chevron,
                       (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size),
                       disabled            ? g->muted
                       : hover || menuOpen ? g->fg
                                           : g->muted);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (subjectButton) {
            t.left += px(6);
            t.right -= px(24);
            drawUiIcon(d->hDC, UiIcon::Edit, static_cast<float>(r.right - px(22)),
                       (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)),
                       disabled ? g->muted
                       : hover  ? g->fg
                                : RGB(143, 178, 225));
        }
        if (quickPick && selected && !disabled) {
            drawUiIcon(d->hDC, UiIcon::Check, static_cast<float>(r.left + px(4)),
                       (r.top + r.bottom - px(10)) / 2.f, static_cast<float>(px(10)), RGB(167, 203, 252));
            t.left += px(10);
        }
        if (locationButton) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Folder, static_cast<float>(r.left + px(color ? 18 : 8)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size),
                       selected ? g->accent : g->muted);
            t.left = r.left + px(color ? 37 : 27);
        }
        if (color) {
            auto dot = CreateSolidBrush(mark);
            auto oldBrush = SelectObject(d->hDC, dot);
            auto oldPen = SelectObject(d->hDC, GetStockObject(NULL_PEN));
            int cy = (r.top + r.bottom) / 2;
            Ellipse(d->hDC, r.left + px(6), cy - px(3), r.left + px(12), cy + px(3));
            SelectObject(d->hDC, oldPen);
            SelectObject(d->hDC, oldBrush);
            DeleteObject(dot);
            t.left += px(10);
        }
        if (isPill && selected && actions.at(static_cast<int>(d->CtlID)).kind == Action::Location) {
            t.right -= px(18);
            int size = px(12);
            drawUiIcon(d->hDC, UiIcon::Check, static_cast<float>(r.right - px(19)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), g->accent);
        }
        if (locationButton && g->locationTab == 2 && GetParent(d->hwndItem) == g->panel) {
            const auto &location = g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index];
            auto parent = std::filesystem::path(location.path).parent_path().wstring();
            RECT nameRect = t, parentRect = t;
            nameRect.right = t.left + (t.right - t.left) * 3 / 5;
            parentRect.left = nameRect.right + px(10);
            DrawTextW(d->hDC, text.c_str(), -1, &nameRect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
            SelectObject(d->hDC, g->smallFont);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, parent.c_str(), -1, &parentRect, DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_VCENTER);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (locationButton && g->locationTab == 0 && panelButton &&
            (g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.find(L"已打开") !=
                 std::wstring::npos ||
             g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(
                 L"命名推荐"))) {
            RECT nameRect = t, openedRect = t;
            nameRect.bottom = t.top + px(15);
            openedRect.top = nameRect.bottom - px(1);
            DrawTextW(d->hDC, text.c_str(), -1, &nameRect,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(d->hDC, g->muted);
            auto badge =
                g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(L"命名推荐")
                    ? L"命名推荐"
                    : L"已打开";
            DrawTextW(d->hDC, badge, -1, &openedRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (structuredField && !g->structure.compact) {
            auto split = text.find(L'\n');
            auto caption = text.substr(0, split), value = split == text.npos ? L"" : text.substr(split + 1);
            RECT top = t, bottom = t;
            top.bottom = t.top + px(17);
            bottom.top = top.bottom;
            SelectObject(d->hDC, g->smallFont);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, caption.c_str(), -1, &top,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, g->font);
            SetTextColor(d->hDC, g->fg);
            DrawTextW(d->hDC, value.c_str(), -1, &bottom,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        } else
            DrawTextW(d->hDC, text.c_str(), -1, &t,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX |
                          (locationButton || subjectButton || appearance ? DT_LEFT : DT_CENTER));
        SelectObject(d->hDC, of);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (!g->building && code == BN_CLICKED && !insideTransientEditor(GetDlgItem(hwnd, id)))
            requestEditorDismiss(true);
        if (!g->building && structuredCommand(id, code))
            return 0;
        if (id >= 5000 && static_cast<size_t>(id - 5000) < g->cfg.projects.size() && code == BN_CLICKED) {
            handleProjectChoice(id - 5000 + 100);
            return 0;
        }
        if (id >= 4000 && static_cast<size_t>(id - 4000) < g->cfg.recipes.size() && code == BN_CLICKED) {
            perform({Action::Recipe, static_cast<size_t>(id - 4000)}, false);
            return 0;
        }

        if (g->building)
            return 0;
        if (code == BN_CLICKED && id != ID_DIRECTORY && id != ID_PRESET_SAVE && id != ID_PRESET_CANCEL)
            closeFlyout(true);
        auto action = g->actions.find(id);
        if (action != g->actions.end() && (code == BN_CLICKED || code == BN_DOUBLECLICKED)) {

            if (code == BN_DOUBLECLICKED &&
                (action->second.kind == Action::Prefix || action->second.kind == Action::Suffix))
                return 0;

            bool quick = code == BN_DOUBLECLICKED || (GetKeyState(VK_CONTROL) & 0x8000);
            perform(action->second, quick);
            return 0;
        }
        if (code == EN_SETFOCUS && (id == ID_RULE_PATTERN || id == ID_RULE_REPLACEMENT ||
                                    id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX))
            checkpointNaming();
        if (id == ID_FREE_NAME_INPUT && code == EN_CHANGE) {
            if (g->structure.enabled && !g->structure.free)
                enterFreeNaming();
            g->stemName = windowText(GetDlgItem(hwnd, ID_FREE_NAME_INPUT));
            g->ruleEnabled = false;
            g->ruleError.clear();
            resetSmartNaming();
            if (!g->structure.enabled)
                g->namingHistory.clear();
            syncNameParts();
            syncSaveProbe();
            g->dirtyName = true;
            g->localNamePending = true;
            if (g->busy)
                g->applyFocus = GetDlgItem(hwnd, ID_FREE_NAME_INPUT);
            g->status = L"正在编辑，稍后同步到原窗口";
            updatePanelContent();
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
            return 0;
        }
        if (id == ID_DIRECTORY_INPUT && code == EN_CHANGE) {
            invalidateNaming();
            g->pathDraft = windowText(GetDlgItem(hwnd, ID_DIRECTORY_INPUT));
            return 0;
        }
        if ((id == ID_RULE_PATTERN || id == ID_RULE_REPLACEMENT) && code == EN_CHANGE) {
            bool wasEmpty = g->rulePattern.empty();
            g->rulePattern = windowText(GetDlgItem(hwnd, ID_RULE_PATTERN));
            g->ruleReplacement = windowText(GetDlgItem(hwnd, ID_RULE_REPLACEMENT));
            if (g->ruleRegex && g->ruleIndex != static_cast<int>(renameRules().size() - 1) &&
                (g->rulePattern != renameRules()[g->ruleIndex].pattern ||
                 g->ruleReplacement != renameRules()[g->ruleIndex].replacement)) {
                g->ruleIndex = static_cast<int>(renameRules().size() - 1);
                SendDlgItemMessageW(hwnd, ID_RULE_MODE, CB_SETCURSEL, g->ruleIndex - 1, 0);
            }
            if (g->rulePattern.empty())
                g->ruleEnabled = false;
            else if (wasEmpty)
                g->ruleEnabled = true;
            updateInlineRule();
            return 0;
        }
        if ((id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX) && code == EN_CHANGE) {
            auto value = windowText(GetDlgItem(hwnd, id));
            bool prefix = id == ID_AFFIX_PREFIX;
            if (!value.empty() && !validAffix(value, prefix)) {
                invalidateNaming();
                g->statusError = true;
                KillTimer(g->control, T_ACTION);
                g->status = L"前后缀不能包含路径字符或无效文件名字符";
                updatePanelContent();
            } else {
                (prefix ? g->parts.prefix : g->parts.suffix) = expandAffix(value, namingOptions().dateFormat);
                updateInlineRule();
            }
            return 0;
        }
        if (id == ID_RULE_MODE && code == CBN_SELCHANGE) {
            checkpointNaming();
            int index = static_cast<int>(SendDlgItemMessageW(hwnd, ID_RULE_MODE, CB_GETCURSEL, 0, 0)) + 1;
            if (index > 0 && static_cast<size_t>(index) < renameRules().size()) {
                g->ruleIndex = index;
                const auto &rule = renameRules()[index];
                g->rulePattern = rule.pattern;
                g->ruleReplacement = rule.replacement;
                g->ruleRegex = rule.regex;
                g->ruleEnabled = !rule.pattern.empty();
                g->building = true;
                SetDlgItemTextW(hwnd, ID_RULE_PATTERN, g->rulePattern.c_str());
                SetDlgItemTextW(hwnd, ID_RULE_REPLACEMENT, g->ruleReplacement.c_str());
                g->building = false;
                updateInlineRule();
            }
            return 0;
        }

        if (id == ID_PRESET_VALUE && code == EN_CHANGE) {
            g->presetDraft = windowText(GetDlgItem(hwnd, ID_PRESET_VALUE));
            return 0;
        }
        if (code == BN_CLICKED) {

            if ((id >= ID_LOC_RECOMMENDED && id <= ID_LOC_HISTORY) || id == ID_LOC_RECENT) {
                g->locationTab = id == ID_LOC_RECENT ? 4 : id - ID_LOC_RECOMMENDED;
                g->locationPage = 0;
                updatePanelContent();
                return 0;
            }
            switch (id) {
            case ID_ROUTE_SUGGEST:
                showSuggestedLocations();
                break;
            case ID_TARGET_INFO:
                showTargetActions();
                break;
            case ID_CLEAR_RECENT:
                if (g->locationTab == 4 && !g->busy) {
                    closeFlyout();
                    removeRecentLocations();
                }
                break;
            case ID_CLEAR_HISTORY:
                if (g->locationTab == 2 && !g->busy) {
                    closeFlyout();
                    removeHistoryLocation();
                }
                break;
            case ID_PANEL_OPTIONS:
                showPanelMenu(ID_PANEL_OPTIONS);
                break;
            case ID_CAPABILITIES:
                showCapabilityMenu();
                break;
            case ID_RULES_SUMMARY:
                showRuleSummaryMenu();
                break;
            case ID_NUMBER_MENU:
                showNumberMenu();
                break;
            case ID_AFFIX_SUMMARY:
                SendMessageW(hwnd, WM_COMMAND, ID_NAMING_MENU, 0);
                break;
            case ID_REPLACE_SUMMARY:
                SendMessageW(hwnd, WM_COMMAND, ID_RENAME, 0);
                break;
            case ID_PANEL_SETTINGS:
                g->managerTab = 3;
                showManager();
                break;
            case ID_BIND_CONTROL:
                startBinding();
                break;
            case ID_COPY_NAME:
                if (g->ruleError.empty() && !g->namingPending && structuredInputError().empty() &&
                    validName(g->stemName)) {
                    if (copyWorkflowText(g->stemName + g->dialog.extension)) {
                        setTextIfChanged(GetDlgItem(hwnd, ID_FINAL_NAME_LABEL), L"最终文件名 · 已复制");
                        SetTimer(hwnd, T_COPY_FEEDBACK, 1600, nullptr);
                    } else
                        workflowNotice(L"剪贴板暂时不可用，请重试。");
                }
                break;
            case ID_GENERIC_SUBMIT:
                if (g->dialog.generic && g->dialog.genericSubmit)
                    apply(g->dialog.directory, g->stemName, true, true);
                break;
            case ID_FOCUS_NAME:
                if (!g->structure.free)
                    structuredCommand(ID_STRUCT_FREE, 0);
                SetFocus(GetDlgItem(hwnd, ID_FREE_NAME_INPUT));
                break;
            case ID_DIRECTORY:
                beginPathEdit();
                break;
            case ID_DIRECTORY_EDIT:
                if (!g->editingPath)
                    beginPathEdit();
                else if (IsWindowVisible(g->panel))
                    SetFocus(GetDlgItem(hwnd, ID_DIRECTORY_INPUT));
                break;
            case ID_DIRECTORY_SEARCH:
                cancelPathEdit();
                showLocationFlyout();
                break;
            case ID_LOCATION_PAGE:
                cancelPathEdit();
                showLocationFlyout(true);
                break;
            case IDOK:
                SendMessageW(hwnd, WM_COMMAND, ID_SUBMIT, 0);
                break;
            case IDCANCEL:
                SetForegroundWindow(g->dialog.hwnd);
                break;
            case ID_CLOSE:
                invalidateNaming();
                g->suppressed = g->dialog.hwnd;
                ++g->generation;
                g->busy = false;
                g->pendingQuick = false;
                KillTimer(g->control, T_ACTION);
                hidePanel();
                break;
            case ID_MANAGER:
                g->managerTab = 1;
                showManager();
                break;
            case ID_NAME_MANAGER:
                g->managerTab = 0;
                showManager();
                break;
            case ID_ADD_PREFIX:
            case ID_ADD_SUFFIX:
                g->namingMode = id == ID_ADD_PREFIX ? 1 : 2;
                openInlinePreset(NEW_PRESET, GetDlgItem(hwnd, id));
                break;
            case ID_PRESET_SAVE:
                saveInlinePreset();
                break;
            case ID_PRESET_CANCEL:
                closeFlyout(true);
                break;
            case ID_NAMING_MENU:
            case ID_RENAME: {
                if (!structuredInputError().empty() || g->structuredIme)
                    break;
                resetStructuredInput();
                int tool = id == ID_NAMING_MENU ? 2 : 3;
                g->inlineTool = g->inlineTool == tool ? 0 : tool;
                closeFlyout();
                rebuildPanel();
                break;
            }
            case ID_RULE_ENABLE:
                checkpointNaming();
                g->ruleEnabled = !g->ruleEnabled && !g->rulePattern.empty();
                updateInlineRule();
                break;
            case ID_RULE_CLEAR:
                checkpointNaming();
                g->rulePattern.clear();
                g->ruleReplacement.clear();
                g->ruleEnabled = false;
                g->ruleIndex = 1;
                g->ruleRegex = false;
                updateInlineRule();
                rebuildPanel();
                break;
            case ID_VARIABLE_EDIT:
                openVariableEditor();
                break;
            case ID_FAVORITE_CURRENT: {
                if (!g->dialog.directory.empty()) {
                    auto name = std::filesystem::path(g->dialog.directory).filename().wstring();
                    toggleFavoriteLocation(
                        {name.empty() ? g->dialog.directory : name, g->dialog.directory, L"收藏"});
                }
                break;
            }
            case 720:
                handleProjectChoice(2);
                break;
            case 721:
                handleProjectChoice(3);
                break;
            case 722:
                handleProjectChoice(4);
                break;
            case 723:
                handleProjectChoice(5);
                break;
            case 724:
                handleProjectChoice(1);
                break;
            case 730:
                transferPresets(false);
                break;
            case 731:
                transferPresets(true);
                break;
            case ID_REFRESH_NUMBER:
                updateInlineRule();
                break;
            case ID_PROJECTS:
                showProjects();
                break;
            case ID_RECORDS:
                showSavedRecords();
                break;
            case ID_EXPLAIN:
                showNameExplanation();
                break;
            case ID_TRANSFER:
                showTransfer();
                break;
            case ID_DIAGNOSTICS:
                showDiagnostics();
                break;
            case ID_CAPTURE_RECIPE:
                saveCurrentRecipe();
                break;
            case ID_PLANS:
                showRecipeMenu();
                break;
            case ID_SUBMIT:
                if (!g->ruleError.empty()) {
                    g->status = g->ruleError;
                    updatePanelContent();
                    break;
                }
                if (g->editingPath) {
                    commitPathEdit();
                    break;
                }
                KillTimer(g->control, T_ACTION);
                apply(g->dialog.directory, g->stemName, !g->dialog.generic);
                break;
            case ID_COMBO_UNDO:
                moveNamingHistory(false);
                break;
            case ID_COMBO_REDO:
                moveNamingHistory(true);
                break;
            case ID_UNDO:
                if (g->structuredIme)
                    break;
                checkpointNaming();
                g->ruleEnabled = false;
                g->ruleError.clear();
                {
                    bool structured = g->structure.enabled;
                    g->freeRecovery.reset();
                    resetStructuredInput();
                    g->inlineTool = 0;
                    g->structuredAdvanced = false;
                    if (structured) {
                        for (auto &f : g->structure.fields) {
                            f.value.clear();
                            f.source = 0;
                            f.omitted = false;
                            if (f.id == g->structure.primaryField)
                                f.value = g->originalStem;
                        }
                        g->structure.free = false;
                        g->structure.literal = true;
                        g->structure.literalName = g->originalStem;
                    } else
                        g->structure = {};
                }
                resetSmartNaming();
                g->stemName = g->originalStem;
                g->parts = splitName(g->stemName, g->cfg.modifiers);
                g->dirtyName = false;
                rebuildPanel();
                apply(g->originalDirectory, g->stemName, false);
                break;
            }
        }
        return 0;
    }
    case WM_DWMCOMPOSITIONCHANGED:
        updatePanelMaterial();
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        return 0;
    case WM_THEMECHANGED:
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        return 0;
    case WM_DPICHANGED:
        g->dpi = HIWORD(wp);
        fonts();
        rebuildPanel();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
