// Field-based naming shares the save/numbering pipeline.
void rememberSubjectSelection(int index) {
    if (!primarySubject(index) || g->structuredIme)
        return;
    const auto &field = g->structure.fields[index];
    if (field.source != 0 || !namingFieldError(field, field.value).empty())
        return;
    if (jump::rememberSubject(g->cfg, g->cfg.activeProject, field.value))
        persistDeferred();
}
void rememberEditedSubject() {
    if (!g->structuredEdited || !g->structuredOriginal || !primarySubject(g->structuredField) ||
        !structuredInputError().empty() ||
        g->structure.fields[g->structuredField].value == g->structuredOriginal->value)
        return;
    rememberSubjectSelection(g->structuredField);
}
void rememberNativeSubject() {
    if (!g->nativeSubjectPending)
        return;
    auto subject = std::move(*g->nativeSubjectPending);
    g->nativeSubjectPending.reset();
    if (jump::rememberSubject(g->cfg, subject.projectId, subject.value))
        persistDeferred();
}
std::wstring recentSavedSubject(const SavedFile &record) {
    // Native saves before this fix have no structured metadata. Their verified
    // basename is still useful; structured saves must retain just the main field.
    if (record.structure.empty())
        return std::filesystem::path(record.path).stem().wstring();
    std::vector<std::wstring> packed;
    if (!unpackNaming(record.structure, packed) || packed.size() != 8 || packed[0] != L"JNSAVE1")
        return {};
    auto saved = deserializeStructure(packed[1]);
    if (!saved.enabled || saved.free)
        return {};
    if (saved.literal)
        return saved.literalName;
    for (const auto &field : saved.fields)
        if (field.id == saved.primaryField && field.source == 0 && !field.omitted)
            return field.value;
    return {};
}
std::vector<std::wstring> recentSubjectChoices(int index) {
    std::vector<std::wstring> values;
    if (!primarySubject(index))
        return values;
    const auto &field = g->structure.fields[index];
    auto add = [&](const std::wstring &value) {
        if (values.size() < 8 && value.size() <= 120 && validName(value) &&
            std::none_of(
                g->cfg.removedSubjects.begin(), g->cfg.removedSubjects.end(),
                [&](const auto &s) { return s.projectId == g->cfg.activeProject && s.value == value; }) &&
            namingFieldError(field, value).empty() &&
            std::find(values.begin(), values.end(), value) == values.end())
            values.push_back(value);
    };
    for (const auto &subject : g->cfg.recentSubjects)
        if (subject.projectId == g->cfg.activeProject)
            add(subject.value);
    // Existing verified saves also supply useful history when upgrading.
    for (const auto &record : g->cfg.savedFiles) {
        if (record.projectId != g->cfg.activeProject)
            continue;
        add(recentSavedSubject(record));
    }
    return values;
}
void resetStructuredInput() {
    g->structuredField = -1;
    g->structuredDraft.clear();
    g->structuredOriginal.reset();
    g->structuredBaseline.reset();
    g->structuredEdited = g->structuredCheckpoint = false;
}
void restoreNamingState(const NamingSnapshot &s) {
    resetStructuredInput();
    invalidateNaming();
    g->parts = s.parts;
    g->sessionExpression = s.expression;
    g->sessionVariables = s.variables;
    g->rulePattern = s.pattern;
    g->ruleReplacement = s.replacement;
    g->numberMode = s.counter;
    g->sessionDigits = s.digits;
    g->sessionDateFormat = s.dateFormat;
    g->ruleIndex = s.ruleIndex;
    g->folderName = s.folder;
    g->variablesEnabled = s.variablesEnabled;
    g->sessionUpper = s.upper;
    g->ruleEnabled = s.ruleEnabled;
    g->ruleRegex = s.regex;
    g->structure = s.structure;
    g->freeRecovery = s.recovery ? std::optional<NamingSnapshot>(*s.recovery) : std::nullopt;
    g->structuredField = -1;
    g->structuredDraft.clear();
}
void enterFreeNaming() {
    if (!g->structure.enabled || g->structure.free)
        return;
    if (!structuredInputError().empty())
        return;
    resetStructuredInput();
    checkpointNaming();
    g->freeRecovery = captureNaming();
    g->structure.free = true;
    g->structuredField = -1;
    g->structuredDraft.clear();
    g->rebuildPending = true;
    PostMessageW(g->control, WM_PANEL_REFRESH, 0, 0);
}
void adoptNativeName() {
    // Keep only the latest native edit. Commit when the user returns to Jump,
    // or after a verified save, rather than recording each polled input fragment.
    g->nativeSubjectPending.reset();
    if (g->dialog.save && validName(g->stemName))
        g->nativeSubjectPending = RecentSubject{g->cfg.activeProject, g->stemName};
    g->ruleEnabled = false;
    g->ruleError.clear();
    resetSmartNaming();
    g->namingHistory.clear();
    if (!g->structure.enabled || g->structure.free) {
        syncNameParts();
        return;
    }
    bool rebuild = g->structuredField >= 0 || g->inlineTool != 0;
    resetStructuredInput();
    g->inlineTool = 0;
    g->freeRecovery.reset();
    auto &fields = g->structure.fields;
    auto subject = std::find_if(fields.begin(), fields.end(),
                                [&](const auto &f) { return f.id == g->structure.primaryField; });
    if (subject == fields.end())
        subject = std::find_if(fields.begin(), fields.end(), [](const auto &f) { return f.required; });
    if (subject == fields.end())
        subject = fields.begin();
    // A native edit is a literal new baseline. Keep the editor and its candidate
    // libraries, but never reapply old fields, inherited values, rules or numbering.
    for (auto &field : fields) {
        field.value.clear();
        field.source = 0;
        field.omitted = false;
    }
    if (subject != fields.end())
        subject->value = g->stemName;
    g->structure.literal = true;
    g->structure.literalName = g->stemName;
    g->structure.counter = 0;
    g->parts = {g->stemName};
    if (rebuild) {
        g->rebuildPending = true;
        PostMessageW(g->control, WM_PANEL_REFRESH, 0, 0);
    }
}
std::wstring savedStructure() {
    if (!g->structure.enabled || g->structure.free || g->variablesEnabled || g->folderName)
        return {};
    auto s = g->structure;
    auto opt = namingOptions();
    s.counter = opt.counter;
    s.digits = opt.digits;
    s.dateFormat = opt.dateFormat;
    s.upper = opt.upperVersion;
    return packNaming({L"JNSAVE1", serializeStructure(s), g->parts.prefix, g->parts.suffix, g->rulePattern,
                       g->ruleReplacement, g->ruleEnabled ? L"1" : L"0", g->ruleRegex ? L"1" : L"0"});
}
bool loadSavedStructure(const std::wstring &text) {
    std::vector<std::wstring> v;
    if (!unpackNaming(text, v) || v.size() != 8 || v[0] != L"JNSAVE1")
        return false;
    auto s = deserializeStructure(v[1]);
    if (!s.enabled || s.free)
        return false;
    g->structure = s;
    g->numberMode = s.counter;
    g->sessionDigits = s.digits;
    g->sessionDateFormat = s.dateFormat;
    g->sessionUpper = s.upper;
    g->parts = {composeStructure(s, g->cfg).name, L"", L"", v[2], v[3]};
    g->rulePattern = v[4];
    g->ruleReplacement = v[5];
    g->ruleEnabled = v[6] == L"1";
    g->ruleRegex = v[7] == L"1";
    return true;
}
void rememberNamingDraft() {
    if (!g->attached || !g->structure.enabled)
        return;
    std::erase_if(g->namingDrafts,
                  [&](const auto &d) { return !IsWindow(d.hwnd) || d.hwnd == g->dialog.hwnd; });
    if (!attachedDialogAlive() || !IsWindowVisible(g->dialog.hwnd))
        return;
    if (g->namingDrafts.size() >= 8)
        g->namingDrafts.erase(g->namingDrafts.begin());
    auto token = ++g->draftToken;
    if (!SetPropW(g->dialog.hwnd, L"Jump.NamingDraft", reinterpret_cast<HANDLE>(token)))
        return;
    g->namingDrafts.push_back({g->dialog.hwnd, g->dialog.pid, g->dialog.name, g->dialog.directory,
                               g->originalStem, g->originalDirectory, captureNaming(), g->namingHistory,
                               g->freeRecovery, g->cfg.activeProject, token});
}
void resumeNamingDraft() {
    resetStructuredInput();
    g->structuredAdvanced = false;
    g->structuredResumeNumber = false;
    if (!g->dialog.save)
        return;
    std::erase_if(g->namingDrafts, [](const auto &d) {
        DWORD pid = 0;
        return !IsWindow(d.hwnd) || !GetWindowThreadProcessId(d.hwnd, &pid) || pid != d.pid;
    });
    g->structuredProject = g->cfg.activeProject;
    auto d = std::find_if(g->namingDrafts.begin(), g->namingDrafts.end(), [](const auto &v) {
        return v.hwnd == g->dialog.hwnd && v.pid == g->dialog.pid && v.project == g->cfg.activeProject &&
               reinterpret_cast<ULONG_PTR>(GetPropW(v.hwnd, L"Jump.NamingDraft")) == v.token &&
               v.observed == g->dialog.name && normalize(v.directory) == normalize(g->dialog.directory);
    });
    if (d != g->namingDrafts.end()) {
        restoreNamingState(d->naming);
        g->namingHistory = d->history;
        g->freeRecovery = d->recovery;
        g->originalStem = d->original;
        g->originalDirectory = d->originalDirectory;
        return;
    }
    auto path = g->dialog.directory.empty()
                    ? L""
                    : (std::filesystem::path(g->dialog.directory) / g->dialog.name).wstring();
    auto found = std::find_if(g->cfg.savedFiles.begin(), g->cfg.savedFiles.end(), [&](const auto &f) {
        return normalize(f.path) == normalize(path) && lower(f.process) == lower(g->dialog.process) &&
               f.projectId == g->cfg.activeProject;
    });
    if (found != g->cfg.savedFiles.end() && loadSavedStructure(found->structure)) {
        g->structuredResumeNumber = g->numberMode != 0;
        return;
    }
    {
        const auto &s = defaultNamingScheme(g->cfg, g->dialog.process);
        g->structure = namingSession(s, g->stemName);
        g->parts = {};
        g->numberMode = s.counter;
        g->sessionDigits = s.digits;
        g->sessionDateFormat = s.dateFormat;
        g->sessionUpper = s.upper;
        auto preview = composeStructure(g->structure, g->cfg);
        g->structuredResumeNumber =
            g->numberMode != 0 || !preview.error.empty() || preview.name != g->stemName;
    }
}
void updateStructuredChoices() {
    g->structuredChoices.clear();
    if (!g->structure.enabled || g->structuredField < 0 ||
        static_cast<size_t>(g->structuredField) >= g->structure.fields.size())
        return;
    const auto &field = g->structure.fields[g->structuredField];
    auto query = g->structuredEdited ? lower(trim(g->structuredDraft)) : L"";
    auto add = [&](const Preset &p) {
        if (p.value.empty() || !namingFieldError(field, p.value).empty())
            return;
        if (!query.empty() &&
            lower(p.name + L" " + p.value + L" " + p.keywords).find(query) == std::wstring::npos)
            return;
        if (std::none_of(g->structuredChoices.begin(), g->structuredChoices.end(),
                         [&](const auto &v) { return v.value == p.value; }))
            g->structuredChoices.push_back(p);
    };
    for (const auto &value : recentSubjectChoices(g->structuredField))
        add({value, value});
    for (const auto &p : quickCandidates(g->structuredField, !primarySubject(g->structuredField)))
        add(p);
    for (int i = 0; i < 6; ++i) {
        auto button = GetDlgItem(g->panel, ID_STRUCT_CHOICE + i);
        bool visible = static_cast<size_t>(i) < g->structuredChoices.size();
        showIfChanged(button, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible) {
            const auto &p = g->structuredChoices[i];
            setTextIfChanged(button, p.name.empty() ? p.value : p.name);
            tip(button, L"输出：" + p.value +
                            (namingFieldCanOmit(g->structure, g->structuredField)
                                 ? L"\n点击填入，再次点击取消；不保存文件"
                                 : L"\n点击替换，不保存文件") +
                            L"\n右键改名");
        }
    }
    auto empty = GetDlgItem(g->panel, ID_STRUCT_EMPTY);
    setTextIfChanged(
        empty, query.empty() && primarySubject(g->structuredField)
                   ? L"填写具体对象名称；最近名称可从右侧下拉复用。\n常用选项可在下方“常用选项…”中添加。"
               : query.empty() ? L"此项暂无常用选项，可直接输入。"
                               : L"此项没有匹配的常用选项；输入仍作为本次值使用。");
    showIfChanged(empty, g->structuredChoices.empty() ? SW_SHOWNOACTIVATE : SW_HIDE);
}
void updateStructuredContent() {
    if (!g->structure.enabled)
        return;
    auto visibility = namingVisibility();
    if (visibility != g->structuredVisibility && !g->structuredIme) {
        if (g->structuredField >= 0 && !(visibility & (1u << g->structuredField)))
            resetStructuredInput();
        g->structuredVisibility = visibility;
        g->rebuildPending = true;
        PostMessageW(g->control, WM_PANEL_REFRESH, 0, 0);
    }
    if (quickNamingView()) {
        bool wasBuilding = g->building;
        g->building = true;
        for (size_t i = 0; i < g->structure.fields.size(); ++i) {
            auto value = g->structuredField == static_cast<int>(i)
                             ? g->structuredDraft
                             : fieldValue(g->structure.fields[i], g->cfg, g->structure.dateFormat);
            if (!g->structuredIme)
                setTextIfChanged(GetDlgItem(g->panel, ID_QUICK_INPUT + static_cast<int>(i)), value);
            setTextIfChanged(GetDlgItem(g->panel, ID_QUICK_EDIT + static_cast<int>(i)),
                             value.empty() ? L"点击填写" : value);
            auto picks = g->quickPicks.find(static_cast<int>(i));
            if (picks != g->quickPicks.end()) {
                picks->second = visibleQuickChoices(static_cast<int>(i));
                for (size_t j = 0; j < 6; ++j) {
                    auto button =
                        GetDlgItem(g->panel, ID_QUICK_PICK + static_cast<int>(i) * 6 + static_cast<int>(j));
                    if (!button)
                        continue;
                    bool present = j < picks->second.size();
                    showIfChanged(button, present ? SW_SHOWNOACTIVATE : SW_HIDE);
                    if (!present)
                        continue;
                    const auto &p = picks->second[j];
                    setTextIfChanged(button, p.name.empty() ? p.value : p.name);
                    if (button)
                        tip(button, p.value +
                                        (namingFieldCanOmit(g->structure, i) ? L"\n点击替换，再点取消"
                                                                             : L"\n点击替换此项") +
                                        L"；右键改名");
                    auto mark = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(p.value == value ? 2 : 1));
                    if (button && GetPropW(button, L"Jump.Selected") != mark) {
                        SetPropW(button, L"Jump.Selected", mark);
                        InvalidateRect(button, nullptr, FALSE);
                    }
                }
            }
        }
        g->building = wasBuilding;
    }
    std::wstring summary = g->structure.free  ? L"直接编辑完整文件名"
                           : g->namingPending ? L"正在检查可用编号…"
                                              : L"确认文件名后，在原窗口保存";
    if (normalize(g->originalDirectory) != normalize(g->dialog.directory))
        summary += L"\n位置已改变";
    setTextIfChanged(GetDlgItem(g->panel, ID_STRUCT_SUMMARY), summary);
    tip(GetDlgItem(g->panel, ID_STRUCT_SUMMARY), L"原名：" + g->originalStem + L"\n本次：" + g->stemName +
                                                     L"\n" + summary + L"\n撤销命名不回退目录。");
    auto full = g->stemName + g->dialog.extension;
    tip(GetDlgItem(g->panel, ID_PREVIEW),
        full + L"\n右键复制完整文件名。" +
            (quickNamingView() ? L"\n点选字段即可修改；整名编辑、规则和编号均在“命名设置”中。" : L""));
    // Resize the result and footer in place, preserving active editors and IME.
    int previewHeight = structuredPreviewHeight(full, g->width - 2 * PANEL_MARGIN);
    if (g->structuredFullHeight != previewHeight) {
        auto preview = GetDlgItem(g->panel, ID_PREVIEW);
        if (preview) {
            RECT r{};
            GetWindowRect(preview, &r);
            MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
            positionChild(preview, r.left, r.top, r.right - r.left, px(previewHeight));
            g->structuredFullHeight = previewHeight;
        }
    }
    auto input = GetDlgItem(g->panel, ID_STRUCT_INPUT);
    if (input && g->structuredField >= 0 &&
        static_cast<size_t>(g->structuredField) < g->structure.fields.size()) {
        const auto &field = g->structure.fields[g->structuredField];
        setTextIfChanged(GetDlgItem(g->panel, ID_STRUCT_TITLE), field.label + L" · 输入即生效");
        setTextIfChanged(GetDlgItem(g->panel, ID_STRUCT_SOURCE), field.source == 1   ? L"继承项目"
                                                                 : field.source == 2 ? L"动态日期"
                                                                                     : L"本次值");
    }
    for (size_t i = 0; i < g->structure.fields.size(); ++i) {
        const auto &field = g->structure.fields[i];
        auto value = fieldValue(field, g->cfg, g->structure.dateFormat);
        auto button = GetDlgItem(g->panel, ID_STRUCT_FIELD + static_cast<int>(i));
        setTextIfChanged(button, g->structure.compact
                                     ? (value.empty() ? L"+" + field.label : value)
                                     : field.label + L"\n" + (value.empty() ? L"未添加" : value));
        if (button)
            tip(button, field.label + L"：" + value);
    }
    auto inputError = structuredInputError();
    if (!inputError.empty()) {
        setTextIfChanged(GetDlgItem(g->panel, ID_STATUS), inputError);
        showIfChanged(GetDlgItem(g->panel, ID_STATUS), SW_SHOWNOACTIVATE);
    }
    std::wstring rules;
    if (!g->parts.prefix.empty())
        rules += L"前缀：" + g->parts.prefix;
    if (!g->parts.suffix.empty())
        rules += (rules.empty() ? L"" : L" · ") + std::wstring(L"后缀：") + g->parts.suffix;
    if (g->ruleEnabled)
        rules += (rules.empty() ? L"" : L" · ") + std::wstring(L"替换已启用");
    auto settings = GetDlgItem(g->panel, ID_STRUCT_SCHEMES);
    setTextIfChanged(settings, rules.empty() ? L"命名设置 ▾" : L"命名设置 • ▾");
    tip(settings, L"切换方案、编辑字段、修饰规则、末尾编号和撤销还原。\n当前方案：" + g->structure.name +
                      L"\n末尾编号：" +
                      (g->numberMode == 1   ? std::wstring(L"自动版本")
                       : g->numberMode == 2 ? L"自动序号"
                                            : L"关闭") +
                      (rules.empty() ? L"" : L"\n" + rules));
    enableIfChanged(GetDlgItem(g->panel, ID_COMBO_UNDO), !g->busy && g->namingHistory.canUndo());
    enableIfChanged(GetDlgItem(g->panel, ID_COMBO_REDO), !g->busy && g->namingHistory.canRedo());
    enableIfChanged(GetDlgItem(g->panel, ID_STRUCT_DELIVERY), !g->busy && !g->structure.free);
    updateStructuredChoices();
}
void commitStructure(bool rebuild = true) {
    g->structure.free = false;
    g->parts.stage.clear();
    g->parts.date.clear();
    g->folderName = g->variablesEnabled = false;
    updateInlineRule(true);
    if (rebuild)
        rebuildPanel();
}
void beginStructuredEdit(StructuredNaming &structure) {
    if (!structure.literal)
        return;
    structure.literal = false;
    structure.literalName.clear();
    for (auto &f : structure.fields)
        if ((f.id != structure.primaryField || namingFieldDefaultSource(f) != 0) && f.defaultSource >= 0) {
            f.source = f.defaultSource;
            f.value = f.defaultValue;
            f.omitted = false;
        }
}
void beginStructuredEdit() {
    beginStructuredEdit(g->structure);
}
void selectStructuredField(int index) {
    if (index < 0 || static_cast<size_t>(index) >= g->structure.fields.size() || g->structure.free ||
        g->structuredIme || !namingFieldVisible(g->structure, index, g->cfg) ||
        !structuredInputError().empty())
        return;
    if (namingFieldFixed(g->structure.fields[index]))
        return;
    rememberEditedSubject();
    resetEditorDismiss();
    resetStructuredInput();
    g->structuredField = index;
    g->structuredOriginal = g->structure.fields[index];
    g->structuredEdited = g->structuredCheckpoint = false;
    if (g->structure.literal)
        g->structuredBaseline = g->structure;
    g->structuredDraft = fieldValue(g->structure.fields[index], g->cfg, g->structure.dateFormat);
    g->inlineTool = 0;
    g->structuredHistoryStart = g->namingHistory.mark();
    int inputId = quickNamingView() ? ID_QUICK_INPUT + index : ID_STRUCT_INPUT;
    if (auto input = GetDlgItem(g->panel, inputId)) {
        g->building = true;
        setTextIfChanged(input, g->structuredDraft);
        g->building = false;
        updatePanelContent();
    } else
        rebuildPanel();
    SetForegroundWindow(g->panel);
    SetFocus(GetDlgItem(g->panel, inputId));
    SendDlgItemMessageW(g->panel, inputId, EM_SETSEL, 0, -1);
}
std::wstring structuredInputError() {
    for (int id : {ID_AFFIX_PREFIX, ID_AFFIX_SUFFIX})
        if (auto input = GetDlgItem(g->panel, id)) {
            auto value = windowText(input);
            if (!value.empty() && !validAffix(value, id == ID_AFFIX_PREFIX))
                return L"前后缀包含无效字符，请修改或清空。";
        }
    if (!g->structure.enabled || g->structure.free || !g->structuredEdited || g->structuredField < 0 ||
        static_cast<size_t>(g->structuredField) >= g->structure.fields.size())
        return {};
    const auto &field = g->structure.fields[g->structuredField];
    auto value = trim(g->structuredDraft);
    auto error = namingFieldError(field, value);
    if (!error.empty() || value.size() > 120)
        return (error.empty() ? L"字段内容过长" : error) + L"；Esc 撤回本次输入。";
    return {};
}
void liveStructuredInput() {
    if (g->structuredIme || g->structuredField < 0 ||
        static_cast<size_t>(g->structuredField) >= g->structure.fields.size() || !g->structure.enabled ||
        g->structure.free)
        return;
    if (namingFieldFixed(g->structure.fields[g->structuredField]))
        return;
    if (!structuredInputError().empty()) {
        KillTimer(g->control, T_ACTION);
        invalidateNaming();
        updatePanelContent();
        return;
    }
    auto value = trim(g->structuredDraft);
    auto &field = g->structure.fields[g->structuredField];
    if (!g->structuredEdited) {
        updatePanelContent();
        return;
    }
    if (value == fieldValue(field, g->cfg, g->structure.dateFormat)) {
        updateInlineRule();
        return;
    }
    if (!g->structuredCheckpoint) {
        checkpointNaming();
        g->structuredCheckpoint = true;
    }
    beginStructuredEdit();
    field.value = value;
    field.source = 0;
    field.omitted = false;
    g->parts.stage.clear();
    g->parts.date.clear();
    g->folderName = g->variablesEnabled = false;
    updateInlineRule();
}
bool closeStructuredInput(bool cancel = false) {
    if (g->structuredIme)
        return false;
    if (!cancel && !structuredInputError().empty()) {
        updatePanelContent();
        return false;
    }
    bool restore = cancel && g->structuredOriginal && g->structuredField >= 0 &&
                   static_cast<size_t>(g->structuredField) < g->structure.fields.size();
    if (restore) {
        g->namingHistory.cancelField(g->structuredHistoryStart, g->structure.id, *g->structuredOriginal,
                                     g->structuredBaseline ? &*g->structuredBaseline : nullptr);
        if (g->structuredBaseline)
            g->structure = *g->structuredBaseline;
        else
            g->structure.fields[g->structuredField] = *g->structuredOriginal;
    }
    if (!cancel)
        rememberEditedSubject();
    resetStructuredInput();
    if (restore)
        updateInlineRule();
    rebuildPanel();
    return true;
}
// Chips, recent names, all-field choices and source restoration use one mutation.
void applyStructuredValue(int index, std::wstring value, bool toggle = false, int source = 0) {
    if (index < 0 || static_cast<size_t>(index) >= g->structure.fields.size() || g->structuredIme ||
        g->busySubmitting || g->structure.free || !namingFieldVisible(g->structure, index, g->cfg))
        return;
    auto &field = g->structure.fields[index];
    if (namingFieldFixed(field))
        return;
    value = trim(value);
    if (toggle && namingFieldCanOmit(g->structure, index) &&
        value == fieldValue(field, g->cfg, g->structure.dateFormat))
        value.clear();
    bool omitted = source == 0 && value.empty() && namingFieldCanOmit(g->structure, index);
    auto check = field;
    if (omitted)
        check.required = false;
    auto error = namingFieldError(check, value);
    if (!error.empty() || value.size() > 120) {
        g->status = error.empty() ? L"字段内容过长" : error;
        g->statusError = true;
        updatePanelContent();
        return;
    }
    checkpointNaming();
    beginStructuredEdit();
    field.value = value;
    field.source = source;
    field.omitted = omitted;
    g->structuredBaseline.reset();
    rememberSubjectSelection(index);
    if (g->structuredField == index) {
        g->structuredDraft = value;
        g->structuredOriginal = field;
        g->structuredEdited = g->structuredCheckpoint = false;
        g->structuredHistoryStart = g->namingHistory.mark();
        bool building = g->building;
        g->building = true;
        setTextIfChanged(GetDlgItem(g->panel, quickNamingView() ? ID_QUICK_INPUT + index : ID_STRUCT_INPUT),
                         value);
        g->building = building;
    }
    updateInlineRule(true);
}
void useStructuredValue(std::wstring value) {
    applyStructuredValue(g->structuredField, std::move(value));
}
void useStructuredSource(int index, int source) {
    if (index < 0 || static_cast<size_t>(index) >= g->structure.fields.size())
        return;
    auto field = g->structure.fields[index];
    field.source = source;
    field.omitted = false;
    applyStructuredValue(index, fieldValue(field, g->cfg, g->structure.dateFormat), false, source);
}

void configureStructure(int initialPage = 0, int initialField = -1);
void editFieldLibrary() {
    if (g->structuredField >= 0)
        configureStructure(2, g->structuredField);
}
std::wstring schemeSubject() {
    for (const auto &f : g->structure.fields)
        if (f.id == g->structure.primaryField && !namingFieldFixed(f) && f.source == 0) {
            auto value = fieldValue(f, g->cfg, g->structure.dateFormat);
            if (!value.empty())
                return value;
        }
    return g->originalStem.empty() ? g->stemName : g->originalStem;
}
bool storeNamingScheme(const StructuredNaming &scheme, bool activate,
                       const std::vector<std::wstring> *applications, const bool *globalDefault) {
    auto error = namingSchemaError(scheme);
    if (!error.empty()) {
        workflowNotice(error);
        return false;
    }
    auto definition = namingDefinition(scheme);
    auto text = serializeStructure(definition);
    if (text.size() > 110000) {
        workflowNotice(L"方案内容过多，请精简常用选项。");
        return false;
    }
    auto found = g->cfg.recipes.end();
    for (auto it = g->cfg.recipes.begin(); it != g->cfg.recipes.end(); ++it) {
        auto current = deserializeStructure(it->structure);
        if (it->namingOnly && current.id == scheme.id) {
            found = it;
            continue;
        }
        if (lower(it->name) == lower(scheme.name)) {
            workflowNotice(L"已有同名方案，请修改方案名称。");
            return false;
        }
    }
    if (found == g->cfg.recipes.end() && g->cfg.recipes.size() >= 100) {
        workflowNotice(L"最多保存 100 套方案。");
        return false;
    }
    auto previous = g->cfg;
    Recipe recipe;
    recipe.name = scheme.name;
    recipe.value = L"Asset";
    recipe.structure = std::move(text);
    recipe.complete = recipe.namingOnly = true;
    recipe.counter = scheme.counter;
    recipe.digits = scheme.digits;
    recipe.dateFormat = scheme.dateFormat;
    recipe.upperVersion = scheme.upper;
    if (found == g->cfg.recipes.end())
        g->cfg.recipes.push_back(std::move(recipe));
    else
        *found = std::move(recipe);
    if (activate)
        g->cfg.activeNamingScheme = scheme.id;
    if (applications && !setNamingSchemeApplications(g->cfg, scheme.id, *applications)) {
        g->cfg = std::move(previous);
        workflowNotice(L"软件关联未保存，请选择有效的 exe；最多关联 100 个软件。");
        return false;
    }
    if (globalDefault) {
        if (*globalDefault)
            g->cfg.defaultNamingScheme = scheme.id;
        else if (g->cfg.defaultNamingScheme == scheme.id)
            g->cfg.defaultNamingScheme.clear();
    }
    if (!persist()) {
        g->cfg = std::move(previous);
        workflowNotice(L"方案未保存，请检查配置目录权限。");
        return false;
    }
    return true;
}
void useNamingScheme(StructuredNaming scheme, bool remember) {
    if (!scheme.enabled || !namingSchemaError(scheme).empty())
        return;
    rememberEditedSubject();
    auto subject = schemeSubject();
    if (remember) {
        auto previous = g->cfg;
        g->cfg.activeNamingScheme = scheme.id;
        if (!persist()) {
            g->cfg = std::move(previous);
            workflowNotice(L"无法记住当前方案，请检查配置目录权限。");
            return;
        }
    }
    checkpointNaming();
    resetStructuredInput();
    resetSmartNaming();
    g->freeRecovery.reset();
    g->inlineTool = 0;
    g->structuredAdvanced = false;
    g->parts = {};
    g->ruleEnabled = false;
    g->rulePattern.clear();
    g->ruleReplacement.clear();
    g->structure = namingSession(std::move(scheme), subject);
    g->numberMode = g->structure.counter;
    g->sessionDigits = g->structure.digits;
    g->sessionDateFormat = g->structure.dateFormat;
    g->sessionUpper = g->structure.upper;
    commitStructure();
}
void applySchemeRevision(const StructuredNaming &definition, const StructuredNaming &previous,
                         const std::wstring &subject) {
    auto options = namingOptions();
    checkpointNaming();
    resetStructuredInput();
    invalidateNaming();
    g->freeRecovery.reset();
    g->inlineTool = 0;
    g->structure = reviseNamingSession(definition, previous, g->structure, subject);
    g->numberMode = definition.counter != previous.counter ? definition.counter : options.counter;
    g->sessionDigits = definition.digits != previous.digits ? definition.digits : options.digits;
    g->sessionDateFormat =
        definition.dateFormat != previous.dateFormat ? definition.dateFormat : options.dateFormat;
    g->sessionUpper = definition.upper != previous.upper ? definition.upper : options.upperVersion;
    commitStructure();
}
void configureStructure(int initialPage, int initialField) {
    auto previous = namingDefinition(g->structure);
    for (const auto &saved : namingSchemeCatalog(g->cfg))
        if (saved.id == previous.id) {
            previous = saved;
            break;
        }
    auto draft = previous;
    auto subject = schemeSubject();
    auto applications = namingSchemeApplications(g->cfg, draft.id);
    CompactFormStyle style{g->smallFont, g->inputBrush, g->field,   g->muted,         g->line,
                           g->accent,    g->primary,    g->surface, paintRoundedRect, styleDropdown};
    style.popupMenu = trackPopupMenuWithHelp;
    auto theme = workflowForm(L"命名方案", L"", {});
    theme.compact = &style;
    auto target = g->dialog.hwnd;
    bool globalDefault = defaultNamingScheme(g->cfg).id == draft.id;
    while (showNamingDesigner(g->panel, draft, g->cfg, theme, subject, applications, true, initialPage,
                              initialField, &globalDefault)) {
        if (g->dialog.hwnd != target)
            return;
        if (!storeNamingScheme(draft, true, &applications, &globalDefault))
            continue;
        if (g->manager)
            DestroyWindow(g->manager);
        SetForegroundWindow(target);
        applySchemeRevision(draft, previous, subject);
        return;
    }
}
void showNamingSettings(bool direct) {
    if (composingNaming() || g->busySubmitting)
        return;
    constexpr int schemeFirst = 10000;
    UINT editFlags = MF_STRING | (structuredInputError().empty() ? 0 : MF_GRAYED);
    bool editable = !(editFlags & MF_GRAYED), fieldMode = !g->structure.free;
    auto menu = CreatePopupMenu(), schemeMenu = CreatePopupMenu();
    namingCatalog(g->cfg);
    auto catalog = g->cfg.namingCatalog;
    const auto &schemes = catalog->schemes;
    for (size_t i = 0; i < schemes.size(); ++i)
        AppendMenuW(schemeMenu, MF_STRING | (schemes[i].id == g->structure.id ? MF_CHECKED : 0),
                    schemeFirst + static_cast<UINT_PTR>(i), schemes[i].name.c_str());
    AppendMenuW(schemeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, editFlags | (fieldMode ? 0 : MF_GRAYED), ID_STRUCT_DESIGN, L"方案设置…");
    if (direct)
        AppendMenuW(schemeMenu, editFlags | (fieldMode ? 0 : MF_GRAYED), ID_STRUCT_DESIGN, L"方案设置…");
    AppendMenuW(schemeMenu, editFlags, ID_STRUCT_EXAMPLE, L"从现有名称建立方案…");
    AppendMenuW(schemeMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(schemeMenu, MF_STRING, 3, L"导入命名方案…");
    AppendMenuW(schemeMenu, MF_STRING, 4, L"选择方案并导出…");
    bool removable = namingCatalog(g->cfg).recipeIndices.contains(g->structure.id);
    if (removable) {
        AppendMenuW(schemeMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(schemeMenu, MF_STRING, 2,
                    builtinNamingScheme(g->structure.id) ? L"还原当前方案初始规则…" : L"删除当前方案…");
    }
    AppendMenuW(menu, MF_POPUP | (editFlags & MF_GRAYED), reinterpret_cast<UINT_PTR>(schemeMenu),
                L"命名方案");
    auto current = CreatePopupMenu();
    AppendMenuW(current, MF_STRING | (fieldMode ? 0 : MF_GRAYED) | (g->structuredAdvanced ? MF_CHECKED : 0),
                g->structuredAdvanced ? ID_STRUCT_QUICK_VIEW : ID_STRUCT_FIELDS_VIEW, L"显示全部字段");
    AppendMenuW(current, MF_STRING, ID_STRUCT_FREE,
                g->structure.free ? L"恢复分项填写" : L"直接修改完整文件名");
    AppendMenuW(current, MF_SEPARATOR, 0, nullptr);
    bool rulesActive = !g->parts.prefix.empty() || !g->parts.suffix.empty() || g->ruleEnabled;
    AppendMenuW(current, MF_STRING | (!g->parts.prefix.empty() || !g->parts.suffix.empty() ? MF_CHECKED : 0),
                ID_NAMING_MENU, L"前后缀…");
    AppendMenuW(current, MF_STRING | (g->ruleEnabled ? MF_CHECKED : 0), ID_RENAME, L"替换 / 正则…");
    AppendMenuW(current, MF_STRING | (rulesActive ? 0 : MF_GRAYED), ID_STRUCT_RULE_CLEAR, L"清除修饰规则");
    AppendMenuW(current, MF_SEPARATOR, 0, nullptr);
    for (auto [mode, caption] :
         {std::pair{1, L"自动版本 vNNN"}, std::pair{2, L"自动序号 NNN"}, std::pair{0, L"关闭自动编号"}})
        AppendMenuW(current,
                    MF_STRING | (fieldMode ? 0 : MF_GRAYED) | (g->numberMode == mode ? MF_CHECKED : 0),
                    ID_STRUCT_NUMBER_NONE + mode, caption);
    AppendMenuW(menu, MF_POPUP | (editable ? 0 : MF_GRAYED), reinterpret_cast<UINT_PTR>(current),
                L"本次命名");
    auto recovery = CreatePopupMenu();
    AppendMenuW(recovery, MF_STRING | (g->namingHistory.canUndo() ? 0 : MF_GRAYED), ID_COMBO_UNDO,
                L"撤销命名组合");
    AppendMenuW(recovery, MF_STRING | (g->namingHistory.canRedo() ? 0 : MF_GRAYED), ID_COMBO_REDO,
                L"重做命名组合");
    AppendMenuW(recovery, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(recovery, MF_STRING | (g->dirtyName ? 0 : MF_GRAYED), ID_UNDO, L"还原本次名称与位置");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(recovery), L"撤销与还原");
    auto advanced = CreatePopupMenu();
    AppendMenuW(advanced, MF_STRING | (fieldMode ? 0 : MF_GRAYED), ID_STRUCT_DELIVERY,
                L"切到交付目录并应用交付选项…");
    AppendMenuW(advanced, MF_STRING, ID_PLANS, L"整套保存流程…");
    AppendMenuW(advanced, MF_STRING, ID_PROJECTS, L"项目设置…");
    AppendMenuW(advanced, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(advanced, MF_STRING, ID_PANEL_OPTIONS, L"面板设置…");
    AppendMenuW(menu, MF_POPUP | (editFlags & MF_GRAYED), reinterpret_cast<UINT_PTR>(advanced), L"更多工具");
    int anchorId = direct ? ID_STRUCT_NAME : ID_STRUCT_SCHEMES;
    auto control = GetDlgItem(g->panel, anchorId);
    RECT anchor{};
    GetWindowRect(control, &anchor);
    SetForegroundWindow(g->panel);
    g->openPanelMenu = anchorId;
    InvalidateRect(control, nullptr, FALSE);
    UpdateWindow(control);
    int choice = trackPopupMenu(direct ? schemeMenu : menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_RIGHTALIGN,
                                anchor.right, anchor.bottom + px(4), g->panel);
    g->openPanelMenu = 0;
    if (IsWindow(control)) {
        InvalidateRect(control, nullptr, FALSE);
        UpdateWindow(control);
    }
    DestroyMenu(menu);
    if (choice == 3 || choice == 4) {
        transferPresets(choice == 3);
        return;
    }
    if (choice == 2 && removable) {
        bool builtin = builtinNamingScheme(g->structure.id);
        if (MessageBoxW(g->panel,
                        builtin ? L"还原这套命名方案的初始规则？默认方案的设置会保留。"
                                : L"删除这套命名方案？已保存的资产文件不会修改。",
                        builtin ? L"还原命名方案" : L"删除命名方案", MB_OKCANCEL | MB_DEFBUTTON2) != IDOK)
            return;
        auto previous = g->cfg;
        auto id = g->structure.id;
        std::erase_if(g->cfg.recipes, [&](const auto &r) {
            return r.namingOnly && deserializeStructure(r.structure).id == id;
        });
        if (builtin)
            std::erase_if(g->cfg.candidateRenames, [&](const auto &r) { return r.schema == id; });
        if (!builtin)
            removeNamingSchemeDefaults(g->cfg, id);
        if (!persist()) {
            g->cfg = std::move(previous);
            workflowNotice(L"删除未保存，请检查配置目录权限。");
            return;
        }
        for (const auto &s : namingSchemeCatalog(g->cfg))
            if (s.id == (builtin ? id : L"asset-v2")) {
                useNamingScheme(s);
                break;
            }
        return;
    }
    if (choice < schemeFirst) {
        if (choice)
            PostMessageW(g->panel, WM_COMMAND, choice, 0);
        return;
    }
    if (static_cast<size_t>(choice - schemeFirst) >= schemes.size())
        return;
    auto scheme = schemes[choice - schemeFirst];
    useNamingScheme(std::move(scheme));
}
void structureFromExample() {
    openSettingsPage(0);
    auto first = settingsForm(L"从名称建立方案",
                              L"输入一个现有文件名（不含扩展名）。按分隔符列出各段后，由你确认每段含义；不会"
                              L"猜测日期、LOD 或其他数字。",
                              {{L"文件名（不含扩展名）", g->stemName}, {L"分隔符", L"_"}});
    if (!showForm(g->panel, first))
        return;
    auto name = trim(first.fields[0].value), sep = first.fields[1].value;
    if (!validName(name) || sep.empty() || sep.size() > 3) {
        workflowNotice(L"请输入有效名称和 1–3 字的分隔符。");
        return;
    }
    auto s = basicStructure(name);
    s.fields.clear();
    s.separator = sep;
    s.counter = 0;
    s.name = L"我的字段方案";
    std::vector<std::wstring> pieces;
    size_t at = 0;
    while (at <= name.size()) {
        auto end = name.find(sep, at);
        pieces.push_back(name.substr(at, end == name.npos ? name.npos : end - at));
        if (end == name.npos)
            break;
        at = end + sep.size();
    }
    if (pieces.size() > 8 ||
        std::any_of(pieces.begin(), pieces.end(), [](const auto &v) { return v.empty(); })) {
        workflowNotice(L"请使用最多 8 个非空片段；字段内的分隔符可在建立后编辑字段合并。");
        return;
    }
    std::vector<FormField> fields;
    for (size_t i = 0; i < pieces.size(); ++i)
        fields.push_back({pieces[i] + L" 的含义（版本填“版本”，其余自定义）",
                          i == 0 ? L"主要名称" : L"字段" + std::to_wstring(i + 1)});
    auto form = settingsForm(L"确认名称字段", L"仅标记为“版本”的末段 vNNN 会转成自动编号；其他内容保留原样。",
                             fields);
    while (showForm(g->panel, form)) {
        s.fields.clear();
        s.counter = 0;
        bool ok = true;
        for (size_t i = 0; i < pieces.size(); ++i) {
            auto label = trim(form.fields[i].value);
            if (label.empty() || label.size() > 40) {
                ok = false;
                break;
            }
            if (label == L"版本") {
                const auto &v = pieces[i];
                if (i + 1 != pieces.size() || v.size() < 2 || (v[0] != L'v' && v[0] != L'V') ||
                    !std::all_of(v.begin() + 1, v.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) {
                    ok = false;
                    break;
                }
                s.counter = 1;
                s.digits = std::clamp(static_cast<int>(v.size() - 1), 2, 4);
                s.upper = v[0] == L'V';
                continue;
            }
            auto id = (label == L"主要名称" || label == L"主体")   ? L"subject"
                      : (label == L"当前状态" || label == L"阶段") ? L"stage"
                      : (label == L"补充说明" || label == L"变体") ? L"variant"
                                                                   : L"field" + std::to_wstring(i);
            if (std::any_of(s.fields.begin(), s.fields.end(), [&](const auto &f) { return f.id == id; })) {
                ok = false;
                break;
            }
            NamingField field{id, label, pieces[i], true};
            upgradeNamingFieldLabel(field);
            s.fields.push_back(std::move(field));
        }
        if (!ok || s.fields.empty()) {
            workflowNotice(L"请填写不同字段的名称；版本必须是末段 v 后接数字。");
            continue;
        }
        GUID guid{};
        CoCreateGuid(&guid);
        wchar_t id[40]{};
        StringFromGUID2(guid, id, 40);
        s.id = id;
        auto primary =
            std::find_if(s.fields.begin(), s.fields.end(), [](const auto &f) { return f.id == L"subject"; });
        s.primaryField = (primary == s.fields.end() ? s.fields.front() : *primary).id;
        for (auto &f : s.fields) {
            f.defaultSource = 0;
            if (f.id != s.primaryField)
                f.defaultValue = f.value;
        }
        checkpointNaming();
        resetSmartNaming();
        g->parts = {};
        g->ruleEnabled = false;
        g->structure = s;
        g->numberMode = s.counter;
        g->sessionDigits = s.digits;
        g->sessionUpper = s.upper;
        g->structuredField = -1;
        commitStructure();
        return;
    }
}
void liveQuickInput(int index) {
    if (!quickNamingView() || index != g->structuredField)
        return;
    auto edit = GetDlgItem(g->panel, ID_QUICK_INPUT + index);
    if (!edit)
        return;
    g->structuredDraft = windowText(edit);
    g->structuredEdited = true;
    liveStructuredInput();
}
void pickQuickValue(int index, std::wstring value, bool toggle) {
    applyStructuredValue(index, std::move(value), toggle);
}
bool renameStructuredCandidate(HWND control) {
    if (GetParent(control) != g->panel)
        return false;
    int id = GetDlgCtrlID(control), index = -1, slot = -1;
    Preset candidate;
    if (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6) {
        index = g->structuredField;
        slot = id - ID_STRUCT_CHOICE;
        if (static_cast<size_t>(slot) >= g->structuredChoices.size())
            return true;
        candidate = g->structuredChoices[slot];
    } else if (id >= ID_QUICK_PICK && id < ID_QUICK_PICK + 48) {
        index = (id - ID_QUICK_PICK) / 6;
        slot = (id - ID_QUICK_PICK) % 6;
        auto found = g->quickPicks.find(index);
        if (found == g->quickPicks.end() || static_cast<size_t>(slot) >= found->second.size())
            return true;
        candidate = found->second[slot];
    } else
        return false;
    if (!g->structure.enabled || g->structure.free || g->structuredIme || !structuredInputError().empty() ||
        index < 0 || static_cast<size_t>(index) >= g->structure.fields.size())
        return true;
    auto schema = g->structure.id, fieldId = g->structure.fields[index].id;
    auto target = g->dialog.hwnd;
    auto form = workflowForm(L"重命名候选", L"同步更新已选中的文件名",
                             {{L"名称", candidate.name.empty() ? candidate.value : candidate.name}});
    CompactFormStyle style{g->smallFont, g->inputBrush, g->field,   g->muted,        g->line,
                           g->accent,    g->primary,    g->surface, paintRoundedRect};
    form.compact = &style;
    SetForegroundWindow(g->panel);
    while (showForm(g->panel, form)) {
        if (g->dialog.hwnd != target || g->structure.id != schema ||
            static_cast<size_t>(index) >= g->structure.fields.size() ||
            g->structure.fields[index].id != fieldId)
            return true;
        auto value = trim(form.fields[0].value);
        auto check = g->structure.fields[index];
        check.required = false;
        if (value.empty() || value.size() > 80 || !namingFieldError(check, value).empty()) {
            workflowNotice(L"请输入符合字段规则的 1–80 字名称。");
            continue;
        }
        if (value == candidate.value && (candidate.name.empty() || value == candidate.name))
            return true;
        auto choices = quickCandidates(index);
        if (std::any_of(choices.begin(), choices.end(), [&](const auto &p) {
                return p.value != candidate.value && (p.value == value || p.name == value);
            })) {
            workflowNotice(L"此字段已有同名选项，请换一个名称。");
            continue;
        }
        bool managed = builtinNamingScheme(schema) ||
                       std::any_of(g->cfg.recipes.begin(), g->cfg.recipes.end(), [&](const auto &r) {
                           return r.namingOnly && deserializeStructure(r.structure).id == schema;
                       });
        if (managed) {
            auto next = g->structure;
            auto &changed = next.fields[index];
            bool selected = fieldValue(changed, g->cfg, next.dateFormat) == candidate.value;
            bool existing = false;
            for (auto &p : changed.candidates)
                if (p.value == candidate.value) {
                    p.name = p.value = value;
                    existing = true;
                }
            if (!existing)
                changed.candidates.push_back({value, value, value});
            if (changed.defaultValue == candidate.value)
                changed.defaultValue = value;
            if (selected && !namingFieldFixed(changed)) {
                changed.value = value;
                changed.source = 0;
            }
            for (auto &f : next.fields)
                if (f.conditionField == fieldId &&
                    f.conditionValue == namingFieldOutput(changed, candidate.value))
                    f.conditionValue = namingFieldOutput(changed, value);
            for (auto &rule : next.locations)
                if (rule.field == fieldId && rule.value == namingFieldOutput(changed, candidate.value))
                    rule.value = namingFieldOutput(changed, value);
            if (!storeNamingScheme(next))
                continue;
            checkpointNaming();
            g->structure = std::move(next);
            if (g->structuredField == index) {
                g->structuredDraft = fieldValue(g->structure.fields[index], g->cfg, g->structure.dateFormat);
                g->structuredOriginal = g->structure.fields[index];
                g->structuredEdited = false;
                g->building = true;
                setTextIfChanged(
                    GetDlgItem(g->panel, quickNamingView() ? ID_QUICK_INPUT + index : ID_STRUCT_INPUT),
                    g->structuredDraft);
                g->building = false;
            }
            commitStructure(false);
            return true;
        }
        auto previous = g->cfg.candidateRenames;
        auto found =
            std::find_if(g->cfg.candidateRenames.begin(), g->cfg.candidateRenames.end(), [&](const auto &r) {
                return r.schema == schema && r.field == fieldId && r.from == candidate.value;
            });
        if (found == g->cfg.candidateRenames.end()) {
            if (g->cfg.candidateRenames.size() >= 512) {
                workflowNotice(L"改名记录已满，请通过方案设置整理常用选项。");
                return true;
            }
            g->cfg.candidateRenames.push_back({schema, fieldId, candidate.value, value});
        } else
            found->to = value;
        // Resolve repeated renames for built-in, saved and legacy candidates alike.
        for (auto &rename : g->cfg.candidateRenames)
            if (rename.schema == schema && rename.field == fieldId && rename.to == candidate.value)
                rename.to = value;
        if (!persist()) {
            g->cfg.candidateRenames = std::move(previous);
            workflowNotice(L"改名未保存，请检查配置目录权限。");
            continue;
        }
        auto &field = g->structure.fields[index];
        if (fieldValue(field, g->cfg, g->structure.dateFormat) == candidate.value) {
            checkpointNaming();
            field.value = value;
            field.source = 0;
            if (g->structuredField == index) {
                g->structuredDraft = value;
                g->structuredOriginal = field;
                g->structuredEdited = g->structuredCheckpoint = false;
                g->structuredHistoryStart = g->namingHistory.mark();
                g->building = true;
                setTextIfChanged(
                    GetDlgItem(g->panel, quickNamingView() ? ID_QUICK_INPUT + index : ID_STRUCT_INPUT),
                    value);
                g->building = false;
            }
            commitStructure(false);
        } else
            updatePanelContent();
        return true;
    }
    return true;
}
void quickChoices(int index) {
    if (index < 0 || static_cast<size_t>(index) >= g->structure.fields.size() || g->structuredIme)
        return;
    if (namingFieldFixed(g->structure.fields[index])) {
        if (structuredInputError().empty())
            configureStructure();
        return;
    }
    const auto &field = g->structure.fields[index];
    bool subject = primarySubject(index);
    auto choices = quickCandidates(index, !subject);
    auto recent = recentSubjectChoices(index);
    auto menu = CreatePopupMenu();
    auto current = fieldValue(field, g->cfg, g->structure.dateFormat);
    auto captionText = [](const std::wstring &value) {
        std::wstring text;
        for (auto c : value) {
            text += c;
            if (c == L'&')
                text += c;
        }
        return text;
    };
    auto appendChoices = [&](HMENU target) {
        for (size_t i = 0; i < choices.size(); ++i) {
            auto caption = choices[i].name.empty() ? choices[i].value : choices[i].name;
            if (caption != choices[i].value)
                caption += L" → " + choices[i].value;
            AppendMenuW(target, MF_STRING | (current == choices[i].value ? MF_CHECKED : 0),
                        100 + static_cast<UINT_PTR>(i), captionText(caption).c_str());
        }
    };
    if (subject) {
        if (recent.empty())
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 11, L"暂无最近名称");
        for (size_t i = 0; i < recent.size(); ++i)
            AppendMenuW(menu, MF_STRING | (current == recent[i] ? MF_CHECKED : 0), 1000 + i,
                        captionText(recent[i]).c_str());
        std::erase_if(choices, [&](const auto &p) {
            return std::find(recent.begin(), recent.end(), p.value) != recent.end();
        });
        if (!choices.empty()) {
            auto common = CreatePopupMenu();
            appendChoices(common);
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(common), L"常用选项");
        }
        if (!recent.empty()) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            auto remove = CreatePopupMenu();
            for (size_t i = 0; i < recent.size(); ++i)
                AppendMenuW(remove, MF_STRING, 2000 + i, (L"删除：" + captionText(recent[i])).c_str());
            AppendMenuW(remove, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(remove, MF_STRING, 7, L"清空最近名称");
            AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(remove), L"管理记录");
        }
    } else {
        if (namingFieldCanOmit(g->structure, index))
            AppendMenuW(menu, MF_STRING | (current.empty() ? MF_CHECKED : 0), 1, L"不添加");
        appendChoices(menu);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 4, L"使用项目名称");
        AppendMenuW(menu, MF_STRING, 5, L"使用当天日期");
        if (namingFieldDefaultSource(field) != 0)
            AppendMenuW(menu, MF_STRING, 6, L"恢复方案默认来源");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 3, L"自定义…");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 2, L"编辑常用选项…");
    }
    int choice = experienceMenu(menu, ID_QUICK_CHOICE + index);
    DestroyMenu(menu);
    bool removeOne = choice >= 2000 && static_cast<size_t>(choice - 2000) < recent.size();
    if (removeOne || (subject && choice == 7 && !recent.empty())) {
        auto previous = g->cfg;
        bool changed = false;
        if (removeOne)
            changed = removeRecentSubject(g->cfg, g->cfg.activeProject, recent[choice - 2000]);
        else {
            // Clear the entire project history, including names outside this
            // scheme's filter or the eight visible rows, so older saves stay hidden.
            std::set<std::wstring> values;
            for (const auto &item : g->cfg.recentSubjects)
                if (item.projectId == g->cfg.activeProject)
                    values.insert(item.value);
            for (const auto &record : g->cfg.savedFiles)
                if (record.projectId == g->cfg.activeProject)
                    values.insert(recentSavedSubject(record));
            for (const auto &value : values)
                changed = removeRecentSubject(g->cfg, g->cfg.activeProject, value) || changed;
        }
        if (changed && !persist()) {
            g->cfg = std::move(previous);
            workflowNotice(L"删除未保存，请检查配置目录权限。");
        } else if (changed) {
            g->status = removeOne ? L"已删除最近名称" : L"已清空最近名称";
            g->statusError = false;
        }
        updatePanelContent();
        return;
    }
    if (choice == 3) {
        if (g->structuredField != index)
            selectStructuredField(index);
        return;
    }
    if (choice == 4 || choice == 5 || choice == 6) {
        useStructuredSource(index, choice == 4 ? 1 : choice == 5 ? 2 : namingFieldDefaultSource(field));
        return;
    }
    if (choice == 2) {
        if (!structuredInputError().empty())
            return;
        rememberEditedSubject();
        resetStructuredInput();
        g->structuredAdvanced = true;
        selectStructuredField(index);
        editFieldLibrary();
        return;
    }
    if (choice >= 1000 && static_cast<size_t>(choice - 1000) < recent.size()) {
        pickQuickValue(index, recent[choice - 1000], false);
        return;
    }
    if (choice != 1 && (choice < 100 || static_cast<size_t>(choice - 100) >= choices.size()))
        return;
    pickQuickValue(index, choice == 1 ? L"" : choices[choice - 100].value, choice != 1);
}
bool structuredCommand(int id, int code) {
    rememberNativeSubject();
    if (id >= ID_QUICK_PICK && id < ID_QUICK_PICK + 48) {
        int index = (id - ID_QUICK_PICK) / 6, slot = (id - ID_QUICK_PICK) % 6;
        if (code == BN_CLICKED && g->quickPicks.contains(index) &&
            static_cast<size_t>(slot) < g->quickPicks[index].size())
            pickQuickValue(index, g->quickPicks[index][slot].value, true);
        return true;
    }
    if (id == ID_QUICK_DONE || (id >= ID_QUICK_EDIT && id < ID_QUICK_EDIT + 8)) {
        if (code != BN_CLICKED || !quickNamingView() || g->structuredIme)
            return true;
        if (!structuredInputError().empty()) {
            SetFocus(GetDlgItem(g->panel, ID_QUICK_INPUT + g->structuredField));
            return true;
        }
        int index = id == ID_QUICK_DONE || id - ID_QUICK_EDIT == g->structuredField ? -1 : id - ID_QUICK_EDIT;
        if (index >= 0 && static_cast<size_t>(index) < g->structure.fields.size() &&
            namingFieldFixed(g->structure.fields[index])) {
            g->status = L"固定文字请从“命名设置”中的方案编辑修改。";
            g->statusError = false;
            updatePanelContent();
            return true;
        }
        if (index < 0)
            closeStructuredInput();
        else
            selectStructuredField(index);
        return true;
    }
    if (id >= ID_QUICK_INPUT && id < ID_QUICK_INPUT + 8) {
        int index = id - ID_QUICK_INPUT;
        if (code == EN_CHANGE)
            liveQuickInput(index);
        return true;
    }
    if (id >= ID_QUICK_CHOICE && id < ID_QUICK_CHOICE + 8) {
        if (code == BN_CLICKED)
            quickChoices(id - ID_QUICK_CHOICE);
        return true;
    }
    if (id == ID_QUICK_CANCEL) {
        closeStructuredInput(true);
        return true;
    }
    if (id == ID_STRUCT_INPUT && code == EN_CHANGE) {
        g->structuredDraft = windowText(GetDlgItem(g->panel, id));
        g->structuredEdited = true;
        if (!g->structuredIme)
            liveStructuredInput();
        return true;
    }
    if (id == ID_STRUCT_LIVE) {
        if (g->structuredEdited)
            liveStructuredInput();
        return true;
    }
    if (id == ID_STRUCT_CANCEL) {
        closeStructuredInput(true);
        return true;
    }
    if ((id == ID_STRUCT_CLOSE || id == ID_STRUCT_USE) && code == BN_CLICKED) {
        closeStructuredInput();
        return true;
    }
    if (code != BN_CLICKED)
        return false;
    if (id < 753 || id > ID_STRUCT_CHOICE + 5)
        return false;
    if (g->busy && !liveStructuredControl(id))
        return true;
    if (id == ID_STRUCT_SCHEMES || id == ID_STRUCT_NAME) {
        if (!g->structuredIme && !g->busySubmitting &&
            (id == ID_STRUCT_SCHEMES || structuredInputError().empty()))
            showNamingSettings(id == ID_STRUCT_NAME);
        return true;
    }
    if (id == ID_STRUCT_DESIGN) {
        if (!g->structure.free && structuredInputError().empty() && !g->structuredIme)
            configureStructure();
        return true;
    }

    if (id == ID_STRUCT_EXAMPLE) {
        if (structuredInputError().empty() && !g->structuredIme)
            structureFromExample();
        return true;
    }
    if (!g->structure.enabled)
        return false;
    if ((g->structuredIme || !structuredInputError().empty()) && id != ID_STRUCT_CLEAR &&
        id != ID_STRUCT_SCHEMES && !(id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6))
        return true;
    if (id >= ID_STRUCT_NUMBER_NONE && id <= ID_STRUCT_NUMBER_SEQUENCE) {
        if (!g->structure.free) {
            checkpointNaming();
            g->numberMode = id - ID_STRUCT_NUMBER_NONE;
            commitStructure();
        }
        return true;
    }

    if (id == ID_STRUCT_RULE_CLEAR) {
        if (!structuredInputError().empty() || g->structuredIme)
            return true;
        checkpointNaming();
        g->parts.prefix.clear();
        g->parts.suffix.clear();
        g->ruleEnabled = false;
        g->rulePattern.clear();
        g->ruleReplacement.clear();
        updateInlineRule();
        rebuildPanel();
        return true;
    }
    if (id == ID_STRUCT_QUICK_VIEW || id == ID_STRUCT_FIELDS_VIEW) {
        if (!structuredInputError().empty() || g->structuredIme)
            return true;
        if (g->structure.free)
            return true;
        bool advanced = id != ID_STRUCT_QUICK_VIEW;
        if (g->structuredAdvanced == advanced)
            return true;
        rememberEditedSubject();
        resetStructuredInput();
        g->structuredAdvanced = advanced;
        g->inlineTool = 0;
        rebuildPanel();
        return true;
    }
    if (id >= ID_STRUCT_FIELD && id < ID_STRUCT_FIELD + 8) {
        int index = id - ID_STRUCT_FIELD;
        if (static_cast<size_t>(index) >= g->structure.fields.size() ||
            !namingFieldVisible(g->structure, index, g->cfg))
            return true;
        if (namingFieldFixed(g->structure.fields[index])) {
            g->status = L"固定文字请从“命名设置”中的方案编辑修改。";
            g->statusError = false;
            updatePanelContent();
            return true;
        }
        g->structuredAdvanced = true;
        if (id - ID_STRUCT_FIELD == g->structuredField)
            closeStructuredInput();
        else if (structuredInputError().empty())
            selectStructuredField(id - ID_STRUCT_FIELD);
        return true;
    }
    if (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6) {
        size_t i = id - ID_STRUCT_CHOICE;
        if (i < g->structuredChoices.size() && g->structuredField >= 0 &&
            static_cast<size_t>(g->structuredField) < g->structure.fields.size()) {
            applyStructuredValue(g->structuredField, g->structuredChoices[i].value, true);
        }
        return true;
    }
    if (id == ID_STRUCT_LIBRARY) {
        editFieldLibrary();
        return true;
    }
    if ((id == ID_STRUCT_INHERIT || id == ID_STRUCT_TODAY) && g->structuredField >= 0) {
        useStructuredSource(g->structuredField, id == ID_STRUCT_INHERIT ? 1 : 2);
        return true;
    }
    if (id == ID_STRUCT_CLEAR) {
        useStructuredValue(L"");
        return true;
    }

    if (id == ID_STRUCT_FREE) {
        if (g->structure.free) {
            if (g->freeRecovery) {
                checkpointNaming();
                auto previous = *g->freeRecovery;
                restoreNamingState(previous);
                updateInlineRule();
                rebuildPanel();
            } else {
                g->status = L"原结构来自其他会话，请重新选择方案。";
                g->statusError = true;
                updatePanelContent();
            }
        } else {
            enterFreeNaming();
            if (!g->structure.free)
                return true;
            resetSmartNaming();
            g->parts = {g->stemName};
            g->ruleEnabled = false;
            rebuildPanel();
            SetForegroundWindow(g->panel);
            SetFocus(GetDlgItem(g->panel, ID_FREE_NAME_INPUT));
            SendDlgItemMessageW(g->panel, ID_FREE_NAME_INPUT, EM_SETSEL, 0, -1);
        }
        return true;
    }
    if (g->structure.free)
        return true;

    if (id == ID_STRUCT_DELIVERY) {
        const auto *p = activeProject(g->cfg);
        auto path = p ? projectPath(*p, p->delivery) : L"";
        if (path.empty()) {
            workflowNotice(L"请先在项目中配置交付位置。");
            return true;
        }
        auto next = g->structure;
        beginStructuredEdit(next);
        bool marked = false;
        for (size_t i = 0; i < next.fields.size(); ++i) {
            auto &field = next.fields[i];
            if (field.id == next.primaryField || namingFieldFixed(field) ||
                !namingFieldVisible(next, i, g->cfg))
                continue;
            auto choice = std::find_if(field.candidates.begin(), field.candidates.end(), [&](const auto &p) {
                return (p.name == L"交付" || p.value == L"交付") && namingFieldError(field, p.value).empty();
            });
            if (choice != field.candidates.end()) {
                field.value = choice->value;
                field.source = 0;
                field.omitted = false;
                marked = true;
            }
        }
        if (!marked)
            next = g->structure;
        auto description = marked ? L"使用方案中已有的交付选项" : L"沿用当前方案的字段和编号";
        if (MessageBoxW(
                g->panel,
                (std::wstring(description) + L"，位置切换到：\n" + path + L"\n\n本操作只填入，不保存。")
                    .c_str(),
                L"应用交付方案", MB_OKCANCEL) != IDOK)
            return true;
        checkpointNaming();
        rememberEditedSubject();
        resetStructuredInput();
        g->structure = std::move(next);
        invalidateNaming();
        g->stemName = composedName(&path);
        g->dirtyName = g->localNamePending = true;
        rebuildPanel();
        apply(path, g->stemName, false);
        return true;
    }

    return false;
}

void showNumberMenu() {
    if (g->busySubmitting || composingNaming() || !structuredInputError().empty())
        return;
    auto menu = CreatePopupMenu();
    bool fields = !g->structure.free;
    for (auto [mode, caption] :
         {std::pair{1, L"自动版本 vNNN"}, std::pair{2, L"自动序号 NNN"}, std::pair{0, L"关闭自动编号"}})
        AppendMenuW(menu, MF_STRING | (fields ? 0 : MF_GRAYED) | (g->numberMode == mode ? MF_CHECKED : 0),
                    ID_STRUCT_NUMBER_NONE + mode, caption);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 1,
                fields ? L"按当前目录、文件名和扩展名查找已有文件"
                       : L"整名编辑保留手动名称；恢复分项填写后可编号");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 2,
                g->dialog.directory.empty() ? L"请先在原窗口选择目录，再计算可用编号"
                                            : L"编号结果以最终文件名为准；格式由原软件决定");
    if (fields && g->numberMode && !g->dialog.directory.empty())
        AppendMenuW(menu, MF_STRING | (g->busy ? MF_GRAYED : 0), ID_REFRESH_NUMBER, L"重新核验可用编号");
    int choice = experienceMenu(menu, ID_NUMBER_MENU);
    DestroyMenu(menu);
    if ((choice >= ID_STRUCT_NUMBER_NONE && choice <= ID_STRUCT_NUMBER_SEQUENCE) ||
        choice == ID_REFRESH_NUMBER)
        PostMessageW(g->panel, WM_COMMAND, choice, 0);
}
void showRuleSummaryMenu() {
    if (g->busy || g->busySubmitting || composingNaming() || !structuredInputError().empty())
        return;
    auto menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_NAMING_MENU, L"前后缀…");
    AppendMenuW(menu, MF_STRING, ID_RENAME, L"替换 / 正则…");
    int choice = experienceMenu(menu, ID_RULES_SUMMARY);
    DestroyMenu(menu);
    if (choice == ID_NAMING_MENU || choice == ID_RENAME)
        PostMessageW(g->panel, WM_COMMAND, choice, 0);
}
void showCapabilityMenu() {
    auto menu = CreatePopupMenu();
    auto info = [&](const std::wstring &text) { AppendMenuW(menu, MF_STRING | MF_GRAYED, 1, text.c_str()); };
    if (!g->attached) {
        info(L"先在软件中打开“另存为”窗口");
        info(L"未自动出现时按 Ctrl+Alt+J 尝试识别");
        info(L"也可点选文件名输入框后按 Ctrl+Alt+N 关联");
    } else {
        info(g->dialog.save ? L"文件名：可读取和回填，实际保存由原软件完成"
                            : L"当前为打开窗口：可跳转目录，保留原窗口的打开操作");
        info(g->dialog.generic && !g->dialog.genericNavigate ? L"目录：请在原窗口选择，当前窗口不支持快捷切换"
                                                             : L"目录：可从 Jump 的位置栏切换");
        if (g->dialog.save) {
            info(g->dialog.extension.empty() ? L"格式：未确认扩展名，请在原窗口核对"
                                             : L"格式：沿用原软件，最终名称包含 " + g->dialog.extension);
            info(g->dialog.directory.empty() ? L"编号：等待选择有效目录"
                                             : L"编号：按当前目录中的已有文件计算");
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDCANCEL, L"返回原文件窗口");
    }
    AppendMenuW(menu, MF_STRING, ID_DIAGNOSTICS, L"查看接入详情与诊断…");
    int choice = experienceMenu(menu, ID_CAPABILITIES);
    DestroyMenu(menu);
    if (choice == IDCANCEL || choice == ID_DIAGNOSTICS)
        PostMessageW(g->panel, WM_COMMAND, choice, 0);
}
