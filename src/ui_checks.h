// Hidden-window layout, paint stability, and settings-center self-checks.
bool saveHiddenPanelPreview(const std::filesystem::path &path, HWND target = nullptr) {
    if (!target)
        target = g->panel;
    const bool manager = target == g->manager;
    RECT bounds{};
    if (manager)
        GetClientRect(target, &bounds);
    else
        GetWindowRect(target, &bounds);
    int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
    void *pixels = nullptr;
    HDC screen = GetDC(target), dc = CreateCompatibleDC(screen);
    auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) {
        DeleteDC(dc);
        ReleaseDC(target, screen);
        return false;
    }
    auto old = SelectObject(dc, bitmap);
    if (manager) {
        // Print the client and each child in client coordinates; native caption
        // offsets otherwise misalign the custom frames in a hidden-window render.
        SendMessageW(target, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
        for (HWND child = GetWindow(target, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE))
                continue;
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, target, reinterpret_cast<POINT *>(&r), 2);
            int saved = SaveDC(dc);
            SetViewportOrgEx(dc, r.left, r.top, nullptr);
            IntersectClipRect(dc, 0, 0, r.right - r.left, r.bottom - r.top);
            auto kind = windowClass(child);
            bool custom =
                kind == WC_TABCONTROLW ||
                (kind == L"Button" && (GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX);
            SendMessageW(child, custom ? WM_PRINTCLIENT : WM_PRINT, reinterpret_cast<WPARAM>(dc),
                         PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
            RestoreDC(dc, saved);
        }
    } else
        SendMessageW(target, WM_PRINT, reinterpret_cast<WPARAM>(dc),
                     PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    GdiFlush();
    BITMAPFILEHEADER header{};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    DWORD size = static_cast<DWORD>(width * height * 4);
    header.bfSize = header.bfOffBits + size;
    auto file =
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    bool ok = file != INVALID_HANDLE_VALUE && WriteFile(file, &header, sizeof(header), &written, nullptr) &&
              WriteFile(file, &info.bmiHeader, sizeof(BITMAPINFOHEADER), &written, nullptr) &&
              WriteFile(file, pixels, size, &written, nullptr) && written == size;
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(target, screen);
    return ok;
}

bool selfTestStructuredLayout(const std::filesystem::path &directory) {
    if (!liveStructuredControl(ID_STRUCT_SCHEMES))
        return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
    g->cfg = defaults();
    g->attached = false;
    g->busy = false;
    g->namingMode = 1;
    g->inlineTool = 0;
    g->ruleEnabled = g->variablesEnabled = g->folderName = false;
    g->numberMode = 0;
    g->paths.clear();
    g->locationTab = 0;
    g->dialog.directory = directory.wstring();
    g->dialog.extension = L".txt";
    g->dialog.save = true;
    g->originalDirectory = g->dialog.directory;
    g->originalStem = L"原文件";
    StructuredNaming unchanged;
    unchanged.id = L"unchanged";
    unchanged.name = L"不编号";
    unchanged.enabled = true;
    unchanged.counter = 0;
    unchanged.fields = {{L"subject", L"主要名称", L"", true}};
    Recipe unchangedRecipe;
    unchangedRecipe.name = unchanged.name;
    unchangedRecipe.value = L"Asset";
    unchangedRecipe.namingOnly = true;
    unchangedRecipe.structure = serializeStructure(unchanged);
    g->cfg.recipes = {unchangedRecipe};
    g->cfg.defaultNamingScheme = unchanged.id;
    g->stemName = L"Guard";
    resumeNamingDraft();
    if (g->structuredResumeNumber)
        return hiddenCheckFailure("unchangedSchemeDoesNotWrite", __LINE__);
    g->cfg = defaults();
    g->structure = basicStructure(L"Guard");
    g->structure.id = L"custom-stage-options";
    g->structure.fields[4].candidates = {{L"Only", L"Only"}};
    if (quickCandidates(4).size() != 1)
        return hiddenCheckFailure("customSchemeCandidateIsolation", __LINE__);
    for (int dpi : {96, 120, 144})
        for (bool bottom : {false, true})
            for (int mode : {0, 1, 2}) {
                bool editing = mode == 2;
                g->dpi = dpi;
                fonts();
                g->bottom = bottom;
                g->width = bottom ? 680 : PANEL_WIDTH;
                g->structuredAdvanced = editing;
                g->structure = basicStructure(L"Guard");
                g->structure.fields[2].value = L"模型";
                g->structure.fields[4].value = L"工作";
                g->structuredField = mode ? 1 : -1;
                g->structuredDraft = L"Guard";
                g->stemName = composeStructure(g->structure, g->cfg).name;
                g->parts = {g->stemName};
                rebuildPanel();
                if (GetDlgItem(g->panel, ID_SUBMIT) || GetDlgItem(g->panel, ID_UNDO) ||
                    !GetDlgItem(g->panel, ID_COMBO_UNDO) || !GetDlgItem(g->panel, ID_COMBO_REDO) ||
                    !GetDlgItem(g->panel, ID_NUMBER_MENU) || !GetDlgItem(g->panel, ID_AFFIX_SUMMARY) ||
                    !GetDlgItem(g->panel, ID_REPLACE_SUMMARY) || !GetDlgItem(g->panel, ID_CAPABILITIES))
                    return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
                if (!GetDlgItem(g->panel, ID_STRUCT_SCHEMES) || !GetDlgItem(g->panel, ID_STRUCT_NAME))
                    return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
                for (int id : {750, 751, 752, 754, 756, 758, 759, 765, 768, 776, 777, ID_STRUCT_DESIGN,
                               ID_STRUCT_QUICK_VIEW, ID_STRUCT_FIELDS_VIEW})
                    if (GetDlgItem(g->panel, id))
                        return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
                if (mode == 0 && (GetDlgItem(g->panel, ID_QUICK_INPUT + 1) ||
                                  !GetDlgItem(g->panel, ID_QUICK_PICK + 2 * 6) ||
                                  !GetDlgItem(g->panel, ID_QUICK_PICK + 4 * 6) ||
                                  GetDlgItem(g->panel, ID_STRUCT_DELIVERY)))
                    return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
                SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                             SWP_NOZORDER | SWP_NOACTIVATE);
                RECT client{};
                GetClientRect(g->panel, &client);
                for (auto h : g->children) {
                    if (!(GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE))
                        continue;
                    RECT r{};
                    GetWindowRect(h, &r);
                    MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
                    if (r.left < 0 || r.top < 0 || r.right > client.right || r.bottom > client.bottom)
                        return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
                }
                RECT nr{}, rsr{}, ur{}, rdr{};
                GetWindowRect(GetDlgItem(g->panel, ID_NUMBER_MENU), &nr);
                GetWindowRect(GetDlgItem(g->panel, ID_RULES_SUMMARY), &rsr);
                GetWindowRect(GetDlgItem(g->panel, ID_COMBO_UNDO), &ur);
                GetWindowRect(GetDlgItem(g->panel, ID_COMBO_REDO), &rdr);
                if (nr.top != rsr.top || nr.top != ur.top || nr.top != rdr.top || nr.right >= rsr.left ||
                    rsr.right >= ur.left || ur.right >= rdr.left)
                    return hiddenCheckFailure("compactActionsRowAlignment", __LINE__);
                int collapsedHeight = g->height;
                g->parts.prefix = L"PRE_";
                updateExperienceContent();
                updatePanelStatusLayout();
                RECT ar{}, flr{};
                GetWindowRect(GetDlgItem(g->panel, ID_AFFIX_SUMMARY), &ar);
                GetWindowRect(GetDlgItem(g->panel, ID_FINAL_NAME_LABEL), &flr);
                if ((GetWindowLongPtrW(GetDlgItem(g->panel, ID_RULES_SUMMARY), GWL_STYLE) & WS_VISIBLE) ||
                    !(GetWindowLongPtrW(GetDlgItem(g->panel, ID_AFFIX_SUMMARY), GWL_STYLE) & WS_VISIBLE) ||
                    ar.bottom - ar.top != px(24) || ar.bottom >= flr.top || g->height != collapsedHeight + 30)
                    return hiddenCheckFailure("modifierRowExpand", __LINE__);
                g->parts.prefix.clear();
                updateExperienceContent();
                updatePanelStatusLayout();
                if (g->height != collapsedHeight ||
                    !(GetWindowLongPtrW(GetDlgItem(g->panel, ID_RULES_SUMMARY), GWL_STYLE) & WS_VISIBLE) ||
                    (GetWindowLongPtrW(GetDlgItem(g->panel, ID_AFFIX_SUMMARY), GWL_STYLE) & WS_VISIBLE))
                    return hiddenCheckFailure("modifierRowCollapse", __LINE__);
                auto name = L"structured-" + std::to_wstring(dpi) + (bottom ? L"-bottom" : L"-side") +
                            (editing     ? L"-edit.bmp"
                             : mode == 1 ? L"-input.bmp"
                                         : L".bmp");
                if (!saveHiddenPanelPreview(directory / name) || IsWindowVisible(g->panel))
                    return hiddenCheckFailure("selfTestStructuredLayout", __LINE__);
            }
    return true;
}

bool selfTestRecommendedLocations(const std::filesystem::path &directory) {
    g->cfg = defaults();
    g->attached = g->busy = false;
    g->locationTab = g->locationPage = 0;
    g->structuredAdvanced = false;
    g->structuredField = -1;
    g->cfg.favorites = {{L"工作目录", L"D:\\Work"}};
    std::vector<Location> opened{{L"工作目录", L"d:\\work\\"}};
    for (int i = 0; i < 12; ++i)
        opened.push_back({L"打开目录 " + std::to_wstring(i), L"D:\\Open\\" + std::to_wstring(i)});
    g->paths = rankLocations(g->cfg, L"D:\\Source", opened);
    for (int dpi : {96, 120, 144})
        for (bool bottom : {false, true}) {
            g->dpi = dpi;
            fonts();
            g->bottom = bottom;
            g->width = bottom ? 680 : PANEL_WIDTH;
            g->locationPage = 0;
            rebuildPanel();
            SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            for (int id : {ID_LOC_RECOMMENDED, ID_LOC_FAVORITES, ID_LOC_RECENT, ID_LOC_HISTORY})
                if (!GetDlgItem(g->panel, id))
                    return hiddenCheckFailure("recommendedTabs", __LINE__);
            if (GetDlgItem(g->panel, 123) || g->folderRows != 3 || g->displayPaths.size() != 9)
                return hiddenCheckFailure("recommendedMerge", __LINE__);
            RECT firstTab{}, lastTab{};
            GetWindowRect(GetDlgItem(g->panel, ID_LOC_RECOMMENDED), &firstTab);
            GetWindowRect(GetDlgItem(g->panel, ID_LOC_HISTORY), &lastTab);
            MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&firstTab), 2);
            MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&lastTab), 2);
            if (std::abs(firstTab.left - g->locationTabsRect.left - px(3)) > 1 ||
                std::abs(lastTab.right - g->locationTabsRect.right + px(3)) > 1 ||
                firstTab.top != lastTab.top)
                return hiddenCheckFailure("locationTabsContentAlignment", __LINE__);
            if (g->displayPaths[1].name != L"工作目录" ||
                g->displayPaths[1].sources.find(L"已打开") == std::wstring::npos)
                return hiddenCheckFailure("recommendedDeduplication", __LINE__);
            auto name = L"recommended-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
            if (!saveHiddenPanelPreview(directory / name))
                return false;
            auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
            if (!(GetWindowLongPtrW(scroll, GWL_STYLE) & WS_VISIBLE))
                return hiddenCheckFailure("recommendedScroll", __LINE__);
            RECT before{};
            GetWindowRect(GetDlgItem(g->panel, ID_DIRECTORY), &before);
            SendMessageW(g->panel, WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), reinterpret_cast<LPARAM>(scroll));
            RECT after{};
            GetWindowRect(GetDlgItem(g->panel, ID_DIRECTORY), &after);
            if (g->locationPage == 0 || !EqualRect(&before, &after) ||
                g->displayPaths.back().path != opened.back().path)
                return hiddenCheckFailure("recommendedAllOpenedReachable", __LINE__);
            g->locationPage = 0;
            updateLocationShortcuts();
            // Source-only changes must refresh the badge even when name/path stay the same.
            g->paths[1].sources = L"收藏";
            updateLocationShortcuts();
            if (g->displayPaths[1].sources != L"收藏")
                return hiddenCheckFailure("recommendedBadgeRefresh", __LINE__);
            g->paths[1].sources += L" / 已打开";
            auto allPaths = g->paths;
            g->paths.resize(4);
            updateLocationShortcuts();
            RECT first{}, second{}, third{}, fourth{};
            GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST), &first);
            GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 1), &second);
            GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 2), &third);
            GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 3), &fourth);
            if (g->folderRows != 2 || g->displayPaths.size() != 4 || first.top != second.top ||
                first.left != third.left || second.left != fourth.left || third.top != fourth.top ||
                third.top <= first.top)
                return hiddenCheckFailure("fourLocationsBalancedGrid", __LINE__);
            name = L"recommended-four-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
            if (!saveHiddenPanelPreview(directory / name))
                return false;
            g->paths = std::move(allPaths);
            updateLocationShortcuts();
            auto favorites = std::move(g->cfg.favorites);
            g->locationTab = 1;
            updatePanelContent();
            RECT empty{};
            auto message = GetDlgItem(g->panel, ID_LOCATION_EMPTY);
            GetWindowRect(message, &empty);
            MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&empty), 2);
            if (!(GetWindowLongPtrW(message, GWL_STYLE) & WS_VISIBLE) ||
                empty.left != g->locationTabsRect.left)
                return hiddenCheckFailure("emptyLocationContentAlignment", __LINE__);
            name = L"locations-empty-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
            if (!saveHiddenPanelPreview(directory / name))
                return false;
            g->cfg.favorites = std::move(favorites);
            g->locationTab = 0;
            updatePanelContent();
        }
    return true;
}

bool selfTestPanelLayer() {
    auto foreground = GetForegroundWindow();
    auto originalTarget = g->dialog.hwnd;
    bool originalAttached = g->attached;
    bool createdTopmost = (GetWindowLongPtrW(g->panel, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    // Exercise Z-order recovery with hidden windows only, without touching the
    // user's dialog, showing a test host or activating any Jump window.
    SetWindowPos(g->panel, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    g->dialog.hwnd = g->control;
    g->attached = true;
    reposition();
    auto style = GetWindowLongPtrW(g->panel, GWL_EXSTYLE);
    g->dialog.hwnd = originalTarget;
    g->attached = originalAttached;
    return createdTopmost && (style & WS_EX_TOPMOST) && (style & WS_EX_NOACTIVATE) &&
           !IsWindowVisible(g->panel) && GetForegroundWindow() == foreground;
}
bool saveReferencePreview(const std::filesystem::path &directory) {
    g->attached = false;
    g->dialog = {};
    g->dialog.save = true;
    g->dialog.directory = L"D:\\Pictures";
    g->dialog.extension = L".blend";
    g->cfg = defaults();
    g->dpi = 120;
    g->width = PANEL_WIDTH;
    g->bottom = false;
    fonts();
    paintTheme();
    g->cfg.recentSaves = {{L"图片", L"D:\\Pictures", L"最近"},
                          {L"下载", L"D:\\Downloads", L"最近"},
                          {L"项目", L"D:\\Project", L"最近"}};
    g->locationTab = 4;
    g->locationPage = 0;
    g->inlineTool = 0;
    g->editingPath = g->busy = g->localNamePending = g->statusError = g->settingsDirty = false;
    g->parts = {L"角色设定_原文件", L"", L"", L"", L""};
    g->stemName = g->parts.base;
    g->dirtyName = true;
    rebuildPanel();
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (!saveHiddenPanelPreview(directory / L"reference-layout.bmp"))
        return false;
    return !GetDlgItem(g->panel, 100) && GetDlgItem(g->panel, ID_STRUCT_SCHEMES);
}
LRESULT CALLBACK CountPositionChanges(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR count) {
    if (msg == WM_WINDOWPOSCHANGED)
        ++*reinterpret_cast<int *>(count);
    return DefSubclassProc(h, msg, wp, lp);
}
bool selfTestPaintStability() {
    HWND button = GetDlgItem(g->panel, ID_LOCATION_FIRST);
    SendMessageW(button, WM_MOUSEMOVE, 0, MAKELPARAM(4, 4));
    ValidateRect(button, nullptr);
    for (int i = 0; i < 24; ++i)
        SendMessageW(button, WM_MOUSEMOVE, 0, MAKELPARAM(4 + i % 8, 4));
    bool stableHover = !GetUpdateRect(button, nullptr, FALSE);
    SendMessageW(button, WM_MOUSELEAVE, 0, 0);
    int moved = 0;
    SetWindowSubclass(button, CountPositionChanges, 99, reinterpret_cast<DWORD_PTR>(&moved));
    auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
    SetWindowSubclass(scroll, CountPositionChanges, 99, reinterpret_cast<DWORD_PTR>(&moved));
    for (int i = 0; i < 12; ++i)
        updateLocationShortcuts();
    RemoveWindowSubclass(button, CountPositionChanges, 99);
    RemoveWindowSubclass(scroll, CountPositionChanges, 99);
    if (!stableHover || moved != 0)
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    g->inlineTool = 3;
    rebuildPanel();
    HWND input = GetDlgItem(g->panel, ID_RULE_PATTERN);
    RECT frame{};
    GetWindowRect(input, &frame);
    MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&frame), 2);
    SendMessageW(input, WM_MOUSELEAVE, 0, 0);
    auto idleBorder = inputBorder(g->panel, frame);
    SendMessageW(input, WM_MOUSEMOVE, 0, MAKELPARAM(3, 3));
    if (inputBorder(g->panel, frame) == idleBorder)
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    ValidateRect(g->panel, nullptr);
    for (int i = 0; i < 12; ++i)
        SendMessageW(input, WM_MOUSEMOVE, 0, MAKELPARAM(3 + i, 3));
    if (GetUpdateRect(g->panel, nullptr, FALSE))
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    SendMessageW(input, WM_MOUSELEAVE, 0, 0);
    if (inputBorder(g->panel, frame) != idleBorder)
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    auto source = GetDlgItem(g->panel, ID_RULE_SOURCE);
    SendMessageW(source, EM_SETSEL, 1, 3);
    updateInlineControls();
    DWORD start = 0, end = 0;
    SendMessageW(source, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    if (start != 1 || end != 3)
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    g->inlineTool = 0;
    rebuildPanel();
    return true;
}

bool selfTestManagerPosition() {
    auto originalDialog = g->dialog;
    bool originalAttached = g->attached;
    bool saved = g->cfg.managerPositionSaved;
    int savedX = g->cfg.managerX, savedY = g->cfg.managerY;
    auto foreground = GetForegroundWindow();
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST), &monitor);
    const auto &work = monitor.rcWork;
    int centerX =
        (work.left + work.right) / 2 + std::max(0L, (work.right - work.left - px(MANAGER_WIDTH)) / 8);
    int centerY =
        (work.top + work.bottom) / 2 + std::max(0L, (work.bottom - work.top - px(MANAGER_HEIGHT + 40)) / 8);
    auto anchor =
        CreateWindowExW(0, L"STATIC", L"Jump placement check", WS_POPUP, centerX - px(150), centerY - px(100),
                        px(300), px(200), nullptr, nullptr, g->instance, nullptr);
    g->dialog.hwnd = anchor;
    g->attached = true;
    g->cfg.managerPositionSaved = false;
    bool ok = [&]() {
        showManager(false);
        RECT r{};
        GetWindowRect(g->manager, &r);
        if (std::abs((r.left + r.right) / 2 - centerX) > 1 || std::abs((r.top + r.bottom) / 2 - centerY) > 1)
            return false;
        auto point =
            fitWindowPoint({r.left + px(24), r.top + px(16)}, r.right - r.left, r.bottom - r.top, work);
        SetWindowPos(g->manager, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(g->manager, WM_EXITSIZEMOVE, 0, 0);
        auto stored = load(g->configFile);
        if (!stored.managerPositionSaved || stored.managerX != point.x || stored.managerY != point.y)
            return false;
        g->attached = false;
        DestroyWindow(g->manager);
        g->attached = true;
        showManager(false);
        GetWindowRect(g->manager, &r);
        if (r.left != point.x || r.top != point.y)
            return false;
        g->attached = false;
        DestroyWindow(g->manager);
        g->attached = true;
        g->cfg.managerX = 100000;
        g->cfg.managerY = -100000;
        showManager(false);
        GetWindowRect(g->manager, &r);
        MONITORINFO nearest{sizeof(nearest)};
        GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &nearest);
        return r.left >= nearest.rcWork.left && r.top >= nearest.rcWork.top &&
               r.right <= nearest.rcWork.right && r.bottom <= nearest.rcWork.bottom &&
               !IsWindowVisible(g->manager) && GetForegroundWindow() == foreground;
    }();
    g->attached = false;
    if (g->manager)
        DestroyWindow(g->manager);
    DestroyWindow(anchor);
    g->dialog = originalDialog;
    g->attached = originalAttached;
    g->cfg.managerPositionSaved = saved;
    g->cfg.managerX = savedX;
    g->cfg.managerY = savedY;
    persist();
    return ok || hiddenCheckFailure("selfTestManagerPosition", __LINE__);
}
bool selfTestCurrentManager(const std::filesystem::path &directory) {
    auto foreground = GetForegroundWindow();
    g->attached = false;
    const wchar_t *pages[]{L"manager-schemes.bmp",  L"manager-locations.bmp", L"manager-workflows.bmp",
                           L"manager-options.bmp",  L"manager-prefixes.bmp",  L"manager-expressions.bmp",
                           L"manager-projects.bmp", L"manager-backups.bmp",   L"manager-connections.bmp"};
    for (int page = 0; page < MANAGER_PAGES; ++page) {
        g->managerTab = page;
        showManager(false);
        if (TabCtrl_GetItemCount(GetDlgItem(g->manager, M_TAB)) != MANAGER_PAGES)
            return hiddenCheckFailure("currentManager", __LINE__);
        RECT client{};
        GetClientRect(g->manager, &client);
        for (HWND child = GetWindow(g->manager, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE))
                continue;
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, g->manager, reinterpret_cast<POINT *>(&r), 2);
            if (r.left < 0 || r.top < 0 || r.right > client.right || r.bottom > client.bottom)
                return hiddenCheckFailure("currentManagerBounds", __LINE__);
        }
        if (page == 0 && (GetDlgItem(g->manager, 201) || GetDlgItem(g->manager, 205) ||
                          !GetDlgItem(g->manager, M_COPY_SCHEME)))
            return hiddenCheckFailure("currentSchemePage", __LINE__);
        if (page == 2 &&
            (GetDlgItem(g->manager, 226) || GetDlgItem(g->manager, 227) || GetDlgItem(g->manager, 225)))
            return hiddenCheckFailure("currentWorkflowPage", __LINE__);
        if (!saveHiddenPanelPreview(directory / pages[page], g->manager))
            return false;
        if (IsWindowVisible(g->manager) || GetForegroundWindow() != foreground)
            return false;
    }
    DestroyWindow(g->manager);
    return true;
}

bool checkManagerShow(const std::filesystem::path &directory, const std::wstring &entry) {
    // Run before any other top-level ShowWindow call, including panel hiding.
    // STARTF_USESHOWWINDOW / SW_HIDE must not swallow this first explicit user action.
    g->managerTab = entry == L"locations"                            ? 1
                    : (entry == L"prefixes" || entry == L"suffixes") ? MANAGER_AFFIXES
                                                                     : 0;
    g->affixCategory = entry == L"suffixes" ? 1 : 0;
    showManager();
    HWND firstWindow = g->manager;
    bool first = firstWindow && IsWindowVisible(firstWindow) && !IsIconic(firstWindow);
    bool reused = false, restored = false;
    if (first) {
        showManager();
        reused = g->manager == firstWindow && IsWindowVisible(firstWindow);
        ShowWindow(firstWindow, SW_MINIMIZE);
        showManager();
        restored = g->manager == firstWindow && IsWindowVisible(firstWindow) && !IsIconic(firstWindow);
    }
    std::filesystem::create_directories(directory);
    std::ofstream report(directory / L"result.json");
    report << "{\"firstVisible\":" << (first ? "true" : "false")
           << ",\"reused\":" << (reused ? "true" : "false")
           << ",\"restored\":" << (restored ? "true" : "false") << "}";
    if (g->manager)
        DestroyWindow(g->manager);
    return first && reused && restored;
}
