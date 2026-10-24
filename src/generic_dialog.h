// Included inside main.cpp: generic UIA adapter, independent of panel rendering.
// Worker-thread-only, session-scoped evidence. A breadcrumb caption is never
// converted into a filesystem path (localized names and virtual roots differ).
struct BreadcrumbEvidence {
    HWND hwnd{}; DWORD pid{}; ULONGLONG generation{};
    std::wstring signature, directory;
    ComPtr<IUIAutomationElement> container;
} breadcrumbEvidence;
std::wstring confirmedBreadcrumbDirectory(IUIAutomation *uia, const Job &job, const filename_controls::Scan &scan) {
    return scan.complete && scan.breadcrumbs.size() == 1 && breadcrumbEvidence.hwnd == job.hwnd &&
        breadcrumbEvidence.pid == job.pid && breadcrumbEvidence.generation == job.generation &&
        breadcrumbEvidence.signature == scan.breadcrumbs[0].signature &&
        filename_controls::equal(uia, breadcrumbEvidence.container.Get(), scan.breadcrumbs[0].container.Get()) ? breadcrumbEvidence.directory : L"";
}
bool inspectGeneric(IUIAutomation *uia, const Job &job, DialogData &data, ComPtr<IUIAutomationElement> &field) {
    if (!sameTarget(job)) return false;
    ComPtr<IUIAutomationElement> root;
    if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root) return false;
    auto executable = processImage(job.hwnd), cls = windowClass(job.hwnd), title = windowText(job.hwnd);
    if (executable.empty() || cls.empty() || title.empty()) return false;
    std::wstring key = job.selector;
    if (!key.empty() && !filename_controls::scope(job.controlScope, executable, cls, title)) return false;
    if (key.empty()) for (const auto &binding : job.bindings)
        if (filename_controls::scope(binding, executable, cls, title)) { key = binding.selector; break; }
    auto scan = filename_controls::scan(uia, root.Get());
    field = filename_controls::choose(scan, key);
    if (!field) return false;
    data.hwnd = job.hwnd; data.pid = job.pid; data.generic = true; data.save = true;
    data.process = std::filesystem::path(executable).filename().wstring();
    ComPtr<IUIAutomationTreeWalker> walker; uia->get_ControlViewWalker(&walker);
    if (!walker) return false;
    data.selector = filename_controls::selector(uia, walker.Get(), root.Get(), field.Get());
    data.controlScope = {executable, cls, lower(trim(title)), data.selector};
    data.rawName = automationValue(field.Get());
    // A complete filename is also an output destination; never treat its parent
    // as evidence that a browser's file list navigated.
    auto rawPath = std::filesystem::path(data.rawName);
    data.name = rawPath.filename().wstring();
    if (rawPath.is_absolute()) {
        auto attributes = GetFileAttributesW(data.rawName.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
        data.directory = rawPath.parent_path().wstring();
    }
    else if (rawPath.has_parent_path()) return false;
    if (data.directory.empty()) data.directory = filename_controls::directory(scan);
    if (data.directory.empty()) data.directory = confirmedBreadcrumbDirectory(uia, job, scan);
    data.genericSubmit = filename_controls::saveControl(scan) != nullptr;
    data.genericAddress = filename_controls::breadcrumbNavigation(scan, field.Get());
    data.genericNavigate = filename_controls::qtNavigation(scan, field.Get()) || data.genericAddress;
    if (!data.genericNavigate) {
        if (rawPath.is_absolute()) data.genericDestination = 1;
        else if (auto output = filename_controls::outputDirectory(scan);
                 output && (!data.directory.empty() || trim(automationValue(output.Get())).empty())) data.genericDestination = 2;
        data.genericNavigate = data.genericDestination != 0;
    }
    if (data.genericSubmit) data.submitSelector = scan.saves.front().selector;
    data.extension = filename_controls::extension(data.name);
    data.format = L"保留原文件扩展名";
    if (scan.complete && scan.formats.size() == 1) {
        auto format = filename_controls::value(scan.formats.front().element.Get());
        if (!format.empty()) {
            data.format = format;
            if (data.extension.empty()) data.extension = filename_controls::filterExtension(format);
        }
    }
    return !data.selector.empty() && sameTarget(job);
}
bool bindFilenameControl(IUIAutomation *uia, const Job &job, Result &result) {
    if (GetForegroundWindow() != job.hwnd || blenderBrowserWindow(job.hwnd) || houdiniBrowserWindow(job.hwnd)) {
        result.error = L"请在目标保存窗口中指定输入框；Blender 和 Houdini 使用原有接入。"; return false;
    }
    ComPtr<IUIAutomationElement> element, root;
    if (job.focusedBinding) uia->GetFocusedElement(&element);
    else uia->ElementFromPoint(job.point, &element);
    uia->ElementFromHandle(job.hwnd, &root);
    ComPtr<IUIAutomationTreeWalker> walker; uia->get_ControlViewWalker(&walker);
    if (!walker) { result.error = L"辅助功能接口不可用。"; return false; }
    for (int depth = 0; element && depth < 5 && !filename_controls::writable(element.Get()); ++depth) {
        ComPtr<IUIAutomationElement> parent; walker->GetParentElement(element.Get(), &parent); element = parent;
    }
    auto key = root && element ? filename_controls::selector(uia, walker.Get(), root.Get(), element.Get()) : L"";
    auto scan = root ? filename_controls::scan(uia, root.Get()) : filename_controls::Scan{};
    if (key.empty() || !filename_controls::choose(scan, key)) {
        result.error = L"该输入框未提供可读写且唯一的控件接口，未建立关联。"; return false;
    }
    auto inspect = job; inspect.selector = key;
    inspect.controlScope = {processImage(job.hwnd), windowClass(job.hwnd), lower(trim(windowText(job.hwnd))), key};
    ComPtr<IUIAutomationElement> field;
    if (!inspectGeneric(uia, inspect, result.data, field) || GetForegroundWindow() != job.hwnd) {
        result.error = L"保存窗口或输入框已改变，请重新指定。"; return false;
    }
    result.binding = {processImage(job.hwnd), windowClass(job.hwnd), lower(trim(windowText(job.hwnd))), key};
    return true;
}
bool setGenericValue(IUIAutomationElement *field, const std::wstring &text) {
    ComPtr<IUIAutomationValuePattern> pattern;
    field->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
    if (!pattern || !filename_controls::writable(field)) return false;
    BSTR input = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    auto hr = input ? pattern->SetValue(input) : E_OUTOFMEMORY; SysFreeString(input);
    return SUCCEEDED(hr) && automationValue(field) == text;
}
bool navigateBreadcrumb(IUIAutomation *uia, const Job &job, Result &result,
                       ComPtr<IUIAutomationElement> &field, IUIAutomationElement *root,
                       const filename_controls::Scan &initial) {
    const auto before = result.data;
    auto fail = [&](const wchar_t *message) { breadcrumbEvidence = {}; result.error = message; return false; };
    auto safe = [&]() { return sameTarget(job) && focusAllowsAction(job); };
    auto openAddress = [&](const filename_controls::Scan &scan) -> ComPtr<IUIAutomationElement> {
        if (!filename_controls::breadcrumbNavigation(scan, field.Get()) || !safe()) return {};
        RECT bounds{}; scan.breadcrumbs[0].container->get_CurrentBoundingRectangle(&bounds);
        if (!filename_controls::addressAction(scan.breadcrumbs[0].opener.Get(), true)) return {};
        auto deadline = GetTickCount64() + 1500;
        while (safe() && GetTickCount64() < deadline) {
            auto current = filename_controls::scan(uia, root);
            ComPtr<IUIAutomationElement> candidate;
            if (current.complete && current.breadcrumbs.empty()) for (const auto &item : current.fields) {
                RECT r{}; item.element->get_CurrentBoundingRectangle(&r);
                // The newly exposed editor must occupy the observed address bar,
                // not the adjacent search box or the filename editor.
                if (r.right > r.left && r.bottom > r.top && r.left >= bounds.left - 4 &&
                    r.right <= bounds.right + 4 && r.top >= bounds.top - 4 && r.bottom <= bounds.bottom + 4 &&
                    !filename_controls::equal(uia, item.element.Get(), field.Get())) {
                    if (candidate) return {};
                    candidate = item.element;
                }
            }
            if (candidate) return candidate;
            Sleep(30);
        }
        return {};
    };
    if (!safe() || automationValue(field.Get()) != before.rawName)
        return fail(L"原窗口状态已改变，未切换目录。");
    // Address submission needs keyboard input; never type into whichever field
    // happens to have focus. Activate this exact dialog and verify UIA focus.
    SetForegroundWindow(job.hwnd);
    auto activationDeadline = GetTickCount64() + 700;
    while (safe() && GetForegroundWindow() != job.hwnd && GetTickCount64() < activationDeadline) Sleep(20);
    if (GetForegroundWindow() != job.hwnd) return fail(L"无法激活原保存窗口，未切换目录。");
    auto editor = openAddress(initial);
    if (!editor) return fail(L"未找到地址栏输入框，未切换目录。");
    auto previousAddress = automationValue(editor.Get());
    auto restore = [&]() {
        if (safe() && automationValue(editor.Get()) == job.directory) setGenericValue(editor.Get(), previousAddress);
        if (safe()) field->SetFocus();
    };
    if (!before.directory.empty() && normalize(previousAddress) != normalize(before.directory)) {
        field->SetFocus(); return fail(L"原窗口目录已改变，请重新选择目标目录。");
    }
    if (!safe() || !setGenericValue(editor.Get(), job.directory) || FAILED(editor->SetFocus())) {
        restore(); return fail(L"地址栏填入失败，未执行跳转。");
    }
    ComPtr<IUIAutomationElement> focused; uia->GetFocusedElement(&focused);
    auto attributes = GetFileAttributesW(job.directory.c_str());
    bool modifiers = false;
    for (int key : {VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN}) modifiers = modifiers || (GetAsyncKeyState(key) & 0x8000);
    if (!safe() || GetForegroundWindow() != job.hwnd || modifiers ||
        !filename_controls::equal(uia, focused.Get(), editor.Get()) || automationValue(editor.Get()) != job.directory ||
        automationValue(field.Get()) != before.rawName || attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        restore(); return fail(L"地址栏焦点或目录已改变，未执行跳转；请松开快捷键后重试。");
    }
    INPUT keys[2]{}; keys[0].type = keys[1].type = INPUT_KEYBOARD;
    keys[0].ki.wVk = keys[1].ki.wVk = VK_RETURN; keys[1].ki.dwFlags = KEYEVENTF_KEYUP;
    if (SendInput(2, keys, sizeof(INPUT)) != 2) { restore(); return fail(L"目录跳转未确认，请检查原窗口。"); }
    filename_controls::Scan arrived;
    auto deadline = GetTickCount64() + 3000;
    while (safe() && GetTickCount64() < deadline) {
        arrived = filename_controls::scan(uia, root);
        if (filename_controls::breadcrumbNavigation(arrived, field.Get())) break;
        Sleep(40);
    }
    // Reopen the rebuilt breadcrumb. Reading back the text we just entered would
    // only confirm editing, not actual navigation.
    auto verified = openAddress(arrived);
    if (!verified) return fail(L"未确认目录跳转，请检查原窗口；未提交保存。");
    auto actual = automationValue(verified.Get());
    if (safe()) field->SetFocus();
    deadline = GetTickCount64() + 1000;
    while (safe() && GetTickCount64() < deadline) {
        arrived = filename_controls::scan(uia, root);
        if (filename_controls::breadcrumbNavigation(arrived, field.Get())) break;
        Sleep(30);
    }
    if (!safe() || normalize(actual) != normalize(job.directory) ||
        !filename_controls::breadcrumbNavigation(arrived, field.Get()) || automationValue(field.Get()) != before.rawName)
        return fail(L"跳转后的目录或名称未通过核验；未提交保存，请检查原窗口。");
    breadcrumbEvidence = {job.hwnd, job.pid, job.generation, arrived.breadcrumbs[0].signature, actual, arrived.breadcrumbs[0].container};
    DialogData after; ComPtr<IUIAutomationElement> updated;
    if (!inspectGeneric(uia, job, after, updated) || normalize(after.directory) != normalize(job.directory) ||
        after.rawName != before.rawName || after.format != before.format)
        return fail(L"跳转后窗口状态已改变；未提交保存。");
    result.data = std::move(after); field = updated; result.navigated = true;
    return true;
}
bool navigateGeneric(IUIAutomation *uia, const Job &job, Result &result, ComPtr<IUIAutomationElement> &field) {
    auto before = result.data;
    auto fail = [&](const wchar_t *message) { result.error = message; return false; };
    if (!before.genericNavigate) return fail(L"当前窗口未提供可核验的目录导航，请在原软件中切换目录。");
    std::filesystem::path destination(job.directory);
    if (!destination.is_absolute() || job.directory.find_first_of(L"\r\n\t\"<>|*?") != job.directory.npos)
        return fail(L"请输入完整且有效的文件夹路径。");
    // Qt QFileDialog treats an existing directory + trailing separator as navigation.
    // Never use this operation for unknown dialogs or a nonexistent path (which
    // could otherwise be interpreted as a filename). The path bar is independent
    // evidence of navigation; echoing the filename edit is not sufficient.
    auto attributes = GetFileAttributesW(job.directory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return fail(L"目标目录不存在或无法访问，未切换目录。");
    auto path = destination.lexically_normal().wstring();
    if (!path.ends_with(L"\\")) path += L"\\";
    ComPtr<IUIAutomationElement> root;
    if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root) return fail(L"原窗口已失效，未切换目录。");
    auto scan = filename_controls::scan(uia, root.Get());
    if (filename_controls::breadcrumbNavigation(scan, field.Get()))
        return navigateBreadcrumb(uia, job, result, field, root.Get(), scan);
    if (!filename_controls::qtNavigation(scan, field.Get()) || !sameTarget(job) || !focusAllowsAction(job) ||
        automationValue(field.Get()) != before.rawName || normalize(filename_controls::directory(scan)) != normalize(before.directory))
        return fail(L"原窗口状态已改变，未切换目录。");
    const auto originalButton = scan.saves.front().element;
    if (!setGenericValue(field.Get(), path)) return fail(L"目录指令填入失败，未切换目录。");
    auto restore = [&]() {
        if (!sameTarget(job) || !focusAllowsAction(job)) return;
        auto text = automationValue(field.Get());
        if (text == path || text.empty()) setGenericValue(field.Get(), before.rawName);
    };
    scan = filename_controls::scan(uia, root.Get());
    // Qt relabels the same Save button to Open while a directory is entered.
    // Keep its element identity; never choose a different Open button by caption.
    auto button = scan.complete && scan.navigationButtons.size() == 1 ?
        scan.navigationButtons.front().element : ComPtr<IUIAutomationElement>{};
    attributes = GetFileAttributesW(job.directory.c_str());
    if (!button || !filename_controls::equal(uia, button.Get(), originalButton.Get()) ||
        !filename_controls::invokable(button.Get()) || !sameTarget(job) || !focusAllowsAction(job) ||
        automationValue(field.Get()) != path || attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        restore(); return fail(L"目录或导航控件已改变，未执行跳转。");
    }
    ComPtr<IUIAutomationInvokePattern> invoke;
    button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    if (!invoke || FAILED(invoke->Invoke())) { restore(); return fail(L"原窗口未接受目录导航，请检查原窗口。"); }
    bool arrived = false;
    auto deadline = GetTickCount64() + 3000;
    while (sameTarget(job) && GetTickCount64() < deadline) {
        scan = filename_controls::scan(uia, root.Get());
        if (normalize(filename_controls::directory(scan)) == normalize(job.directory)) { arrived = true; break; }
        if (!focusAllowsAction(job)) break;
        Sleep(40);
    }
    restore();
    if (!arrived) return fail(L"未确认进入目标目录，请检查原窗口；未提交保存。");
    DialogData after; ComPtr<IUIAutomationElement> updated;
    if (!inspectGeneric(uia, job, after, updated) || normalize(after.directory) != normalize(job.directory) ||
        after.rawName != before.rawName || after.format != before.format || !sameTarget(job) || !focusAllowsAction(job))
        return fail(L"目录已响应，但名称或格式未通过核验；未提交保存，请检查原窗口。");
    result.data = std::move(after); field = updated; result.navigated = true;
    return true;
}
void applyGeneric(IUIAutomation *uia, const Job &job, Result &result, IUIAutomationElement *initialField) {
    ComPtr<IUIAutomationElement> field = initialField;
    auto &data = result.data;
    if (!sameTarget(job) || !focusAllowsAction(job)) { result.error = L"前台或保存窗口已改变，未填入。"; return; }
    if (data.rawName != job.expectedRawName || normalize(data.directory) != normalize(job.expectedDirectory) ||
        data.extension != job.expectedExtension) {
        result.error = L"原窗口的文件名、目录或格式已改变，请检查更新后的预览再操作。"; return;
    }
    if (!validName(job.name) && !(job.name.empty() && !job.submit)) { result.error = L"请输入有效的文件名。"; return; }
    const bool destinationChanged = !job.directory.empty() && normalize(job.directory) != normalize(data.directory);
    if (data.genericDestination == 1 && job.name.empty()) {
        result.error = L"完整输出路径需要文件名；请先填写名称，目录尚未改变。"; return;
    }
    std::wstring outputDirectory = data.directory;
    if (destinationChanged && data.genericDestination) {
        std::filesystem::path destination(job.directory);
        auto attrs = GetFileAttributesW(job.directory.c_str());
        if (!destination.is_absolute() || job.directory.find_first_of(L"\r\n\t\"<>|*?") != job.directory.npos ||
            attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            result.error = L"目标目录不存在或无法访问，未更新输出位置。"; return;
        }
        outputDirectory = job.directory;
        if (data.genericDestination == 2) {
            ComPtr<IUIAutomationElement> root;
            if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root) { result.error = L"原窗口已失效。"; return; }
            auto scan = filename_controls::scan(uia, root.Get());
            auto directory = filename_controls::outputDirectory(scan);
            if (!directory || normalize(filename_controls::directory(scan)) != normalize(data.directory) ||
                !sameTarget(job) || !focusAllowsAction(job) || automationValue(field.Get()) != data.rawName ||
                !setGenericValue(directory.Get(), outputDirectory)) {
                result.error = L"输出目录回填未通过核验；未提交保存，请检查原窗口。"; return;
            }
            DialogData live; ComPtr<IUIAutomationElement> updated;
            if (!inspectGeneric(uia, job, live, updated) || live.rawName != data.rawName ||
                normalize(live.directory) != normalize(outputDirectory) || live.format != data.format || live.genericDestination != 2) {
                result.error = L"输出位置改变后名称或格式发生变化；未提交保存，请检查原窗口。"; return;
            }
            data = live; field = updated;
        }
    } else if (destinationChanged) {
        if (!navigateGeneric(uia, job, result, field)) return;
    }
    if (job.submit && (!data.genericSubmit || data.submitSelector != job.submitSelector)) {
        result.error = L"原保存按钮不可用或已改变；请在原软件中确认保存。"; return;
    }
    auto text = job.name.empty() ? L"" : job.name + data.extension;
    if (!text.empty() && std::filesystem::path(data.rawName).is_absolute())
        text = (std::filesystem::path(outputDirectory) / text).wstring();
    ComPtr<IUIAutomationValuePattern> pattern;
    field->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
    if (!pattern || !filename_controls::writable(field.Get()) || !sameTarget(job) ||
        !focusAllowsAction(job) || automationValue(field.Get()) != data.rawName) {
        result.error = L"文件名输入框已改变，未填入。"; return;
    }
    BSTR input = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    HRESULT hr = input ? pattern->SetValue(input) : E_OUTOFMEMORY;
    SysFreeString(input);
    if (FAILED(hr) || automationValue(field.Get()) != text) {
        result.error = L"文件名回读不一致，请检查原窗口。"; return;
    }
    if (data.genericDestination) {
        DialogData live; ComPtr<IUIAutomationElement> updated;
        if (!inspectGeneric(uia, job, live, updated) || !sameTarget(job) || !focusAllowsAction(job) ||
            live.rawName != text || normalize(live.directory) != normalize(outputDirectory) ||
            live.extension != data.extension || live.format != data.format) {
            result.error = L"填入后窗口状态发生变化，未提交保存；请检查原窗口。"; return;
        }
        data = live; field = updated; result.navigated = destinationChanged;
    }
    data.rawName = text; data.name = job.name.empty() ? L"" : job.name + data.extension;
    if (!job.submit) return;

    // SetValue may trigger validation, change the directory or rebuild controls.
    // Rescan and compare the complete destination before invoking the original action.
    DialogData live; ComPtr<IUIAutomationElement> liveField, root;
    if (!inspectGeneric(uia, job, live, liveField) || live.rawName != text ||
        normalize(live.directory) != normalize(data.directory) || live.submitSelector != job.submitSelector ||
        FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root) {
        result.error = L"填入后窗口状态发生变化，请在原软件中检查并保存。"; return;
    }
    auto scan = filename_controls::scan(uia, root.Get());
    auto button = filename_controls::saveControl(scan);
    if (!button || scan.saves.front().selector != job.submitSelector) {
        result.error = L"未找到唯一且可调用的原保存按钮，请在原软件中保存。"; return;
    }
    result.probe = saveProbeFor(data); // Baseline must precede Invoke, which may write immediately.
    ComPtr<IUIAutomationInvokePattern> invoke;
    button->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    if (!invoke || !sameTarget(job) || !focusAllowsAction(job) ||
        automationValue(liveField.Get()) != text || !filename_controls::invokable(button.Get()) ||
        (!std::filesystem::path(text).is_absolute() && filename_controls::directory(scan) != live.directory)) {
        result.error = L"保存前控件状态已改变，未提交。"; return;
    }
    if (FAILED(invoke->Invoke())) {
        result.error = L"原保存按钮未确认接受操作，请检查原窗口；Jump 不会自动重试。"; return;
    }
    result.submitted = true;
}
