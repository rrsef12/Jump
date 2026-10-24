// Included in main.cpp after the common manager controls.
void startBinding();
void settingsPageList() {
    auto list = GetDlgItem(g->manager, M_LIST);
    if (!list)
        return;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    if (g->managerTab == MANAGER_PROJECTS) {
        for (const auto &project : g->cfg.projects) {
            auto name = project.name + (project.id == g->cfg.activeProject ? L" · 当前" : L"");
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
    } else if (g->managerTab == MANAGER_BACKUP) {
        for (const auto &file : configBackups(g->configFile))
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(file.filename().c_str()));
    } else if (g->managerTab == MANAGER_CONNECTIONS) {
        for (const auto &binding : g->cfg.filenameBindings) {
            auto name =
                std::filesystem::path(binding.executable).filename().wstring() + L" · " + binding.windowTitle;
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
    }
    auto count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
    g->editing = count ? std::min<size_t>(g->editing, count - 1) : 0;
    SendMessageW(list, LB_SETCURSEL, count ? g->editing : -1, 0);
}
void settingsPageSelect() {
    int index = static_cast<int>(SendDlgItemMessageW(g->manager, M_LIST, LB_GETCURSEL, 0, 0));
    g->editing = index >= 0 ? index : 0;
    if (g->managerTab == MANAGER_PROJECTS) {
        bool exists = index >= 0 && g->editing < g->cfg.projects.size();
        std::wstring detail = L"添加项目后填写根目录、工作位置和项目变量。\r\n更换硬盘时，只需修改根目录。";
        if (exists) {
            const auto &p = g->cfg.projects[g->editing];
            detail = L"根目录\r\n" + p.root + L"\r\n\r\n工作位置\r\n" + p.work + L"\r\n\r\n交付位置\r\n" +
                     p.delivery + L"\r\n\r\n项目变量\r\n" + p.variables;
        }
        SetDlgItemTextW(g->manager, M_INFO, detail.c_str());
        for (int id : {M_SAVE, M_DELETE, M_PROJECT_USE})
            EnableWindow(GetDlgItem(g->manager, id), exists);
        for (int id : {M_PROJECT_WORK, M_PROJECT_DELIVERY})
            EnableWindow(GetDlgItem(g->manager, id), activeProject(g->cfg) && g->attached);
    } else if (g->managerTab == MANAGER_BACKUP)
        EnableWindow(GetDlgItem(g->manager, M_RESTORE), index >= 0);
    else if (g->managerTab == MANAGER_CONNECTIONS)
        EnableWindow(GetDlgItem(g->manager, M_UNBIND), index >= 0);
}
int buildSettingsPage(HWND parent) {
    auto button = [&](const wchar_t *text, int id, int x, int y, int width) {
        return managerChild(parent, L"BUTTON", text, WS_TABSTOP, id, x, y, width, 28);
    };
    auto list = [&](int x, int y, int width, int height) {
        return managerChild(parent, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, M_LIST, x, y,
                            width, height);
    };
    if (g->managerTab == MANAGER_PROJECTS) {
        list(12, 52, 148, 290);
        managerChild(parent, L"EDIT", L"", ES_READONLY | ES_MULTILINE | WS_VSCROLL | WS_TABSTOP, M_INFO, 172,
                     52, 336, 210);
        button(L"编辑项目", M_SAVE, 172, 278, 104);
        button(L"使用项目", M_PROJECT_USE, 284, 278, 104);
        button(L"移除", M_DELETE, 396, 278, 112);
        button(L"新建项目", M_ADD, 12, 356, 148);
        button(L"不使用项目", M_PROJECT_NONE, 172, 356, 104);
        button(L"工作位置", M_PROJECT_WORK, 284, 356, 104);
        button(L"交付位置", M_PROJECT_DELIVERY, 396, 356, 112);
    } else if (g->managerTab == MANAGER_BACKUP) {
        managerLabel(parent, L"迁移命名方案、保存流程和项目", 12, 52, 496);
        button(L"选择并导出…", M_EXPORT, 12, 82, 240);
        button(L"导入并预览…", M_IMPORT, 264, 82, 244);
        managerLabel(parent, L"配置备份 · 最多保留 5 份", 12, 144, 496);
        list(12, 172, 496, 132);
        button(L"立即备份", M_BACKUP, 12, 320, 240);
        button(L"恢复选中备份…", M_RESTORE, 264, 320, 244);
        managerLabel(parent, L"恢复前先备份当前配置；导入时保留原有方案。", 12, 368, 496);
    } else {
        managerLabel(parent, L"当前保存窗口 · 诊断内容已省略私人路径和文件名", 12, 52, 496);
        managerChild(parent, L"EDIT", diagnosticsText(), ES_READONLY | ES_MULTILINE | WS_VSCROLL | WS_TABSTOP,
                     M_INFO, 12, 80, 496, 190);
        button(L"刷新诊断", M_SAVE, 12, 282, 156);
        button(L"复制诊断", M_COPY_DIAGNOSTICS, 180, 282, 156);
        managerLabel(parent, L"已记住的文件名输入框 · Ctrl + Alt + N 可快速关联", 12, 334, 496);
        list(12, 360, 496, 92);
        button(L"指定输入框…", M_BIND, 12, 466, 240);
        button(L"解除选中关联", M_UNBIND, 264, 466, 244);
    }
    settingsPageList();
    settingsPageSelect();
    return g->managerTab == MANAGER_CONNECTIONS ? 516 : 430;
}
bool settingsPageCommand(int id, int code) {
    if (g->managerTab < MANAGER_PROJECTS)
        return false;
    if (id == M_LIST && code == LBN_SELCHANGE) {
        settingsPageSelect();
        return true;
    }
    if (code != BN_CLICKED)
        return true;
    if (g->managerTab == MANAGER_PROJECTS) {
        if (id == M_ADD || id == M_SAVE)
            editProject(id == M_ADD, g->editing);
        if (id == M_PROJECT_NONE)
            handleProjectChoice(1);
        if (id == M_PROJECT_USE && g->editing < g->cfg.projects.size())
            handleProjectChoice(100 + static_cast<int>(g->editing));
        if (id == M_PROJECT_WORK || id == M_PROJECT_DELIVERY)
            handleProjectChoice(id == M_PROJECT_WORK ? 4 : 5);
        if (id == M_DELETE && g->editing < g->cfg.projects.size()) {
            if (MessageBoxW(g->manager, L"移除此项目设置？项目文件会保留。", L"移除项目",
                            MB_YESNO | MB_DEFBUTTON2) == IDYES) {
                auto previous = g->cfg;
                auto identity = g->cfg.projects[g->editing].id;
                g->cfg.projects.erase(g->cfg.projects.begin() + g->editing);
                if (g->cfg.activeProject == identity)
                    g->cfg.activeProject.clear();
                if (!persist())
                    g->cfg = std::move(previous);
                else
                    refreshProjectContext();
            }
        }
    } else if (g->managerTab == MANAGER_BACKUP) {
        if (id == M_EXPORT || id == M_IMPORT)
            transferAction(id == M_IMPORT ? 2 : 1, {});
        if (id == M_BACKUP)
            transferAction(3, {});
        if (id == M_RESTORE) {
            auto backups = configBackups(g->configFile);
            if (g->editing < backups.size())
                transferAction(100 + static_cast<int>(g->editing), backups);
        }
    } else {
        if (id == M_SAVE)
            SetDlgItemTextW(g->manager, M_INFO, diagnosticsText().c_str());
        if (id == M_COPY_DIAGNOSTICS)
            copyWorkflowText(windowText(GetDlgItem(g->manager, M_INFO)));
        if (id == M_BIND) {
            ShowWindow(g->manager, SW_MINIMIZE);
            startBinding();
        }
        if (id == M_UNBIND && g->editing < g->cfg.filenameBindings.size()) {
            auto previous = g->cfg;
            g->cfg.filenameBindings.erase(g->cfg.filenameBindings.begin() + g->editing);
            if (!persist())
                g->cfg = std::move(previous);
            else {
                detach();
                g->inspectAttempts = 0;
            }
        }
    }
    settingsPageList();
    settingsPageSelect();
    return true;
}
