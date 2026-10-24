// Contextual assistance shares existing location navigation and naming actions.
void resizeSaveAssistRow(HWND row, int height) {
    if (!row)
        return;
    RECT rect{};
    GetWindowRect(row, &rect);
    MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&rect), 2);
    int delta = height - (rect.bottom - rect.top);
    if (!delta)
        return;
    int cutoff = rect.bottom;
    for (auto control : g->children) {
        if (control == row)
            continue;
        RECT r{};
        GetWindowRect(control, &r);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
        if (r.top >= cutoff)
            positionChild(control, r.left, r.top + delta, r.right - r.left, r.bottom - r.top);
    }
    for (auto &r : g->fields)
        if (r.top >= cutoff)
            OffsetRect(&r, 0, delta);
    for (auto &label : g->labels)
        if (label.rect.top >= cutoff)
            OffsetRect(&label.rect, 0, delta);
    for (auto &y : g->dividers)
        if (y >= cutoff)
            y += delta;
    if (g->pathEditRect.top >= cutoff)
        OffsetRect(&g->pathEditRect, 0, delta);
    positionChild(row, rect.left, rect.top, rect.right - rect.left, height);
    g->height += MulDiv(delta, 96, g->dpi);
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    reposition(false);
    InvalidateRect(g->panel, nullptr, FALSE);
}
std::wstring currentSaveTarget() {
    if (!g->attached || !g->dialog.save || !g->structure.warnExisting || g->namingPending ||
        g->structuredIme || g->pastePending || !g->ruleError.empty() || !structuredInputError().empty() ||
        !validName(previewName()) || g->dialog.directory.empty() ||
        !std::filesystem::path(g->dialog.directory).is_absolute())
        return {};
    return (std::filesystem::path(g->dialog.directory) / previewName()).wstring();
}
void updateSaveAssist() {
    auto routes = GetDlgItem(g->panel, ID_ROUTE_SUGGEST);
    auto locations = namingRecommendedLocations(g->structure, g->cfg);
    std::erase_if(locations,
                  [](const auto &r) { return normalize(r.path) == normalize(g->dialog.directory); });
    bool visible = routes && g->dialog.save && !locations.empty();
    if (visible) {
        auto title = L"推荐：" + locations[0].name +
                     (locations.size() > 1 ? L" 等 " + std::to_wstring(locations.size()) + L" 个目录" : L"") +
                     L" ▾";
        setTextIfChanged(routes, title);
        std::wstring help = L"根据当前命名内容推荐；点击选择目录，不会直接保存。";
        for (const auto &location : locations)
            help += L"\n" + location.sources + L" → " + location.path;
        tip(routes, help);
        enableIfChanged(routes, !g->busy && !g->editingPath && !g->namingPending && !composingNaming() &&
                                    structuredInputError().empty());
    }
    resizeSaveAssistRow(routes, visible ? px(30) : 0);
    showIfChanged(routes, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto path = currentSaveTarget();
    if (path != g->targetPath || g->targetGeneration != g->generation) {
        g->targetPath = path;
        g->targetGeneration = g->generation;
        g->targetInfo.reset();
        if (g->targetWorker)
            g->targetWorker->cancel();
        KillTimer(g->control, T_TARGET_CHECK);
        if (!path.empty() && g->targetWorker)
            SetTimer(g->control, T_TARGET_CHECK, 220, nullptr);
    }
    auto target = GetDlgItem(g->panel, ID_TARGET_INFO);
    visible = target && g->targetInfo && (g->targetInfo->exists || g->targetInfo->error) &&
              g->targetInfo->request.path == path;
    if (visible) {
        const auto &info = *g->targetInfo;
        auto title = info.error    ? L"同名检查暂不可用"
                     : info.folder ? L"已有同名文件夹 · 点击处理"
                                   : L"已有同名文件 · 点击处理";
        setTextIfChanged(target, std::wstring(title) + L"\n" + saveTargetDetails(info));
        tip(target, info.request.path + L"\n" + saveTargetDetails(info) +
                        L"\n可换用下一个编号、修改名称或重新检查。\n这是当前目标的检查结果；保存时仍由原软件"
                        L"确认是否覆盖。");
        enableIfChanged(target, !g->busy && !g->namingPending && !composingNaming());
    }
    if (target) {
        RECT rect{};
        GetWindowRect(target, &rect);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&rect), 2);
        positionChild(target, rect.left, rect.top, rect.right - rect.left, visible ? px(56) : 0);
        showIfChanged(target, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}
void showSuggestedLocations() {
    if (g->busy || g->namingPending || composingNaming())
        return;
    auto locations = namingRecommendedLocations(g->structure, g->cfg);
    std::erase_if(locations,
                  [](const auto &r) { return normalize(r.path) == normalize(g->dialog.directory); });
    if (locations.empty())
        return;
    auto menu = CreatePopupMenu();
    std::vector<PopupMenuHint> hints;
    for (size_t i = 0; i < locations.size(); ++i) {
        auto text = locations[i].name + L" · " + locations[i].sources;
        AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(i + 1), text.c_str());
        hints.push_back({menu, static_cast<UINT>(i),
                         locations[i].path + L"\n" + locations[i].sources +
                             L"\n点击只切换目录，保留命名；目录必须已存在。"});
    }
    RECT anchor{};
    GetWindowRect(GetDlgItem(g->panel, ID_ROUTE_SUGGEST), &anchor);
    int choice = trackPopupMenuWithHelp(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, anchor.left, anchor.bottom,
                                        g->panel, hints);
    DestroyMenu(menu);
    if (choice > 0 && static_cast<size_t>(choice) <= locations.size()) {
        const auto &path = locations[choice - 1].path;
        invalidateNaming();
        auto name = composedName(&path);
        if (g->ruleError.empty())
            apply(path, name, false, false);
        else
            updatePanelContent();
    }
}
void showTargetActions() {
    if (g->busy || !g->targetInfo || g->targetInfo->request.path != currentSaveTarget())
        return;
    auto info = *g->targetInfo;
    auto menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (!info.exists ? MF_GRAYED : 0), 1, L"换用下一个编号");
    AppendMenuW(menu, MF_STRING, 2, L"修改文件名…");
    AppendMenuW(menu, MF_STRING, 3, L"打开所在文件夹");
    AppendMenuW(menu, MF_STRING, 4, L"重新检查同名文件");
    auto generation = g->generation.load();
    int choice = experienceMenu(menu, ID_TARGET_INFO);
    DestroyMenu(menu);
    if (!g->attached || generation != g->generation || info.request.path != currentSaveTarget())
        return;
    if (choice == 1) {
        checkpointNaming();
        if (g->structure.free) {
            g->parts = {g->stemName};
            g->structure.free = false;
            g->structure.literal = true;
            g->structure.literalName = g->stemName;
        }
        if (!g->numberMode)
            g->numberMode = 1;
        updateInlineRule();
    } else if (choice == 2) {
        PostMessageW(g->panel, WM_COMMAND, ID_FOCUS_NAME, 0);
    } else if (choice == 3)
        ShellExecuteW(nullptr, L"open", g->dialog.directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    else if (choice == 4) {
        g->targetPath.clear();
        updatePanelContent();
    }
}
