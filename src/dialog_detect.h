// Save-dialog inspection, UIA/Shell adapters, and background worker loop.
std::wstring windowClass(HWND h) {
    wchar_t b[256]{};
    GetClassNameW(h, b, 256);
    return b;
}
std::wstring windowText(HWND h) {
    wchar_t b[8192]{};
    DWORD own = 0;
    GetWindowThreadProcessId(h, &own);
    if (own == GetCurrentProcessId()) {
        GetWindowTextW(h, b, 8192);
        return b;
    }
    DWORD_PTR result = 0;
    if (!h || !SendMessageTimeoutW(h, WM_GETTEXT, 8192, reinterpret_cast<LPARAM>(b),
                                   SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &result))
        return {};
    return b;
}
bool setText(HWND h, const std::wstring &v) {
    DWORD_PTR result = 0;
    return SendMessageTimeoutW(h, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(v.c_str()),
                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &result) != 0;
}
std::wstring processImage(HWND h) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p)
        return {};
    wchar_t b[32768];
    DWORD n = 32768;
    std::wstring result;
    if (QueryFullProcessImageNameW(p, 0, b, &n))
        result = std::wstring(b, n);
    CloseHandle(p);
    return lower(result);
}
std::wstring processName(HWND h) {
    return std::filesystem::path(processImage(h)).filename().wstring();
}
bool sameTarget(const Job &job) {
    DWORD pid = 0;
    if (!IsWindow(job.hwnd) || !IsWindowVisible(job.hwnd) || !IsWindowEnabled(job.hwnd))
        return false;
    GetWindowThreadProcessId(job.hwnd, &pid);
    return pid == job.pid && (!g->testTargetPid || pid == g->testTargetPid) &&
           (g->testTargetImage.empty() || processImage(job.hwnd) == g->testTargetImage) &&
           g->generation.load() == job.generation;
}
bool focusAllowsAction(const Job &job) {
    HWND foreground = GetForegroundWindow();
    return foreground == job.hwnd || foreground == g->panel;
}
std::wstring extractPath(const std::wstring &text) {
    size_t pos = std::wstring::npos;
    for (size_t i = 0; i + 2 < text.size(); ++i)
        if (iswalpha(text[i]) && text[i + 1] == L':' && (text[i + 2] == L'\\' || text[i + 2] == L'/')) {
            pos = i;
            break;
        }
    if (pos == text.npos)
        pos = text.find(L"\\\\");
    if (pos == text.npos)
        return {};
    auto path = trim(text.substr(pos));
    auto quote = path.find(L'"');
    if (quote != path.npos)
        path.resize(quote);
    return path;
}
std::wstring directoryFromShellAddress(const std::wstring &text) {
    auto path = extractPath(text);
    if (!path.empty())
        return path;
    auto address = trim(text);
    auto colon = address.find(L':');
    if (colon != address.npos)
        address = trim(address.substr(colon + 1));
    if (address.empty())
        return {};

    // Breadcrumbs use localized shell names (e.g. Downloads) instead of drive paths.
    // Resolve the actual known folder, including OneDrive/redirection, without editing the address bar.
    struct Folder {
        const KNOWNFOLDERID *id;
        const wchar_t *canonical;
    };
    const Folder folders[] = {
        {&FOLDERID_Downloads, L"Downloads"}, {&FOLDERID_Desktop, L"Desktop"},
        {&FOLDERID_Documents, L"Documents"}, {&FOLDERID_Pictures, L"Pictures"},
        {&FOLDERID_Music, L"Music"},         {&FOLDERID_Videos, L"Videos"},
        {&FOLDERID_Profile, L"User"},        {&FOLDERID_SkyDrive, L"OneDrive"},
        {&FOLDERID_Public, L"Public"},       {&FOLDERID_PublicDocuments, L"Public Documents"}};
    for (const auto &folder : folders) {
        PWSTR value = nullptr;
        if (FAILED(SHGetKnownFolderPath(*folder.id, KF_FLAG_DONT_VERIFY, nullptr, &value)))
            continue;
        std::filesystem::path physical(value);
        CoTaskMemFree(value);
        std::vector<std::wstring> names{folder.canonical, physical.filename().wstring()};
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (SUCCEEDED(SHGetKnownFolderIDList(*folder.id, KF_FLAG_DONT_VERIFY, nullptr, &pidl))) {
            ComPtr<IShellItem> item;
            if (SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item))) &&
                SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &value))) {
                names.emplace_back(value);
                CoTaskMemFree(value);
            }
            CoTaskMemFree(pidl);
        }
        for (const auto &name : names) {
            if (name.empty())
                continue;
            auto prefix = lower(name), current = lower(address);
            if (current != prefix &&
                !(current.starts_with(prefix + L"\\") || current.starts_with(prefix + L"/")))
                continue;
            auto resolved = physical;
            if (address.size() > name.size())
                resolved /= address.substr(name.size() + 1);
            auto candidate = resolved.lexically_normal().wstring();
            DWORD attrs = GetFileAttributesW(candidate.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
                return candidate;
        }
    }
    return {};
}
std::wstring directoryFromToolbar(HWND hwnd) {
    struct Search {
        std::wstring path;
    } s;
    EnumChildWindows(
        hwnd,
        [](HWND child, LPARAM data) -> BOOL {
            auto &s = *reinterpret_cast<Search *>(data);
            if (windowClass(child) == L"ToolbarWindow32" &&
                windowClass(GetParent(child)) == L"Breadcrumb Parent") {
                s.path = directoryFromShellAddress(windowText(child));
                if (!s.path.empty())
                    return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&s));
    return s.path;
}
bool selfTestShellAddresses() {
    for (auto id : {&FOLDERID_Downloads, &FOLDERID_Desktop, &FOLDERID_Documents}) {
        PWSTR path = nullptr, display = nullptr;
        PIDLIST_ABSOLUTE pidl = nullptr;
        if (FAILED(SHGetKnownFolderPath(*id, 0, nullptr, &path)))
            return false;
        std::wstring expected(path);
        CoTaskMemFree(path);
        if (FAILED(SHGetKnownFolderIDList(*id, 0, nullptr, &pidl)))
            return false;
        ComPtr<IShellItem> item;
        auto hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item));
        CoTaskMemFree(pidl);
        if (FAILED(hr) || FAILED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &display)))
            return false;
        std::wstring address(display);
        CoTaskMemFree(display);
        if (normalize(directoryFromShellAddress(L"地址: " + address)) != normalize(expected) ||
            normalize(directoryFromShellAddress(L"Address: " + address)) != normalize(expected) ||
            normalize(directoryFromShellAddress(L"地址: " + expected)) != normalize(expected))
            return false;
    }
    return directoryFromShellAddress(L"地址: Jump不存在的系统位置_987654321").empty();
}
bool blenderBrowserWindow(HWND hwnd) {
    auto cls = windowClass(hwnd);
    if (!cls.starts_with(L"GHOST_WindowClass"))
        return false;
    auto title = windowText(hwnd);
    return (title == L"Blender File View" || title == L"Blender文件视图" || title == L"Blender 文件视图") &&
           processName(hwnd) == L"blender.exe";
}
bool houdiniBrowserWindow(HWND hwnd) {
    if (!windowClass(hwnd).starts_with(L"Qt"))
        return false;
    auto title = windowText(hwnd);
    if (title != L"Save As" && title != L"另存为")
        return false;
    auto process = processName(hwnd);
    return process == L"houdinifx.exe" || process == L"houdini.exe" || process == L"houdinicore.exe";
}
bool classicFileDialog(HWND hwnd) {
    if (hwnd && IsWindowVisible(hwnd) && IsWindowEnabled(hwnd) &&
        (blenderBrowserWindow(hwnd) || houdiniBrowserWindow(hwnd)))
        return true;
    if (!hwnd || windowClass(hwnd) != L"#32770" || !IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd))
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == g->pid || IsHungAppWindow(hwnd))
        return false;
    struct Scan {
        bool shell = false, ok = false;
    } s;
    EnumChildWindows(
        hwnd,
        [](HWND child, LPARAM p) -> BOOL {
            auto &s = *reinterpret_cast<Scan *>(p);
            auto cls = windowClass(child);
            if (cls == L"SHELLDLL_DefView" || cls == L"SysListView32")
                s.shell = true;
            if (GetDlgCtrlID(child) == IDOK && cls == L"Button")
                s.ok = true;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&s));
    return s.shell && s.ok;
}
bool genericCandidate(HWND hwnd) {
    if (!hwnd || !IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd) || IsHungAppWindow(hwnd))
        return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == g->pid || !pid)
        return false;
    if (reinterpret_cast<ULONG_PTR>(GetPropW(hwnd, L"Jump.SaveBridge.v1")) == pid)
        return true;
    auto title = windowText(hwnd);
    if (title.empty())
        return false;
    if (filename_controls::saveTitle(title))
        return true;
    // Owned modal dialogs can use document-specific or localized titles. Inspect
    // their controls before deciding; a title is a discovery hint, not a contract.
    auto owner = GetWindow(hwnd, GW_OWNER);
    DWORD ownerPid = 0;
    if (owner && GetWindowThreadProcessId(owner, &ownerPid) && ownerPid == pid && !IsWindowEnabled(owner))
        return true;
    auto image = processImage(hwnd), cls = windowClass(hwnd);
    return std::any_of(
        g->cfg.filenameBindings.begin(), g->cfg.filenameBindings.end(),
        [&](const auto &binding) { return filename_controls::scope(binding, image, cls, title); });
}
bool looksLikeFileDialog(HWND hwnd) {
    return classicFileDialog(hwnd) || genericCandidate(hwnd);
}
std::wstring sourceDirectory(HWND dialog, const std::wstring &filename) {
    std::vector<std::wstring> candidates = {filename};
    for (HWND owner = GetWindow(dialog, GW_OWNER); owner; owner = GetWindow(owner, GW_OWNER))
        candidates.push_back(windowText(owner));
    for (auto value : candidates) {
        auto p = extractPath(value);
        if (p.empty() || p.starts_with(L"\\\\"))
            continue;
        for (int attempt = 0; attempt < 2; ++attempt) {
            DWORD attrs = GetFileAttributesW(p.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
                return std::filesystem::path(p).parent_path().wstring();
            auto suffix = p.rfind(L" - ");
            if (suffix == p.npos)
                break;
            p.resize(suffix);
        }
    }
    return {};
}
std::wstring bstrText(BSTR b) {
    return b ? std::wstring(b, SysStringLen(b)) : L"";
}
std::wstring automationValue(IUIAutomationElement *el) {
    if (!el)
        return {};
    ComPtr<IUIAutomationValuePattern> pattern;
    el->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
    if (!pattern)
        return {};
    BSTR value = nullptr;
    pattern->get_CurrentValue(&value);
    auto out = bstrText(value);
    SysFreeString(value);
    return out;
}
ComPtr<IUIAutomationElement> findId(IUIAutomation *uia, IUIAutomationElement *root, const wchar_t *id) {
    VARIANT v{};
    v.vt = VT_BSTR;
    v.bstrVal = SysAllocString(id);
    ComPtr<IUIAutomationCondition> condition;
    uia->CreatePropertyCondition(UIA_AutomationIdPropertyId, v, &condition);
    VariantClear(&v);
    ComPtr<IUIAutomationElement> el;
    if (condition)
        root->FindFirst(TreeScope_Descendants, condition.Get(), &el);
    return el;
}
ComPtr<IUIAutomationElement> filenameElement(IUIAutomation *uia, IUIAutomationElement *root, HWND hwnd) {
    auto el = findId(uia, root, L"FileNameControlHost");
    if (el)
        return el;
    HWND classic = GetDlgItem(hwnd, 0x480);
    if (classic)
        uia->ElementFromHandle(classic, &el);
    if (!el)
        el = findId(uia, root, L"1148");
    return el;
}
std::wstring extensionFromFormat(const std::wstring &text) {
    auto start = text.find(L"*.");
    if (start == text.npos)
        return {};
    start++;
    auto end = text.find_first_of(L";), \t", start);
    auto ext = text.substr(start, end == text.npos ? text.size() - start : end - start);
    if (ext == L".*" || ext.size() > 24 || ext.find_first_of(L"\\/:*?\"<>|") != ext.npos)
        return {};
    return lower(ext);
}
bool nativeValueControl(HWND control, HWND dialog, const wchar_t *kind) {
    if (!control || !IsWindow(control) || GetAncestor(control, GA_ROOT) != dialog ||
        windowClass(control) != kind || !IsWindowVisible(control) || !IsWindowEnabled(control))
        return false;
    DWORD owner = 0, target = 0;
    GetWindowThreadProcessId(control, &owner);
    GetWindowThreadProcessId(dialog, &target);
    if (!owner || owner != target)
        return false;
    return kind != std::wstring_view(L"Edit") ||
           !(GetWindowLongPtrW(control, GWL_STYLE) & (ES_READONLY | ES_PASSWORD));
}
HWND verifiedNativeValueControl(IUIAutomation *uia, IUIAutomationElement *element, HWND dialog,
                                const std::wstring &value, bool filename) {
    if (!element)
        return nullptr;
    const wchar_t *kind = filename ? L"Edit" : L"ComboBox";
    UIA_HWND handle = nullptr;
    element->get_CurrentNativeWindowHandle(&handle);
    auto control = reinterpret_cast<HWND>(handle);
    if (!nativeValueControl(control, dialog, kind)) {
        VARIANT type{};
        type.vt = VT_I4;
        type.lVal = filename ? UIA_EditControlTypeId : UIA_ComboBoxControlTypeId;
        ComPtr<IUIAutomationCondition> condition;
        uia->CreatePropertyCondition(UIA_ControlTypePropertyId, type, &condition);
        ComPtr<IUIAutomationElementArray> children;
        if (condition)
            element->FindAll(TreeScope_Descendants, condition.Get(), &children);
        int count = 0;
        if (children)
            children->get_Length(&count);
        ComPtr<IUIAutomationElement> child;
        if (count == 1)
            children->GetElement(0, &child);
        handle = nullptr;
        if (child)
            child->get_CurrentNativeWindowHandle(&handle);
        control = reinterpret_cast<HWND>(handle);
    }
    // Bind only a native control beneath the UIA-identified filename/format.
    // Handles are revalidated against the original dialog before every use.
    return nativeValueControl(control, dialog, kind) && windowText(control) == value ? control : nullptr;
}
bool inspectDialog(IUIAutomation *uia, const Job &job, DialogData &d) {
    if (!sameTarget(job))
        return false;
    if (nativeValueControl(job.nameControl, job.hwnd, L"Edit") &&
        nativeValueControl(job.formatControl, job.hwnd, L"ComboBox")) {
        d.nameControl = job.nameControl;
        d.formatControl = job.formatControl;
        d.name = windowText(d.nameControl);
        d.format = windowText(d.formatControl);
    } else {
        ComPtr<IUIAutomationElement> root;
        if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root)
            return false;
        auto field = filenameElement(uia, root.Get(), job.hwnd);
        ComPtr<IUIAutomationValuePattern> value;
        if (field)
            field->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value));
        if (!value)
            return false;
        d.name = automationValue(field.Get());
        auto type = findId(uia, root.Get(), L"FileTypeControlHost");
        if (type)
            d.format = automationValue(type.Get());
        d.nameControl = verifiedNativeValueControl(uia, field.Get(), job.hwnd, d.name, true);
        d.formatControl = verifiedNativeValueControl(uia, type.Get(), job.hwnd, d.format, false);
    }
    d.hwnd = job.hwnd;
    d.pid = job.pid;
    d.directory = directoryFromToolbar(job.hwnd);
    if (d.format.empty()) {
        HWND combo = GetDlgItem(job.hwnd, 0x470);
        d.format = windowText(combo);
    }
    d.extension = extensionFromFormat(d.format);
    auto button = lower(windowText(GetDlgItem(job.hwnd, IDOK)));
    auto title = lower(windowText(job.hwnd));
    d.save = job.saveDialog || button.find(L"保存") != button.npos || button.find(L"save") != button.npos ||
             title.find(L"另存为") != title.npos || title.find(L"save as") != title.npos;
    d.process = processName(job.hwnd);
    d.source = sourceDirectory(job.hwnd, d.name);
    return true;
}
bool setFilename(IUIAutomation *uia, const Job &job, const std::wstring &text) {
    if (!sameTarget(job) || !focusAllowsAction(job))
        return false;
    if (nativeValueControl(job.nameControl, job.hwnd, L"Edit")) {
        if (windowText(job.nameControl) == text)
            return true;
        if (!sameTarget(job) || !focusAllowsAction(job) ||
            !nativeValueControl(job.nameControl, job.hwnd, L"Edit"))
            return false;
        DWORD_PTR written = 0;
        return SendMessageTimeoutW(job.nameControl, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()),
                                   SMTO_ABORTIFHUNG | SMTO_BLOCK, 400, &written) &&
               written && sameTarget(job) && nativeValueControl(job.nameControl, job.hwnd, L"Edit") &&
               windowText(job.nameControl) == text;
    }
    ComPtr<IUIAutomationElement> root;
    if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root)
        return false;
    auto field = filenameElement(uia, root.Get(), job.hwnd);
    ComPtr<IUIAutomationValuePattern> value;
    if (field)
        field->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value));
    if (!value)
        return false;
    BOOL ro = TRUE;
    value->get_CurrentIsReadOnly(&ro);
    if (ro || !sameTarget(job) || !focusAllowsAction(job))
        return false;
    if (automationValue(field.Get()) == text)
        return true;
    BSTR input = SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    if (!input)
        return false;
    HRESULT hr = value->SetValue(input);
    SysFreeString(input);
    return SUCCEEDED(hr) && automationValue(field.Get()) == text;
}
SaveProbe saveProbeFor(const DialogData &data);
#include "generic_dialog.h"
#include "save_bridge.h"
bool submitDialog(const Job &job) {
    if (!sameTarget(job) || !focusAllowsAction(job))
        return false;
    HWND button = GetDlgItem(job.hwnd, IDOK);
    if (!button || !IsWindowEnabled(button))
        return false;
    return PostMessageW(job.hwnd, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED),
                        reinterpret_cast<LPARAM>(button)) != 0;
}
VARIANT property(IDispatch *object, const wchar_t *name) {
    VARIANT v;
    VariantInit(&v);
    if (!object)
        return v;
    DISPID id = 0;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(object->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id)))
        return v;
    DISPPARAMS args{};
    object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &args, &v, nullptr, nullptr);
    return v;
}
std::vector<Location> explorerLocations() {
    std::set<HWND> visible;
    EnumWindows(
        [](HWND h, LPARAM p) -> BOOL {
            if (IsWindowVisible(h) && windowClass(h) == L"CabinetWClass")
                reinterpret_cast<std::set<HWND> *>(p)->insert(h);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&visible));
    std::vector<Location> out;
    if (visible.empty())
        return out;
    ComPtr<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&windows))) || !windows)
        return out;
    long count = 0;
    windows->get_Count(&count);
    std::set<std::wstring> seen;
    for (long i = 0; i < std::min(count, 80L); ++i) {
        VARIANT index{};
        index.vt = VT_I4;
        index.lVal = i;
        ComPtr<IDispatch> item;
        if (FAILED(windows->Item(index, &item)) || !item)
            continue;
        ComPtr<IWebBrowser2> browser;
        item.As(&browser);
        if (!browser)
            continue;
        SHANDLE_PTR hwnd = 0;
        browser->get_HWND(&hwnd);
        if (!visible.contains(reinterpret_cast<HWND>(hwnd)))
            continue;
        ComPtr<IDispatch> document;
        browser->get_Document(&document);
        VARIANT folder = property(document.Get(), L"Folder"), self{};
        VariantInit(&self);
        if (folder.vt == VT_DISPATCH)
            self = property(folder.pdispVal, L"Self");
        VARIANT path{};
        VariantInit(&path);
        if (self.vt == VT_DISPATCH)
            path = property(self.pdispVal, L"Path");
        if (path.vt == VT_BSTR) {
            std::wstring value = bstrText(path.bstrVal);
            if ((value.size() > 2 && value[1] == L':') || value.starts_with(L"\\\\")) {
                if (seen.insert(normalize(value)).second) {
                    auto name = std::filesystem::path(value).filename().wstring();
                    out.push_back({name.empty() ? value : name, value, L"已打开"});
                }
            }
        }
        VariantClear(&path);
        VariantClear(&self);
        VariantClear(&folder);
    }
    return out;
}
SaveProbe saveProbeFor(const DialogData &data) {
    SaveProbe probe;
    if (!data.save || data.directory.empty() || data.name.empty())
        return probe;
    auto name = trim(data.name);
    bool quoted = name.size() >= 2 && name.front() == L'"' && name.back() == L'"';
    if (quoted)
        name = name.substr(1, name.size() - 2);
    std::filesystem::path file(name);
    if (file.is_relative())
        file = std::filesystem::path(data.directory) / file;
    if (!validName(file.filename().wstring()))
        return probe;
    auto exact = file.wstring();
    // Native save dialogs can append the selected extension to a dotted name,
    // while quoted input requests the exact filename, including no extension.
    if (!quoted && !data.extension.empty() && !lower(exact).ends_with(lower(data.extension))) {
        file += data.extension;
        probe.alternatives.push_back(readSaveStamp(exact));
    }
    probe.process = data.process;
    static_cast<SaveStamp &>(probe) = readSaveStamp(file.wstring());
    probe.directory = file.parent_path().wstring();
    return probe;
}
// A short-lived asynchronous watch of known targets, without directory enumeration.
struct SaveWatch {
    Job job;
    HANDLE directory = INVALID_HANDLE_VALUE, event = nullptr;
    OVERLAPPED io{};
    std::array<DWORD, 1024> buffer{};
    bool pending = false;
    ULONGLONG expires = GetTickCount64() + 60000;
    ~SaveWatch() {
        if (pending) {
            CancelIoEx(directory, &io);
            DWORD bytes = 0;
            GetOverlappedResult(directory, &io, &bytes, TRUE);
        }
        if (directory != INVALID_HANDLE_VALUE)
            CloseHandle(directory);
        if (event)
            CloseHandle(event);
    }
    bool arm() {
        if (!event || directory == INVALID_HANDLE_VALUE)
            return false;
        ResetEvent(event);
        io = {};
        io.hEvent = event;
        pending = ReadDirectoryChangesW(directory, buffer.data(), static_cast<DWORD>(sizeof(buffer)), FALSE,
                                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
                                            FILE_NOTIFY_CHANGE_LAST_WRITE,
                                        nullptr, &io, nullptr) != 0;
        return pending;
    }
    bool targetNotified() {
        DWORD bytes = 0;
        pending = false;
        if (!GetOverlappedResult(directory, &io, &bytes, FALSE) || !bytes)
            return true;
        auto target = lower(std::filesystem::path(job.probe.file).filename().wstring());
        const auto *data = reinterpret_cast<const BYTE *>(buffer.data());
        for (DWORD offset = 0; offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= bytes;) {
            auto info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(data + offset);
            if (info->FileNameLength > bytes - offset - offsetof(FILE_NOTIFY_INFORMATION, FileName))
                break;
            auto changed = lower(std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t)));
            if (changed == target ||
                std::any_of(job.probe.alternatives.begin(), job.probe.alternatives.end(),
                            [&](const SaveStamp &stamp) {
                                return lower(std::filesystem::path(stamp.file).filename().wstring()) ==
                                       changed;
                            }))
                return true;
            if (!info->NextEntryOffset || info->NextEntryOffset > bytes - offset)
                break;
            offset += info->NextEntryOffset;
        }
        return false;
    }
};
void publishSaveCheck(const Job &job, bool saved) {
    auto result = std::make_unique<Result>();
    result->job = job;
    result->saved = saved;
    if (saved) {
        if (savedStampChanged(job.probe))
            result->verifiedFile = job.probe.file;
        else
            for (const auto &alternative : job.probe.alternatives)
                if (savedStampChanged(alternative)) {
                    result->verifiedFile = alternative.file;
                    break;
                }
    }
    if (PostMessageW(g->control, WM_RESULT, 0, reinterpret_cast<LPARAM>(result.get())))
        result.release();
}
void enqueue(Job job) {
    if (job.type == JobType::Inspect)
        job.bridge = reinterpret_cast<ULONG_PTR>(GetPropW(job.hwnd, L"Jump.SaveBridge.v1")) == job.pid;
    else if (job.type != JobType::VerifySave) {
        job.bridge = g->dialog.bridge;
        job.bridgeRevision = g->dialog.bridgeRevision;
    }
    if (job.type == JobType::Inspect)
        job.blender = blenderBrowserWindow(job.hwnd);
    else if (job.type != JobType::VerifySave)
        job.blender = g->dialog.blender;
    if (job.type == JobType::Inspect)
        job.houdini = houdiniBrowserWindow(job.hwnd);
    else if (job.type != JobType::VerifySave)
        job.houdini = g->dialog.houdini;
    if (job.type == JobType::Refresh || job.type == JobType::Apply) {
        job.saveDialog = g->dialog.save;
        job.nameControl = g->dialog.nameControl;
        job.formatControl = g->dialog.formatControl;
    }
    if (job.type == JobType::Inspect || job.type == JobType::BindControl) {
        job.generic = !job.blender && !job.houdini && !classicFileDialog(job.hwnd);
        job.bindings = g->cfg.filenameBindings;
    } else if (job.type != JobType::VerifySave) {
        job.generic = g->dialog.generic;
        job.selector = g->dialog.selector;
        job.controlScope = g->dialog.controlScope;
        if (job.type == JobType::Apply) {
            job.expectedRawName = g->dialog.rawName;
            job.expectedDirectory = g->dialog.directory;
            job.expectedExtension = g->dialog.extension;
            job.submitSelector = g->dialog.submitSelector;
        }
    }
    std::lock_guard lock(g->jobsMutex);
    if (job.type == JobType::Refresh) {
        for (const auto &queued : g->jobs)
            if (queued.type == job.type && queued.generation == job.generation)
                return;
    }
    g->jobs.push_back(std::move(job));
    SetEvent(g->workEvent);
}
void publishResult(std::unique_ptr<Result> result, bool preserveSaveProbe) {
    if (result->job.type == JobType::Apply && result->error.empty() && !result->probe.file.empty() &&
        lower(stripExtension(std::filesystem::path(result->probe.file).filename().wstring(),
                             result->job.expectedExtension)) == lower(result->job.name)) {
        result->probe.namingStructure = result->job.namingStructure;
        result->probe.namingProject = result->job.namingProject;
        result->probe.namingKnown = true;
    }
    // Closing a save dialog may detach the panel before its verified target arrives.
    if (g->generation.load() != result->job.generation && !(preserveSaveProbe && !result->probe.file.empty()))
        return;
    if (PostMessageW(g->control, WM_RESULT, 0, reinterpret_cast<LPARAM>(result.get())))
        result.release();
}
DWORD WINAPI workerMain(void *) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ComPtr<IUIAutomation> uia;
    std::vector<std::unique_ptr<SaveWatch>> watches;
    for (;;) {
        DWORD timeout = INFINITE;
        auto now = GetTickCount64();
        for (auto it = watches.begin(); it != watches.end();) {
            if ((*it)->expires <= now) {
                publishSaveCheck((*it)->job, savedFileChanged((*it)->job.probe));
                it = watches.erase(it);
            } else {
                timeout = std::min(timeout, static_cast<DWORD>((*it)->expires - now));
                ++it;
            }
        }
        std::vector<HANDLE> handles{g->workEvent};
        for (auto &watch : watches)
            handles.push_back(watch->event);
        auto wake =
            WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, timeout);
        if (g->stopWorker.load())
            break;
        if (wake >= WAIT_OBJECT_0 + 1 && wake < WAIT_OBJECT_0 + handles.size()) {
            auto index = wake - WAIT_OBJECT_0 - 1;
            auto &watch = watches[index];
            bool saved = watch->targetNotified() && savedFileChanged(watch->job.probe);
            if (saved || !watch->arm()) {
                publishSaveCheck(watch->job, saved);
                watches.erase(watches.begin() + index);
            }
        }
        for (;;) {
            Job job;
            {
                std::lock_guard lock(g->jobsMutex);
                if (g->jobs.empty())
                    break;
                job = std::move(g->jobs.front());
                g->jobs.pop_front();
            }
            // Explicitly scoped fixture delay keeps Save-during-fill checks
            // deterministic even when the normal native fill finishes in milliseconds.
            if (job.type == JobType::Apply && !job.submit && g->testFillDelayMs && sameTarget(job))
                Sleep(g->testFillDelayMs);
            if (job.type == JobType::VerifySave) {
                if (savedFileChanged(job.probe)) {
                    std::erase_if(watches,
                                  [&](const auto &watch) { return watch->job.probe.file == job.probe.file; });
                    publishSaveCheck(job, true);
                    continue;
                }
                if (std::any_of(watches.begin(), watches.end(),
                                [&](const auto &watch) { return watch->job.probe.file == job.probe.file; }))
                    continue;
                auto watch = std::make_unique<SaveWatch>();
                watch->job = job;
                watch->job.verifyAttempt = 4;
                watch->directory =
                    CreateFileW(job.probe.directory.c_str(), FILE_LIST_DIRECTORY,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
                watch->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (!watch->arm()) {
                    // Filesystems without notifications retain a brief bounded fallback.
                    publishSaveCheck(job, false);
                } else if (savedFileChanged(job.probe)) {
                    // Close the race between the first metadata read and arming notifications.
                    publishSaveCheck(watch->job, true);
                } else {
                    if (watches.size() >= 32)
                        watches.erase(watches.begin());
                    watches.push_back(std::move(watch));
                }
                continue;
            }
            if (!sameTarget(job))
                continue;
            if (job.type == JobType::ResolvePaste) {
                auto result = std::make_unique<Result>();
                result->job = job;
                resolvePastedTarget(*result);
                publishResult(std::move(result), false);
                continue;
            }
            if (job.bridge) {
                auto result = std::make_unique<Result>();
                result->job = job;
                applySaveBridge(job, *result);
                publishResult(std::move(result), true);
                continue;
            }
            if (job.blender || job.houdini) {
                auto result = std::make_unique<Result>();
                result->job = job;
                auto request = [&](const wchar_t *verb, const std::wstring &directory = L"",
                                   const std::wstring &name = L"", const std::wstring &extension = L"",
                                   bool submit = false) {
                    return blender_bridge::request(job.hwnd, job.pid, g->panel, verb, directory, name,
                                                   extension, submit, job.houdini ? L"Houdini" : L"Blender",
                                                   job.houdini ? L"JUMP_HOUDINI_1" : L"JUMP_BLENDER_1");
                };
                auto snapshot = request(L"INSPECT");
                auto adopt = [&](const blender_bridge::Reply &reply) {
                    result->data.hwnd = job.hwnd;
                    result->data.pid = job.pid;
                    result->data.blender = job.blender;
                    result->data.houdini = job.houdini;
                    result->data.save = reply.save;
                    result->data.process = job.houdini ? processName(job.hwnd) : L"blender.exe";
                    result->data.name = reply.name;
                    result->data.directory = reply.directory;
                    result->data.extension = reply.extension;
                    result->data.format = reply.format;
                    result->data.source = reply.source;
                };
                if (!snapshot.ok)
                    result->error = snapshot.error;
                else {
                    adopt(snapshot);
                    result->probe = saveProbeFor(result->data);
                    if (job.type == JobType::Inspect)
                        result->opened = explorerLocations();
                    if (job.type == JobType::Apply) {
                        if (!sameTarget(job) || !focusAllowsAction(job))
                            result->error = L"前台或原文件窗口已改变，未执行操作。";
                        else if (snapshot.save && !validName(job.name))
                            result->error = L"请输入有效的文件名。";
                        else {
                            auto destination = result->data;
                            destination.name = job.name + snapshot.extension;
                            if (!job.directory.empty())
                                destination.directory = job.directory;
                            result->probe = saveProbeFor(destination);
                            auto reply =
                                request(L"APPLY", job.directory, job.name, snapshot.extension, job.submit);
                            if (!reply.ok)
                                result->error = reply.error;
                            else {
                                adopt(reply);
                                result->navigated = reply.navigated;
                                result->submitted = reply.submitted;
                            }
                        }
                    }
                }
                publishResult(std::move(result), true);
                continue;
            }
            if (!uia) {
                CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia));
                ComPtr<IUIAutomation2> bounded;
                if (uia && SUCCEEDED(uia.As(&bounded))) {
                    bounded->put_ConnectionTimeout(500);
                    bounded->put_TransactionTimeout(700);
                }
            }
            if (job.type == JobType::BindControl || job.generic) {
                auto result = std::make_unique<Result>();
                result->job = job;
                ComPtr<IUIAutomationElement> field;
                if (!uia)
                    result->error = L"辅助功能接口不可用。";
                else if (job.type == JobType::BindControl)
                    bindFilenameControl(uia.Get(), job, *result);
                else if (!inspectGeneric(uia.Get(), job, result->data, field))
                    result->error = L"未找到唯一可写的文件名控件；可从设置中心“软件接入”指定。";
                else if (job.type == JobType::Apply)
                    applyGeneric(uia.Get(), job, *result, field.Get());
                if (result->error.empty() && !result->submitted)
                    result->probe = saveProbeFor(result->data);
                publishResult(std::move(result), true);
                continue;
            }
            auto result = std::make_unique<Result>();
            result->job = job;
            if (!uia || !inspectDialog(uia.Get(), job, result->data)) {
                result->error = L"未找到可用的文件名控件。";
            } else {
                result->probe = saveProbeFor(result->data);
            }
            if (result->error.empty() && job.type == JobType::Inspect) {
                result->opened = explorerLocations();
            } else if (result->error.empty() && job.type == JobType::Apply) {
                if (!job.directory.empty() && normalize(job.directory) != normalize(result->data.directory)) {
                    auto destination = job.directory;
                    bool validPath =
                        (destination.size() > 2 && destination[1] == L':' && destination[2] == L'\\') ||
                        destination.starts_with(L"\\\\");
                    if (validPath && !destination.starts_with(L"\\\\")) {
                        DWORD attrs = GetFileAttributesW(destination.c_str());
                        validPath = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
                    }
                    if (!validPath)
                        result->error = L"目录不存在或路径格式无效。";
                    else {
                        if (!destination.ends_with(L"\\"))
                            destination += L"\\";
                        if (!setFilename(uia.Get(), job, destination) || !submitDialog(job))
                            result->error = L"无法开始目录导航。";
                        else {
                            ULONGLONG deadline = GetTickCount64() + 2500;
                            bool ready = false;
                            while (sameTarget(job) && GetTickCount64() < deadline) {
                                if (normalize(directoryFromToolbar(job.hwnd)) == normalize(job.directory)) {
                                    ready = true;
                                    break;
                                }
                                Sleep(35);
                            }
                            if (!ready)
                                result->error = L"目录导航未完成，未提交保存。";
                            else
                                result->navigated = true;
                        }
                    }
                }
                if (result->error.empty() && sameTarget(job)) {
                    if (!validName(job.name) && result->data.save)
                        result->error = L"文件名无效，不能包含路径字符或保留名称。";
                    else if (!setFilename(uia.Get(), job, job.name))
                        result->error = L"文件名填入失败，未提交保存。";
                    else {
                        DialogData after;
                        if (!inspectDialog(uia.Get(), job, after) ||
                            stripExtension(after.name, after.extension) != job.name)
                            result->error = L"文件名核验失败，未提交保存。";
                        else if (!job.directory.empty() &&
                                 normalize(after.directory) != normalize(job.directory))
                            result->error = L"目录已改变，未提交保存。";
                        else {
                            result->data = after;
                            result->probe = saveProbeFor(after);
                            if (job.submit) {
                                if (!after.save)
                                    result->error = L"这不是保存对话框。";
                                else if (after.directory.empty())
                                    result->error = L"未能确认保存目录，请使用原软件保存按钮。";
                                else if (submitDialog(job))
                                    result->submitted = true;
                                else
                                    result->error = L"原软件保存按钮不可用。";
                            }
                        }
                    }
                }
            }
            publishResult(std::move(result), true);
        }
    }
    watches.clear();
    uia.Reset();
    CoUninitialize();
    return 0;
}

int px(int v) {
    return MulDiv(v, g->dpi, 96);
}
