// Simulated values have their own identity map. A simulation is always derived
// from the current definition and evaluated by the production naming engine.
inline void designerLoadSimulation(HWND window, NamingDesigner &data) {
    bool loading = data.loading;
    data.loading = true;
    for (size_t i = 0; i < data.draft.fields.size(); ++i) {
        const auto &f = data.draft.fields[i];
        int index = static_cast<int>(i);
        designerSet(window, ND_SIM_LABEL + index, f.label);
        auto combo = GetDlgItem(window, ND_SIM_VALUE + index);
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const auto &p : f.candidates) {
            auto name = p.name + (p.name == p.value ? L"" : L" → " + p.value);
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
        auto value = data.simulated.find(f.id);
        designerSet(window, ND_SIM_VALUE + index,
                    value == data.simulated.end() ? f.defaultValue : value->second);
        auto source = namingFieldDefaultSource(f);
        designerSet(window, ND_SIM_AUTO + index,
                    f.id == data.draft.primaryField ? L"使用上方的模拟原文件名"
                    : source == 1                   ? L"自动取当前项目（不改方案）"
                    : source == 2                   ? L"自动取当天日期"
                                                    : L"固定文字：" + f.defaultValue);
    }
    data.loading = loading;
}
inline void designerUsageSummary(HWND window, NamingDesigner &data) {
    std::wstring apps;
    for (const auto &app : data.applications) {
        if (!apps.empty())
            apps += L"、";
        apps += app;
    }
    auto summary = std::wstring(L"保存后的使用方式：\r\n") +
                   (data.globalDefault ? L"全局默认：没有专用方案的软件会自动使用此方案。"
                                       : L"全局默认：此方案未设为默认；其他软件沿用原来的默认方案。") +
                   L"\r\n软件专用：" +
                   (apps.empty() ? L"尚未关联软件。" : apps + L" 的新保存窗口优先使用此方案。") +
                   L"\r\n手动使用：在保存窗口旁的 Jump 面板 → 命名设置 → 命名方案，选择此方案。" +
                   (data.applyOnSave ? L"\r\n本次点击“保存并使用”也会应用到当前保存窗口。"
                                     : L"\r\n这里保存规则；在原软件打开保存窗口后即可选择使用。");
    designerSet(window, ND_USAGE_SUMMARY, summary);
    std::wstring state = data.globalDefault ? L"保存后自动应用：全局默认" : L"保存后使用：在主面板手动选择";
    if (!data.applications.empty())
        state = data.globalDefault
                    ? L"保存后自动应用：全局默认及 " + std::to_wstring(data.applications.size()) + L" 个软件"
                    : L"保存后自动应用：" + (data.applications.size() == 1
                                                 ? data.applications.front()
                                                 : std::to_wstring(data.applications.size()) + L" 个软件");
    designerSet(window, ND_USAGE_STATUS, state);
    designerTip(window, data, ND_USAGE_STATUS, summary);
    designerSet(window, ND_SECTION_APPS, L"更改范围…");
}
inline std::wstring designerStructureText(const StructuredNaming &next) {
    std::wstring text;
    for (size_t i = 0; i < next.fields.size(); ++i) {
        if (i)
            text += next.fields[i].ownJoiner ? next.fields[i].joiner : next.separator;
        text += L"[" + next.fields[i].label + L"]";
    }
    if (next.counter)
        text += next.counter == 1 ? L"_[自动版本]" : L"_[自动序号]";
    return text;
}
inline void designerLocateIssue(NamingDesigner &data, const StructuredNaming &next) {
    data.errorField = -1;
    data.errorPage = 0;
    data.errorControl = ND_NAME;
    if (next.name.empty() || next.name.size() > 80)
        return;
    if (next.separator.size() > 3 ||
        next.separator.find_first_of(L"\\/:*?\"<>|\r\n\t") != next.separator.npos) {
        data.errorPage = -1;
        data.errorControl = ND_QUICK_SEPARATOR;
        return;
    }
    for (size_t i = 0; i < next.fields.size(); ++i) {
        const auto &f = next.fields[i];
        int page = 0, control = ND_LABEL;
        bool bad = f.label.empty() || f.label.size() > 40;
        auto checked = f;
        checked.required = false;
        if (!f.enabled) {
            checked.validation = 0;
            checked.width = 0;
        }
        if (!bad && !namingFieldError(checked, f.defaultValue).empty()) {
            bad = true;
            control = ND_DEFAULT;
        }
        auto literal = [](const std::wstring &v, size_t max) {
            return v.size() <= max && v.find_first_of(L"\\/:*?\"<>|\r\n\t") == v.npos;
        };
        if (!bad && (!literal(f.joiner, 3) || !literal(f.prefix, 40) || !literal(f.suffix, 40))) {
            bad = true;
            page = 1;
            control = ND_JOINER;
        }
        std::vector<NamingChoiceRow> rows;
        for (const auto &p : f.candidates)
            rows.push_back({p, p.value});
        if (!bad && !validateNamingChoices(f, rows).empty()) {
            bad = true;
            page = 2;
            control = ND_CHOICE_NAME;
        }
        std::set<std::wstring> seen;
        auto at = &f;
        while (!bad && !at->conditionField.empty()) {
            auto found = std::find_if(next.fields.begin(), next.fields.end(),
                                      [&](const auto &other) { return other.id == at->conditionField; });
            if (!seen.insert(at->id).second || found == next.fields.end() || at->conditionValue.empty()) {
                bad = true;
                page = 7;
                control = ND_CONDITION_FIELD;
                break;
            }
            at = &*found;
        }
        if (bad) {
            data.errorField = static_cast<int>(i);
            data.errorPage = page;
            data.errorControl = control;
            return;
        }
    }
    if (data.error.find(L"目录") != data.error.npos || data.error.find(L"文件夹") != data.error.npos) {
        data.errorPage = 4;
        data.errorControl = 4703;
        return;
    }
    if (data.advanced && data.page < 8) {
        data.errorField = data.selected;
        data.errorPage = data.page;
        data.errorControl = data.page == 1   ? ND_WIDTH
                            : data.page == 2 ? ND_CHOICE_NAME
                            : data.page == 7 ? ND_CONDITION_FIELD
                                             : ND_DEFAULT;
    }
}
inline void designerPreview(HWND window, NamingDesigner &data) {
    if (data.loading)
        return;
    data.previewMatches.clear();
    data.previewFields.clear();
    StructuredNaming next;
    bool valid = readDesignerField(window, data, next);
    auto stamp = packNaming({serializeStructure(namingDefinition(next)), packNaming(data.applications),
                             data.globalDefault ? L"1" : L"0"});
    if (!data.baselineReady) {
        data.baseline = stamp;
        data.baselineReady = true;
    }
    bool changed = !valid || stamp != data.baseline;
    if (auto host = settingsDialog(window))
        host->unsavedChanges = changed || !data.initialSaved;
    designerSet(window, ND_DIRTY,
                changed             ? L"有未保存的修改"
                : data.initialSaved ? L"尚无修改"
                                    : L"新方案尚未保存");
    data.errorField = -1;
    data.errorControl = 0;
    auto structure = designerStructureText(next);
    designerSet(window, ND_STRUCTURE, structure);
    designerTip(window, data, ND_STRUCTURE, structure);
    if (valid) {
        data.previewSchema = next;
        auto original = trim(designerText(window, ND_EXAMPLE));
        std::filesystem::path originalPath(original);
        auto extension = originalPath.extension().wstring();
        auto session = namingSession(next, extension.empty() ? original : originalPath.stem().wstring());
        for (auto &f : session.fields)
            if (f.source == 0 && f.id != session.primaryField) {
                auto value = data.simulated.find(f.id);
                if (value != data.simulated.end())
                    f.value = value->second;
            }
        auto preview = composeStructure(session, *data.config);
        data.previewMatches = preview.matches;
        std::wstring explanation;
        for (size_t i = 0; i < session.fields.size(); ++i) {
            const auto &f = session.fields[i];
            auto value = fieldValue(f, *data.config, session.dateFormat);
            auto reason = f.label + L"：";
            if (!f.enabled)
                reason += L"已停用，不加入文件名";
            else if (!namingFieldVisible(session, i, *data.config)) {
                auto from = std::find_if(session.fields.begin(), session.fields.end(),
                                         [&](const auto &other) { return other.id == f.conditionField; });
                reason += L"不符合添加条件（需要 " +
                          (from == session.fields.end() ? f.conditionField : from->label) + L" 写入 " +
                          f.conditionValue + L"）";
            } else if (value.empty())
                reason += f.source == 1 ? L"自动来源不可用：尚未选择项目" : L"未填写内容";
            else {
                reason += namingFieldOutput(f, value);
                reason += f.id == session.primaryField    ? L" ← 模拟原文件名"
                          : f.source == 1                 ? L" ← 当前项目"
                          : f.source == 2                 ? L" ← 当天日期"
                          : f.source == 3                 ? L" ← 方案固定文字"
                          : data.simulated.contains(f.id) ? L" ← 本次模拟输入"
                                                          : L" ← 方案默认内容";
            }
            if (f.enabled && namingFieldVisible(session, i, *data.config)) {
                auto error = namingFieldError(f, value);
                if (!error.empty()) {
                    reason += L"；" + error;
                    if (data.errorControl == 0) {
                        data.errorField = static_cast<int>(i);
                        data.errorPage = f.source == 0 ? 8 : 0;
                        data.errorControl = f.source != 0 ? ND_SOURCE
                                            : f.id == session.primaryField
                                                ? ND_EXAMPLE
                                                : ND_SIM_VALUE + static_cast<int>(i);
                    }
                }
                if (!value.empty() && data.previewFields.size() < preview.matches.size())
                    data.previewFields.push_back(i);
            }
            explanation += reason + L"\r\n";
        }
        if (preview.error.empty() && next.counter) {
            SmartNaming options{next.counter};
            options.digits = next.digits;
            options.upperVersion = next.upper;
            preview.name = numberingExample(preview.name, options);
            if (preview.name.empty())
                preview.error = L"名字太长或示例编号已用完";
        }
        if (preview.error.empty() && !extension.empty()) {
            preview.name += extension;
            if (!validName(preview.name))
                preview.error = L"包含扩展名后过长或含有无效字符";
            explanation += L"扩展名：" + extension + L"（来自模拟原文件名；真实保存格式由原软件决定）\r\n";
        }
        if (next.counter)
            explanation += L"自动编号：此处仅为示例；真实保存按目标文件夹重新计算。\r\n";
        auto locations = namingRecommendedLocations(session, *data.config);
        for (const auto &location : locations)
            explanation += L"推荐目录：" + location.path + L"（点击推荐才采用）\r\n";
        designerSet(window, ND_SIM_EXPLANATION, explanation);
        designerSet(window, ND_PREVIEW,
                    preview.error.empty() ? preview.name : L"模拟未完成：" + preview.error);
        designerSet(window, ND_ERROR,
                    preview.error.empty()
                        ? L""
                        : L"模拟输入：" + preview.error + L"。规则可保存，实际使用时需填写有效内容。");
        designerSet(window, ND_PREVIEW_NOTE,
                    next.counter ? L"仅模拟，不改方案默认或当前保存输入；编号为示例，保存时按目录续号。"
                                 : L"仅模拟，不改方案默认值或当前真实保存输入。");
    } else {
        designerLocateIssue(data, next);
        auto issue = data.error;
        if (data.errorField >= 0)
            issue = L"“" + next.fields[data.errorField].label + L"” · " + issue;
        designerSet(window, ND_PREVIEW, L"规则需要修正");
        designerSet(window, ND_ERROR, issue);
        designerSet(window, ND_PREVIEW_NOTE, L"当前规则尚未保存；可点击“定位并修正”返回对应部分。");
        designerSet(window, ND_SIM_EXPLANATION, L"先修正规则，再模拟保存。\r\n" + issue);
    }
    if (data.advanced && data.page == 9) {
        auto description = structure + L"\r\n预览：" + designerText(window, ND_PREVIEW) + L"\r\n";
        description += data.example == data.adoptedExample
                           ? L"这是当前采用的草稿。选择其他示例不会改写配置。"
                           : L"正在试用，尚未采用。采用后建立新方案草稿，原方案保留；最后保存才生效。返回原方"
                             L"案可恢复之前的修改。";
        designerSet(window, ND_TEMPLATE_EFFECT, description);
        designerSet(window, ND_ADOPT, data.example ? L"采用这个示例，继续编辑" : L"使用原方案草稿，继续编辑");
    }
    ShowWindow(GetDlgItem(window, ND_FIX), data.errorControl ? SW_SHOW : SW_HIDE);
    auto issue = designerText(window, ND_ERROR);
    ShowWindow(GetDlgItem(window, ND_ERROR), issue.empty() ? SW_HIDE : SW_SHOW);
    designerTip(window, data, ND_ERROR, issue);
    designerTip(window, data, ND_FIX, issue);
    designerSet(window, ND_FIX, valid && data.errorPage == 8 ? L"修改模拟输入…" : L"定位并修正…");
    designerTip(window, data, ND_PREVIEW, designerText(window, ND_PREVIEW));
    InvalidateRect(GetDlgItem(window, ND_PREVIEW), nullptr, FALSE);
    InvalidateRect(GetDlgItem(window, ND_FIELDS), nullptr, FALSE);
}
