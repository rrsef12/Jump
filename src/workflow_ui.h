// Included inside main.cpp's UI namespace. Data and worker logic live separately.
HWND workflowOwner() {
    return g->manager && IsWindowVisible(g->manager) ? g->manager : g->attached ? g->panel : g->control;
}
FormData workflowForm(const std::wstring &title, const std::wstring &note, std::vector<FormField> fields) {
    return {title, note, std::move(fields), g->font, g->brush, g->bg, g->fg};
}
FormData settingsForm(const std::wstring &title, const std::wstring &note, std::vector<FormField> fields,
                      const wchar_t *confirm = L"保存") {
    auto form = workflowForm(title, note, std::move(fields));
    form.settings = true;
    form.confirmText = confirm;
    return form;
}
void workflowNotice(const std::wstring &text) {
    MessageBoxW(workflowOwner(), text.c_str(), L"Jump", MB_OK | MB_ICONINFORMATION);
}
int workflowMenu(HMENU menu) {
    POINT point{};
    GetCursorPos(&point);
    auto owner = workflowOwner();
    SetForegroundWindow(owner);
    int choice = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, owner);
    DestroyMenu(menu);
    return choice;
}
bool copyWorkflowText(const std::wstring &text) {
    if (!OpenClipboard(workflowOwner()))
        return false;
    bool copied = false;
    auto memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (memory) {
        auto buffer = GlobalLock(memory);
        if (buffer) {
            memcpy(buffer, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(memory);
            copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
            if (!copied)
                GlobalFree(memory);
        } else
            GlobalFree(memory);
    }
    CloseClipboard();
    return copied;
}
void refreshProjectContext() {
    invalidateNaming();
    g->namingHistory.clear();
    if (g->structure.enabled && g->structuredProject != g->cfg.activeProject) {
        resetStructuredInput();
        g->structure = namingSession(namingDefinition(g->structure), L"");
        g->structuredProject = g->cfg.activeProject;
        g->freeRecovery.reset();
        g->namingDrafts.clear();
    }
    if (g->attached) {
        g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
        updateInlineRule();
        rebuildPanel();
    }
}
void editProject(bool create, size_t index = static_cast<size_t>(-1)) {
    openSettingsPage(MANAGER_PROJECTS);
    Project project;
    if (!create && index < g->cfg.projects.size())
        project = g->cfg.projects[index];
    else if (!create && activeProject(g->cfg))
        project = *activeProject(g->cfg);
    else {
        if (g->cfg.projects.size() >= 100) {
            workflowNotice(L"最多保留 100 个项目。");
            return;
        }
        GUID id{};
        CoCreateGuid(&id);
        wchar_t value[40]{};
        StringFromGUID2(id, value, 40);
        project.id = value;
        project.name = L"新项目";
        project.root = g->dialog.directory;
        project.variables = L"项目=\r\n作者=";
    }
    auto form = settingsForm(create ? L"新建项目" : L"编辑项目",
                             L"项目变量优先于方案默认值。工作与交付位置相对于根目录；搬盘时只需修改根目录。",
                             {{L"项目名称", project.name},
                              {L"项目根目录（完整路径）", project.root},
                              {L"工作位置（相对路径）", project.work},
                              {L"交付位置（相对路径）", project.delivery},
                              {L"项目变量（每行 名称=值）", project.variables, true}});
    while (showForm(workflowOwner(), form)) {
        project.name = trim(form.fields[0].value);
        project.root = pathEditValue(form.fields[1].value);
        project.work = trim(form.fields[2].value);
        project.delivery = trim(form.fields[3].value);
        project.variables = form.fields[4].value;
        if (project.name.empty() || projectPath(project, project.work).empty() ||
            projectPath(project, project.delivery).empty()) {
            workflowNotice(L"请输入项目名称、完整根目录和根目录内的相对位置；相对位置不能包含 ..。");
            continue;
        }
        auto validation =
            smartNamingPreview(L"示例", project.root, L"", {0, false, L"{name}", project.variables});
        if (!validation.error.empty()) {
            workflowNotice(validation.error);
            continue;
        }
        auto previous = g->cfg;
        auto found = std::find_if(g->cfg.projects.begin(), g->cfg.projects.end(),
                                  [&](const auto &p) { return p.id == project.id; });
        if (found == g->cfg.projects.end())
            g->cfg.projects.push_back(project);
        else
            *found = project;
        g->cfg.activeProject = project.id;
        if (!persist()) {
            g->cfg = previous;
            workflowNotice(L"项目未保存，请检查配置目录权限。");
            return;
        }
        refreshProjectContext();
        refreshSettingsPage();
        return;
    }
}
void handleProjectChoice(int choice) {
    if (choice == 2 || choice == 3) {
        editProject(choice == 2);
        return;
    }
    if (choice == 4 || choice == 5) {
        if (auto project = activeProject(g->cfg); project && g->attached) {
            auto path = projectPath(*project, choice == 4 ? project->work : project->delivery);
            invalidateNaming();
            g->stemName = composedName(&path);
            apply(path, g->stemName, false);
        }
        return;
    }
    if (choice == 6) {
        if (MessageBoxW(workflowOwner(), L"移除当前项目设置？项目文件不会删除，引用相对目录的方案仍保留。",
                        L"移除项目", MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
        auto previous = g->cfg;
        std::erase_if(g->cfg.projects, [&](const auto &p) { return p.id == g->cfg.activeProject; });
        g->cfg.activeProject.clear();
        if (!persist()) {
            g->cfg = previous;
            return;
        }
        refreshProjectContext();
        return;
    }
    if (choice == 1 || (choice >= 100 && static_cast<size_t>(choice - 100) < g->cfg.projects.size())) {
        auto previous = g->cfg.activeProject;
        g->cfg.activeProject = choice == 1 ? L"" : g->cfg.projects[choice - 100].id;
        if (!persist()) {
            g->cfg.activeProject = previous;
            return;
        }
        refreshProjectContext();
    }
}
void showProjects() {
    openSettingsPage(MANAGER_PROJECTS);
}

void saveCurrentRecipe() {
    if (!g->attached || !validName(g->parts.base) || !g->ruleError.empty()) {
        workflowNotice(L"请先组成有效名称，再保存方案。");
        return;
    }
    if (g->cfg.recipes.size() >= 100) {
        workflowNotice(L"最多保留 100 个方案。");
        return;
    }
    openSettingsPage(2);
    auto form = settingsForm(
        L"保存流程",
        L"记住目录、命名结构、当前状态、动态日期、编号方式、变量模板及替换规则。项目变量继续引用当前项目。",
        {{L"方案名称", L"新保存流程"}});
    if (!showForm(workflowOwner(), form) || trim(form.fields[0].value).empty())
        return;
    auto recipe = captureRecipe(trim(form.fields[0].value));
    auto previous = g->cfg;
    auto duplicate = std::find_if(g->cfg.recipes.begin(), g->cfg.recipes.end(),
                                  [&](const auto &r) { return lower(r.name) == lower(recipe.name); });
    if (duplicate != g->cfg.recipes.end()) {
        if (duplicate->namingOnly) {
            workflowNotice(L"同名命名方案已存在，请为保存流程使用其他名称。");
            return;
        }
        if (MessageBoxW(workflowOwner(), L"已存在同名方案，是否替换其完整设置？", L"替换方案",
                        MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
        *duplicate = recipe;
    } else
        g->cfg.recipes.push_back(recipe);
    if (!persist()) {
        g->cfg = previous;
        workflowNotice(L"方案未保存，请检查配置目录权限。");
    } else {
        refreshSettingsPage();
        workflowNotice(L"保存流程已保存。使用时日期和编号会重新计算。");
    }
}
void addExampleRecipes() {
    auto previous = g->cfg;
    const std::vector<std::pair<std::wstring, std::wstring>> examples{
        {L"示例：角色交付", L"{项目}_{角色}_{LOD}"}, {L"示例：工作版本", L"{项目}_{name}_{作者}"}};
    for (const auto &[name, expression] : examples) {
        if (g->cfg.recipes.size() >= 100)
            break;
        if (std::any_of(g->cfg.recipes.begin(), g->cfg.recipes.end(),
                        [&](const auto &r) { return r.name == name; }))
            continue;
        Recipe recipe{name, L"", L"工作文件", false, false};
        recipe.complete = recipe.variables = true;
        recipe.counter = 1;
        recipe.expression = expression;
        recipe.variableValues = L"项目=\r\n角色=\r\nLOD=LOD0\r\n作者=";
        recipe.ruleEnabled = true;
        recipe.pattern = L" ";
        recipe.replacement = L"_";
        g->cfg.recipes.push_back(recipe);
    }
    if (!persist()) {
        g->cfg = std::move(previous);
        workflowNotice(L"示例未保存，原方案已保留。请检查配置目录写入权限。");
        return;
    }
    workflowNotice(L"示例方案已就绪。选择后，在“配置变量表达式”中填写缺失字段。");
}
void showSavedRecords() {
    auto menu = CreatePopupMenu();
    if (g->cfg.savedFiles.empty())
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 1, L"尚无已核验的文件记录");
    for (size_t i = 0; i < g->cfg.savedFiles.size(); ++i) {
        const auto &file = g->cfg.savedFiles[i];
        auto label = file.verifiedAt + L" · " + std::filesystem::path(file.path).filename().wstring();
        AppendMenuW(menu, MF_STRING, 100 + i, label.c_str());
    }
    int choice = workflowMenu(menu);
    if (choice < 100 || static_cast<size_t>(choice - 100) >= g->cfg.savedFiles.size())
        return;
    auto file = g->cfg.savedFiles[choice - 100];
    auto detail = CreatePopupMenu();
    for (const auto &line : {file.path, L"软件：" + file.process, L"已核验写入：" + file.verifiedAt})
        AppendMenuW(detail, MF_STRING | MF_GRAYED, 0, line.c_str());
    AppendMenuW(detail, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(detail, MF_STRING, 1, L"打开所在目录");
    AppendMenuW(detail, MF_STRING, 2, L"复制完整路径");
    choice = workflowMenu(detail);
    if (choice == 1)
        ShellExecuteW(workflowOwner(), L"open", std::filesystem::path(file.path).parent_path().c_str(),
                      nullptr, nullptr, SW_SHOW);
    if (choice == 2)
        copyWorkflowText(file.path);
}
void showNameExplanation() {
    auto options = namingOptions();
    auto source = compose(g->parts);
    auto ruled = g->ruleEnabled
                     ? renamePreview(source, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex)
                     : RenamePreview{source};
    auto lightweight = options;
    lightweight.counter = 0;
    auto expanded = smartNamingPreview(ruled.name, g->dialog.directory, g->dialog.extension, lightweight);
    auto menu = CreatePopupMenu();
    auto line = [&](const std::wstring &text, int id) { AppendMenuW(menu, MF_STRING, id, text.c_str()); };
    line(L"命名字段：" + g->parts.base, ID_STRUCT_FIELDS_VIEW);
    line(L"前缀：" + g->parts.prefix + L"　后缀：" + g->parts.suffix, ID_NAMING_MENU);
    line(L"1. 原始组合：" + source, ID_FOCUS_NAME);
    line(g->ruleEnabled ? L"2. 替换规则：" + (ruled.error.empty() ? ruled.name : ruled.error)
                        : L"2. 替换规则：未启用",
         ID_RENAME);
    line(options.expression.empty() ? L"3. 变量模板：未启用" : L"3. 变量模板：" + options.expression,
         ID_VARIABLE_EDIT);
    if (options.folder)
        line(L"目录名称已加入组合", ID_DIRECTORY_EDIT);
    if (!expanded.error.empty())
        line(L"需要处理：" + expanded.error, ID_VARIABLE_EDIT);
    line(g->numberMode ? L"4. 自动编号：" + std::wstring(g->namingPending ? L"计算中" : g->stemName)
                       : L"4. 自动编号：未启用",
         ID_STRUCT_SCHEMES);
    if (g->numberMode)
        line(L"重新核验编号预览", ID_REFRESH_NUMBER);
    line(L"最终名称：" + g->stemName + g->dialog.extension, ID_FOCUS_NAME);
    if (!g->ruleError.empty())
        line(L"错误：" + g->ruleError, g->ruleEnabled && !ruled.error.empty() ? ID_RENAME : ID_VARIABLE_EDIT);
    auto choice = workflowMenu(menu);
    int tool = choice == ID_NAMING_MENU ? 2 : choice == ID_RENAME ? 3 : 0;
    if (tool && g->inlineTool == tool) {
        SetFocus(GetDlgItem(g->panel, tool == 2 ? ID_AFFIX_PREFIX : ID_RULE_PATTERN));
    } else if (choice)
        SendMessageW(g->panel, WM_COMMAND, choice, 0);
}
std::wstring chooseTransferFile(bool saving) {
    wchar_t path[32768]{};
    if (saving)
        wcscpy_s(path, L"Jump-presets.ini");
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = workflowOwner();
    ofn.lpstrFilter = L"Jump 配置 (*.ini)\0*.ini\0\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = 32768;
    ofn.lpstrDefExt = L"ini";
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                (saving ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (saving ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn)) ? path : L"";
}
std::vector<size_t> selectedTransferItems(const std::wstring &text, size_t count) {
    std::vector<size_t> selected;
    if (trim(text) == L"*") {
        for (size_t i = 0; i < count; ++i)
            selected.push_back(i);
        return selected;
    }
    std::wistringstream stream(text);
    std::wstring token;
    while (std::getline(stream, token, L',')) {
        token = trim(token);
        if (token.empty())
            continue;
        if (!std::all_of(token.begin(), token.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }))
            throw std::runtime_error("selection");
        auto index = std::stoul(token);
        if (!index || index > count)
            throw std::runtime_error("selection");
        if (std::find(selected.begin(), selected.end(), index - 1) == selected.end())
            selected.push_back(index - 1);
    }
    return selected;
}
bool selectTransfer(Config &source) {
    std::wstring listing;
    auto describe = [&](const auto &items, const wchar_t *kind) {
        listing += std::wstring(kind) + L"：\r\n";
        for (size_t i = 0; i < items.size(); ++i)
            listing += std::to_wstring(i + 1) + L". " + items[i].name + L"\r\n";
    };
    describe(source.recipes, L"方案");
    describe(source.projects, L"项目");
    auto form = settingsForm(
        L"选择迁移内容", L"输入编号并以英文逗号分隔；* 表示全部，留空表示不选。只迁移选中的方案与项目。",
        {{L"可选内容", listing, true, true}, {L"方案编号", L"*"}, {L"项目编号", L"*"}}, L"继续");
    while (showForm(workflowOwner(), form)) {
        try {
            auto recipes = selectedTransferItems(form.fields[1].value, source.recipes.size());
            auto projects = selectedTransferItems(form.fields[2].value, source.projects.size());
            Config selected;
            selected.presetPackage = true;
            selected.recipes.clear();
            selected.projects.clear();
            for (auto i : recipes)
                selected.recipes.push_back(source.recipes[i]);
            for (auto i : projects)
                selected.projects.push_back(source.projects[i]);
            if (selected.recipes.empty() && selected.projects.empty()) {
                workflowNotice(L"请至少选择一项。");
                continue;
            }
            source = std::move(selected);
            return true;
        } catch (...) {
            workflowNotice(L"编号无效，请使用列表中的编号，以英文逗号分隔。");
        }
    }
    return false;
}
void transferPresets(bool importing) {
    openSettingsPage(MANAGER_BACKUP);
    Config source = g->cfg;
    if (importing) {
        auto file = chooseTransferFile(false);
        if (file.empty())
            return;
        if (GetPrivateProfileIntW(L"Jump", L"schema", 0, file.c_str()) != 1) {
            workflowNotice(L"不是支持的 Jump 配置。");
            return;
        }
        source = load(file, true);
    }
    if (!selectTransfer(source))
        return;
    if (!importing) {
        auto file = chooseTransferFile(true);
        if (file.empty())
            return;
        if (normalize(file) == normalize(g->configFile.wstring())) {
            workflowNotice(L"请选择其他文件，不能用导出覆盖当前配置。");
            return;
        }
        workflowNotice(save(source, file) ? L"选中的预设已导出。项目变量和根目录也包含在文件中。"
                                          : L"导出失败，请检查目标权限。");
        return;
    }
    for (auto &project : source.projects) {
        auto form = settingsForm(L"映射项目根目录",
                                 L"为导入的项目指定这台电脑上的根目录。工作与交付的相对位置会保留。",
                                 {{project.name, project.root}}, L"继续");
        for (;;) {
            if (!showForm(workflowOwner(), form))
                return;
            project.root = pathEditValue(form.fields[0].value);
            if (!projectPath(project, project.work).empty() &&
                !projectPath(project, project.delivery).empty())
                break;
            workflowNotice(L"请输入有效的完整根目录。");
        }
    }
    Config merged = g->cfg;
    std::wstring report;
    int duplicates = 0;
    auto merge = [&](const auto &incoming, auto &existing, const wchar_t *kind) {
        for (auto item : incoming) {
            auto original = item.name;
            auto collision = [&] {
                return std::any_of(existing.begin(), existing.end(),
                                   [&](const auto &e) { return lower(e.name) == lower(item.name); });
            };
            if (collision()) {
                ++duplicates;
                int suffix = 1;
                do {
                    item.name = original + L"（导入 " + std::to_wstring(suffix++) + L"）";
                } while (collision());
            }
            if constexpr (requires { item.id; }) {
                GUID id{};
                CoCreateGuid(&id);
                wchar_t text[40]{};
                StringFromGUID2(id, text, 40);
                item.id = text;
            }
            if constexpr (requires { item.structure; }) {
                auto scheme = deserializeStructure(item.structure);
                if (scheme.enabled) {
                    scheme.id = newNamingIdentity();
                    scheme.name = item.name;
                    item.structure = serializeStructure(scheme);
                }
            }
            if (existing.size() >= 100)
                throw std::runtime_error("limit");
            report += std::wstring(kind) + L"：" + original +
                      (original == item.name ? L" → 新增" : L" → " + item.name) + L"\r\n";
            existing.push_back(std::move(item));
        }
    };
    try {
        merge(source.recipes, merged.recipes, L"方案");
        merge(source.projects, merged.projects, L"项目");
    } catch (...) {
        workflowNotice(L"导入后超过每类 100 项的上限，请减少选择。");
        return;
    }
    auto form = settingsForm(L"确认合并结果",
                             L"重复项 " + std::to_wstring(duplicates) +
                                 L" 个：保留原项，导入项自动改名。确认后先备份，再写入配置。",
                             {{L"本次变更", report, true, true}}, L"确认导入");
    if (!showForm(workflowOwner(), form))
        return;
    if (!flushPersistence() || !backupConfig(g->configFile)) {
        workflowNotice(L"备份失败，未导入。");
        return;
    }
    auto previous = g->cfg;
    g->cfg = merged;
    if (!persist()) {
        g->cfg = previous;
        workflowNotice(L"写入失败，原设置仍保留。");
        return;
    }
    refreshProjectContext();
    refreshSettingsPage();
    workflowNotice(L"导入完成；项目可从设置中心手动切换。");
}
void transferAction(int choice, const std::vector<std::filesystem::path> &backups) {
    if (choice == 1 || choice == 2) {
        transferPresets(choice == 2);
        return;
    }
    if (!flushPersistence()) {
        workflowNotice(L"当前记录尚未写入，无法创建或恢复备份。请检查配置目录权限。");
        return;
    }
    if (choice == 3) {
        workflowNotice(backupConfig(g->configFile) ? L"配置已备份，最多保留 5 份。" : L"备份失败。");
        return;
    }
    if (choice >= 100 && static_cast<size_t>(choice - 100) < backups.size()) {
        auto file = backups[choice - 100];
        if (GetPrivateProfileIntW(L"Jump", L"schema", 0, file.c_str()) != 1) {
            workflowNotice(L"备份格式无效。");
            return;
        }
        auto restored = load(file);
        auto text = L"恢复此备份中的所有配置？\n方案：" + std::to_wstring(restored.recipes.size()) +
                    L"，项目：" + std::to_wstring(restored.projects.size()) + L"\n当前配置会先备份。";
        if (MessageBoxW(workflowOwner(), text.c_str(), L"恢复配置", MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
        if (!backupConfig(g->configFile)) {
            workflowNotice(L"当前配置备份失败，未恢复。");
            return;
        }
        auto previous = g->cfg;
        g->cfg = restored;
        if (!persist()) {
            g->cfg = previous;
            workflowNotice(L"恢复写入失败。");
            return;
        }
        resetSmartNaming();
        refreshProjectContext();
        workflowNotice(L"配置已恢复。");
    }
}
void showTransfer() {
    openSettingsPage(MANAGER_BACKUP);
}
std::wstring diagnosticsText() {
    auto supported = [](bool value) { return value ? L"支持" : L"不可用 / 未确认"; };
    auto software = std::filesystem::path(g->dialog.process).filename().wstring();
    auto text =
        L"软件：" + software + L"\r\n接入：" +
        std::wstring(!g->attached        ? L"尚未关联"
                     : g->dialog.bridge  ? L"通用程序接口 v1"
                     : g->dialog.blender ? L"Blender"
                     : g->dialog.houdini ? L"Houdini"
                     : g->dialog.generic ? L"通用文件名控件"
                                         : L"Windows 文件窗口") +
        L"\r\n名称读写：" + supported(g->attached) + L"\r\n目录读取：" +
        supported(g->attached && !g->dialog.directory.empty()) + L"\r\n目录切换 / 输出位置：" +
        supported(g->attached && (!g->dialog.generic || g->dialog.genericNavigate)) + L"\r\n位置接入方式：" +
        std::wstring(!g->attached                                     ? L"尚未关联"
                     : g->dialog.genericDestination == 1              ? L"完整输出路径回填"
                     : g->dialog.genericDestination == 2              ? L"独立输出目录回填"
                     : g->dialog.bridge                               ? L"程序接口设置并重新读取"
                     : g->dialog.genericAddress                       ? L"地址栏跳转并重新读取"
                     : g->dialog.generic && g->dialog.genericNavigate ? L"Qt 文件窗口导航"
                     : g->dialog.generic                              ? L"未发现可核验的位置控件"
                                                                      : L"原生窗口 / 程序接口") +
        L"\r\n格式读取：" +
        supported(
            g->attached &&
            ((!g->dialog.generic && (!g->dialog.extension.empty() || !g->dialog.format.empty())) ||
             (g->dialog.generic && !g->dialog.format.empty() && g->dialog.format != L"保留原文件扩展名"))) +
        L"\r\n提交保存：" +
        supported(g->attached && g->dialog.save && (!g->dialog.generic || g->dialog.genericSubmit)) +
        L"\r\n文件名扩展名：" + supported(g->attached && !g->dialog.extension.empty()) +
        L"\r\n目标文件核验：" + supported(g->attached && !g->saveProbe.file.empty()) + L"\r\n编号计算：" +
        std::wstring(g->namingPending ? L"进行中" : L"空闲") +
        L"\r\n\r\n提交前编号核验适用于 Jump 发起的保存。原软件直接保存仍由原软件处理。\r\n" +
        L"通用接入的填入按钮不提交；支持时可从面板菜单调用原保存按钮。支持目录导航的窗口可直接使用 Jump "
        L"的快捷位置；格式选择仍在原软件完成。\r\n文件核验依据目标文件元数据变化，不读取文件内容。\r\n此记录"
        L"已省略路径、文件名、窗口标题、变量值及控件选择器。";
    return text;
}
void showDiagnostics() {
    openSettingsPage(MANAGER_CONNECTIONS);
}
