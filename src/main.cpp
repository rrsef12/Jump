#include "model.h"
#include "structured_naming.h"
#include "save_session_types.h"
#include "ui_types.h"
#include "app_state.h"
#include "paste_target.h"
#include "naming_worker.h"
#include "naming_session.h"
#include "form_dialog.h"
#include "naming_designer.h"
#include "workflow_checks.h"
#include "save_verification.h"
#include <sstream>
#include "blender_bridge.h"
#include "filename_controls.h"
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <UIAutomation.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <gdiplus.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstdio>
#include <fstream>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace jump;
namespace {
constexpr UINT WM_EVENT = WM_APP + 1, WM_RESULT = WM_APP + 2, WM_TRAY = WM_APP + 3,
               WM_PANEL_REFRESH = WM_APP + 4, WM_PATH_BLUR = WM_APP + 5, WM_BLENDER_BROWSER = WM_APP + 6,
               WM_HOUDINI_BROWSER = WM_APP + 7, WM_BIND_CAPTURE = WM_APP + 8, WM_DROPDOWN_COMMIT = WM_APP + 9;
constexpr UINT WM_NAMING_RESULT = WM_APP + 10, WM_SAVE_BRIDGE = WM_APP + 11;
constexpr UINT WM_EDITOR_DISMISS = WM_APP + 12;
constexpr UINT WM_TARGET_RESULT = WM_APP + 15;
constexpr UINT_PTR T_TARGET_CHECK = 0x4a53;
constexpr UINT WM_CONFIG_SAVED = WM_APP + 14;
constexpr UINT_PTR T_EDITOR_DISMISS = 0x4a51;
constexpr UINT_PTR T_COPY_FEEDBACK = 0x4a52;
constexpr int ID_COMBO_UNDO = 700, ID_COMBO_REDO = 701, ID_PROJECTS = 702, ID_RECORDS = 703, ID_EXPLAIN = 704, ID_TRANSFER = 705, ID_DIAGNOSTICS = 706, ID_CAPTURE_RECIPE = 707, ID_REFRESH_NUMBER = 708, ID_GENERIC_SUBMIT = 709, ID_COPY_NAME = 710, ID_GENERIC_DIRECTORY = 712;
constexpr UINT T_INSPECT = 1, T_REFRESH = 2, T_ACTION = 3, T_SEARCH = 4, T_SAVE_VERIFY = 5, T_BIND = 6, T_GENERIC_REFRESH = 7;
constexpr UINT NAME_INPUT_DELAY = 150;
constexpr int ID_BIND_CONTROL = 188;
constexpr int ID_STRUCT_DELIVERY=753, ID_STRUCT_FREE=755, ID_STRUCT_EXAMPLE=757, ID_STRUCT_INPUT=760, ID_STRUCT_USE=761,
    ID_STRUCT_CLEAR=762, ID_STRUCT_CLOSE=763, ID_STRUCT_LIBRARY=764,
    ID_STRUCT_SUMMARY=766, ID_STRUCT_SOURCE=767,
    ID_STRUCT_INHERIT=770, ID_STRUCT_TODAY=771, ID_STRUCT_LIVE=772, ID_STRUCT_CANCEL=773, ID_STRUCT_TITLE=774, ID_STRUCT_EMPTY=775,
    ID_QUICK_CANCEL=778, ID_STRUCT_RULE_CLEAR=779, ID_STRUCT_FIELD=780,
    ID_STRUCT_QUICK_VIEW=788, ID_STRUCT_FIELDS_VIEW=789, ID_STRUCT_SCHEMES=790, ID_STRUCT_DESIGN=791, ID_STRUCT_NAME=792,
    ID_STRUCT_NUMBER_NONE=793, ID_STRUCT_NUMBER_VERSION=794, ID_STRUCT_NUMBER_SEQUENCE=795, ID_STRUCT_CHOICE=800,
    ID_QUICK_INPUT=810, ID_QUICK_CHOICE=820, ID_QUICK_EDIT=830, ID_QUICK_PICK=840, ID_QUICK_DONE=888;
constexpr int ID_NUMBER_MENU=889, ID_AFFIX_SUMMARY=890, ID_REPLACE_SUMMARY=891,
    ID_CAPABILITIES=892, ID_RULES_SUMMARY=894, ID_FINAL_NAME_LABEL=895, ID_FREE_NAME_INPUT=896;
constexpr int ID_FIELD_ERROR=900, ID_PREFIX_ERROR=908, ID_SUFFIX_ERROR=909, ID_FREE_NAME_ERROR=910, ID_RULE_ERROR=911;
constexpr int ID_ROUTE_SUGGEST=912,ID_TARGET_INFO=913;
std::wstring savedStructure();
constexpr int ID_VARIABLE_TEMPLATE = 240, ID_VARIABLE_VALUES = 241, ID_VARIABLE_HINT = 242,
              ID_VARIABLE_SAVE = 243, ID_VARIABLE_CANCEL = 244, ID_VARIABLE_EDIT = 245,
              ID_VARIABLE_ADVANCED = 246, ID_TEMPLATE_APPLY = 249, ID_VARIABLE_FIELD_FIRST = 260, ID_VARIABLE_TOKEN_FIRST = 600;
constexpr int PANEL_GAP = 3;
constexpr int PANEL_WIDTH = 320, PANEL_MARGIN = 12, FOLDER_ROW_STEP = 34, FOLDER_BUTTON_HEIGHT = 28;
constexpr int ID_PANEL_OPTIONS = 189, ID_PANEL_SETTINGS = 190;
constexpr int LOCATION_PAGE_SIZE = 9, ID_LOCATION_FIRST = 160, ID_LOCATION_PAGE = 153, ID_LOCATION_EMPTY = 154;
constexpr int ID_FOLDER_SCROLL = 169, ID_COMPOSITION = 170, ID_RULE_PATTERN = 172, ID_RULE_REPLACEMENT = 173,
              ID_RULE_MODE = 174, ID_RULE_SOURCE = 175, ID_RULE_HINT = 176, ID_RULE_ENABLE = 177,
              ID_RULE_CLEAR = 178, ID_ADD_PREFIX = 179, ID_ADD_SUFFIX = 180, ID_MAIN_ADD = 181,
              ID_RULE_RESULT = 185, ID_AFFIX_PREFIX = 186, ID_AFFIX_SUFFIX = 187;
constexpr int ID_DIRECTORY_EDIT = 155, ID_DIRECTORY_INPUT = 156, ID_DIRECTORY_CANCEL = 157, ID_DIRECTORY_SEARCH = 158;
constexpr int ID_PREVIEW = 101,
              ID_MANAGER = 105, ID_SUBMIT = 106, ID_UNDO = 107, ID_CLOSE = 108,
              ID_PLANS = 110, ID_NAME_MANAGER = 112;
constexpr int ID_DIRECTORY = 113, ID_STATUS = 114;
constexpr int ID_RENAME = 116, ID_LOC_RECOMMENDED = 120, ID_LOC_FAVORITES = 121, ID_LOC_HISTORY = 122,
              ID_FAVORITE_CURRENT = 124, ID_LOC_PREVIOUS = 125, ID_LOC_NEXT = 126,
              ID_LOC_RECENT = 127, ID_CLEAR_RECENT = 128, ID_CLEAR_HISTORY = 129;
constexpr int ID_PRESET_VALUE = 137, ID_PRESET_SAVE = 138,
              ID_PRESET_CANCEL = 139, ID_PRESET_HINT = 140;
constexpr int ID_LOCATION_MENU = 141, ID_NAMING_MENU = 142, ID_FILENAME_MENU = 143;
constexpr int ID_FOLDER_SEARCH = 145, ID_FOLDER_LIST = 146, ID_MINIMIZE = 147,
              ID_NATIVE_FORMAT = 148, ID_FLYOUT_MANAGE = 149, ID_FOLDER_SELECT = 150, ID_FOLDER_STAR = 151,
              ID_FOCUS_NAME = 152;
constexpr size_t NEW_PRESET = static_cast<size_t>(-1);
constexpr int MANAGER_AFFIXES = 4, MANAGER_TEMPLATES = 5, MANAGER_PROJECTS=6, MANAGER_BACKUP=7, MANAGER_CONNECTIONS=8;
constexpr int MANAGER_PAGES=9, MANAGER_CONTENT_LEFT=148, MANAGER_SHELL_WIDTH=792;
constexpr UINT WM_SETTINGS_NAVIGATE=WM_APP+13;
const wchar_t *managerTitles[]{L"命名方案",L"收藏位置",L"保存流程",L"常规",L"前后缀",L"变量表达式",L"项目",L"备份与迁移",L"软件接入"};
std::vector<SettingsDialog*> settingsDialogs;
int pendingSettingsPage=-1;
bool pendingSettingsClose=false;
void openSettingsPage(int page);
void refreshSettingsPage();
bool requestSettingsNavigation(int page);
bool managerDraftDirty();
bool discardManagerDraft();
bool handleAppShortcut(const MSG &message);
State *g;
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
std::wstring processName(HWND h) { return std::filesystem::path(processImage(h)).filename().wstring(); }
bool sameTarget(const Job &job) {
    DWORD pid = 0;
    if (!IsWindow(job.hwnd) || !IsWindowVisible(job.hwnd) || !IsWindowEnabled(job.hwnd))
        return false;
    GetWindowThreadProcessId(job.hwnd, &pid);
    return pid == job.pid && (!g->testTargetPid || pid == g->testTargetPid) &&
        (g->testTargetImage.empty() || processImage(job.hwnd) == g->testTargetImage) && g->generation.load() == job.generation;
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
    if (!path.empty()) return path;
    auto address = trim(text);
    auto colon = address.find(L':');
    if (colon != address.npos) address = trim(address.substr(colon + 1));
    if (address.empty()) return {};

    // Breadcrumbs use localized shell names (e.g. Downloads) instead of drive paths.
    // Resolve the actual known folder, including OneDrive/redirection, without editing the address bar.
    struct Folder { const KNOWNFOLDERID *id; const wchar_t *canonical; };
    const Folder folders[] = {
        {&FOLDERID_Downloads, L"Downloads"}, {&FOLDERID_Desktop, L"Desktop"},
        {&FOLDERID_Documents, L"Documents"}, {&FOLDERID_Pictures, L"Pictures"},
        {&FOLDERID_Music, L"Music"}, {&FOLDERID_Videos, L"Videos"},
        {&FOLDERID_Profile, L"User"}, {&FOLDERID_SkyDrive, L"OneDrive"},
        {&FOLDERID_Public, L"Public"}, {&FOLDERID_PublicDocuments, L"Public Documents"}
    };
    for (const auto &folder : folders) {
        PWSTR value = nullptr;
        if (FAILED(SHGetKnownFolderPath(*folder.id, KF_FLAG_DONT_VERIFY, nullptr, &value))) continue;
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
            if (name.empty()) continue;
            auto prefix = lower(name), current = lower(address);
            if (current != prefix && !(current.starts_with(prefix + L"\\") || current.starts_with(prefix + L"/"))) continue;
            auto resolved = physical;
            if (address.size() > name.size()) resolved /= address.substr(name.size() + 1);
            auto candidate = resolved.lexically_normal().wstring();
            DWORD attrs = GetFileAttributesW(candidate.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) return candidate;
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
        if (FAILED(SHGetKnownFolderPath(*id, 0, nullptr, &path))) return false;
        std::wstring expected(path);
        CoTaskMemFree(path);
        if (FAILED(SHGetKnownFolderIDList(*id, 0, nullptr, &pidl))) return false;
        ComPtr<IShellItem> item;
        auto hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item));
        CoTaskMemFree(pidl);
        if (FAILED(hr) || FAILED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &display))) return false;
        std::wstring address(display);
        CoTaskMemFree(display);
        if (normalize(directoryFromShellAddress(L"地址: " + address)) != normalize(expected) ||
            normalize(directoryFromShellAddress(L"Address: " + address)) != normalize(expected) ||
            normalize(directoryFromShellAddress(L"地址: " + expected)) != normalize(expected)) return false;
    }
    return directoryFromShellAddress(L"地址: Jump不存在的系统位置_987654321").empty();
}
bool blenderBrowserWindow(HWND hwnd) {
    auto cls = windowClass(hwnd);
    if (!cls.starts_with(L"GHOST_WindowClass")) return false;
    auto title = windowText(hwnd);
    return (title == L"Blender File View" || title == L"Blender文件视图" || title == L"Blender 文件视图") &&
           processName(hwnd) == L"blender.exe";
}
bool houdiniBrowserWindow(HWND hwnd) {
    if (!windowClass(hwnd).starts_with(L"Qt")) return false;
    auto title = windowText(hwnd);
    if (title != L"Save As" && title != L"另存为") return false;
    auto process = processName(hwnd);
    return process == L"houdinifx.exe" || process == L"houdini.exe" || process == L"houdinicore.exe";
}
bool classicFileDialog(HWND hwnd) {
    if (hwnd && IsWindowVisible(hwnd) && IsWindowEnabled(hwnd) &&
        (blenderBrowserWindow(hwnd) || houdiniBrowserWindow(hwnd))) return true;
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
    if (!hwnd || !IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd) || IsHungAppWindow(hwnd)) return false;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == g->pid || !pid) return false;
    if (reinterpret_cast<ULONG_PTR>(GetPropW(hwnd, L"Jump.SaveBridge.v1")) == pid) return true;
    auto title = windowText(hwnd);
    if (title.empty()) return false;
    if (filename_controls::saveTitle(title)) return true;
    // Owned modal dialogs can use document-specific or localized titles. Inspect
    // their controls before deciding; a title is a discovery hint, not a contract.
    auto owner = GetWindow(hwnd, GW_OWNER);
    DWORD ownerPid = 0;
    if (owner && GetWindowThreadProcessId(owner, &ownerPid) && ownerPid == pid && !IsWindowEnabled(owner)) return true;
    auto image = processImage(hwnd), cls = windowClass(hwnd);
    return std::any_of(g->cfg.filenameBindings.begin(), g->cfg.filenameBindings.end(), [&](const auto &binding) {
        return filename_controls::scope(binding, image, cls, title);
    });
}
bool looksLikeFileDialog(HWND hwnd) { return classicFileDialog(hwnd) || genericCandidate(hwnd); }
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
        windowClass(control) != kind || !IsWindowVisible(control) || !IsWindowEnabled(control)) return false;
    DWORD owner = 0, target = 0;
    GetWindowThreadProcessId(control, &owner); GetWindowThreadProcessId(dialog, &target);
    if (!owner || owner != target) return false;
    return kind != std::wstring_view(L"Edit") || !(GetWindowLongPtrW(control, GWL_STYLE) & (ES_READONLY | ES_PASSWORD));
}
HWND verifiedNativeValueControl(IUIAutomation *uia, IUIAutomationElement *element, HWND dialog,
                               const std::wstring &value, bool filename) {
    if (!element) return nullptr;
    const wchar_t *kind = filename ? L"Edit" : L"ComboBox";
    UIA_HWND handle = nullptr; element->get_CurrentNativeWindowHandle(&handle);
    auto control = reinterpret_cast<HWND>(handle);
    if (!nativeValueControl(control, dialog, kind)) {
        VARIANT type{}; type.vt = VT_I4; type.lVal = filename ? UIA_EditControlTypeId : UIA_ComboBoxControlTypeId;
        ComPtr<IUIAutomationCondition> condition; uia->CreatePropertyCondition(UIA_ControlTypePropertyId, type, &condition);
        ComPtr<IUIAutomationElementArray> children;
        if (condition) element->FindAll(TreeScope_Descendants, condition.Get(), &children);
        int count = 0; if (children) children->get_Length(&count);
        ComPtr<IUIAutomationElement> child;
        if (count == 1) children->GetElement(0, &child);
        handle = nullptr; if (child) child->get_CurrentNativeWindowHandle(&handle);
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
        d.nameControl = job.nameControl; d.formatControl = job.formatControl;
        d.name = windowText(d.nameControl); d.format = windowText(d.formatControl);
    } else {
        ComPtr<IUIAutomationElement> root;
        if (FAILED(uia->ElementFromHandle(job.hwnd, &root)) || !root) return false;
        auto field = filenameElement(uia, root.Get(), job.hwnd);
        ComPtr<IUIAutomationValuePattern> value;
        if (field) field->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value));
        if (!value) return false;
        d.name = automationValue(field.Get());
        auto type = findId(uia, root.Get(), L"FileTypeControlHost");
        if (type) d.format = automationValue(type.Get());
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
        if (windowText(job.nameControl) == text) return true;
        if (!sameTarget(job) || !focusAllowsAction(job) ||
            !nativeValueControl(job.nameControl, job.hwnd, L"Edit")) return false;
        DWORD_PTR written = 0;
        return SendMessageTimeoutW(job.nameControl, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()),
                   SMTO_ABORTIFHUNG | SMTO_BLOCK, 400, &written) && written && sameTarget(job) &&
            nativeValueControl(job.nameControl, job.hwnd, L"Edit") && windowText(job.nameControl) == text;
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
    if (!data.save || data.directory.empty() || data.name.empty()) return probe;
    auto name = trim(data.name);
    bool quoted = name.size() >= 2 && name.front() == L'"' && name.back() == L'"';
    if (quoted) name = name.substr(1, name.size() - 2);
    std::filesystem::path file(name);
    if (file.is_relative()) file = std::filesystem::path(data.directory) / file;
    if (!validName(file.filename().wstring())) return probe;
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
        if (directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
        if (event) CloseHandle(event);
    }
    bool arm() {
        if (!event || directory == INVALID_HANDLE_VALUE) return false;
        ResetEvent(event);
        io = {};
        io.hEvent = event;
        pending = ReadDirectoryChangesW(directory, buffer.data(), static_cast<DWORD>(sizeof(buffer)), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
            nullptr, &io, nullptr) != 0;
        return pending;
    }
    bool targetNotified() {
        DWORD bytes = 0;
        pending = false;
        if (!GetOverlappedResult(directory, &io, &bytes, FALSE) || !bytes) return true;
        auto target = lower(std::filesystem::path(job.probe.file).filename().wstring());
        const auto *data = reinterpret_cast<const BYTE *>(buffer.data());
        for (DWORD offset = 0; offset + offsetof(FILE_NOTIFY_INFORMATION, FileName) <= bytes;) {
            auto info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(data + offset);
            if (info->FileNameLength > bytes - offset - offsetof(FILE_NOTIFY_INFORMATION, FileName)) break;
            auto changed = lower(std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t)));
            if (changed == target || std::any_of(job.probe.alternatives.begin(), job.probe.alternatives.end(),
                [&](const SaveStamp &stamp) { return lower(std::filesystem::path(stamp.file).filename().wstring()) == changed; })) return true;
            if (!info->NextEntryOffset || info->NextEntryOffset > bytes - offset) break;
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
        if (savedStampChanged(job.probe)) result->verifiedFile = job.probe.file;
        else for (const auto &alternative : job.probe.alternatives)
            if (savedStampChanged(alternative)) { result->verifiedFile = alternative.file; break; }
    }
    if (PostMessageW(g->control, WM_RESULT, 0, reinterpret_cast<LPARAM>(result.get()))) result.release();
}
void enqueue(Job job) {
    if (job.type == JobType::Inspect)
        job.bridge = reinterpret_cast<ULONG_PTR>(GetPropW(job.hwnd, L"Jump.SaveBridge.v1")) == job.pid;
    else if (job.type != JobType::VerifySave) {
        job.bridge = g->dialog.bridge; job.bridgeRevision = g->dialog.bridgeRevision;
    }
    if (job.type == JobType::Inspect) job.blender = blenderBrowserWindow(job.hwnd);
    else if (job.type != JobType::VerifySave) job.blender = g->dialog.blender;
    if (job.type == JobType::Inspect) job.houdini = houdiniBrowserWindow(job.hwnd);
    else if (job.type != JobType::VerifySave) job.houdini = g->dialog.houdini;
    if (job.type == JobType::Refresh || job.type == JobType::Apply) {
        job.saveDialog = g->dialog.save;
        job.nameControl = g->dialog.nameControl; job.formatControl = g->dialog.formatControl;
    }
    if (job.type == JobType::Inspect || job.type == JobType::BindControl) {
        job.generic = !job.blender && !job.houdini && !classicFileDialog(job.hwnd);
        job.bindings = g->cfg.filenameBindings;
    } else if (job.type != JobType::VerifySave) {
        job.generic = g->dialog.generic; job.selector = g->dialog.selector;
        job.controlScope = g->dialog.controlScope;
        if (job.type == JobType::Apply) {
            job.expectedRawName = g->dialog.rawName; job.expectedDirectory = g->dialog.directory;
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
    if(result->job.type==JobType::Apply&&result->error.empty()&&!result->probe.file.empty()&&
       lower(stripExtension(std::filesystem::path(result->probe.file).filename().wstring(),result->job.expectedExtension))==lower(result->job.name)){
        result->probe.namingStructure=result->job.namingStructure;result->probe.namingProject=result->job.namingProject;result->probe.namingKnown=true;
    }
    // Closing a save dialog may detach the panel before its verified target arrives.
    if (g->generation.load() != result->job.generation &&
        !(preserveSaveProbe && !result->probe.file.empty())) return;
    if (PostMessageW(g->control, WM_RESULT, 0, reinterpret_cast<LPARAM>(result.get()))) result.release();
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
        for (auto &watch : watches) handles.push_back(watch->event);
        auto wake = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, timeout);
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
                    std::erase_if(watches, [&](const auto &watch) { return watch->job.probe.file == job.probe.file; });
                    publishSaveCheck(job, true);
                    continue;
                }
                if (std::any_of(watches.begin(), watches.end(), [&](const auto &watch) {
                        return watch->job.probe.file == job.probe.file;
                    })) continue;
                auto watch = std::make_unique<SaveWatch>();
                watch->job = job;
                watch->job.verifyAttempt = 4;
                watch->directory = CreateFileW(job.probe.directory.c_str(), FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
                watch->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (!watch->arm()) {
                    // Filesystems without notifications retain a brief bounded fallback.
                    publishSaveCheck(job, false);
                } else if (savedFileChanged(job.probe)) {
                    // Close the race between the first metadata read and arming notifications.
                    publishSaveCheck(watch->job, true);
                } else {
                    if (watches.size() >= 32) watches.erase(watches.begin());
                    watches.push_back(std::move(watch));
                }
                continue;
            }
            if (!sameTarget(job))
                continue;
            if (job.type == JobType::ResolvePaste) {
                auto result = std::make_unique<Result>(); result->job = job;
                resolvePastedTarget(*result);
                publishResult(std::move(result), false);
                continue;
            }
            if (job.bridge) {
                auto result = std::make_unique<Result>(); result->job = job;
                applySaveBridge(job, *result);
                publishResult(std::move(result), true);
                continue;
            }
            if (job.blender || job.houdini) {
                if (job.type == JobType::FocusFormat) continue;
                auto result = std::make_unique<Result>(); result->job = job;
                auto request = [&](const wchar_t *verb, const std::wstring &directory = L"",
                                   const std::wstring &name = L"", const std::wstring &extension = L"", bool submit = false) {
                    return blender_bridge::request(job.hwnd, job.pid, g->panel, verb, directory, name, extension, submit,
                        job.houdini ? L"Houdini" : L"Blender", job.houdini ? L"JUMP_HOUDINI_1" : L"JUMP_BLENDER_1");
                };
                auto snapshot = request(L"INSPECT");
                auto adopt = [&](const blender_bridge::Reply &reply) {
                    result->data.hwnd = job.hwnd; result->data.pid = job.pid;
                    result->data.blender = job.blender; result->data.houdini = job.houdini; result->data.save = reply.save;
                    result->data.process = job.houdini ? processName(job.hwnd) : L"blender.exe";
                    result->data.name = reply.name; result->data.directory = reply.directory;
                    result->data.extension = reply.extension; result->data.format = reply.format;
                    result->data.source = reply.source;
                };
                if (!snapshot.ok) result->error = snapshot.error;
                else {
                    adopt(snapshot); result->probe = saveProbeFor(result->data);
                    if (job.type == JobType::Inspect) result->opened = explorerLocations();
                    if (job.type == JobType::Apply) {
                        if (!sameTarget(job) || !focusAllowsAction(job)) result->error = L"前台或原文件窗口已改变，未执行操作。";
                        else if (snapshot.save && !validName(job.name)) result->error = L"请输入有效的文件名。";
                        else {
                            auto destination = result->data;
                            destination.name = job.name + snapshot.extension;
                            if (!job.directory.empty()) destination.directory = job.directory;
                            result->probe = saveProbeFor(destination);
                            auto reply = request(L"APPLY", job.directory, job.name, snapshot.extension, job.submit);
                            if (!reply.ok) result->error = reply.error;
                            else {
                                adopt(reply); result->navigated = reply.navigated; result->submitted = reply.submitted;
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
                    bounded->put_ConnectionTimeout(500); bounded->put_TransactionTimeout(700);
                }
            }
            if (job.type == JobType::BindControl || job.generic) {
                if (job.type == JobType::FocusFormat) continue;
                auto result = std::make_unique<Result>(); result->job = job;
                ComPtr<IUIAutomationElement> field;
                if (!uia) result->error = L"辅助功能接口不可用。";
                else if (job.type == JobType::BindControl) bindFilenameControl(uia.Get(), job, *result);
                else if (!inspectGeneric(uia.Get(), job, result->data, field))
                    result->error = L"未找到唯一可写的文件名控件；可从设置中心“软件接入”指定。";
                else if (job.type == JobType::Apply) applyGeneric(uia.Get(), job, *result, field.Get());
                if (result->error.empty() && !result->submitted) result->probe = saveProbeFor(result->data);
                publishResult(std::move(result), true);
                continue;
            }
            if (job.type == JobType::FocusFormat) {
                if (uia && focusAllowsAction(job)) {
                    ComPtr<IUIAutomationElement> root, type;
                    uia->ElementFromHandle(job.hwnd, &root);
                    if (root) type = findId(uia.Get(), root.Get(), L"FileTypeControlHost");
                    if (!type && GetDlgItem(job.hwnd, 0x470)) uia->ElementFromHandle(GetDlgItem(job.hwnd, 0x470), &type);
                    if (type && sameTarget(job) && focusAllowsAction(job)) {
                        type->SetFocus();
                        ComPtr<IUIAutomationExpandCollapsePattern> expand;
                        type->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(&expand));
                        if (expand && sameTarget(job) && focusAllowsAction(job)) expand->Expand();
                    }
                }
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
#include "panel_material.h"
class PaintBuffer {
    HDC target{}, memory{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    RECT bounds{};
    bool presented = false;
    DWORD *pixels = nullptr;
    bool material = false, revealBackdrop = false;
public:
    HDC dc{};
    PaintBuffer(HDC output, const RECT &rect, bool composited = false, bool backdrop = true)
        : target(output), bounds(rect), material(composited), revealBackdrop(backdrop), dc(output) {
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) return;
        memory = CreateCompatibleDC(output);
        if (!memory) return;
        if (material) {
            BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(output, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&pixels), nullptr, 0);
        } else bitmap = CreateCompatibleBitmap(output, width, height);
        if (!bitmap) { DeleteDC(memory); memory = nullptr; return; }
        previous = SelectObject(memory, bitmap);
        SetViewportOrgEx(memory, -rect.left, -rect.top, nullptr);
        dc = memory;
    }
    void present() {
        if (memory && !presented) {
            if (pixels) {
                GdiFlush();
                prepareMaterialPixels(pixels, static_cast<size_t>(bounds.right-bounds.left)*(bounds.bottom-bounds.top), revealBackdrop);
            }
            BitBlt(target, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                   memory, bounds.left, bounds.top, SRCCOPY);
            presented = true;
        }
    }
    ~PaintBuffer() {
        present();
        if (memory) { SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); }
    }
    PaintBuffer(const PaintBuffer &) = delete;
    PaintBuffer &operator=(const PaintBuffer &) = delete;
};
class ClientPaint {
    HWND window{};
    bool painting{};
    PAINTSTRUCT ps{};
    HDC target{};
    static RECT client(HWND h) { RECT r{}; GetClientRect(h, &r); return r; }
    PaintBuffer buffer;
public:
    ClientPaint(HWND h, UINT msg, WPARAM output) : window(h), painting(msg == WM_PAINT),
        target(painting ? BeginPaint(h, &ps) : reinterpret_cast<HDC>(output)), buffer(target, client(h), panelComposited(h) && msg == WM_PAINT) {}
    HDC dc() const { return buffer.dc; }
    ~ClientPaint() { buffer.present(); if (painting) EndPaint(window, &ps); }
};
// Filename fragments can be a single long word. Use the same explicit line
// breaks for measuring and painting instead of relying on word boundaries.
std::wstring wrappedFilename(HDC dc,const std::wstring &text,int width){
    std::wstring result;
    for(size_t at=0;at<text.size();){
        int fit=0;SIZE extent{};
        GetTextExtentExPointW(dc,text.c_str()+at,static_cast<int>(text.size()-at),std::max(1,width),&fit,nullptr,&extent);
        size_t count=std::max(1,fit);
        if(at+count<text.size()&&IS_HIGH_SURROGATE(text[at+count-1])&&IS_LOW_SURROGATE(text[at+count])){
            if(count>1)--count;else ++count;
        }
        if(!result.empty())result+=L'\n';
        result+=text.substr(at,count);at+=count;
    }
    return result;
}
std::wstring trailingPath(HDC dc,const std::wstring &path,int width){
    auto fits=[&](const std::wstring &text){SIZE size{};GetTextExtentPoint32W(dc,text.c_str(),static_cast<int>(text.size()),&size);return size.cx<=width;};
    if(fits(path))return path;
    for(size_t at=0;(at=path.find_first_of(L"\\/",at))!=path.npos;++at){
        auto tail=L"…"+path.substr(at);
        if(fits(tail))return tail;
    }
    // An unusually long final directory still keeps its end visible.
    for(size_t at=0;at<path.size();++at){
        if(IS_LOW_SURROGATE(path[at]))continue;
        auto tail=L"…"+path.substr(at);if(fits(tail))return tail;
    }
    return L"…";
}
void paintMaterialText(HWND h, HDC dc, RECT r, DWORD style) {
    FillRect(dc, &r, g->brush); SetBkMode(dc, TRANSPARENT);
    auto id = GetDlgCtrlID(h);
    SetTextColor(dc, (id == ID_STATUS && g->statusError) || (id == ID_RULE_HINT && !g->rulePreview.error.empty()) ? RGB(226,122,122) : g->muted);
    if(id==ID_PREVIEW)SetTextColor(dc,g->fg);
    if(id==ID_STATUS&&!g->statusError)SetTextColor(dc,RGB(124,130,140));
    if(id>=ID_FIELD_ERROR&&id<=ID_RULE_ERROR)SetTextColor(dc,RGB(226,122,122));
    auto font = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0)));
    UINT flags = DT_NOPREFIX | DT_WORDBREAK;
    if ((style & SS_TYPEMASK) == SS_CENTER) flags |= DT_CENTER;
    if (style & SS_ELLIPSISMASK) flags = (flags & ~DT_WORDBREAK) | DT_SINGLELINE | DT_END_ELLIPSIS;
    if ((style & SS_ELLIPSISMASK) == SS_PATHELLIPSIS) flags = DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX;
    if (id == ID_LOCATION_EMPTY) flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
    auto text = windowText(h);
    if(id==ID_GENERIC_DIRECTORY){text=trailingPath(dc,text,r.right-r.left);flags=DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX;}
    if(id==ID_PREVIEW){
        text=wrappedFilename(dc,text,r.right-r.left);flags=DT_NOPREFIX;
        if(text.find(L'\n')==text.npos)flags|=DT_SINGLELINE|DT_VCENTER;
    }
    DrawTextW(dc, text.c_str(), -1, &r, flags);
    SelectObject(dc, font);
}
LRESULT CALLBACK MaterialControlProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR textStyle) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp); RECT r{}; GetClientRect(h, &r);
        paintMaterialText(h, paint.dc(), r, static_cast<DWORD>(textStyle));
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void positionChild(HWND h, int x, int y, int width, int height) {
    RECT r{}; GetWindowRect(h, &r); MapWindowPoints(nullptr, GetParent(h), reinterpret_cast<POINT *>(&r), 2);
    if (r.left == x && r.top == y && r.right - r.left == width && r.bottom - r.top == height) return;
    SetWindowPos(h, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}
void setTextIfChanged(HWND h, const std::wstring &text) {
    if (h && windowText(h) != text) SetWindowTextW(h, text.c_str());
}
void showIfChanged(HWND h, int command) {
    if (h && ((GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE) != 0) != (command != SW_HIDE)) ShowWindow(h, command);
}
void enableIfChanged(HWND h, BOOL enabled) {
    if (h && IsWindowEnabled(h) != !!enabled) EnableWindow(h, enabled);
}
void setHighlightedText(HWND h, const std::wstring &text, const std::vector<std::pair<size_t, size_t>> &matches) {
    if (!h) return;
    auto previous = g->highlights.find(h);
    if (previous != g->highlights.end() && previous->second.text == text && previous->second.matches == matches) return;
    DWORD start = 0, end = 0; POINT scroll{};
    SendMessageW(h, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    SendMessageW(h, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    bool visible = IsWindowVisible(h) != FALSE;
    if (visible) SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    setTextIfChanged(h, text);
    SendMessageW(h, EM_SETBKGNDCOLOR, 0, g->field);
    SendMessageW(h, EM_SETSEL, 0, -1);
    CHARFORMAT2W format{}; format.cbSize = sizeof(format); format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = g->fg; format.crBackColor = g->field;
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    format.crTextColor = g->accent; format.crBackColor = g->selection;
    for (auto [at, length] : matches) {
        if (!length) continue;
        SendMessageW(h, EM_SETSEL, at, at + length);
        SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    SendMessageW(h, EM_SETSEL, start, end);
    SendMessageW(h, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    if (visible) SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, nullptr, FALSE);
    g->highlights[h] = {text, matches};
}
enum class UiIcon { Folder, Edit, Search, Star, Settings, Close, Plus, Check, Chevron, ChevronUp,
    Layers, Number, Undo, Redo, Affix, Replace, Info, CheckCircle };
void ensureGraphics() {
    struct Runtime {
        ULONG_PTR token{};
        Runtime() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token, &input, nullptr); }
        ~Runtime() { if (token) Gdiplus::GdiplusShutdown(token); }
    };
    static Runtime runtime;
}
void paintRoundedRect(HDC dc, int left, int top, int right, int bottom, int ellipseWidth, int ellipseHeight) {
    ensureGraphics();
    LOGBRUSH brush{}; LOGPEN pen{};
    GetObjectW(GetCurrentObject(dc, OBJ_BRUSH), sizeof(brush), &brush);
    GetObjectW(GetCurrentObject(dc, OBJ_PEN), sizeof(pen), &pen);
    const bool fill = brush.lbStyle != BS_NULL;
    const bool stroke = pen.lopnStyle != PS_NULL && (!fill || pen.lopnColor != brush.lbColor);
    float inset = stroke ? .5f : 0.f;
    float x = left + inset, y = top + inset, width = right - left - inset * 2, height = bottom - top - inset * 2;
    if (width <= 0 || height <= 0) return;
    float rx = std::min(width / 2, ellipseWidth / 2.f), ry = std::min(height / 2, ellipseHeight / 2.f);
    if (rx <= 0 || ry <= 0) return;
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, rx * 2, ry * 2, 180.f, 90.f);
    path.AddArc(x + width - rx * 2, y, rx * 2, ry * 2, 270.f, 90.f);
    path.AddArc(x + width - rx * 2, y + height - ry * 2, rx * 2, ry * 2, 0.f, 90.f);
    path.AddArc(x, y + height - ry * 2, rx * 2, ry * 2, 90.f, 90.f);
    path.CloseFigure();
    auto color = [](COLORREF value) { return Gdiplus::Color(255, GetRValue(value), GetGValue(value), GetBValue(value)); };
    if (fill) { Gdiplus::SolidBrush background(color(brush.lbColor)); graphics.FillPath(&background, &path); }
    if (stroke) { Gdiplus::Pen border(color(pen.lopnColor), 1.f); graphics.DrawPath(&border, &path); }
}
void drawUiIcon(HDC dc, UiIcon icon, float x, float y, float size, COLORREF color, bool filled = false) {
    ensureGraphics();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.TranslateTransform(x, y);
    graphics.ScaleTransform(size / 16.f, size / 16.f);
    Gdiplus::Color ink(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(ink, 1.45f);
    pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    auto line = [&](std::initializer_list<Gdiplus::PointF> points) {
        graphics.DrawLines(&pen, points.begin(), static_cast<INT>(points.size()));
    };
    switch (icon) {
    case UiIcon::Folder: {
        Gdiplus::GraphicsPath path;
        path.StartFigure(); path.AddLine(2.f, 5.f, 2.f, 3.5f);
        path.AddBezier(2.f, 3.5f, 2.f, 2.8f, 2.4f, 2.5f, 3.f, 2.5f);
        path.AddLine(3.f, 2.5f, 6.f, 2.5f); path.AddLine(6.f, 2.5f, 8.f, 4.5f);
        path.AddLine(8.f, 4.5f, 13.f, 4.5f);
        path.AddBezier(13.f, 4.5f, 13.7f, 4.5f, 14.f, 4.8f, 14.f, 5.5f);
        path.AddLine(14.f, 5.5f, 14.f, 12.5f);
        path.AddBezier(14.f, 12.5f, 14.f, 13.2f, 13.7f, 13.5f, 13.f, 13.5f);
        path.AddLine(13.f, 13.5f, 3.f, 13.5f);
        path.AddBezier(3.f, 13.5f, 2.3f, 13.5f, 2.f, 13.2f, 2.f, 12.5f);
        path.CloseFigure(); graphics.DrawPath(&pen, &path);
        line({{2, 6}, {14, 6}}); break;
    }
    case UiIcon::Edit:
        line({{3, 10.5f}, {10.5f, 3}, {13, 5.5f}, {5.5f, 13}, {2.5f, 13.5f}, {3, 10.5f}});
        line({{9, 4.5f}, {11.5f, 7}}); break;
    case UiIcon::Search:
        graphics.DrawEllipse(&pen, 2.5f, 2.5f, 8.f, 8.f);
        line({{9.5f, 9.5f}, {13.5f, 13.5f}}); break;
    case UiIcon::Star: {
        Gdiplus::PointF points[]{{8, 1.7f}, {9.9f, 5.6f}, {14.2f, 6.2f}, {11.1f, 9.2f},
            {11.8f, 13.5f}, {8, 11.5f}, {4.2f, 13.5f}, {4.9f, 9.2f}, {1.8f, 6.2f}, {6.1f, 5.6f}};
        Gdiplus::GraphicsPath path; path.AddPolygon(points, 10);
        if (filled) { Gdiplus::SolidBrush brush(ink); graphics.FillPath(&brush, &path); }
        graphics.DrawPath(&pen, &path); break;
    }
    case UiIcon::Settings:
        line({{2, 4}, {5, 4}}); line({{9, 4}, {14, 4}});
        line({{2, 12}, {8, 12}}); line({{12, 12}, {14, 12}});
        graphics.DrawEllipse(&pen, 5, 2, 4, 4); graphics.DrawEllipse(&pen, 8, 10, 4, 4); break;
    case UiIcon::Close: line({{4, 4}, {12, 12}}); line({{12, 4}, {4, 12}}); break;
    case UiIcon::Plus: line({{3, 8}, {13, 8}}); line({{8, 3}, {8, 13}}); break;
    case UiIcon::Check: line({{3, 8}, {6.5f, 11.5f}, {13, 4.5f}}); break;
    case UiIcon::Chevron: line({{4, 6}, {8, 10}, {12, 6}}); break;
    case UiIcon::ChevronUp: line({{4, 10}, {8, 6}, {12, 10}}); break;
    case UiIcon::Layers:
        line({{2, 5}, {8, 2}, {14, 5}, {8, 8}, {2, 5}});
        line({{2, 8.5f}, {8, 11.5f}, {14, 8.5f}});line({{2, 12}, {8, 15}, {14, 12}});break;
    case UiIcon::Number:
        line({{6, 2}, {4, 14}});line({{12, 2}, {10, 14}});
        line({{3, 6}, {14, 6}});line({{2, 10}, {13, 10}});break;
    case UiIcon::Undo:
    case UiIcon::Redo: {
        if(icon==UiIcon::Redo){graphics.TranslateTransform(16.f,0.f);graphics.ScaleTransform(-1.f,1.f);}
        Gdiplus::GraphicsPath path;path.AddBezier(14.f,12.f,14.f,7.f,11.f,5.f,7.f,5.f);
        path.AddLine(7.f,5.f,2.f,5.f);graphics.DrawPath(&pen,&path);
        line({{5, 2}, {2, 5}, {5, 8}});break;
    }
    case UiIcon::Affix:
        line({{5, 3}, {2, 3}, {2, 13}, {5, 13}});line({{11, 3}, {14, 3}, {14, 13}, {11, 13}});
        line({{7, 6}, {9, 6}});line({{7, 10}, {9, 10}});break;
    case UiIcon::Replace:
        line({{2, 5}, {14, 5}});line({{11, 2}, {14, 5}, {11, 8}});
        line({{14, 11}, {2, 11}});line({{5, 8}, {2, 11}, {5, 14}});break;
    case UiIcon::Info:
        graphics.DrawEllipse(&pen,2.f,2.f,12.f,12.f);line({{8, 7}, {8, 11}});
        graphics.DrawEllipse(&pen,7.6f,4.2f,.8f,.8f);break;
    case UiIcon::CheckCircle:
        graphics.DrawEllipse(&pen,1.5f,1.5f,13.f,13.f);line({{4.5f, 8}, {7, 10.5f}, {11.5f, 5.5f}});break;
    }
}
void paintTheme() {
    g->highlights.clear();
    g->dark = true;
    g->bg = RGB(22, 26, 33); g->field = RGB(16, 20, 27);
    g->surface = RGB(31, 37, 47); g->primary = RGB(68, 116, 193);
    g->fg = RGB(234, 238, 245); g->muted = RGB(148, 158, 175);
    g->line = RGB(43, 50, 62); g->accent = RGB(137, 172, 225);
    g->selection = RGB(41, 59, 83);
    if (g->brush)
        DeleteObject(g->brush);
    if (g->inputBrush)
        DeleteObject(g->inputBrush);
    g->brush = CreateSolidBrush(g->bg);
    g->inputBrush = CreateSolidBrush(g->field);
    BOOL dark = g->dark;
    DwmSetWindowAttribute(g->panel, 20, &dark, sizeof(dark));
    DWORD corners = 2;
    DwmSetWindowAttribute(g->panel, 33, &corners, sizeof(corners));
    COLORREF border = RGB(65, 73, 87);
    DwmSetWindowAttribute(g->panel, 34, &border, sizeof(border));
    updatePanelMaterial();
}
void fonts() {
    if (g->font)
        DeleteObject(g->font);
    if (g->smallFont)
        DeleteObject(g->smallFont);
    if (g->boldFont)
        DeleteObject(g->boldFont);
    if (g->bodyFont)
        DeleteObject(g->bodyFont);
    // Use the Windows Chinese UI sans-serif family consistently on every device.
    auto make = [](int size, int weight) {
        return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    };
    g->font = make(12, FW_NORMAL);
    g->smallFont = make(11, FW_NORMAL);
    g->boldFont = make(12, FW_MEDIUM);
    g->bodyFont = make(12, FW_NORMAL);
    if (g->savedNotice) {
        for (int id = 1; id <= 4; ++id)
            SendDlgItemMessageW(g->savedNotice, id, WM_SETFONT, reinterpret_cast<WPARAM>(id == 2 ? g->smallFont : g->font), TRUE);
    }
}
std::wstring previewName() {
    return g->stemName + (g->dialog.extension.empty() ? L"" : g->dialog.extension);
}
void syncNameParts() {
    auto prefix = g->parts.prefix, suffix = g->parts.suffix, body = g->stemName;
    if (!prefix.empty() && body.starts_with(prefix)) body.erase(0, prefix.size());
    else prefix.clear();
    if (!suffix.empty() && body.ends_with(suffix)) body.resize(body.size() - suffix.size());
    else suffix.clear();
    g->parts = splitName(body, g->cfg.modifiers);
    g->parts.prefix = prefix;
    g->parts.suffix = suffix;
}
void invalidateNaming() {
    g->saveAfterFill.reset();
    ++g->pasteId; g->pastePending = false;
    if (g->namingWorker) g->namingId = g->namingWorker->cancel();
    g->namingPending = g->namingSubmit = false; g->readyNaming.reset(); g->smartCacheKey.clear();
}
std::wstring effectiveVariables() {
    const auto *project = activeProject(g->cfg);
    return mergedVariables(g->sessionExpression.empty() ? g->cfg.variableValues : g->sessionVariables,
                           project ? project->variables : L"");
}
SmartNaming namingOptions() {
    SmartNaming options{g->numberMode, g->folderName,
        g->variablesEnabled ? (g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression) : L"",
        g->variablesEnabled ? effectiveVariables() : L""};
    options.dateFormat = g->sessionDateFormat < 0 ? g->cfg.dateFormat : g->sessionDateFormat;
    options.digits = g->sessionDigits ? g->sessionDigits : g->cfg.numberDigits;
    options.upperVersion = g->sessionDigits ? g->sessionUpper : g->cfg.upperVersion;
    return options;
}
std::wstring composedName(const std::wstring *targetDirectory = nullptr) {
    if (g->structure.enabled && !g->structure.free) {
        g->structure.dateFormat = namingOptions().dateFormat;
        auto composed = composeStructure(g->structure,g->cfg);
        if (!composed.error.empty()) { g->ruleError = composed.error; return g->stemName; }
        g->parts.base = composed.name;
        // A structured field owns stage/date; legacy modifiers cannot add them again.
        g->parts.stage.clear();g->parts.date.clear();
    }
    auto source = compose(g->parts);
    g->rulePreview = renamePreview(source, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex);
    g->ruleError = g->ruleEnabled ? g->rulePreview.error : L"";
    auto name = g->ruleEnabled && g->ruleError.empty() ? g->rulePreview.name : source;
    if (!g->ruleError.empty() || (!g->numberMode && !g->folderName && !g->variablesEnabled)) return name;
    auto options = namingOptions();
    const auto &directory = targetDirectory ? *targetDirectory : g->dialog.directory;
    auto key = name + L"\n" + directory + L"\n" + g->dialog.extension + L"\n" +
               std::to_wstring(options.counter) + (options.folder ? L"+" : L"-") + L"\n" +
               options.expression + L"\n" + options.variables + L"\n" + today(options.dateFormat) +
               std::to_wstring(options.digits) + (options.upperVersion ? L"V" : L"v");
    if (key != g->smartCacheKey) {
        g->smartCacheKey = key;
        if (options.counter && g->namingWorker) {
            auto lightweight = options; lightweight.counter = 0;
            auto preview = smartNamingPreview(name, directory, g->dialog.extension, lightweight);
            if (!preview.error.empty()) { g->smartPreview = preview; return g->ruleError = preview.error, name; }
            g->namingRequest = {0, g->generation.load(), key, name, directory, g->dialog.extension, g->stemName, options, false};
            g->namingId = g->namingWorker->request(g->namingRequest);
            g->namingPending = true; g->ruleError.clear();
            g->status = L"正在计算编号，可继续编辑…";
            return g->stemName;
        }
        g->smartPreview = smartNamingPreview(name, directory, g->dialog.extension, options);
    }
    if (g->namingPending) return g->stemName;
    g->ruleError = g->smartPreview.error;
    return g->ruleError.empty() ? g->smartPreview.name : name;
}
void resetSmartNaming() {
    invalidateNaming();
    g->numberMode = 0; g->folderName = g->variablesEnabled = false;
    g->sessionExpression.clear(); g->sessionVariables.clear(); g->sessionDigits = 0; g->sessionDateFormat = -1;
}
std::wstring variableKey(std::wstring key) {
    if (key == L"名称") return L"name";
    if (key == L"文件夹") return L"folder";
    if (key == L"日期") return L"date";
    return key;
}
std::wstring variableCaption(const std::wstring &key) {
    return key == L"name" ? L"原文件名" : key == L"folder" ? L"文件夹" : key == L"date" ? L"日期" : key;
}
bool builtInVariable(const std::wstring &key) { return key == L"name" || key == L"folder" || key == L"date"; }
std::vector<std::wstring> expressionKeys(const std::wstring &expression) {
    std::vector<std::wstring> keys;
    for (size_t at = 0; (at = expression.find(L'{', at)) != expression.npos;) {
        auto end = expression.find(L'}', at + 1); if (end == expression.npos) break;
        auto key = variableKey(expression.substr(at + 1, end - at - 1));
        if (!key.empty() && std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
        at = end + 1;
    }
    return keys;
}
std::map<std::wstring, std::wstring> variableValues(const std::wstring &text) {
    std::map<std::wstring, std::wstring> values;
    size_t at = 0;
    while (at < text.size()) {
        auto end = text.find(L'\n', at); if (end == text.npos) end = text.size();
        auto line = trim(text.substr(at, end - at)); auto equal = line.find(L'=');
        if (equal != line.npos) values[trim(line.substr(0, equal))] = trim(line.substr(equal + 1));
        at = end + 1;
    }
    return values;
}
void setVariableDraftValue(const std::wstring &key, const std::wstring &value) {
    // Replace only this field, retaining other values and any advanced draft text.
    auto &text = g->variableValuesDraft;
    for (size_t at = 0; at < text.size();) {
        auto end = text.find(L'\n', at); if (end == text.npos) end = text.size();
        auto line = trim(text.substr(at, end - at)); auto equal = line.find(L'=');
        if (equal != line.npos && trim(line.substr(0, equal)) == key) {
            bool cr = end > at && text[end - 1] == L'\r';
            text.replace(at, end - at, key + L"=" + value + (cr ? L"\r" : L"")); return;
        }
        at = end + 1;
    }
    if (!text.empty() && text.back() != L'\n') text += L"\r\n";
    text += key + L"=" + value;
}
void toggleVariableToken(const std::wstring &key) {
    auto &expression = g->variableTemplateDraft;
    bool removed = false;
    for (size_t at = 0; (at = expression.find(L'{', at)) != expression.npos;) {
        auto end = expression.find(L'}', at + 1); if (end == expression.npos) break;
        if (variableKey(expression.substr(at + 1, end - at - 1)) != key) { at = end + 1; continue; }
        size_t first = at, last = end + 1;
        if (last < expression.size() && expression[last] == L'_') ++last;
        else if (first && expression[first - 1] == L'_') --first;
        expression.erase(first, last - first); at = first; removed = true;
    }
    if (!removed) {
        if (!expression.empty() && expression.back() != L'_') expression += L"_";
        expression += L"{" + key + L"}";
    }
}

std::wstring affixLabel(std::wstring value) {
    size_t at = 0;
    while ((at = value.find(L"{date}", at)) != value.npos) {
        value.replace(at, 6, L"日期"); at += 2;
    }
    return value;
}
struct BubbleColorEntry {
    std::map<std::wstring, int> *colors = nullptr;
    std::wstring key;
};
BubbleColorEntry bubbleColorEntry(const ButtonAction &action) {
    if (action.kind == Action::Location && action.index < g->displayPaths.size())
        return {&g->cfg.locationColors, normalize(g->displayPaths[action.index].path)};
    if (action.kind == Action::Prefix && action.index < g->cfg.prefixes.size())
        return {&g->cfg.prefixColors, g->cfg.prefixes[action.index]};
    if (action.kind == Action::Suffix && action.index < g->cfg.suffixes.size())
        return {&g->cfg.suffixColors, g->cfg.suffixes[action.index]};
    return {};
}
int bubbleColorIndex(const ButtonAction &action) {

    auto entry = bubbleColorEntry(action);
    if (!entry.colors) return 0;
    auto found = entry.colors->find(entry.key);
    return found == entry.colors->end() ? 0 : std::clamp(found->second, 0, PRESET_COLOR_COUNT);
}
void moveAffixColor(bool prefix, const std::wstring &before, const std::wstring &after = L"") {
    if (before == after) return;
    auto &colors = prefix ? g->cfg.prefixColors : g->cfg.suffixColors;
    auto found = colors.find(before);
    if (found == colors.end()) return;
    int color = found->second;
    colors.erase(found);
    if (!after.empty()) colors[after] = color;
}
LRESULT CALLBACK PillProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
void invalidateSubjectGroup(HWND control);
COLORREF mixColor(COLORREF base, COLORREF tint, int percent) {
    return RGB((GetRValue(base) * (100 - percent) + GetRValue(tint) * percent) / 100,
               (GetGValue(base) * (100 - percent) + GetGValue(tint) * percent) / 100,
               (GetBValue(base) * (100 - percent) + GetBValue(tint) * percent) / 100);
}
COLORREF controlFill(COLORREF base, bool selected, bool hover, bool pressed, bool primary, bool enabled) {
    if (!enabled) return base;
    auto fill = primary ? g->primary : selected ? g->selection : base;
    if (pressed) return mixColor(fill, RGB(0, 0, 0), 12);
    return hover ? mixColor(fill, RGB(255, 255, 255), selected || primary ? 5 : 7) : fill;
}
constexpr UINT_PTR HOVER_TIMER = 0x4a50;
int hoverAmount(HWND h, bool hover) {
    auto motion = g->hoverMotions.find(h);
    if (motion == g->hoverMotions.end() || !g->animations) return hover ? 100 : 0;
    const auto &m = motion->second;
    double t = std::min(1.0, (GetTickCount64()-m.started)/140.0);
    return m.from + static_cast<int>((m.to-m.from)*t*(2-t));
}
struct LocationScrollState {
    bool hover = false, dragging = false;
    int grabOffset = 0, trackPosition = 0, repeatDirection = -1;
};
struct LocationScrollGeometry {
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    RECT client{}, thumb{};
    int maximum = 0, travel = 0;
};
LocationScrollGeometry locationScrollGeometry(HWND h) {
    LocationScrollGeometry layout;
    GetClientRect(h, &layout.client); GetScrollInfo(h, SB_CTL, &layout.info);
    auto &info = layout.info;
    int extent = std::max(1, info.nMax - info.nMin + 1);
    layout.maximum = std::max(info.nMin, info.nMax - static_cast<int>(info.nPage ? info.nPage - 1 : 0));
    int height = std::max(0L, layout.client.bottom - px(4));
    int thumbHeight = std::min(height, std::max(px(20), MulDiv(height, info.nPage, extent)));
    layout.travel = height - thumbHeight;
    int top = px(2) + (layout.maximum > info.nMin ?
        MulDiv(layout.travel, info.nPos - info.nMin, layout.maximum - info.nMin) : 0);
    layout.thumb = {0, top, layout.client.right, top + thumbHeight};
    return layout;
}
LRESULT CALLBACK LocationScrollProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    auto &state = *reinterpret_cast<LocationScrollState *>(data);
    auto notify = [&](int code) {
        SendMessageW(GetParent(h), WM_VSCROLL, MAKEWPARAM(code, state.trackPosition), reinterpret_cast<LPARAM>(h));
    };
    auto stop = [&]() {
        state.dragging = false; state.repeatDirection = -1;
        KillTimer(h, 1); InvalidateRect(h, nullptr, FALSE);
    };
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    case WM_PRINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(h, msg, wp); auto dc = paint.dc();
        auto layout = locationScrollGeometry(h);
        FillRect(dc, &layout.client, g->brush);
        int center = layout.client.right / 2;
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        auto trackBrush = CreateSolidBrush(g->line);
        auto oldBrush = SelectObject(dc, trackBrush);
        paintRoundedRect(dc, center - px(1), px(2), center + px(1), layout.client.bottom - px(2), px(2), px(2));
        bool active = state.hover || state.dragging || GetFocus() == h;
        int width = px(active ? 8 : 6);
        auto color = state.dragging || GetFocus() == h ? g->accent : mixColor(g->bg, g->muted, active ? 80 : 55);
        auto thumbBrush = CreateSolidBrush(IsWindowEnabled(h) ? color : g->line);
        SelectObject(dc, thumbBrush);
        paintRoundedRect(dc, center - width / 2, layout.thumb.top, center - width / 2 + width,
                         layout.thumb.bottom, width, width);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen);
        DeleteObject(trackBrush); DeleteObject(thumbBrush);
        return 0;
    }
    case SBM_SETSCROLLINFO: {
        // Keep native range/accessibility storage, but never invoke its light-theme painting.
        SCROLLINFO before{sizeof(before), SIF_RANGE | SIF_PAGE | SIF_POS}, after = before;
        GetScrollInfo(h, SB_CTL, &before);
        auto result = DefSubclassProc(h, msg, FALSE, lp);
        GetScrollInfo(h, SB_CTL, &after);
        if (before.nMin != after.nMin || before.nMax != after.nMax || before.nPage != after.nPage || before.nPos != after.nPos)
            InvalidateRect(h, nullptr, FALSE);
        return result;
    }
    case SBM_GETSCROLLINFO: {
        auto result = DefSubclassProc(h, msg, wp, lp);
        auto info = reinterpret_cast<SCROLLINFO *>(lp);
        if (result && state.dragging && (info->fMask & SIF_TRACKPOS)) info->nTrackPos = state.trackPosition;
        return result;
    }
    case WM_MOUSEMOVE: {
        if (!state.hover) {
            state.hover = true;
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
            InvalidateRect(h, nullptr, FALSE);
        }
        if (state.dragging && GetCapture() == h) {
            auto layout = locationScrollGeometry(h);
            int offset = std::clamp(GET_Y_LPARAM(lp) - state.grabOffset - px(2), 0, layout.travel);
            int position = layout.info.nMin + (layout.travel ?
                MulDiv(offset, layout.maximum - layout.info.nMin, layout.travel) : 0);
            if (position != state.trackPosition) { state.trackPosition = position; notify(SB_THUMBTRACK); }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        state.hover = false; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        if (!IsWindowEnabled(h)) return 0;
        auto layout = locationScrollGeometry(h);
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        state.trackPosition = layout.info.nPos;
        SetCapture(h);
        if (PtInRect(&layout.thumb, point)) {
            state.dragging = true; state.grabOffset = point.y - layout.thumb.top;
        } else {
            state.repeatDirection = point.y < layout.thumb.top ? SB_PAGEUP : SB_PAGEDOWN;
            SetTimer(h, 1, 400, nullptr); notify(state.repeatDirection);
        }
        InvalidateRect(h, nullptr, FALSE); return 0;
    }
    case WM_TIMER:
        if (wp == 1 && state.repeatDirection != -1 && GetCapture() == h) {
            SetTimer(h, 1, 70, nullptr);
            POINT point{}; GetCursorPos(&point); ScreenToClient(h, &point);
            auto layout = locationScrollGeometry(h);
            if (PtInRect(&layout.client, point) &&
                ((state.repeatDirection == SB_PAGEUP && point.y < layout.thumb.top) ||
                 (state.repeatDirection == SB_PAGEDOWN && point.y >= layout.thumb.bottom))) notify(state.repeatDirection);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state.dragging) notify(SB_THUMBPOSITION);
        stop(); if (GetCapture() == h) ReleaseCapture(); return 0;
    case WM_CANCELMODE:
        stop(); if (GetCapture() == h) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED: stop(); return 0;
    case WM_ENABLE:
        if (!wp) { stop(); if (GetCapture() == h) ReleaseCapture(); }
        InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_MOUSEWHEEL: return SendMessageW(GetParent(h), msg, wp, lp);
    case WM_KEYDOWN:
        if (!IsWindowEnabled(h)) return 0;
        switch (wp) {
        case VK_UP: notify(SB_LINEUP); return 0;
        case VK_DOWN: notify(SB_LINEDOWN); return 0;
        case VK_PRIOR: notify(SB_PAGEUP); return 0;
        case VK_NEXT: notify(SB_PAGEDOWN); return 0;
        case VK_HOME: notify(SB_TOP); return 0;
        case VK_END: notify(SB_BOTTOM); return 0;
        }
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(h, LocationScrollProc, 1);
        delete &state;
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
// Input frames belong to the parent, so invalidate only the frame when feedback
// changes. Do not repaint on every mouse move or change native text selection.
LRESULT CALLBACK InputFeedbackProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    auto before = feedback;
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        feedback |= 1;
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE || msg == WM_ENABLE) feedback &= ~DWORD_PTR(1);
    if (before != feedback) SetWindowSubclass(h, InputFeedbackProc, 1, feedback);
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (before != feedback || msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE) {
        RECT frame{}; GetWindowRect(h, &frame);
        MapWindowPoints(nullptr, GetParent(h), reinterpret_cast<POINT *>(&frame), 2);
        InflateRect(&frame, px(10), px(8));
        InvalidateRect(GetParent(h), &frame, FALSE);
        invalidateSubjectGroup(h);
    }
    return result;
}
COLORREF inputBorder(HWND parent, const RECT &frame) {
    auto h = ChildWindowFromPointEx(parent, {(frame.left + frame.right) / 2, (frame.top + frame.bottom) / 2},
                                    CWP_SKIPINVISIBLE);
    DWORD_PTR feedback = 0;
    if(h&&GetPropW(h,L"Jump.InputError"))return RGB(191,106,110);
    if (h && h != parent && GetWindowSubclass(h, InputFeedbackProc, 1, &feedback) && IsWindowEnabled(h)) {
        if (GetFocus() == h) return g->accent;
        if (feedback & 1) return mixColor(g->line, g->muted, 35);
    }
    return g->line;
}
void paintInputFrame(HDC dc, HWND parent, const RECT &frame, int radius) {
    auto pen = CreatePen(PS_SOLID, 1, inputBorder(parent, frame));
    auto op = SelectObject(dc, pen), ob = SelectObject(dc, g->inputBrush);
    paintRoundedRect(dc, frame.left, frame.top, frame.right, frame.bottom, radius, radius);
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
}
LRESULT CALLBACK FlyoutProc(HWND, UINT, WPARAM, LPARAM);
void closeFlyout(bool focusPanel = false);
void showPresetFlyout(HWND anchor = nullptr);
void showLocationFlyout(bool historyOnly = false);
void beginPathEdit();
void commitPathEdit();
void cancelPathEdit(bool restoreFocus = true);
void rebuildPanel();
void enterFreeNaming();
void resetStructuredInput();
HWND transientEditor();
bool insideTransientEditor(HWND target);
void resetEditorDismiss();
void requestEditorDismiss(bool explicitClick = false);
void finishEditorDismiss(HWND editor, bool explicitClick);
LRESULT CALLBACK TransientEditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
void restoreNamingState(const NamingSnapshot &state);
void updateStructuredContent();
std::wstring structuredInputError();
void reposition(bool show);
void updateInlineRule(bool immediate = false);
void showManager(bool present = true);

void rememberManagerPosition();
void buildManagerPage(bool preserveDraft = false);
void initializeVariableDraft();
void openVariableEditor(bool present = true);
void updateVariableDraftPreview();
void buildInlineTools(int &y, int available);
void buildNamingHint(int id,int x,int y,int width);
void updateInlineControls();
HWND createChild(HWND parent, const wchar_t *cls, const std::wstring &text, DWORD style, int id,
                 int x, int y, int width, int height, HFONT font) {
    auto control = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style,
        px(x), px(y), px(width), px(height), parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g->instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}
HWND child(const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y, int w, int h) {
    if (cls == std::wstring(L"BUTTON"))
        style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW | BS_NOTIFY;
    bool edit = cls == std::wstring(L"EDIT");
    bool staticText = cls == std::wstring(L"STATIC");
    DWORD textStyle = style;
    // Native STATIC also draws immediately on text/enable changes. Route every
    // path through our alpha-aware renderer, retaining the original text layout.
    if (staticText) style = (style & ~SS_TYPEMASK) | SS_OWNERDRAW;
    if (edit)
        g->fields.push_back({px(x), px(y), px(x + w), px(y + h)});
    HWND control = createChild(g->panel, cls, text, style, id, x + (edit ? 10 : 0),
        y + (edit ? 7 : 0), w - (edit ? 20 : 0), h - (edit ? 12 : 0), g->bodyFont);
    g->children.push_back(control);
    SetWindowSubclass(control, TransientEditProc, 4, 0);
    if (cls == std::wstring(L"BUTTON"))
        SetWindowSubclass(control, PillProc, 1, 0);
    if (edit) SetWindowSubclass(control, InputFeedbackProc, 1, 0);
    if (staticText) SetWindowSubclass(control, MaterialControlProc, 2, textStyle);
    if (edit || cls == std::wstring(MSFTEDIT_CLASS)) configureMaterialEditor(control);
    if (id == ID_FOLDER_SCROLL) {
        auto state = new LocationScrollState;
        if (!SetWindowSubclass(control, LocationScrollProc, 1, reinterpret_cast<DWORD_PTR>(state))) delete state;
    }
    return control;
}
void label(int x, int y, int w, const std::wstring &text, bool heading = false) {
    g->labels.push_back({{px(x), px(y), px(x + w), px(y + 22)}, text, heading});
}
void tip(HWND control, const std::wstring &text) {
    bool exists = g->tipStrings.contains(control);
    if (exists && g->tipStrings.at(control) == text) return;
    g->tipStrings[control] = text;
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = g->panel;
    info.uId = reinterpret_cast<UINT_PTR>(control);
    info.lpszText = g->tipStrings[control].data();
    SendMessageW(g->tooltip, exists ? TTM_UPDATETIPTEXTW : TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
}
LRESULT CALLBACK PillProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    int controlId=GetDlgCtrlID(h);
    // BS_NOTIFY turns the second fast press into BN_DOUBLECLICKED. Candidate
    // chips toggle per completed click, so keep native press/release handling
    // for that press too; location buttons retain their double-click Save.
    if(msg==WM_LBUTTONDBLCLK&&GetParent(h)==g->panel&&
       ((controlId>=ID_QUICK_PICK&&controlId<ID_QUICK_PICK+48)||
        (controlId>=ID_STRUCT_CHOICE&&controlId<ID_STRUCT_CHOICE+6)))msg=WM_LBUTTONDOWN;
    if (msg == WM_ERASEBKGND) return 1; // Owner drawing paints the complete button atomically.
    DWORD_PTR originalFeedback = feedback;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000))) feedback |= 2;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS) feedback &= ~DWORD_PTR(2);
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        feedback |= 1;
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE) feedback &= ~DWORD_PTR(1);
    if (feedback != originalFeedback) {
        if (((feedback ^ originalFeedback) & 1) && GetParent(h) == g->panel && g->animations && IsWindowVisible(h)) {
            int from = hoverAmount(h, (originalFeedback & 1) != 0);
            g->hoverMotions[h] = {from, feedback & 1 ? 100 : 0, GetTickCount64()};
            SetTimer(h, HOVER_TIMER, 15, nullptr);
        }
        SetWindowSubclass(h, PillProc, 1, feedback);
        InvalidateRect(h, nullptr, FALSE);
    }
    if (msg == WM_TIMER && wp == HOVER_TIMER) {
        InvalidateRect(h, nullptr, FALSE);
        auto motion = g->hoverMotions.find(h);
        if (motion == g->hoverMotions.end() || !g->animations || GetTickCount64()-motion->second.started >= 140) {
            KillTimer(h, HOVER_TIMER); g->hoverMotions.erase(h);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) { KillTimer(h, HOVER_TIMER); g->hoverMotions.erase(h); }
    if (msg == WM_MOUSEWHEEL && GetParent(h) != g->manager) return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN && GetParent(h) == g->panel) {
        int id = GetDlgCtrlID(h);
        if (g->editingPath && id != ID_DIRECTORY && id != ID_DIRECTORY_EDIT) cancelPathEdit(false);
        // These commands open an editing window. Activate on mouse-down, before
        // the button's focus/capture handling and the later window hand-off.
        if (id == ID_MANAGER || id == ID_NAME_MANAGER ||
            id == ID_RENAME || id == ID_DIRECTORY || id == ID_DIRECTORY_EDIT)
            SetForegroundWindow(g->panel);
    }
    if (msg == WM_CONTEXTMENU) {
        int id = GetDlgCtrlID(h);
        if (GetParent(h) == g->panel && ((id >= ID_QUICK_PICK && id < ID_QUICK_PICK + 48) ||
            (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6)))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
        auto action = g->actions.find(id);
        if (GetParent(h) == g->panel && action != g->actions.end() &&
            (action->second.kind == Action::Prefix ||
             action->second.kind == Action::Suffix || action->second.kind == Action::Location ||
             action->second.kind == Action::Variables))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    if(feedback!=originalFeedback||msg==WM_SETFOCUS||msg==WM_KILLFOCUS)invalidateSubjectGroup(h);
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK VariableEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) { SetForegroundWindow(GetAncestor(h, GA_ROOT)); SetFocus(h); }
    bool submit = GetDlgCtrlID(h) != ID_VARIABLE_VALUES;
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_ESCAPE || (submit && key->wParam == VK_RETURN)))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || (submit && wp == VK_RETURN))) {
        SendMessageW(GetParent(h), WM_COMMAND, wp == VK_ESCAPE ? ID_VARIABLE_CANCEL : ID_VARIABLE_SAVE, 0); return 0;
    }
    if (msg == WM_CHAR && (wp == VK_ESCAPE || (submit && wp == VK_RETURN))) return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
bool pasteTarget(HWND editor);
LRESULT CALLBACK PathEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_PASTE && pasteTarget(h)) return 0;
    if (msg == WM_MOUSEWHEEL) return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_KILLFOCUS) PostMessageW(g->panel, WM_PATH_BLUR, 0, 0);
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_RETURN) commitPathEdit(); else cancelPathEdit();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK PreviewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if(msg==WM_IME_STARTCOMPOSITION)SetPropW(h,L"Jump.IME",reinterpret_cast<HANDLE>(1));
    if(msg==WM_IME_ENDCOMPOSITION)RemovePropW(h,L"Jump.IME");
    if(GetPropW(h,L"Jump.IME")&&(msg==WM_KEYDOWN||msg==WM_CHAR))return DefSubclassProc(h,msg,wp,lp);
    if (msg == WM_PASTE && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT && pasteTarget(h)) return 0;
    if (msg == WM_MOUSEWHEEL) return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_GETDLGCODE && lp && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && key->wParam == VK_RETURN)
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (GetDlgCtrlID(h) == ID_PRESET_VALUE) {
        if (msg == WM_GETDLGCODE && lp) {
            auto key = reinterpret_cast<MSG *>(lp);
            if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
                return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
        }
        if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
            SendMessageW(g->panel, WM_COMMAND, wp == VK_RETURN ? ID_PRESET_SAVE : ID_PRESET_CANCEL, 0);
            return 0;
        }
    }
    if (msg == WM_LBUTTONDOWN) {
        if (g->editingPath) cancelPathEdit(false);
        closeFlyout();
        SetForegroundWindow(g->panel);
        SetFocus(h);
    }
    if (msg == WM_KEYDOWN && wp == VK_RETURN && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT) {
        SendMessageW(g->panel, WM_COMMAND, ID_SUBMIT, 0);
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SetForegroundWindow(g->dialog.hwnd);
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK InlineEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL) return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) {
        cancelPathEdit(false); closeFlyout(); SetForegroundWindow(g->panel); SetFocus(h);
    }
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_ESCAPE) {
            if(!structuredInputError().empty()){
                g->building=true;
                setTextIfChanged(GetDlgItem(g->panel,ID_AFFIX_PREFIX),g->parts.prefix);
                setTextIfChanged(GetDlgItem(g->panel,ID_AFFIX_SUFFIX),g->parts.suffix);
                g->building=false;g->statusError=false;
            }
            g->inlineTool = 0; rebuildPanel();
        }
        else if (GetDlgCtrlID(h) == ID_AFFIX_PREFIX || GetDlgCtrlID(h) == ID_AFFIX_SUFFIX)
            SendMessageW(g->panel, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), EN_CHANGE), reinterpret_cast<LPARAM>(h));
        else updateInlineRule();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK DropdownProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    auto setFeedback = [&](DWORD_PTR value) {
        if (feedback == value) return;
        feedback = value;
        SetWindowSubclass(h, DropdownProc, 1, value);
        InvalidateRect(h, nullptr, FALSE);
    };
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
        setFeedback(feedback | 1);
    }
    if (msg == WM_MOUSELEAVE) setFeedback(feedback & ~DWORD_PTR(1));
    if (msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) setFeedback(feedback | 2);
    if (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000)) setFeedback(feedback | 2);
    if (msg == WM_KILLFOCUS) setFeedback(feedback & ~DWORD_PTR(2));
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp); HDC dc = paint.dc();
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, g->brush);
        const bool expanded = (feedback & 4) != 0;
        const bool hover = (feedback & 1) != 0;
        const bool keyboardFocus = (feedback & 2) && GetFocus() == h;
        COLORREF fill = controlFill(expanded ? g->surface : g->field,
                                   false, hover, false, false, IsWindowEnabled(h));
        auto brush = CreateSolidBrush(fill);
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : fill);
        auto ob = SelectObject(dc, brush), op = SelectObject(dc, pen);
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(10), px(10));
        SelectObject(dc, op); SelectObject(dc, ob);
        DeleteObject(pen); DeleteObject(brush);
        auto font = SelectObject(dc, g->bodyFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g->fg);
        r.left += px(6);
        r.right -= px(22);
        auto text = windowText(h);
        DrawTextW(dc, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        r.left = r.right; r.right += px(22);
        drawUiIcon(dc, expanded ? UiIcon::ChevronUp : UiIcon::Chevron, (r.left + r.right - px(14)) / 2.f,
                   (r.top + r.bottom - px(14)) / 2.f, static_cast<float>(px(14)),
                   expanded ? g->accent : hover || keyboardFocus ? g->fg : g->muted);
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_LBUTTONDOWN) {
        setFeedback(feedback & ~DWORD_PTR(2));
        if (g->editingPath) cancelPathEdit(false);
        if (GetParent(h) == g->panel) closeFlyout();
        SetForegroundWindow(GetAncestor(h, GA_ROOT));
        SetFocus(h);
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(h, nullptr, FALSE);
    return result;
}
LRESULT drawDropdownItem(DRAWITEMSTRUCT item) {
    PaintBuffer buffer(item.hDC, item.rcItem); item.hDC = buffer.dc;
    auto brush = CreateSolidBrush(g->surface);
    FillRect(item.hDC, &item.rcItem, brush); DeleteObject(brush);
    if (item.itemID == static_cast<UINT>(-1)) return TRUE;
    auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
    if (length == CB_ERR) return TRUE;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(length));
    DWORD_PTR feedback = 0; GetWindowSubclass(item.hwndItem, DropdownProc, 1, &feedback);
    auto currentIndex = (feedback & 4) && (feedback >> 8) ? static_cast<LRESULT>((feedback >> 8) - 1) :
                        SendMessageW(item.hwndItem, CB_GETCURSEL, 0, 0);
    const bool current = currentIndex == item.itemID;
    bool hovered = false;
    COMBOBOXINFO combo{sizeof(combo)};
    if (GetComboBoxInfo(item.hwndItem, &combo) && IsWindowVisible(combo.hwndList)) {
        POINT mouse{}; GetCursorPos(&mouse); ScreenToClient(combo.hwndList, &mouse);
        hovered = PtInRect(&item.rcItem, mouse) != FALSE;
    }
    if ((item.itemState & ODS_SELECTED) && (!current || hovered)) {
        RECT hover = item.rcItem; InflateRect(&hover, -px(4), -px(2));
        auto fill = CreateSolidBrush(RGB(45, 56, 72));
        auto pen = CreatePen(PS_SOLID, 1, RGB(45, 56, 72));
        auto ob = SelectObject(item.hDC, fill), op = SelectObject(item.hDC, pen);
        paintRoundedRect(item.hDC, hover.left, hover.top, hover.right, hover.bottom, px(8), px(8));
        SelectObject(item.hDC, op); SelectObject(item.hDC, ob); DeleteObject(pen); DeleteObject(fill);
    }

    auto old = SelectObject(item.hDC, g->font);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, item.itemState & ODS_DISABLED ? g->muted :
                 g->fg);
    RECT textRect = item.rcItem; textRect.left += px(12); textRect.right -= px(30);
    DrawTextW(item.hDC, text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (current) {
        int size = px(14);
        drawUiIcon(item.hDC, UiIcon::Check, static_cast<float>(item.rcItem.right - px(24)),
                   (item.rcItem.top + item.rcItem.bottom - size) / 2.f, static_cast<float>(size), g->accent);
    }
    SelectObject(item.hDC, old);
    return TRUE;
}
LRESULT CALLBACK DropdownListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dimensions) {
    auto border = [&](HDC dc) {
        RECT r{}; GetClientRect(h, &r);
        auto pen = CreatePen(PS_SOLID, 1, g->line);
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(12), px(12));
        SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(pen);
    };
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        ClientPaint paint(h, msg, wp);
        RECT r{}; GetClientRect(h, &r);
        auto fill = CreateSolidBrush(g->surface); FillRect(paint.dc(), &r, fill); DeleteObject(fill);
        DefSubclassProc(h, WM_PRINT, reinterpret_cast<WPARAM>(paint.dc()), PRF_CLIENT | PRF_ERASEBKGND);
        border(paint.dc());
        return 0;
    }
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        int previous = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(h, L"Jump.PopupHotRow"))) - 1;
        auto hit = msg == WM_MOUSEMOVE ? SendMessageW(h, LB_ITEMFROMPOINT, 0, lp) : -1;
        int next = hit != -1 && !HIWORD(hit) ? LOWORD(hit) : -1;
        if (next != previous) {
            if (next < 0) RemovePropW(h, L"Jump.PopupHotRow");
            else SetPropW(h, L"Jump.PopupHotRow", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(next + 1)));
            for (int row : {previous, next}) {
                RECT r{};
                if (row >= 0 && SendMessageW(h, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&r)) != LB_ERR)
                    InvalidateRect(h, &r, FALSE);
            }
        }
        if (msg == WM_MOUSEMOVE && previous < 0) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
        }
    }
    if (msg == WM_NCDESTROY) RemovePropW(h, L"Jump.PopupHotRow");
    if (msg == WM_WINDOWPOSCHANGED) {
        RECT r{}; GetWindowRect(h, &r);
        int width = r.right - r.left, height = r.bottom - r.top;
        DWORD_PTR size = static_cast<DWORD_PTR>(MAKELONG(width, height));
        if (size != dimensions && width > 0 && height > 0) {
            SetWindowSubclass(h, DropdownListProc, 1, size);
            // Window owns the region after a successful SetWindowRgn call.
            auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, px(12), px(12));
            if (!SetWindowRgn(h, region, TRUE)) DeleteObject(region);
        }
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_PRINT || msg == WM_PRINTCLIENT) border(reinterpret_cast<HDC>(wp));
    return result;
}
void styleDropdownList(HWND combo) {
    COMBOBOXINFO info{sizeof(info)};
    if (!GetComboBoxInfo(combo, &info) || !info.hwndList) return;
    DWORD_PTR previous = 0;
    if (GetWindowSubclass(info.hwndList, DropdownListProc, 1, &previous)) return;
    SetWindowSubclass(info.hwndList, DropdownListProc, 1, 0);
    SetWindowTheme(info.hwndList, L"", L"");
    SetWindowLongPtrW(info.hwndList, GWL_STYLE, GetWindowLongPtrW(info.hwndList, GWL_STYLE) & ~WS_BORDER);
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE, GetWindowLongPtrW(info.hwndList, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    SetWindowPos(info.hwndList, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
LRESULT CALLBACK DropdownParentProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_DROPDOWN_COMMIT) {
        auto control = reinterpret_cast<HWND>(lp);
        DWORD_PTR feedback = 0;
        if (!IsWindow(control) || GetParent(control) != h || !GetWindowSubclass(control, DropdownProc, 1, &feedback) || !(feedback & 32))
            return 0;
        auto original = static_cast<LRESULT>(feedback >> 8) - 1;
        SetWindowSubclass(control, DropdownProc, 1, feedback & 3);
        InvalidateRect(control, nullptr, FALSE);
        if (!(feedback & 8) || (feedback & 16)) {
            if (original >= 0) SendMessageW(control, CB_SETCURSEL, original, 0);
            return 0;
        }
        if (SendMessageW(control, CB_GETCURSEL, 0, 0) != original)
            return DefSubclassProc(h, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(control), CBN_SELCHANGE), lp);
        return 0;
    }
    if (msg == WM_COMMAND && lp) {
        auto control = reinterpret_cast<HWND>(lp);
        DWORD_PTR feedback = 0;
        if (GetWindowSubclass(control, DropdownProc, 1, &feedback)) {
            DWORD_PTR originalFeedback = feedback;
            int code = HIWORD(wp);
            if (code == CBN_DROPDOWN) {
                styleDropdownList(control);
                auto selected = SendMessageW(control, CB_GETCURSEL, 0, 0);
                feedback = (feedback & 3) | 4 | (static_cast<DWORD_PTR>(selected + 1) << 8);
                if (GetKeyState(VK_LBUTTON) & 0x8000) feedback &= ~DWORD_PTR(2);
            }
            else if (code == CBN_SELCHANGE && (feedback & (4 | 32))) return 0;
            else if (code == CBN_SELENDOK && (feedback & (4 | 32))) feedback = (feedback | 8) & ~DWORD_PTR(16);
            else if (code == CBN_SELENDCANCEL && (feedback & (4 | 32))) feedback = (feedback | 16) & ~DWORD_PTR(8);
            else if (code == CBN_CLOSEUP && (feedback & 4)) {
                // Resolve after the complete native notification sequence; Windows
                // does not guarantee selection and close notification ordering.
                PostMessageW(h, WM_DROPDOWN_COMMIT, 0, lp);
                feedback = (feedback & ~DWORD_PTR(4)) | 32;
            }
            if (originalFeedback != feedback) {
                SetWindowSubclass(control, DropdownProc, 1, feedback);
                InvalidateRect(control, nullptr, FALSE);
            }
        }
    }
    if (msg == WM_DRAWITEM) {
        const auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (item.CtlType == ODT_COMBOBOX) return drawDropdownItem(item);
    }
    if (msg == WM_CTLCOLORLISTBOX && windowClass(reinterpret_cast<HWND>(lp)) == L"ComboLBox") {
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg); SetBkColor(reinterpret_cast<HDC>(wp), g->surface);
        static HBRUSH popupBrush = CreateSolidBrush(RGB(37, 46, 59));
        return reinterpret_cast<LRESULT>(popupBrush);
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void styleDropdown(HWND combo) {
    SetWindowSubclass(combo, DropdownProc, 1, 0);
    SetWindowSubclass(GetParent(combo), DropdownParentProc, 1, 0);
    styleDropdownList(combo);
}
void pill(const std::wstring &text, Action action, size_t index, int &x, int &y, int available,
          const std::wstring &tooltip, int left = 16, int fixedWidth = 0) {
    HDC dc = GetDC(g->panel);
    auto old = SelectObject(dc, g->bodyFont);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(g->panel, dc);
    int width = fixedWidth ? fixedWidth : std::clamp(MulDiv(size.cx, 96, g->dpi) + 24, 64, available);
    if (x + width > available + left) {
        x = left;
        y += 40;
    }
    int id = g->nextAction++;
    HWND h = child(L"BUTTON", text, BS_OWNERDRAW | BS_NOTIFY | WS_TABSTOP, id, x, y, width, 32);
    g->actions[id] = {action, index};
    SetWindowSubclass(h, PillProc, 1, 0);
    int color = bubbleColorIndex({action, index});
    tip(h, color ? tooltip + L"\n颜色标记：" + bubbleColors[color].name : tooltip);
    x += width + 8;
}
std::vector<Location> locationChoices() {
    const auto &source = g->locationTab == 4 ? g->cfg.recentSaves :
                         g->locationTab == 1 ? g->cfg.favorites : g->locationTab == 2 ? g->cfg.history : g->paths;
    std::vector<Location> locations;
    std::set<std::wstring> seen;
    if(g->locationTab==0&&g->dialog.save)for(const auto &location:namingRecommendedLocations(g->structure,g->cfg))
        if(seen.insert(normalize(location.path)).second)locations.push_back(location);
    for (const auto &location : source)
        if (!location.path.empty() && seen.insert(normalize(location.path)).second) locations.push_back(location);
    return locations;
}
int locationColumnCount(size_t count) {
    return count == 4 ? 2 : static_cast<int>(std::clamp<size_t>(count, 1, 3));
}
int locationRowCount(size_t count) {
    if (g->locationTab == 2) return static_cast<int>(std::clamp<size_t>(count, 1, 3));
    int columns = locationColumnCount(count);
    return std::clamp((static_cast<int>(count) + columns - 1) / columns, 1, 3);
}
void resizeFolderRows(int rows) {
    if (rows == g->folderRows) return;
    int cutoff = px(g->folderTop + g->folderRows * FOLDER_ROW_STEP), delta = px((rows - g->folderRows) * FOLDER_ROW_STEP);
    for (auto h : g->children) {
        int id = GetDlgCtrlID(h);
        if ((id >= ID_LOCATION_FIRST && id < ID_LOCATION_FIRST + LOCATION_PAGE_SIZE) ||
            id == ID_FOLDER_SCROLL || id == ID_LOCATION_EMPTY || id == ID_LOCATION_PAGE) continue;
        RECT r{}; GetWindowRect(h, &r); MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
        if (r.top >= cutoff) SetWindowPos(h, nullptr, r.left, r.top + delta, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    for (auto &r : g->fields) if (r.top >= cutoff) OffsetRect(&r, 0, delta);
    for (auto &label : g->labels) if (label.rect.top >= cutoff) OffsetRect(&label.rect, 0, delta);
    for (auto &y : g->dividers) if (y >= cutoff) y += delta;
    OffsetRect(&g->pathEditRect, 0, delta);
    g->height += (rows - g->folderRows) * FOLDER_ROW_STEP; g->folderRows = rows;
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    reposition(false);
    InvalidateRect(g->panel, nullptr, TRUE);
}
void updateLocationShortcuts() {
    if (g->dialog.generic && !g->dialog.genericNavigate) return;
    auto locations = locationChoices();
    bool history = g->locationTab == 2;
    int columns = locationColumnCount(locations.size());
    int totalRows = (static_cast<int>(locations.size()) + columns - 1) / columns;
    int rows = locationRowCount(locations.size());
    resizeFolderRows(rows);
    g->locationPage = history ? 0 : std::clamp(g->locationPage, 0, std::max(0, totalRows - rows));
    size_t first = static_cast<size_t>(g->locationPage * columns);
    size_t count = history ? 2 : rows * columns;
    std::vector<Location> items(locations.begin() + first, locations.begin() + std::min(first + count, locations.size()));
    bool changed = items.size() != g->displayPaths.size() ||
        !std::equal(items.begin(), items.end(), g->displayPaths.begin(),
                    [](const auto &a, const auto &b) { return a.name == b.name && a.path == b.path && a.sources == b.sources; });
    if (changed) {
        for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
            g->actions.erase(ID_LOCATION_FIRST + i);
            showIfChanged(GetDlgItem(g->panel, ID_LOCATION_FIRST + i), SW_HIDE);
        }
        g->displayPaths = std::move(items);
    }
    int available = g->width - PANEL_MARGIN * 2 - (!history && totalRows > 3 ? 20 : 0);
    for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
        int id = ID_LOCATION_FIRST + i;
        auto button = GetDlgItem(g->panel, id);
        if (!button) continue;
        bool present = static_cast<size_t>(i) < g->displayPaths.size();
        if (present) {
            const auto &location = g->displayPaths[i];
            auto name = location.name.empty() ? location.path : location.name;
            if (windowText(button) != name) setTextIfChanged(button, name.c_str());
            int left = history ? PANEL_MARGIN : PANEL_MARGIN + (available + 8) * (i % columns) / columns;
            int right = history ? PANEL_MARGIN + available : PANEL_MARGIN + (available + 8) * (i % columns + 1) / columns - 8;
            positionChild(button, px(left), px(g->folderTop + (history ? i : i / columns) * FOLDER_ROW_STEP), px(right - left), px(FOLDER_BUTTON_HEIGHT));
            g->actions[id] = {Action::Location, static_cast<size_t>(i)};
            tip(button, location.path + (location.sources.starts_with(L"命名推荐")?L"\n"+location.sources:L"") + (g->locationTab == 0 && location.sources.find(L"已打开") != std::wstring::npos ?
                L"\n已打开 · 资源管理器窗口中的文件夹" : L"") +
                L"\n单击切换目录；右键设置颜色；Ctrl + 单击直接保存");
        } else g->actions.erase(id);
        showIfChanged(button, present ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
    positionChild(scroll, px(g->width - PANEL_MARGIN - 16), px(g->folderTop), px(16), px(rows * FOLDER_ROW_STEP - 8));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0; info.nMax = std::max(0, totalRows - 1); info.nPage = rows; info.nPos = g->locationPage;
    SetScrollInfo(scroll, SB_CTL, &info, TRUE);
    enableIfChanged(scroll, !g->busy);
    showIfChanged(scroll, !history && totalRows > 3 ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto more = GetDlgItem(g->panel, ID_LOCATION_PAGE);
    positionChild(more, px(PANEL_MARGIN), px(g->folderTop + 2 * FOLDER_ROW_STEP), px(available), px(FOLDER_BUTTON_HEIGHT));
    setTextIfChanged(more, L"查看全部历史（" + std::to_wstring(locations.size()) + L"）…");
    showIfChanged(more, history && locations.size() > 2 ? SW_SHOWNOACTIVATE : SW_HIDE);
    const wchar_t *empty = g->locationTab == 1 ? L"暂无收藏 · 点击路径旁的 ☆ 添加" :
                          g->locationTab == 4 ? L"暂无最近保存记录" :
                          g->locationTab == 2 ? L"暂无浏览历史" : L"暂无位置，可点击下方路径输入目录";
    setTextIfChanged(GetDlgItem(g->panel,ID_LOCATION_EMPTY),empty);
    showIfChanged(GetDlgItem(g->panel, ID_LOCATION_EMPTY), locations.empty() ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto clearRecent = GetDlgItem(g->panel, ID_CLEAR_RECENT);
    showIfChanged(clearRecent, g->locationTab == 4 ? SW_SHOWNOACTIVATE : SW_HIDE);
    enableIfChanged(clearRecent, !g->busy && !g->cfg.recentSaves.empty());
    auto clearHistory = GetDlgItem(g->panel, ID_CLEAR_HISTORY);
    showIfChanged(clearHistory, history ? SW_SHOWNOACTIVATE : SW_HIDE);
    enableIfChanged(clearHistory, !g->busy && !g->cfg.history.empty());
}
#include "popup_menu.h"
#include "naming_feedback.h"
#include "experience_ui.h"
void updatePanelContent();
void apply(const std::wstring &,const std::wstring &,bool,bool);
void checkpointNaming();
#include "save_assist_ui.h"
void updatePanelContent() {
    g->building = true;
    if (g->dialog.generic) {
        auto directory = GetDlgItem(g->panel, ID_GENERIC_DIRECTORY);
        setTextIfChanged(directory, g->dialog.directory.empty() ? L"目录未确认 · 自动编号不可用" : L"目录：" + g->dialog.directory);
        if (directory) tip(directory, g->dialog.directory.empty() ? L"请在原软件选择目录；Jump 不猜测保存位置" : g->dialog.directory + L"\n切换目录请在原软件操作");
    }
    auto preview = GetDlgItem(g->panel, ID_PREVIEW);
    setTextIfChanged(preview,g->stemName+g->dialog.extension);
    if (preview) tip(preview, L"最终文件名：" + g->stemName + g->dialog.extension + L"\n结果预览；修改名称请使用上方字段或“命名设置”。");
    setTextIfChanged(GetDlgItem(g->panel,ID_FREE_NAME_INPUT),g->stemName);
    setTextIfChanged(GetDlgItem(g->panel, ID_DIRECTORY),
        g->dialog.directory.empty() ? (g->dialog.genericNavigate ? L"选择快捷位置或输入目录" : L"使用原软件当前目录") : g->dialog.directory.c_str());
    if (auto directory = GetDlgItem(g->panel, ID_DIRECTORY)) tip(directory,
        g->dialog.directory.empty() ? L"使用原软件当前目录" : g->dialog.directory);
    showIfChanged(GetDlgItem(g->panel, ID_DIRECTORY), g->editingPath ? SW_HIDE : SW_SHOWNOACTIVATE);
    auto pathInput = GetDlgItem(g->panel, ID_DIRECTORY_INPUT);
    if (pathInput && windowText(pathInput) != g->pathDraft) setTextIfChanged(pathInput, g->pathDraft.c_str());
    showIfChanged(pathInput, g->editingPath ? SW_SHOWNOACTIVATE : SW_HIDE);
    setTextIfChanged(GetDlgItem(g->panel,ID_DIRECTORY_EDIT),L"编辑路径");
    if (auto edit = GetDlgItem(g->panel, ID_DIRECTORY_EDIT)) {
        tip(edit, g->editingPath ? L"Enter 切换目录；点击别处或 Esc 退出编辑" : L"输入或粘贴其他文件夹路径");
        enableIfChanged(edit, !g->busy);
    }
    enableIfChanged(GetDlgItem(g->panel, ID_DIRECTORY_SEARCH), !g->busy && !g->pastePending);
    InvalidateRect(g->panel, &g->pathEditRect, FALSE);
    bool saveError=g->settingsDirty&&!g->configPending;
    auto status = g->status + (saveError ? L" · 配置未保存" : L"");
    setTextIfChanged(GetDlgItem(g->panel, ID_STATUS), status.c_str());
    showIfChanged(GetDlgItem(g->panel, ID_STATUS), g->statusError || saveError || g->namingPending || g->pastePending ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (auto control = GetDlgItem(g->panel, ID_STATUS)) tip(control, saveError ? g->status +
        L"\n预设和最近记录尚未写入配置；恢复权限后会在下一次操作重试。\n" + g->configFile.wstring() : g->status);
    bool favorite = std::any_of(g->cfg.favorites.begin(), g->cfg.favorites.end(), [](const auto &p) {
        return normalize(p.path) == normalize(g->dialog.directory);
    });
    setTextIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), favorite ? L"已收藏" : L"＋收藏");
    enableIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), !g->busy && !g->dialog.directory.empty());
    if (auto control = GetDlgItem(g->panel, ID_FAVORITE_CURRENT)) tip(control, favorite ? L"取消收藏当前目录" : L"收藏当前目录");
    updateLocationShortcuts();
    updateInlineControls();
    updateStructuredContent();
    updateExperienceContent();
    updateSaveAssist();
    updatePanelStatusLayout();
    enableIfChanged(GetDlgItem(g->panel, ID_SUBMIT), (!g->busy||(!g->busySubmitting&&!g->dialog.generic)) && !g->pastePending && !g->structuredIme && structuredInputError().empty() && g->ruleError.empty() && validName(g->stemName));
    enableIfChanged(GetDlgItem(g->panel, ID_UNDO), !g->busy && g->dirtyName);
    if (auto submit = GetDlgItem(g->panel, ID_SUBMIT)) tip(submit, g->status +
        (g->dialog.generic ? L"\n填入名称后，在原软件中确认保存" : L"\n确认文件名与位置后，提交原软件保存"));
    setTextIfChanged(g->panel, (L"Jump 保存助手 · " + std::wstring(status)).c_str());
    g->building = false;
    for (auto h : g->children) if (windowClass(h) == L"Button") InvalidateRect(h, nullptr, FALSE);
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
    RECT current{}; GetWindowRect(g->panel, &current);
    bool moved = current.left != x || current.top != y;
    bool resized = current.right - current.left != width || current.bottom - current.top != height;
    bool topmost = (GetWindowLongPtrW(g->panel, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (moved || resized || !topmost || (show && !IsWindowVisible(g->panel)))
        SetWindowPos(g->panel, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE |
            (moved ? 0 : SWP_NOMOVE) | (resized ? 0 : SWP_NOSIZE) | (show ? SWP_SHOWWINDOW : 0));
    g->lastTargetRect = r;
}
void updateInlineControls() {
    if (g->inlineTool == 2) {
        for (int id : {ID_AFFIX_PREFIX, ID_AFFIX_SUFFIX}) {
            auto input = GetDlgItem(g->panel, id);
            const auto &value = id == ID_AFFIX_PREFIX ? g->parts.prefix : g->parts.suffix;
            auto draft=windowText(input);
            if (input && GetFocus() != input && draft != value && (draft.empty()||validAffix(draft,id==ID_AFFIX_PREFIX))) SetWindowTextW(input, value.c_str());
        }
    }
    if (g->inlineTool != 3) return;
    auto sourceText = compose(g->parts);
    g->rulePreview = renamePreview(sourceText, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex);
    auto source = GetDlgItem(g->panel, ID_RULE_SOURCE);
    setHighlightedText(source, sourceText, g->rulePreview.matches);
    SetDlgItemTextW(g->panel, ID_RULE_ENABLE, g->ruleEnabled ? L"已启用" : L"启用");
    auto message = !g->rulePreview.error.empty() ? g->rulePreview.error :
        L"匹配 " + std::to_wstring(g->rulePreview.matches.size()) + L" 处 · " +
        (g->ruleEnabled ? L"规则已启用，收起后保留" : L"规则未启用");
    SetDlgItemTextW(g->panel, ID_RULE_HINT, message.c_str());
    showIfChanged(GetDlgItem(g->panel,ID_RULE_HINT),g->rulePreview.error.empty()?SW_SHOWNOACTIVATE:SW_HIDE);
    SetDlgItemTextW(g->panel, ID_RULE_RESULT, (L"→ " + g->stemName + g->dialog.extension).c_str());
}
void buildInlineTools(int &y, int available) {
    g->dividers.push_back(px(y - 4));
    int cell = (available - 16) / 3;
    if (g->inlineTool == 2) {
        for (bool prefix : {true, false}) {
            label(16, y + 4, 36, prefix ? L"前缀" : L"后缀");
            auto input = child(L"EDIT", prefix ? g->parts.prefix : g->parts.suffix, ES_AUTOHSCROLL | WS_TABSTOP,
                               prefix ? ID_AFFIX_PREFIX : ID_AFFIX_SUFFIX, 52, y, available - 72, 32);
            SetWindowSubclass(input, InlineEditProc, 1, 0);
            SendMessageW(input, EM_SETLIMITTEXT, 80, 0);
            SendMessageW(input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"自由输入，留空停用"));
            auto add = child(L"BUTTON", L"＋", WS_TABSTOP, prefix ? ID_ADD_PREFIX : ID_ADD_SUFFIX,
                             g->width - 44, y, 28, 28);
            tip(add, L"搜索全部或添加预设");
            y += 36;
            buildNamingHint(prefix?ID_PREFIX_ERROR:ID_SUFFIX_ERROR,52,y,available-36);
            int x = 16, row = y;
            const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
            for (size_t i = 0; i < values.size() && i < 3; ++i)
                pill(affixLabel(values[i]), prefix ? Action::Prefix : Action::Suffix, i, x, row, available,
                     L"单击启用，再点取消；右键编辑或换色", 16, cell);
            y += 40;
        }
    } else if (g->inlineTool == 3) {
        auto combo = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS |
                           WS_TABSTOP | WS_VSCROLL, ID_RULE_MODE, 16, y, available - 124, 240);
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
        auto pattern = child(L"EDIT", g->rulePattern, ES_AUTOHSCROLL | WS_TABSTOP, ID_RULE_PATTERN,
                             16, y, (available - 8) / 2, 32);
        auto replacement = child(L"EDIT", g->ruleReplacement, ES_AUTOHSCROLL | WS_TABSTOP, ID_RULE_REPLACEMENT,
                                 24 + (available - 8) / 2, y, (available - 8) / 2, 32);
        SendMessageW(pattern, EM_SETLIMITTEXT, 160, 0); SendMessageW(replacement, EM_SETLIMITTEXT, 240, 0);
        SetWindowSubclass(pattern, InlineEditProc, 1, 0); SetWindowSubclass(replacement, InlineEditProc, 1, 0);
        SendMessageW(pattern, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"查找文字 / 正则表达式"));
        SendMessageW(replacement, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"替换为，留空即删除"));
        y += 40;
        buildNamingHint(ID_RULE_ERROR,16,y,available);
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
        if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT+8) || focusId == ID_STRUCT_INPUT || focusId == ID_FREE_NAME_INPUT || focusId == ID_DIRECTORY_INPUT || focusId == ID_RULE_PATTERN || focusId == ID_RULE_REPLACEMENT ||
            focusId == ID_VARIABLE_TEMPLATE || (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
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
        g->width = g->bottom ? std::clamp(MulDiv(target.right - target.left, 96, g->dpi), 648, 760) : PANEL_WIDTH;
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
        const std::pair<int, const wchar_t *> tabs[] = {
            {ID_LOC_RECOMMENDED, L"推荐"}, {ID_LOC_FAVORITES, L"收藏"}, {ID_LOC_RECENT, L"最近"},
            {ID_LOC_HISTORY, L"历史"}
        };
        constexpr int tabCount = static_cast<int>(std::size(tabs));
        g->locationTabsRect = {px(m), px(y), px(m + avail), px(y + 28)};
        for (int i = 0; i < tabCount; ++i) {
            int left = m + 2 + (avail - 4) * i / tabCount;
            int right = m + 2 + (avail - 4) * (i + 1) / tabCount;
            auto tab = child(L"BUTTON", tabs[i].second, WS_TABSTOP, tabs[i].first, left + 1, y + 2, right - left - 2, 24);
            if (tabs[i].first == ID_LOC_RECOMMENDED) tip(tab, L"推荐位置与资源管理器中已打开的文件夹；相同路径只显示一次");
            if (tabs[i].first == ID_LOC_RECENT) tip(tab, L"已确认文件保存成功的位置；仅浏览或取消保存不会计入");
            if (tabs[i].first == ID_LOC_HISTORY) tip(tab, L"在文件窗口浏览或跳转过的目录；未保存或取消保存也会记录");
        }
        y = 68; g->folderTop = y;
        size_t locationCount = locationChoices().size();
        int columns = locationColumnCount(locationCount);
        g->folderRows = locationRowCount(locationCount);
        for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
            int left = m + (avail + 8) * (i % columns) / columns, right = m + (avail + 8) * (i % columns + 1) / columns - 8;
            child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_FIRST + i, left, y + i / columns * FOLDER_ROW_STEP,
                right - left, FOLDER_BUTTON_HEIGHT);
        }
        child(L"SCROLLBAR", L"", SBS_VERT | WS_TABSTOP, ID_FOLDER_SCROLL, w - m - 16, y, 16, g->folderRows * FOLDER_ROW_STEP - 8);
        auto historyMore = child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_PAGE, m, y, avail, FOLDER_BUTTON_HEIGHT);
        tip(historyMore, L"按名称或完整路径搜索浏览历史；记录按最近访问排序");
        auto empty = child(L"STATIC", L"", SS_LEFT, ID_LOCATION_EMPTY, m, y, avail, FOLDER_BUTTON_HEIGHT);
        SendMessageW(empty, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
        y += g->folderRows * FOLDER_ROW_STEP + 2;
        auto directory = child(L"BUTTON", L"", WS_TABSTOP, ID_DIRECTORY, m, y, avail - 82, 24);
        tip(directory, L"当前路径；点击编辑（Ctrl + L）");
        g->pathEditRect = {px(m), px(y), px(m + avail - 82), px(y + 24)};
        auto input = CreateWindowExW(0, L"EDIT", g->pathDraft.c_str(), WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
            px(m + 4), px(y + 2), px(avail - 90), px(20), g->panel,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIRECTORY_INPUT)), g->instance, nullptr);
        g->children.push_back(input);
        SetWindowSubclass(input, TransientEditProc, 4, 0);
        SetWindowSubclass(input, InputFeedbackProc, 1, 0);
        configureMaterialEditor(input);
        SendMessageW(input, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
        SendMessageW(input, EM_SETLIMITTEXT, 32767, 0);
        SendMessageW(input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"输入或粘贴文件夹路径，Enter 切换"));
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
    if(g->dialog.save&&(!g->dialog.generic||g->dialog.genericNavigate)){
        auto route=child(L"BUTTON",L"",WS_TABSTOP,ID_ROUTE_SUGGEST,m,y,avail,0);showIfChanged(route,SW_HIDE);
    }
    g->dividers.push_back(px(y)); y += 8;
    if (g->dialog.save) {
        if(!g->structure.enabled){g->structure=basicStructure(g->stemName,&g->cfg);g->structure.literal=true;g->structure.literalName=g->stemName;}
        buildStructuredControls(y,avail);
        buildNamingActions(y,avail);
        g->dividers.push_back(px(y+2));y+=10;
        auto finalLabel=child(L"STATIC",L"最终文件名",SS_LEFT,ID_FINAL_NAME_LABEL,m,y,avail,18);
        SendMessageW(finalLabel,WM_SETFONT,reinterpret_cast<WPARAM>(g->smallFont),TRUE);y+=20;
        g->structuredFullHeight=structuredPreviewHeight(g->stemName+g->dialog.extension,avail);
        child(L"STATIC",g->stemName+g->dialog.extension,SS_LEFT,ID_PREVIEW,m,y,avail,g->structuredFullHeight);
        y+=g->structuredFullHeight+6;
        auto targetInfo=child(L"BUTTON",L"",WS_TABSTOP,ID_TARGET_INFO,m,y,avail,0);showIfChanged(targetInfo,SW_HIDE);
        auto status = child(L"STATIC", g->status, SS_LEFT | SS_ENDELLIPSIS, ID_STATUS, m, y + 3, avail, 16);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
    } else y += 8;
    auto capabilities=child(L"BUTTON",L"",WS_TABSTOP,ID_CAPABILITIES,m,y,avail,24);
    tip(capabilities,L"查看当前窗口可以操作的内容、限制及下一步。");y+=28;
    g->height = y;
    g->building = false;
    updatePanelContent();
    SendMessageW(g->panel, WM_SETREDRAW, TRUE, 0);
    if (!wasVisible)
        ShowWindow(g->panel, SW_HIDE);
    // WM_SETREDRAW suppresses child painting during reconstruction too. Refresh
    // the complete subtree so old field highlights and candidate captions cannot remain.
    RedrawWindow(g->panel,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
    reposition();
    if (focusId) {
        auto edit = GetDlgItem(g->panel, focusId);
        if (edit) {
            SetFocus(edit);
            if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT+8) || focusId == ID_STRUCT_INPUT || focusId == ID_FREE_NAME_INPUT || focusId == ID_PRESET_VALUE || focusId == ID_VARIABLE_TEMPLATE ||
                (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
                SendMessageW(edit, EM_SETSEL, selectionStart, selectionEnd);
        }
    }
}
bool persist() {
    g->settingsDirty = !(g->configWriter?g->configWriter->saveNow(g->cfg):jump::save(g->cfg, g->configFile));
    g->configPending=false;
    return !g->settingsDirty;
}
void persistDeferred(){
    if(!g->configWriter){persist();return;}
    g->settingsDirty=true;g->configPending=true;g->configWriter->request(g->cfg);
}
bool flushPersistence(){
    if(g->settingsDirty)return persist();
    return !g->configWriter||g->configWriter->flush();
}
bool setBubbleColor(const ButtonAction &action, int color) {
    if (color < 0 || color > PRESET_COLOR_COUNT) return false;
    auto previous = g->cfg;
    {
        auto entry = bubbleColorEntry(action);
        if (!entry.colors) return false;
        if (color) (*entry.colors)[entry.key] = color;
        else entry.colors->erase(entry.key);
    }
    bool saved = persist();
    if (!saved) g->cfg = std::move(previous);
    g->status = !saved ? L"颜色未保存，请检查配置写入权限" :
                color ? L"已设置颜色标记" : L"已清除颜色标记";
    rebuildPanel();
    return saved;
}
void openInlinePreset(size_t index = NEW_PRESET, HWND anchor = nullptr) {
    closeFlyout();
    std::wstring value;
    if (index != NEW_PRESET) {
        const auto &values=g->namingMode==1?g->cfg.prefixes:g->cfg.suffixes;
        if(index>=values.size())return;value=values[index];
    }
    g->presetEditor = true;
    g->presetIndex = index;
    g->presetDraft = value;
    KillTimer(g->control, T_ACTION);
    showPresetFlyout(anchor);
}
void saveInlinePreset() {
    if (!g->presetEditor) return;
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
        if (duplicate != values.end() && (added || static_cast<size_t>(duplicate - values.begin()) != g->presetIndex)) {
            error(L"已有相同预设"); return;
        }
        if (added) {
            if (values.size() >= 100) { error(L"最多 100 个预设"); return; }
            values.insert(values.begin(), value);
        } else {
            if (g->presetIndex >= values.size()) { error(L"预设已变化，请重新选择"); return; }
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

    g->status = added ? L"已新增预设，点泡泡即可使用" : L"已修改预设，当前文件名保持不变";
    rebuildPanel();
    if (IsWindowVisible(g->panel))
        SetFocus(GetDlgItem(g->panel,g->namingMode==1?ID_ADD_PREFIX:ID_ADD_SUFFIX));
}
// Exercise the actual panel commands with a hidden window and isolated settings.
// This does not create a host dialog, start the worker, or change foreground focus.
bool hiddenCheckFailure(const char *name, int line) {
    std::fprintf(stderr, "Hidden check failed: %s, main.cpp:%d\n", name, line);
    auto message = std::string(name) + ", main.cpp:" + std::to_string(line);
    auto file = CreateFileW((g->configFile.parent_path() / L"failure.txt").c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written{}; WriteFile(file, message.data(), static_cast<DWORD>(message.size()), &written, nullptr); CloseHandle(file);
    }
    return false;
}

void rememberCurrentDirectory() {
    const auto &directory = g->dialog.directory;
    if (directory.empty()) return;
    // Polling the same directory is not a new visit, especially after removal.
    if (normalize(g->lastHistoryDirectory) == normalize(directory)) return;
    g->lastHistoryDirectory = directory;
    if (!g->cfg.history.empty() && normalize(g->cfg.history.front().path) == normalize(directory)) {
        if (g->settingsDirty) persistDeferred();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberLocation(g->cfg, {name.empty() ? directory : name, directory, L"历史"});
    persistDeferred();
}
void verifySavedFile(const SaveProbe &probe) {
    if (probe.file.empty()) return;
    Job job;
    job.type = JobType::VerifySave;
    job.probe = probe;
    enqueue(std::move(job));
}
void armSaveChecks() {
    KillTimer(g->control, T_SAVE_VERIFY);
    if (g->saveChecks.empty()) return;
    auto earliest = std::min_element(g->saveChecks.begin(), g->saveChecks.end(),
        [](const Job &a, const Job &b) { return a.verifyAt < b.verifyAt; })->verifyAt;
    auto now = GetTickCount64();
    SetTimer(g->control, T_SAVE_VERIFY, static_cast<UINT>(earliest > now ? earliest - now : 1), nullptr);
}
void rememberSavedDirectory(const std::wstring &directory) {
    if (directory.empty()) return;
    if (!g->cfg.recentSaves.empty() && normalize(g->cfg.recentSaves.front().path) == normalize(directory)) {
        if (g->settingsDirty) persistDeferred();
        if (g->attached) updatePanelContent();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberSavedLocation(g->cfg, {name.empty() ? directory : name, directory, L"最近保存"});
    persistDeferred();
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    if (g->attached && !g->busy && GetForegroundWindow() != g->panel) rebuildPanel();
}
void apply(const std::wstring &path, const std::wstring &name, bool submit, bool explicitGeneric = false) {
    if(g->structuredIme||!structuredInputError().empty())return;
    if (g->pastePending) return;
    // Generic label clicks and Enter remain fill-only. Invoking Save requires its explicit menu command.
    if (g->dialog.generic && !explicitGeneric) submit = false;
    if(!g->attached)return;
    if(g->busy){
        // Explicit Save during a name fill is serialized behind that fill.
        // Further edits invalidate this intent; background fills never create it.
        if(submit&&!g->busySubmitting&&g->ruleError.empty()&&validName(name))
            g->saveAfterFill=Job{JobType::Apply,g->dialog.hwnd,g->dialog.pid,g->generation.load(),path,name,true};
        return;
    }
    if (g->numberMode && g->namingWorker && !g->numberingApproved) {
        if (submit) {
            auto displayed = g->stemName;
            invalidateNaming(); composedName(&path);
            if (!g->ruleError.empty()) { g->status = g->ruleError; g->statusError = true; updatePanelContent(); return; }
            g->namingRequest.submit = true; g->namingRequest.displayed = displayed;
            g->namingId = g->namingWorker->request(g->namingRequest);
            g->namingPending = g->namingSubmit = true;
            g->status = L"保存前重新核验编号…修改命名会取消本次提交";
            updatePanelContent(); return;
        }
        if (g->namingPending) { updatePanelContent(); return; }
    }
    if (!g->ruleError.empty()) { g->status = g->ruleError; updatePanelContent(); return; }
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
    g->busySubmitting=submit;
    g->localNamePending = true;
    g->applyFocus = GetForegroundWindow() == g->panel ? GetFocus() : nullptr;
    g->status = submit ? L"正在核验并提交保存…" : L"正在填入…";
    updatePanelContent();
    Job job{JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path, name, submit};
    job.namingStructure=savedStructure();job.namingProject=g->cfg.activeProject;
    // Transfer focus from the foreground UI thread, before the UIA worker needs
    // to press Enter in a custom address editor.
    if (g->dialog.genericAddress && !path.empty() && normalize(path) != normalize(g->dialog.directory) &&
        focusAllowsAction(job)) SetForegroundWindow(job.hwnd);
    enqueue(job);
    for (auto h : g->children)
        if ((windowClass(h) == L"Button" && GetDlgCtrlID(h) != ID_CLOSE && !liveStructuredControl(GetDlgCtrlID(h)) &&
             !((GetDlgCtrlID(h)>=ID_LOC_RECOMMENDED&&GetDlgCtrlID(h)<=ID_LOC_HISTORY)||GetDlgCtrlID(h)==ID_LOC_RECENT) &&
             !(GetDlgCtrlID(h)==ID_SUBMIT&&!submit&&!g->dialog.generic) &&
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
    if ((!drive && !network) || value.find_first_of(L"\r\n\t\"<>|*?") != value.npos) return {};
    return value;
}
void beginPathEdit() {
    if (g->busy) return;
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
        SetFocus(input); SendMessageW(input, EM_SETSEL, 0, -1);
    }
}
void cancelPathEdit(bool restoreFocus) {
    if (!g->editingPath) return;
    if (g->pastePending) invalidateNaming();
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
    if (!g->editingPath || g->busy) return;
    g->pathDraft = windowText(GetDlgItem(g->panel, ID_DIRECTORY_INPUT));
    auto path = pathEditValue(g->pathDraft);
    if (path.empty()) {
        g->status = L"请输入完整文件夹路径，例如 C:\\Users\\用户名\\Downloads";
        updatePanelContent(); return;
    }
    g->editingPath = false;
    if (normalize(path) == normalize(g->dialog.directory)) {
        g->status = L"已在该目录"; updatePanelContent();
        if (g->attached && g->dirtyName && g->localNamePending)
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
        return;
    }
    if (g->numberMode || g->folderName || g->variablesEnabled) {
        g->stemName = composedName(&path);
        if (!g->ruleError.empty()) { g->status = g->ruleError; g->statusError = true; updatePanelContent(); return; }
    }
    apply(path, g->stemName, false);
    g->pathPending = g->busy;
    g->applyFocus = nullptr;
    // A counter scan continues this navigation asynchronously. Reopening the
    // editor here races its queued blur and can replace the result with "cancelled".
    if (!g->busy && !g->namingPending) { g->editingPath = true; updatePanelContent(); }
}
#include "paste_target_ui.h"
NamingSnapshot captureNaming() {
    auto options = namingOptions();
    auto parts=g->parts;
    // Derived output is recomputed on restore and must not create phantom undo steps.
    if(g->structure.enabled&&!g->structure.free){parts.base.clear();parts.stage.clear();parts.date.clear();}
    return {parts, options.expression, options.variables,
        g->rulePattern, g->ruleReplacement, g->numberMode, options.digits, options.dateFormat, g->ruleIndex,
        g->folderName, g->variablesEnabled, options.upperVersion, g->ruleEnabled, g->ruleRegex, g->structure,
        g->freeRecovery?std::make_shared<NamingSnapshot>(*g->freeRecovery):nullptr};
}
void checkpointNaming() { g->namingHistory.checkpoint(captureNaming());g->structuredCheckpoint=false; }
void moveNamingHistory(bool redo) {
    if (!g->attached || g->busy || composingNaming()) return;
    auto value = g->namingHistory.move(captureNaming(), redo); if (!value) return;
    restoreNamingState(*value);
    g->stemName = composedName(); g->dirtyName = g->localNamePending = true;
    g->statusError = !g->ruleError.empty();
    g->status = g->statusError ? g->ruleError : redo ? L"已重做命名组合" : L"已撤销命名组合";
    rebuildPanel();
    if (g->ruleError.empty()) apply(g->dialog.directory, g->stemName, false);
}
Recipe captureRecipe(const std::wstring &name) {
    Recipe recipe{name, g->dialog.directory, g->parts.base, false, !g->parts.date.empty()};
    recipe.complete = true;
    if(g->structure.enabled&&!g->structure.free){auto structure=g->structure;structure.counter=g->numberMode;
        structure.digits=namingOptions().digits;structure.dateFormat=namingOptions().dateFormat;structure.upper=namingOptions().upperVersion;
        recipe.structure=serializeStructure(structure);}
    recipe.prefix = g->parts.prefix; recipe.suffix = g->parts.suffix; recipe.stageValue = g->parts.stage;
    for (const auto &preset : g->cfg.prefixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.prefix) { recipe.prefix = preset; break; }
    for (const auto &preset : g->cfg.suffixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.suffix) { recipe.suffix = preset; break; }
    auto options = namingOptions();
    recipe.counter = options.counter; recipe.folder = options.folder; recipe.variables = g->variablesEnabled;
    recipe.expression = options.expression;
    // Project values stay with their project. A recipe stores only the session/default values.
    recipe.variableValues = g->sessionExpression.empty() ? g->cfg.variableValues : g->sessionVariables;
    recipe.digits = options.digits; recipe.upperVersion = options.upperVersion; recipe.dateFormat = options.dateFormat;
    recipe.ruleEnabled = g->ruleEnabled; recipe.ruleRegex = g->ruleRegex;
    recipe.pattern = g->rulePattern; recipe.replacement = g->ruleReplacement;
    if (const auto *project = activeProject(g->cfg)) {
        auto relative = std::filesystem::path(recipe.directory).lexically_relative(project->root).wstring();
        if (!relative.empty() && !projectPath(*project, relative).empty()) {
            recipe.directory = relative; recipe.relativeDirectory = true;
        }
    }
    return recipe;
}
void perform(ButtonAction action, bool quick) {
    if (!g->attached)
        return;
    bool affix = action.kind == Action::Prefix || action.kind == Action::Suffix;
    bool smart = action.kind == Action::Folder || action.kind == Action::Variables;
    if (affix || smart) quick = false;
    bool localNaming = affix || smart;
    if (g->busy && (quick || !localNaming)) {
        if (quick && action.kind == g->lastAction.kind && action.index == g->lastAction.index)
            g->pendingQuick = true;
        return;
    }
    // Compose local clicks even while an older fill is in flight. WM_RESULT
    // already keeps the newer stem and sends the latest name after that fill.
    if (g->busy && localNaming) g->pendingQuick = false;
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
        g->folderName = !g->folderName; updateName = true; quick = false;
        break;
    case Action::Variables:
        if (!g->variablesEnabled && (trim(g->cfg.variableTemplate).empty() ||
            !smartNamingPreview(compose(g->parts), path, g->dialog.extension,
                                {0, false, g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression, effectiveVariables(), g->cfg.dateFormat}).error.empty())) {
            openVariableEditor(); return;
        }
        g->variablesEnabled = !g->variablesEnabled; updateName = true; quick = false;
        break;
    case Action::Prefix:
    case Action::Suffix: {
        bool prefix = action.kind == Action::Prefix;
        const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
        if (action.index >= values.size()) return;
        auto value = expandAffix(values[action.index], namingOptions().dateFormat);
        if(!prefix)if(auto field=structuredSuffixField(g->structure,g->cfg,value)){
            auto next=namingFieldCanOmit(g->structure,static_cast<size_t>(field-g->structure.fields.data()))&&fieldValue(*field,g->cfg,g->structure.dateFormat)==value.substr(1)?L"":value.substr(1);
            field->value=next;field->source=0;field->omitted=next.empty();g->parts.suffix.clear();
            if(g->structuredField==static_cast<int>(field-g->structure.fields.data()))resetStructuredInput();
            updateName=true;g->dirtyName=true;break;
        }
        auto &selected = prefix ? g->parts.prefix : g->parts.suffix;
        if (selected == value) selected.clear();
        else {
            // Adopt an identical literal affix already in the name rather than duplicating it.
            auto body = composeBody(g->parts);
            if (selected.empty() && (prefix ? body.starts_with(value) : body.ends_with(value))) {
                if (prefix) body.erase(0, value.size());
                else body.resize(body.size() - value.size());
                auto oldPrefix = g->parts.prefix, oldSuffix = g->parts.suffix;
                g->parts = splitName(body, g->cfg.modifiers);
                g->parts.prefix = oldPrefix; g->parts.suffix = oldSuffix;
            }
            if (!prefix && std::any_of(g->cfg.modifiers.begin(), g->cfg.modifiers.end(), [&](const auto &m) {
                return value == L"_" + m;
            })) g->parts.stage.clear();
            if (!prefix && value == L"_" + g->parts.date) g->parts.date.clear();
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
        if(r.namingOnly){useNamingScheme(deserializeStructure(r.structure));return;}
    
        path = r.directory.empty() ? g->dialog.directory : r.directory;
        if (r.relativeDirectory) {
            const auto *project = activeProject(g->cfg);
            path = project ? projectPath(*project, r.directory) : L"";
            if (path.empty()) {
                g->status = L"此方案使用项目相对目录，请先选择项目并设置根目录。";
                g->statusError = true; updatePanelContent(); return;
            }
        }
        resetStructuredInput();g->inlineTool=0;
        g->structure={};g->structuredField=-1;g->freeRecovery.reset();
        if (r.complete) {
            g->structure=deserializeStructure(r.structure);g->structuredField=-1;g->freeRecovery.reset();
            g->parts = {r.value, r.stageValue, r.date ? today(r.dateFormat) : L"", expandAffix(r.prefix, r.dateFormat), expandAffix(r.suffix, r.dateFormat)};
            g->numberMode = r.counter; g->folderName = r.folder; g->variablesEnabled = r.variables;
            g->sessionExpression = r.expression; g->sessionVariables = r.variableValues;
            g->sessionDigits = r.digits; g->sessionDateFormat = r.dateFormat; g->sessionUpper = r.upperVersion;
            g->ruleEnabled = r.ruleEnabled; g->ruleRegex = r.ruleRegex;
            g->rulePattern = r.pattern; g->ruleReplacement = r.replacement;
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
        g->rebuildPending=true;PostMessageW(g->control,WM_PANEL_REFRESH,0,0);
        break;
    }
    }
    if (updateName) {
        name = composedName(&path);
        g->stemName = name;
        g->dirtyName = true;
        g->statusError = !g->ruleError.empty();
        if (g->statusError) g->status = g->ruleError;
        g->localNamePending = true;
        g->building = true;
        setTextIfChanged(GetDlgItem(g->panel,ID_PREVIEW),name+g->dialog.extension);
        g->building = false;
        updatePanelContent();
    }
    if (g->ruleError.empty()) apply(path, name, quick && g->cfg.quickSave && g->dialog.save);
}
void updateInlineRule(bool immediate) {
    invalidateNaming();
    KillTimer(g->control, T_ACTION);
    g->stemName = composedName();
    g->dirtyName = true; g->localNamePending = true;
    g->status = g->ruleError.empty() ? L"命名预览已更新，规则收起后继续生效" : g->ruleError;
    g->statusError = !g->ruleError.empty();
    updatePanelContent();
    if (g->ruleError.empty()) {
        if (immediate && g->attached && !g->editingPath &&
            focusAllowsAction({JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load()}))
            apply(g->dialog.directory, g->stemName, false);
        else SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    }
}
void updateVariableDraftPreview() {
    if (!g->manager || g->managerTab != MANAGER_TEMPLATES) return;
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_TEMPLATE)) g->variableTemplateDraft = windowText(edit);
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_VALUES)) g->variableValuesDraft = windowText(edit);
    if (!g->variableAdvanced) {
        for (size_t i = 0; i < g->variableInputKeys.size(); ++i)
            if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_FIELD_FIRST + static_cast<int>(i)))
                setVariableDraftValue(g->variableInputKeys[i], trim(windowText(edit)));
    }
    auto preview = smartNamingPreview(g->parts.base.empty() ? L"文件名" : compose(g->parts), g->dialog.directory, g->dialog.extension,
                                     {0, false, g->variableTemplateDraft, g->variableValuesDraft, g->cfg.dateFormat});
    if (trim(g->variableTemplateDraft).empty()) preview.error = L"至少点选一个字段来组成文件名";
    auto hint = GetDlgItem(g->manager, ID_VARIABLE_HINT);
    auto text = preview.error.empty() ? L"预览：" + preview.name : preview.error;
    setTextIfChanged(hint, text);
    enableIfChanged(GetDlgItem(g->manager, ID_VARIABLE_SAVE), preview.error.empty());
}
void initializeVariableDraft() {
    if (!g->variableEditor) {
        g->variableTemplateDraft = g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression;
        g->variableValuesDraft = effectiveVariables();
        g->variableAdvanced = false;
        auto values = variableValues(g->variableValuesDraft);
        if (!values.contains(L"项目")) {
            auto folder = g->dialog.directory;
            while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/')) folder.pop_back();
            folder = std::filesystem::path(folder).filename().wstring();
            if (validName(folder)) setVariableDraftValue(L"项目", folder);
        }
        if (!values.contains(L"作者")) {
            wchar_t user[256]{}; DWORD length = static_cast<DWORD>(std::size(user));
            if (GetUserNameW(user, &length) && validName(user)) setVariableDraftValue(L"作者", user);
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
    invalidateNaming(); g->namingHistory.clear();
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
    bool windowName = event == EVENT_OBJECT_NAMECHANGE && object == OBJID_WINDOW &&
        child == CHILDID_SELF && GetAncestor(hwnd, GA_ROOT) == hwnd;
    if (event != EVENT_SYSTEM_FOREGROUND && !value && (object != OBJID_WINDOW || child != CHILDID_SELF))
        return;
    if (value && !windowName && (!g->attached || GetAncestor(hwnd, GA_ROOT) != g->dialog.hwnd))
        return;
    if ((event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_DESTROY) && hwnd != g->dialog.hwnd)
        return;
    if (event == EVENT_OBJECT_SHOW && (GetAncestor(hwnd, GA_ROOT) != hwnd ||
        (windowClass(hwnd) != L"#32770" && !windowClass(hwnd).starts_with(L"GHOST_WindowClass") && !houdiniBrowserWindow(hwnd) && !genericCandidate(hwnd))))
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
    if (g->testTargetPid && pid != g->testTargetPid) { if (g->attached) hidePanel(); return; }
    if (!g->testTargetImage.empty() && processImage(foreground) != g->testTargetImage) { if (g->attached) hidePanel(); return; }
    if (requested) {
        if (!IsWindowVisible(foreground) || !IsWindowEnabled(foreground) || IsHungAppWindow(foreground)) return;
        g->suppressed = nullptr; g->inspectAttempts = 0;
    }
    if (g->attached && !attachedDialogAlive()) detach();
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
    if (g->bindingHint) return;
    if (g->inspectCandidate != foreground) { g->inspectCandidate = foreground; g->inspectAttempts = 0; }
    if (g->inspectAttempts++ >= 4) return;
    if (g->attached)
        detach();
    ULONGLONG gen = ++g->generation;
    g->dialog.hwnd = foreground;
    GetWindowThreadProcessId(foreground, &g->dialog.pid);
    Job job{JobType::Inspect, foreground, g->dialog.pid, gen}; job.requestedInspect = requested;
    enqueue(std::move(job));
}

void showManager(bool present);
void startBinding();
void showPresetManager(bool add, size_t index = 0, bool present = true);
LRESULT CALLBACK PanelProc(HWND, UINT, WPARAM, LPARAM);
void closeFlyout(bool focusPanel) {
    auto popup = g->flyout;
    g->flyout = nullptr;
    if (popup) DestroyWindow(popup);
    if (focusPanel && IsWindowVisible(g->panel)) {
        SetForegroundWindow(g->panel);
        if (g->attached && g->dirtyName && g->localNamePending && !g->busy)
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    }
}
bool isFavoritePath(const std::wstring &path) {
    return std::any_of(g->cfg.favorites.begin(), g->cfg.favorites.end(), [&](const auto &p) {
        return normalize(p.path) == normalize(path);
    });
}
void toggleFavoriteLocation(const Location &location) {
    auto previous = g->cfg;
    bool favorite = isFavoritePath(location.path);
    if (favorite) std::erase_if(g->cfg.favorites, [&](const auto &p) { return normalize(p.path) == normalize(location.path); });
    else g->cfg.favorites.push_back({location.name, location.path, L"收藏"});
    if (!persist()) { g->cfg = std::move(previous); g->status = L"收藏未保存，请检查配置写入权限"; }
    else g->status = favorite ? L"已取消收藏" : L"已收藏位置";
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
    if (g->flyout) InvalidateRect(GetDlgItem(g->flyout, ID_FOLDER_LIST), nullptr, FALSE);
}
void updateLocationResults();
bool removeHistoryLocation(const std::wstring &path = L"") {
    auto previous = g->cfg.history;
    if (path.empty()) g->cfg.history.clear();
    else std::erase_if(g->cfg.history, [&](const auto &entry) { return normalize(entry.path) == normalize(path); });
    if (g->cfg.history.size() == previous.size()) return true;
    bool saved = persist();
    if (!saved) {
        g->cfg.history = std::move(previous);
        g->status = L"历史清理未保存，请检查配置写入权限。"; g->statusError = true;
    } else {
        g->status = path.empty() ? L"历史位置已清空；收藏和最近保存保留。" : L"已从历史移除；收藏和保存记录保留。";
        g->statusError = false;
        if (path.empty()) {
            g->locationPage = 0;
            if (g->attached) g->lastHistoryDirectory = g->dialog.directory;
        }
        if (g->flyoutKind == State::Flyout::Locations && g->historyOnly) {
            std::erase_if(g->flyoutLocations, [&](const auto &entry) { return path.empty() || normalize(entry.path) == normalize(path); });
            if (g->flyout) updateLocationResults();
        }
    }
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
    return saved;
}
void removeRecentLocations(const std::wstring &path = L"") {
    auto previous = g->cfg.recentSaves;
    if (path.empty()) g->cfg.recentSaves.clear();
    else std::erase_if(g->cfg.recentSaves, [&](const auto &entry) { return normalize(entry.path) == normalize(path); });
    if (g->cfg.recentSaves.size() == previous.size()) return;
    if (!persist()) {
        g->cfg.recentSaves = std::move(previous);
        g->status = L"最近记录清理未保存，请检查配置写入权限。"; g->statusError = true;
    } else {
        g->status = path.empty() ? L"最近保存记录已清空" : L"已从最近保存移除";
        g->statusError = false;
        if (path.empty()) g->locationPage = 0;
    }
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    updatePanelContent();
}
HWND flyoutChild(const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y, int width, int height) {
    bool edit = std::wstring(cls) == L"EDIT";
    auto h = createChild(g->flyout, cls, text, style, id, x + (edit ? 8 : 0),
        y + (edit ? 6 : 0), width - (edit ? 16 : 0), height - (edit ? 12 : 0), g->font);
    if (std::wstring(cls) == L"BUTTON") SetWindowSubclass(h, PillProc, 1, 0);
    if (edit) SetWindowSubclass(h, InputFeedbackProc, 1, 0);
    return h;
}
LRESULT CALLBACK FlyoutEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        SendMessageW(GetParent(h), WM_COMMAND, wp == VK_RETURN ? IDOK : IDCANCEL, 0); return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE)) return 0;
    if (msg == WM_KEYDOWN && wp == VK_DOWN && GetDlgCtrlID(h) == ID_FOLDER_SEARCH) {
        SetFocus(GetDlgItem(GetParent(h), ID_FOLDER_LIST)); return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void createFlyout(State::Flyout kind, int width, int height, HWND anchor) {
    g->flyoutKind = kind;
    RECT r{};
    if (!IsWindow(anchor) || !IsChild(g->panel, anchor) ||
        !GetWindowRect(anchor, &r) || IsRectEmpty(&r)) GetWindowRect(g->panel, &r);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
    int x = std::clamp<int>(r.right - px(width), mi.rcWork.left, std::max<int>(mi.rcWork.left, mi.rcWork.right - px(width)));
    int y = r.bottom + px(4);
    if (y + px(height) > mi.rcWork.bottom) y = std::max<int>(mi.rcWork.top, r.top - px(height) - px(4));
    y = std::clamp<int>(y, mi.rcWork.top, std::max<int>(mi.rcWork.top, mi.rcWork.bottom - px(height)));
    g->flyout = CreateWindowExW(WS_EX_TOOLWINDOW, L"Jump.Flyout", L"Jump · 快速选择", WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
        x, y, px(width), px(height), g->panel, nullptr, g->instance, nullptr);
}
void revealFlyout(int focusId) {
    if (!IsWindowVisible(g->panel)) return;
    ShowWindow(g->flyout, SW_SHOW);
    SetForegroundWindow(g->flyout);
    SetFocus(GetDlgItem(g->flyout, focusId));
}
void updateLocationResults() {
    auto query = lower(trim(windowText(GetDlgItem(g->flyout, ID_FOLDER_SEARCH))));
    auto list = GetDlgItem(g->flyout, ID_FOLDER_LIST);
    std::vector<Location> filtered;
    for (const auto &p : g->flyoutLocations)
        if (query.empty() || lower(p.name + L" " + p.path).find(query) != std::wstring::npos) filtered.push_back(p);
    bool same = filtered.size() == g->filteredLocations.size() && std::equal(filtered.begin(), filtered.end(),
        g->filteredLocations.begin(), [](const Location &a, const Location &b) {
            return a.name == b.name && a.path == b.path && a.sources == b.sources;
        });
    if (same && SendMessageW(list, LB_GETCOUNT, 0, 0) > 0) return;
    int oldSelection = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    std::wstring selectedPath = oldSelection >= 0 && static_cast<size_t>(oldSelection) < g->filteredLocations.size() ?
        g->filteredLocations[oldSelection].path : g->dialog.directory;
    bool visible = IsWindowVisible(list) != FALSE;
    if (visible) SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    g->filteredLocations = std::move(filtered);
    int selected = 0;
    for (size_t i = 0; i < g->filteredLocations.size(); ++i) {
        const auto &p = g->filteredLocations[i];
        if (normalize(p.path) == normalize(selectedPath)) selected = static_cast<int>(i);
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    }
    if (g->filteredLocations.empty()) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"没有匹配的位置"));
    SendMessageW(list, LB_SETCURSEL, selected, 0);
    if (visible) SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, FALSE);
}
LRESULT CALLBACK LocationListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_CONTEXTMENU && g->historyOnly) {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        int index = static_cast<int>(SendMessageW(h, LB_GETCURSEL, 0, 0));
        if (point.x == -1 && point.y == -1) {
            RECT row{}; SendMessageW(h, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&row));
            point = {row.left, row.bottom}; ClientToScreen(h, &point);
        } else {
            POINT client = point; ScreenToClient(h, &client);
            auto hit = SendMessageW(h, LB_ITEMFROMPOINT, 0, MAKELPARAM(client.x, client.y));
            if (HIWORD(hit)) return 0;
            index = LOWORD(hit);
        }
        if (index < 0 || static_cast<size_t>(index) >= g->filteredLocations.size()) return 0;
        auto location = g->filteredLocations[index];
        SendMessageW(h, LB_SETCURSEL, index, 0);
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, isFavoritePath(location.path) ? L"取消收藏" : L"收藏文件夹");
        AppendMenuW(menu, MF_STRING, 2, L"从历史移除");
        int selected = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, g->flyout);
        DestroyMenu(menu);
        if (selected == 1) toggleFavoriteLocation(location);
        else if (selected == 2) removeHistoryLocation(location.path);
        return 0;
    }
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE || key->wParam == VK_SPACE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE || wp == VK_SPACE)) {
        SendMessageW(g->flyout, WM_COMMAND, wp == VK_ESCAPE ? IDCANCEL : wp == VK_SPACE ? ID_FOLDER_STAR : ID_FOLDER_SELECT, 0);
        return 0;
    }
    if (msg == WM_LBUTTONUP) {
        auto result = DefSubclassProc(h, msg, wp, lp);
        DWORD item = static_cast<DWORD>(SendMessageW(h, LB_ITEMFROMPOINT, 0, lp));
        if (!HIWORD(item)) {
            SendMessageW(h, LB_SETCURSEL, LOWORD(item), 0);
            RECT r{}; GetClientRect(h, &r);
            SendMessageW(g->flyout, WM_COMMAND, GET_X_LPARAM(lp) >= r.right - px(32) ? ID_FOLDER_STAR : ID_FOLDER_SELECT, 0);
        }
        return result;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void showLocationFlyout(bool historyOnly) {
    if (g->flyout) { closeFlyout(true); return; }
    g->historyOnly = historyOnly;
    g->flyoutLocations.clear();
    std::set<std::wstring> seen;
    auto add = [&](const Location &p) {
        if (!p.path.empty() && seen.insert(normalize(p.path)).second) g->flyoutLocations.push_back(p);
    };
    if (!historyOnly) {
    for (const auto &p : g->cfg.favorites) add(p);
    for (const auto &p : g->cfg.recentSaves) add(p);
    for (const auto &p : g->paths) add(p);
    for (const auto &p : g->openedLocations) add(p);
    for (const auto &p : g->cfg.history) add(p);
    add({L"当前位置", g->dialog.directory, L"当前"});
    const std::pair<const KNOWNFOLDERID *, const wchar_t *> common[] = {
        {&FOLDERID_Pictures, L"图片"}, {&FOLDERID_Downloads, L"下载"},
        {&FOLDERID_Desktop, L"桌面"}, {&FOLDERID_Documents, L"文档"}
    };
    for (const auto &[id, name] : common) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DONT_VERIFY, nullptr, &path))) {
            add({name, path, L"系统位置"});
            CoTaskMemFree(path);
        }
    }
    }
    if (historyOnly) for (const auto &p : g->cfg.history) add(p);
    int width = std::min(g->width - 32, 420);
    createFlyout(State::Flyout::Locations, width, 306, GetDlgItem(g->panel, ID_DIRECTORY));
    auto search = flyoutChild(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, ID_FOLDER_SEARCH, 12, 12, width - 24, 32);
    SetWindowSubclass(search, FlyoutEditProc, 1, 0);
    SendMessageW(search, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(historyOnly ? L"搜索历史名称或路径…" : L"搜索文件夹名称或路径…"));
    auto list = flyoutChild(L"LISTBOX", L"", LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY |
        WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT, ID_FOLDER_LIST, 4, 52, width - 8, 210);
    SendMessageW(list, LB_SETITEMHEIGHT, 0, px(42));
    SetWindowSubclass(list, LocationListProc, 1, 0);
    flyoutChild(L"BUTTON", L"管理位置…", BS_OWNERDRAW | WS_TABSTOP, ID_FLYOUT_MANAGE, 12, 270, width - 24, 24);
    updateLocationResults();
    revealFlyout(ID_FOLDER_SEARCH);
}
void updatePresetChoices() {
    if (!g->flyout || g->flyoutKind != State::Flyout::Presets) return;
    g->flyoutActions.clear();
    RECT r{}; GetClientRect(g->flyout, &r);
    int width = MulDiv(r.right, 96, g->dpi), cell = (width - 40) / 3;
    auto query = lower(trim(g->presetDraft));
    size_t total = g->namingMode == 1 ? g->cfg.prefixes.size() : g->cfg.suffixes.size();
    int shown = 0;
    for (size_t i = 0; i < total && shown < 6; ++i) {
        auto value = (g->namingMode == 1 ? g->cfg.prefixes : g->cfg.suffixes)[i];
        auto content = value + L" " + affixLabel(value);
        if (!query.empty() && lower(content).find(query) == std::wstring::npos) continue;
        int id = 2000 + shown;
        auto button = GetDlgItem(g->flyout, id);
        if (!button) button = flyoutChild(L"BUTTON", affixLabel(value), BS_OWNERDRAW | WS_TABSTOP, id,
                                        12 + (shown % 3) * (cell + 8), 76 + (shown / 3) * 32, cell, 24);
        else setTextIfChanged(button, affixLabel(value));
        showIfChanged(button, SW_SHOWNOACTIVATE);
        InvalidateRect(button, nullptr, FALSE);
        g->flyoutActions[id] = {g->namingMode == 1 ? Action::Prefix : Action::Suffix, i};
        ++shown;
    }
    for (int i = shown; i < 6; ++i) showIfChanged(GetDlgItem(g->flyout, 2000 + i), SW_HIDE);
    setTextIfChanged(GetDlgItem(g->flyout, ID_PRESET_HINT), !query.empty() && !shown ?
        L"没有匹配的预设，可直接添加" : L"搜索前后缀预设 · Enter 添加 / Esc 取消");
}
void showPresetFlyout(HWND anchor) {
    int width = std::min(g->width - 32, 420);
    // Anchor to the actual source control, never the removed legacy affix button.
    auto usable = [](HWND h) {
        return IsWindow(h) && IsChild(g->panel, h) && (GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE);
    };
    if (!usable(anchor)) anchor = GetDlgItem(g->panel,
        g->namingMode == 1 ? ID_ADD_PREFIX : ID_ADD_SUFFIX);

    createFlyout(State::Flyout::Presets, width, 190, anchor);
    auto edit = flyoutChild(L"EDIT", g->presetDraft, ES_AUTOHSCROLL | WS_TABSTOP, ID_PRESET_VALUE,
                            12, 12, width - 100, 32);
    SetWindowSubclass(edit, FlyoutEditProc, 1, 0);
    SendMessageW(edit, EM_SETLIMITTEXT, 80, 0);
    SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(g->namingMode == 1 ? L"搜索或添加前缀…" : L"搜索或添加后缀…"));
    flyoutChild(L"BUTTON", g->presetIndex == NEW_PRESET ? L"添加" : L"保存", BS_OWNERDRAW | WS_TABSTOP,
                ID_PRESET_SAVE, width - 80, 12, 68, 32);
    auto hint = flyoutChild(L"STATIC", L"Enter 保存预设 · Esc 取消", SS_LEFT, ID_PRESET_HINT, 12, 50, width - 24, 20);
    SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
    updatePresetChoices();
    flyoutChild(L"BUTTON", g->namingMode == 1 ? L"管理前缀预设…" : L"管理后缀预设…",
                BS_OWNERDRAW | WS_TABSTOP, ID_FLYOUT_MANAGE, 12, 148, width - 24, 28);
    revealFlyout(ID_PRESET_VALUE);
    if (IsWindowVisible(g->flyout)) SendMessageW(edit, EM_SETSEL, 0, -1);
}
LRESULT CALLBACK FlyoutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && reinterpret_cast<HWND>(lp) != g->panel) closeFlyout();
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(hwnd, msg, wp); HDC dc = paint.dc();
        RECT r{}; GetClientRect(hwnd, &r); FillRect(dc, &r, g->brush);
        int width = MulDiv(r.right, 96, g->dpi);
        RECT field{px(12), px(12), px(width - (g->flyoutKind == State::Flyout::Presets ? 88 : 12)), px(44)};
        paintInputFrame(dc, hwnd, field, px(8));
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg); SetBkColor(reinterpret_cast<HDC>(wp), g->field);
        return reinterpret_cast<LRESULT>(g->inputBrush);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        SetTextColor(reinterpret_cast<HDC>(wp), g->muted); SetBkColor(reinterpret_cast<HDC>(wp), g->bg);
        return reinterpret_cast<LRESULT>(g->brush);
    case WM_DRAWITEM: {
        auto d = reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (d->CtlID != ID_FOLDER_LIST) return SendMessageW(g->panel, msg, wp, lp);
        DRAWITEMSTRUCT item = *d;
        PaintBuffer buffer(item.hDC, item.rcItem); item.hDC = buffer.dc; d = &item;
        RECT r = d->rcItem; FillRect(d->hDC, &r, g->brush);
        if (d->itemID == static_cast<UINT>(-1)) return TRUE;
        auto font = SelectObject(d->hDC, g->font); SetBkMode(d->hDC, TRANSPARENT);
        if (d->itemID >= g->filteredLocations.size()) {
            SetTextColor(d->hDC, g->muted); DrawTextW(d->hDC, L"没有匹配的位置", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            const auto &p = g->filteredLocations[d->itemID];
            if (d->itemState & ODS_SELECTED) { auto b = CreateSolidBrush(g->selection); FillRect(d->hDC, &r, b); DeleteObject(b); }
            RECT name{r.left + px(12), r.top + px(4), r.right - px(40), r.top + px(22)};
            bool recent = std::any_of(g->cfg.recentSaves.begin(), g->cfg.recentSaves.end(), [&](const auto &saved) {
                return normalize(saved.path) == normalize(p.path);
            });
            auto caption = p.name + (recent ? L" · 最近" : L"");
            SetTextColor(d->hDC, g->fg); DrawTextW(d->hDC, caption.c_str(), -1, &name, DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(d->hDC, g->smallFont); name.top += px(18); name.bottom += px(18);
            SetTextColor(d->hDC, g->muted); DrawTextW(d->hDC, p.path.c_str(), -1, &name, DT_SINGLELINE | DT_PATH_ELLIPSIS);
            RECT star{r.right - px(32), r.top, r.right, r.bottom}; SelectObject(d->hDC, g->font);
            bool pinned = isFavoritePath(p.path); SetTextColor(d->hDC, pinned ? RGB(230, 184, 72) : g->muted);
            DrawTextW(d->hDC, pinned ? L"★" : L"☆", -1, &star, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        SelectObject(d->hDC, font); return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == IDOK && code == BN_CLICKED) {
            int focused = GetDlgCtrlID(GetFocus());
            if (focused == ID_FLYOUT_MANAGE || g->flyoutActions.contains(focused)) id = focused;
        }
        if (id == ID_FOLDER_SEARCH && code == EN_CHANGE) { updateLocationResults(); return 0; }
        if (id == ID_PRESET_VALUE && code == EN_CHANGE) { g->presetDraft = windowText(GetDlgItem(hwnd, id)); updatePresetChoices(); return 0; }
        if (code != BN_CLICKED) return 0;
        if (id == IDCANCEL || id == ID_PRESET_CANCEL) { closeFlyout(true); return 0; }
        if (id == ID_PRESET_SAVE || (id == IDOK && g->flyoutKind == State::Flyout::Presets)) { saveInlinePreset(); return 0; }
        if (id == ID_FLYOUT_MANAGE) {
            bool locations = g->flyoutKind == State::Flyout::Locations;
            size_t index = g->presetIndex == NEW_PRESET ? 0 : g->presetIndex;
            closeFlyout(true);
            if (locations) SendMessageW(g->panel, WM_COMMAND, ID_MANAGER, 0);
            else showPresetManager(false, index);
            return 0;
        }
        if (id == ID_FOLDER_SELECT || id == ID_FOLDER_STAR || id == IDOK) {
            int selected = static_cast<int>(SendDlgItemMessageW(hwnd, ID_FOLDER_LIST, LB_GETCURSEL, 0, 0));
            if (selected < 0 || static_cast<size_t>(selected) >= g->filteredLocations.size()) return 0;
            auto location = g->filteredLocations[selected];
            if (id == ID_FOLDER_STAR) toggleFavoriteLocation(location);
            else {
                closeFlyout(true); invalidateNaming();
                if (g->numberMode || g->folderName || g->variablesEnabled) g->stemName = composedName(&location.path);
                apply(location.path, g->stemName, false);
            }
            return 0;
        }
        auto found = g->flyoutActions.find(id);
        if (found != g->flyoutActions.end()) { auto action = found->second; closeFlyout(true); perform(action, false); }
        return 0;
    }
    case WM_CLOSE: closeFlyout(true); return 0;
    case WM_DESTROY:
        g->flyout = nullptr; g->flyoutKind = State::Flyout::None; g->presetEditor = false; g->flyoutActions.clear(); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
HMENU buildPanelMenu(int id) {
    auto menu = CreatePopupMenu();
    if (id == ID_PANEL_OPTIONS) {
        AppendMenuW(menu,MF_STRING,ID_NAME_MANAGER,L"管理命名方案…");
        AppendMenuW(menu,MF_STRING,ID_STRUCT_EXAMPLE,L"从现有名称建立方案…");
        AppendMenuW(menu, MF_STRING, ID_PROJECTS, (L"项目：" + (activeProject(g->cfg) ? activeProject(g->cfg)->name : L"未选择") + L"…").c_str());
        AppendMenuW(menu, MF_STRING, ID_RECORDS, L"已核验的保存记录…");
        AppendMenuW(menu, MF_STRING, ID_EXPLAIN, L"查看名称组成…");
        AppendMenuW(menu, MF_STRING | (g->namingHistory.canUndo() ? 0 : MF_GRAYED), ID_COMBO_UNDO, L"撤销命名组合");
        AppendMenuW(menu, MF_STRING | (g->namingHistory.canRedo() ? 0 : MF_GRAYED), ID_COMBO_REDO, L"重做命名组合");

        AppendMenuW(menu, MF_STRING, ID_VARIABLE_EDIT, L"配置变量表达式…");
        AppendMenuW(menu, MF_STRING, ID_MANAGER, L"管理收藏位置…");
        AppendMenuW(menu, MF_STRING, ID_PLANS, L"保存流程…");

        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_BIND_CONTROL, L"指定文件名输入框…");
        AppendMenuW(menu, MF_STRING, ID_TRANSFER, L"迁移、备份与恢复…");
        AppendMenuW(menu, MF_STRING, ID_DIAGNOSTICS, L"当前接入与诊断…");
        AppendMenuW(menu, MF_STRING, ID_COPY_NAME, L"复制当前完整文件名");
        if (g->dialog.generic) AppendMenuW(menu, MF_STRING | (g->dialog.genericSubmit ? 0 : MF_GRAYED),
            ID_GENERIC_SUBMIT, L"填入并调用原窗口保存");
        AppendMenuW(menu, MF_STRING, ID_PANEL_SETTINGS, L"选项…");
    } else if (id == ID_LOCATION_MENU) {
        AppendMenuW(menu, MF_STRING | (g->locationTab == 2 ? MF_CHECKED : 0), ID_LOC_HISTORY, L"浏览历史");
        AppendMenuW(menu, MF_STRING | (g->locationTab == 0 ? MF_CHECKED : 0), ID_LOC_RECOMMENDED, L"推荐位置（含已打开文件夹）");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_MANAGER, L"管理收藏位置…");
    } else {
        AppendMenuW(menu, MF_STRING, ID_PLANS, L"保存流程");
        AppendMenuW(menu, MF_STRING | (g->namingHistory.canUndo() ? 0 : MF_GRAYED), ID_COMBO_UNDO, L"撤销命名组合");
        AppendMenuW(menu, MF_STRING | (g->namingHistory.canRedo() ? 0 : MF_GRAYED), ID_COMBO_REDO, L"重做命名组合");
        AppendMenuW(menu, MF_STRING, ID_EXPLAIN, L"查看名称组成…");
        AppendMenuW(menu, MF_STRING, ID_UNDO, L"还原本次名称与位置");
        AppendMenuW(menu, MF_STRING | (g->busy ? MF_GRAYED : 0), ID_RENAME, L"替换 / 正则…");
    }
    return menu;
}
bool saveHiddenPanelPreview(const std::filesystem::path &path, HWND target = nullptr) {
    if (!target) target = g->panel;
    const bool manager = target == g->manager;
    RECT bounds{};
    if (manager) GetClientRect(target, &bounds); else GetWindowRect(target, &bounds);
    int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
    void *pixels = nullptr;
    HDC screen = GetDC(target), dc = CreateCompatibleDC(screen);
    auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) { DeleteDC(dc); ReleaseDC(target, screen); return false; }
    auto old = SelectObject(dc, bitmap);
    if (manager) {
        // Print the client and each child in client coordinates; native caption
        // offsets otherwise misalign the custom frames in a hidden-window render.
        SendMessageW(target, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
        for (HWND child = GetWindow(target, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE)) continue;
            RECT r{}; GetWindowRect(child, &r); MapWindowPoints(nullptr, target, reinterpret_cast<POINT *>(&r), 2);
            int saved = SaveDC(dc); SetViewportOrgEx(dc, r.left, r.top, nullptr);
            IntersectClipRect(dc, 0, 0, r.right - r.left, r.bottom - r.top);
            auto kind = windowClass(child);
            bool custom = kind == WC_TABCONTROLW || (kind == L"Button" &&
                (GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX);
            SendMessageW(child, custom ? WM_PRINTCLIENT : WM_PRINT, reinterpret_cast<WPARAM>(dc),
                         PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
            RestoreDC(dc, saved);
        }
    } else SendMessageW(target, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    GdiFlush();
    BITMAPFILEHEADER header{};
    header.bfType = 0x4D42;
    header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
    DWORD size = static_cast<DWORD>(width * height * 4);
    header.bfSize = header.bfOffBits + size;
    auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written = 0;
    bool ok = file != INVALID_HANDLE_VALUE && WriteFile(file, &header, sizeof(header), &written, nullptr) &&
        WriteFile(file, &info.bmiHeader, sizeof(BITMAPINFOHEADER), &written, nullptr) &&
        WriteFile(file, pixels, size, &written, nullptr) && written == size;
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(target, screen);
    return ok;
}

bool selfTestStructuredLayout(const std::filesystem::path &directory) {
    if(!liveStructuredControl(ID_STRUCT_SCHEMES))
        return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
    g->cfg=defaults();g->attached=false;g->busy=false;g->namingMode=1;
    g->inlineTool=0;g->ruleEnabled=g->variablesEnabled=g->folderName=false;g->numberMode=0;
g->paths.clear();g->locationTab=0;
    g->dialog.directory=directory.wstring();g->dialog.extension=L".txt";g->dialog.save=true;
    g->originalDirectory=g->dialog.directory;g->originalStem=L"原文件";
    StructuredNaming unchanged;unchanged.id=L"unchanged";unchanged.name=L"不编号";unchanged.enabled=true;unchanged.counter=0;
    unchanged.fields={{L"subject",L"主要名称",L"",true}};
    Recipe unchangedRecipe;unchangedRecipe.name=unchanged.name;unchangedRecipe.value=L"Asset";
    unchangedRecipe.namingOnly=true;unchangedRecipe.structure=serializeStructure(unchanged);
    g->cfg.recipes={unchangedRecipe};g->cfg.defaultNamingScheme=unchanged.id;g->stemName=L"Guard";
    resumeNamingDraft();
    if(g->structuredResumeNumber)return hiddenCheckFailure("unchangedSchemeDoesNotWrite",__LINE__);
    g->cfg=defaults();
    g->structure=basicStructure(L"Guard");g->structure.id=L"custom-stage-options";
    g->structure.fields[4].candidates={{L"Only",L"Only"}};
    if(quickCandidates(4).size()!=1)return hiddenCheckFailure("customSchemeCandidateIsolation",__LINE__);
    for(int dpi:{96,120,144})for(bool bottom:{false,true})for(int mode:{0,1,2}){
        bool editing=mode==2;
        g->dpi=dpi;fonts();g->bottom=bottom;g->width=bottom?680:PANEL_WIDTH;
        g->structuredAdvanced=editing;
        g->structure=basicStructure(L"Guard");g->structure.fields[2].value=L"模型";
        g->structure.fields[4].value=L"工作";g->structuredField=mode?1:-1;g->structuredDraft=L"Guard";
        g->stemName=composeStructure(g->structure,g->cfg).name;g->parts={g->stemName};
        rebuildPanel();
        if(GetDlgItem(g->panel,ID_SUBMIT)||GetDlgItem(g->panel,ID_UNDO)||!GetDlgItem(g->panel,ID_COMBO_UNDO)||
            !GetDlgItem(g->panel,ID_COMBO_REDO)||!GetDlgItem(g->panel,ID_NUMBER_MENU)||
            !GetDlgItem(g->panel,ID_AFFIX_SUMMARY)||!GetDlgItem(g->panel,ID_REPLACE_SUMMARY)||
            !GetDlgItem(g->panel,ID_CAPABILITIES))return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
        if(!GetDlgItem(g->panel,ID_STRUCT_SCHEMES)||!GetDlgItem(g->panel,ID_STRUCT_NAME))return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
        for(int id:{750,751,752,754,756,758,759,765,768,776,777,ID_STRUCT_DESIGN,ID_STRUCT_QUICK_VIEW,ID_STRUCT_FIELDS_VIEW})
            if(GetDlgItem(g->panel,id))return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
        if(mode==0&&(GetDlgItem(g->panel,ID_QUICK_INPUT+1)||!GetDlgItem(g->panel,ID_QUICK_PICK+2*6)||!GetDlgItem(g->panel,ID_QUICK_PICK+4*6)||
            GetDlgItem(g->panel,ID_STRUCT_DELIVERY)))return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
        SetWindowPos(g->panel,nullptr,0,0,px(g->width),px(g->height),SWP_NOZORDER|SWP_NOACTIVATE);
        RECT client{};GetClientRect(g->panel,&client);
        for(auto h:g->children){if(!(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE))continue;
            RECT r{};GetWindowRect(h,&r);MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&r),2);
            if(r.left<0||r.top<0||r.right>client.right||r.bottom>client.bottom)return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
        }
        auto name=L"structured-"+std::to_wstring(dpi)+(bottom?L"-bottom":L"-side")+(editing?L"-edit.bmp":mode==1?L"-input.bmp":L".bmp");
        if(!saveHiddenPanelPreview(directory/name)||IsWindowVisible(g->panel))return hiddenCheckFailure("selfTestStructuredLayout",__LINE__);
    }
    return true;
}

bool selfTestRecommendedLocations(const std::filesystem::path &directory) {
    g->cfg = defaults(); g->attached = g->busy = false; g->locationTab = g->locationPage = 0;
    g->structuredAdvanced = false; g->structuredField = -1;
    g->cfg.favorites = {{L"工作目录", L"D:\\Work"}};
    std::vector<Location> opened{{L"工作目录", L"d:\\work\\"}};
    for (int i = 0; i < 12; ++i)
        opened.push_back({L"打开目录 " + std::to_wstring(i), L"D:\\Open\\" + std::to_wstring(i)});
    g->paths = rankLocations(g->cfg, L"D:\\Source", opened);
    for (int dpi : {96, 120, 144}) for (bool bottom : {false, true}) {
        g->dpi = dpi; fonts(); g->bottom = bottom; g->width = bottom ? 680 : PANEL_WIDTH;
        g->locationPage = 0; rebuildPanel();
        SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        for (int id : {ID_LOC_RECOMMENDED, ID_LOC_FAVORITES, ID_LOC_RECENT, ID_LOC_HISTORY})
            if (!GetDlgItem(g->panel, id)) return hiddenCheckFailure("recommendedTabs", __LINE__);
        if (GetDlgItem(g->panel, 123) || g->folderRows != 3 || g->displayPaths.size() != 9)
            return hiddenCheckFailure("recommendedMerge", __LINE__);
        RECT firstTab{}, lastTab{};
        GetWindowRect(GetDlgItem(g->panel, ID_LOC_RECOMMENDED), &firstTab);
        GetWindowRect(GetDlgItem(g->panel, ID_LOC_HISTORY), &lastTab);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&firstTab), 2);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&lastTab), 2);
        if (std::abs(firstTab.left - g->locationTabsRect.left - px(3)) > 1 ||
            std::abs(lastTab.right - g->locationTabsRect.right + px(3)) > 1 || firstTab.top != lastTab.top)
            return hiddenCheckFailure("locationTabsContentAlignment", __LINE__);
        if (g->displayPaths[1].name != L"工作目录" || g->displayPaths[1].sources.find(L"已打开") == std::wstring::npos)
            return hiddenCheckFailure("recommendedDeduplication", __LINE__);
        auto name = L"recommended-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
        if (!saveHiddenPanelPreview(directory / name)) return false;
        auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
        if (!(GetWindowLongPtrW(scroll, GWL_STYLE) & WS_VISIBLE))
            return hiddenCheckFailure("recommendedScroll", __LINE__);
        RECT before{}; GetWindowRect(GetDlgItem(g->panel, ID_DIRECTORY), &before);
        SendMessageW(g->panel, WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), reinterpret_cast<LPARAM>(scroll));
        RECT after{}; GetWindowRect(GetDlgItem(g->panel, ID_DIRECTORY), &after);
        if (g->locationPage == 0 || !EqualRect(&before, &after) || g->displayPaths.back().path != opened.back().path)
            return hiddenCheckFailure("recommendedAllOpenedReachable", __LINE__);
        g->locationPage = 0; updateLocationShortcuts();
        // Source-only changes must refresh the badge even when name/path stay the same.
        g->paths[1].sources = L"收藏"; updateLocationShortcuts();
        if (g->displayPaths[1].sources != L"收藏") return hiddenCheckFailure("recommendedBadgeRefresh", __LINE__);
        g->paths[1].sources += L" / 已打开";
        auto allPaths = g->paths;
        g->paths.resize(4); updateLocationShortcuts();
        RECT first{}, second{}, third{}, fourth{};
        GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST), &first);
        GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 1), &second);
        GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 2), &third);
        GetWindowRect(GetDlgItem(g->panel, ID_LOCATION_FIRST + 3), &fourth);
        if (g->folderRows != 2 || g->displayPaths.size() != 4 || first.top != second.top ||
            first.left != third.left || second.left != fourth.left || third.top != fourth.top || third.top <= first.top)
            return hiddenCheckFailure("fourLocationsBalancedGrid", __LINE__);
        name = L"recommended-four-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
        if (!saveHiddenPanelPreview(directory / name)) return false;
        g->paths = std::move(allPaths); updateLocationShortcuts();
        auto favorites = std::move(g->cfg.favorites);
        g->locationTab = 1; updatePanelContent();
        RECT empty{}; auto message = GetDlgItem(g->panel, ID_LOCATION_EMPTY);
        GetWindowRect(message, &empty); MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&empty), 2);
        if (!(GetWindowLongPtrW(message, GWL_STYLE) & WS_VISIBLE) || empty.left != g->locationTabsRect.left)
            return hiddenCheckFailure("emptyLocationContentAlignment", __LINE__);
        name = L"locations-empty-" + std::to_wstring(dpi) + (bottom ? L"-bottom.bmp" : L"-side.bmp");
        if (!saveHiddenPanelPreview(directory / name)) return false;
        g->cfg.favorites = std::move(favorites); g->locationTab = 0; updatePanelContent();
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
    SetWindowPos(g->panel, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    g->attached = false; g->dialog = {}; g->dialog.save = true;
    g->dialog.directory = L"D:\\Pictures"; g->dialog.extension = L".blend";
    g->cfg = defaults();  g->dpi = 120; g->width = PANEL_WIDTH; g->bottom = false;
    fonts(); paintTheme();
    g->cfg.recentSaves = {{L"图片", L"D:\\Pictures", L"最近"}, {L"下载", L"D:\\Downloads", L"最近"},
        {L"项目", L"D:\\Project", L"最近"}};
    g->locationTab = 4; g->locationPage = 0; g->inlineTool = 0;
    g->editingPath = g->busy = g->localNamePending = g->statusError = g->settingsDirty = false;
    g->parts = {L"角色设定_原文件", L"", L"", L"", L""};
    g->stemName = g->parts.base; g->dirtyName = true;
    rebuildPanel();
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (!saveHiddenPanelPreview(directory / L"reference-layout.bmp")) return false;
    return !GetDlgItem(g->panel,100)&&GetDlgItem(g->panel,ID_STRUCT_SCHEMES);
}
LRESULT CALLBACK CountPositionChanges(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR count) {
    if (msg == WM_WINDOWPOSCHANGED) ++*reinterpret_cast<int *>(count);
    return DefSubclassProc(h, msg, wp, lp);
}
bool selfTestPaintStability() {
    HWND button = GetDlgItem(g->panel, ID_LOCATION_FIRST);
    SendMessageW(button, WM_MOUSEMOVE, 0, MAKELPARAM(4, 4));
    ValidateRect(button, nullptr);
    for (int i = 0; i < 24; ++i) SendMessageW(button, WM_MOUSEMOVE, 0, MAKELPARAM(4 + i % 8, 4));
    bool stableHover = !GetUpdateRect(button, nullptr, FALSE);
    SendMessageW(button, WM_MOUSELEAVE, 0, 0);
    int moved = 0;
    SetWindowSubclass(button, CountPositionChanges, 99, reinterpret_cast<DWORD_PTR>(&moved));
    auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
    SetWindowSubclass(scroll, CountPositionChanges, 99, reinterpret_cast<DWORD_PTR>(&moved));
    for (int i = 0; i < 12; ++i) updateLocationShortcuts();
    RemoveWindowSubclass(button, CountPositionChanges, 99);
    RemoveWindowSubclass(scroll, CountPositionChanges, 99);
    if (!stableHover || moved != 0) return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    g->inlineTool = 3; rebuildPanel();
    HWND input = GetDlgItem(g->panel, ID_RULE_PATTERN);
    RECT frame{}; GetWindowRect(input, &frame);
    MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&frame), 2);
    SendMessageW(input, WM_MOUSELEAVE, 0, 0);
    auto idleBorder = inputBorder(g->panel, frame);
    SendMessageW(input, WM_MOUSEMOVE, 0, MAKELPARAM(3, 3));
    if (inputBorder(g->panel, frame) == idleBorder)
        return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    ValidateRect(g->panel, nullptr);
    for (int i = 0; i < 12; ++i) SendMessageW(input, WM_MOUSEMOVE, 0, MAKELPARAM(3 + i, 3));
    if (GetUpdateRect(g->panel, nullptr, FALSE)) return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    SendMessageW(input, WM_MOUSELEAVE, 0, 0);
    if (inputBorder(g->panel, frame) != idleBorder) return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    auto source = GetDlgItem(g->panel, ID_RULE_SOURCE);
    SendMessageW(source, EM_SETSEL, 1, 3);
    updateInlineControls();
    DWORD start = 0, end = 0;
    SendMessageW(source, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    if (start != 1 || end != 3) return hiddenCheckFailure("selfTestPaintStability", __LINE__);
    g->inlineTool = 0; rebuildPanel();
    return true;
}
void showPanelMenu(int id) {
    RECT rect{};
    HWND control = GetDlgItem(g->panel, id);
    GetWindowRect(control, &rect);
    auto menu = buildPanelMenu(id);
    SetForegroundWindow(g->panel);
    g->openPanelMenu = id;
    InvalidateRect(control, nullptr, FALSE); UpdateWindow(control);
    int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_RIGHTBUTTON,
                                 rect.right, rect.bottom, g->panel);
    g->openPanelMenu = 0;
    if (IsWindow(control)) { InvalidateRect(control, nullptr, FALSE); UpdateWindow(control); }
    DestroyMenu(menu);
    if (command) SendMessageW(g->panel, WM_COMMAND, command, 0);
}


#include "workflow_ui.h"
#include "naming_defaults_ui.h"
#include "structured_ui.h"
#include "transient_edit.h"
#include "panel_appearance.h"

void showRecipeMenu() {
    if (!IsWindowVisible(g->panel)) return;
    auto menu = CreatePopupMenu();
    for (size_t i = 0; i < g->cfg.recipes.size(); ++i)if(!g->cfg.recipes[i].namingOnly)
        AppendMenuW(menu, MF_STRING, 4000 + i, g->cfg.recipes[i].name.c_str());
    if (GetMenuItemCount(menu)) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_CAPTURE_RECIPE, L"将当前整套设置保存为流程…");
    AppendMenuW(menu, MF_STRING, 2, L"添加可填写变量的示例方案");
    AppendMenuW(menu, MF_STRING, 1, L"管理保存流程…");
    auto anchor=GetDlgItem(g->panel,g->structure.enabled?ID_STRUCT_SCHEMES:ID_NAMING_MENU);
    RECT rect{}; GetWindowRect(anchor?anchor:g->panel, &rect);
    SetForegroundWindow(g->panel);
    int selected = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, rect.left, rect.bottom, g->panel);
    DestroyMenu(menu);
    if (selected == 2) addExampleRecipes();
    else if (selected == ID_CAPTURE_RECIPE) SendMessageW(g->panel, WM_COMMAND, ID_CAPTURE_RECIPE, 0);
    else if (selected == 1) { g->managerTab = 2; showManager(); }
    else if (selected >= 4000 && static_cast<size_t>(selected - 4000) < g->cfg.recipes.size())
        SendMessageW(g->panel, WM_COMMAND, selected, 0);
}
LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g && !g->testTargetImage.empty()) {
        static LRESULT paintSerial = 0, accessibilityQueries = 0;
        if (msg == WM_APP+200) return wp ? accessibilityQueries : paintSerial;
        if (msg == WM_PAINT || msg == WM_DRAWITEM) ++paintSerial;
        if (msg == WM_GETOBJECT) ++accessibilityQueries;
    }
    switch (msg) {
    case WM_VSCROLL:
        if (reinterpret_cast<HWND>(lp) == GetDlgItem(hwnd, ID_FOLDER_SCROLL) && !g->busy) {
            SCROLLINFO info{sizeof(info), SIF_TRACKPOS};
            GetScrollInfo(reinterpret_cast<HWND>(lp), SB_CTL, &info);
            int code = LOWORD(wp);
            if (code == SB_LINEUP) --g->locationPage;
            else if (code == SB_LINEDOWN) ++g->locationPage;
            else if (code == SB_PAGEUP) g->locationPage -= 3;
            else if (code == SB_PAGEDOWN) g->locationPage += 3;
            else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) g->locationPage = info.nTrackPos;
            else if (code == SB_TOP) g->locationPage = 0;
            else if (code == SB_BOTTOM) g->locationPage = INT_MAX;
            updatePanelContent();
            return 0;
        }
        break;
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}; ScreenToClient(hwnd, &point);
        RECT folders{px(PANEL_MARGIN), px(g->folderTop), px(g->width - PANEL_MARGIN), px(g->folderTop + g->folderRows * FOLDER_ROW_STEP - 8)};
        if (PtInRect(&folders, point) && !g->busy) {
            g->folderWheel += GET_WHEEL_DELTA_WPARAM(wp);
            int rows = g->folderWheel / WHEEL_DELTA; g->folderWheel %= WHEEL_DELTA;
            if (rows) { g->locationPage -= rows; updatePanelContent(); }
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN: cancelPathEdit(false); closeFlyout(); return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: requestEditorDismiss(true); return 0;
    case WM_EDITOR_DISMISS:
        if ((static_cast<ULONG_PTR>(lp) >> 1) == g->editorRevision)
            finishEditorDismiss(reinterpret_cast<HWND>(wp), (lp & 1) != 0);
        return 0;
    case WM_TIMER:
        if(wp==T_COPY_FEEDBACK){
            KillTimer(hwnd,T_COPY_FEEDBACK);
            setTextIfChanged(GetDlgItem(hwnd,ID_FINAL_NAME_LABEL),L"最终文件名");return 0;
        }
        if (wp == T_EDITOR_DISMISS) {
            KillTimer(hwnd, T_EDITOR_DISMISS);
            finishEditorDismiss(g->pendingEditorDismiss, g->explicitEditorDismiss);
            return 0;
        }
        break;
    case WM_PATH_BLUR:
        if (g->editingPath && (GetForegroundWindow() != hwnd ||
            (GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_INPUT) && GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_EDIT))))
            cancelPathEdit(false);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g->editingPath) PostMessageW(hwnd, WM_PATH_BLUR, 0, 0);
        if (LOWORD(wp) != WA_INACTIVE) rememberNativeSubject();
        if (LOWORD(wp) == WA_INACTIVE)
            requestEditorDismiss((GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000));
        break;
    case WM_CONTEXTMENU: {
        closeFlyout();
        auto h = reinterpret_cast<HWND>(wp);
        if(GetParent(h)==hwnd&&GetDlgCtrlID(h)==ID_PREVIEW){
            bool copyable=g->ruleError.empty()&&!g->namingPending&&structuredInputError().empty()&&validName(g->stemName);
            auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|(copyable?0:MF_GRAYED),ID_COPY_NAME,L"复制完整文件名");
            POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
            if(point.x==-1&&point.y==-1){RECT r{};GetWindowRect(h,&r);point={r.left,r.bottom};}
            SetForegroundWindow(hwnd);
            int command=trackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,hwnd);DestroyMenu(menu);
            if(command)SendMessageW(hwnd,WM_COMMAND,command,0);return 0;
        }
        if (g->busy) return 0;
        if (renameStructuredCandidate(h)) return 0;
        if (GetDlgCtrlID(h) == ID_DIRECTORY) {
            auto menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_EDIT, L"编辑路径");
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_SEARCH, L"搜索位置…");
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (point.x == -1 && point.y == -1) {
                RECT r{}; GetWindowRect(h, &r); point = {r.left, r.bottom};
            }
            SetForegroundWindow(hwnd);
            int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
            DestroyMenu(menu);
            if (command) SendMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        auto found = g->actions.find(GetDlgCtrlID(h));
        if (GetParent(h) != hwnd || found == g->actions.end()) return 0;
        auto action = found->second;
        if (action.kind == Action::Variables) { openVariableEditor(); return 0; }
        bool folder = action.kind == Action::Location;
        if (!folder && action.kind != Action::Prefix && action.kind != Action::Suffix) return 0;
        if (folder && action.index >= g->displayPaths.size()) return 0;
        HMENU menu = CreatePopupMenu();
        if (!folder) AppendMenuW(menu, MF_STRING, 3, L"修改预设");
        HMENU colors = CreatePopupMenu();
        int marked = bubbleColorIndex(action);
        for (int i = 0; i <= PRESET_COLOR_COUNT; ++i)
            AppendMenuW(colors, MF_STRING | (i == marked ? MF_CHECKED : 0), 100 + i, bubbleColors[i].name);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colors), L"颜色标记");
        if (folder) AppendMenuW(menu, MF_STRING, 4,
            isFavoritePath(g->displayPaths[action.index].path) ? L"取消收藏" : L"收藏文件夹");
        else AppendMenuW(menu, MF_STRING, 1, L"移除此预设");
        if (folder && g->locationTab == 2) AppendMenuW(menu, MF_STRING, 6, L"从历史移除");
        if (folder && g->locationTab == 4) AppendMenuW(menu, MF_STRING, 7, L"从最近移除");
        AppendMenuW(menu, MF_STRING, 2, folder ? L"管理收藏…" :
            action.kind == Action::Prefix ? L"管理前缀预设…" : action.kind == Action::Suffix ? L"管理后缀预设…" : L"管理命名预设…");
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (point.x == -1 && point.y == -1) {
            RECT r{}; GetWindowRect(h, &r); point = {r.left, r.bottom};
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
                if (!saved) g->cfg = std::move(previous);
                g->status = saved ? L"已移除泡泡，当前文件名保持不变" : L"移除未保存，原预设已保留";
                g->statusError = !saved; rebuildPanel();
            }
        } else if (command == 2) {
            if (folder) { g->managerTab = 1; showManager(); }
            else {
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
        ClientPaint paint(hwnd, msg, wp); HDC dc = paint.dc();
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
            SelectObject(dc, oldPen); SelectObject(dc, oldBrush); DeleteObject(track);
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
        SelectObject(dc, op); DeleteObject(pen);
        for (auto &[rect, text, heading] : g->labels) {
            auto copy = rect;
            SelectObject(dc, heading ? g->boldFont : g->smallFont);
            SetTextColor(dc, heading ? g->fg : g->muted);
            auto separator = text.find(L" · ");
            if (heading && rect.top < px(12) && separator != text.npos) {
                auto brand = text.substr(0, separator), caption = text.substr(separator+3);
                DrawTextW(dc, brand.c_str(), -1, &copy, DT_SINGLELINE | DT_VCENTER);
                SIZE size{}; GetTextExtentPoint32W(dc, brand.c_str(), static_cast<int>(brand.size()), &size);
                copy.left += size.cx + px(10);
                SelectObject(dc, g->smallFont); SetTextColor(dc, g->muted);
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
        if(windowClass(reinterpret_cast<HWND>(lp))==L"Edit"){
            SetTextColor(reinterpret_cast<HDC>(wp),g->fg);SetBkColor(reinterpret_cast<HDC>(wp),g->field);
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
        if (d->CtlType == ODT_COMBOBOX) return drawDropdownItem(*d);
        if (d->CtlType == ODT_STATIC) {
            DWORD_PTR textStyle = 0;
            if (!GetWindowSubclass(d->hwndItem, MaterialControlProc, 2, &textStyle)) return FALSE;
            PaintBuffer buffer(d->hDC, d->rcItem, panelComposited(d->hwndItem) && IsWindowVisible(g->panel));
            paintMaterialText(d->hwndItem, buffer.dc, d->rcItem, static_cast<DWORD>(textStyle));
            return TRUE;
        }
        if (d->CtlType != ODT_BUTTON)
            return FALSE;
        DRAWITEMSTRUCT item = *d;
        PaintBuffer buffer(item.hDC, item.rcItem, panelComposited(item.hwndItem) && IsWindowVisible(g->panel)); item.hDC = buffer.dc; d = &item;
        if(drawSubjectGroup(*d))return TRUE;
        auto text = windowText(d->hwndItem);
        const auto &actions = GetParent(d->hwndItem) == g->flyout ? g->flyoutActions : g->actions;
        bool isPill = actions.contains(static_cast<int>(d->CtlID));
        bool variableChip = GetParent(d->hwndItem) == g->panel && g->variableEditor &&
                            d->CtlID >= ID_VARIABLE_TOKEN_FIRST && d->CtlID < ID_VARIABLE_TOKEN_FIRST + g->variableTokens.size();
        RECT r = d->rcItem;
        FillRect(d->hDC, &r, g->brush);
        DWORD_PTR dropdownFeedback = 0;
        GetWindowSubclass(d->hwndItem, PillProc, 1, &dropdownFeedback);
        bool hover = (dropdownFeedback & 1) != 0;
        bool selected = false;
        bool namingView = d->CtlID == ID_STRUCT_QUICK_VIEW || d->CtlID == ID_STRUCT_FIELDS_VIEW;
        if (namingView) selected = d->CtlID == ID_STRUCT_QUICK_VIEW ? quickNamingView() : !quickNamingView();
        bool quickPick=d->CtlID>=ID_QUICK_PICK&&d->CtlID<ID_QUICK_PICK+48;
        if(quickPick){int index=(d->CtlID-ID_QUICK_PICK)/6,slot=(d->CtlID-ID_QUICK_PICK)%6;
            if(static_cast<size_t>(index)<g->structure.fields.size()&&g->quickPicks.contains(index)&&static_cast<size_t>(slot)<g->quickPicks[index].size())
                selected=g->quickPicks[index][slot].value==fieldValue(g->structure.fields[index],g->cfg,g->structure.dateFormat);
        }
        bool structuredField=d->CtlID>=ID_STRUCT_FIELD&&d->CtlID<ID_STRUCT_FIELD+g->structure.fields.size();
        if(structuredField)selected=static_cast<int>(d->CtlID)-ID_STRUCT_FIELD==g->structuredField;
        if(d->CtlID>=ID_STRUCT_CHOICE&&d->CtlID<ID_STRUCT_CHOICE+g->structuredChoices.size()&&
           g->structuredField>=0&&static_cast<size_t>(g->structuredField)<g->structure.fields.size())
            selected=g->structuredChoices[d->CtlID-ID_STRUCT_CHOICE].value==fieldValue(g->structure.fields[g->structuredField],g->cfg,g->structure.dateFormat);
        if (isPill) {
            auto action = actions.at(static_cast<int>(d->CtlID));
            if (action.kind == Action::Location)
                selected = action.index < g->displayPaths.size() &&
                           normalize(g->displayPaths[action.index].path) == normalize(g->dialog.directory);
            else if (action.kind == Action::Folder) selected = g->folderName;
            else if (action.kind == Action::Variables) selected = g->variablesEnabled;
            else if (action.kind == Action::Prefix)
                selected = g->parts.prefix == expandAffix(g->cfg.prefixes[action.index], namingOptions().dateFormat);
            else if (action.kind == Action::Suffix) {
                selected = g->parts.suffix == expandAffix(g->cfg.suffixes[action.index], namingOptions().dateFormat);
                auto value=expandAffix(g->cfg.suffixes[action.index],namingOptions().dateFormat);
                if(auto field=structuredSuffixField(g->structure,g->cfg,value))selected=selected||fieldValue(*field,g->cfg,g->structure.dateFormat)==value.substr(1);
            }
        }
        if (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY)
            selected = static_cast<int>(d->CtlID) - ID_LOC_RECOMMENDED == g->locationTab;
        if (d->CtlID == ID_LOC_RECENT) selected = g->locationTab == 4;
        bool affixTool=d->CtlID==ID_NAMING_MENU||d->CtlID==ID_AFFIX_SUMMARY;
        bool replaceTool=d->CtlID==ID_RENAME||d->CtlID==ID_REPLACE_SUMMARY;
        bool tool = affixTool||replaceTool;
        bool active = affixTool ? !g->parts.prefix.empty() || !g->parts.suffix.empty() :
                      replaceTool ? g->ruleEnabled && !g->rulePattern.empty() : false;
        int toolIndex = affixTool ? 2 : 3;
        if (tool) selected = g->inlineTool == toolIndex || active;
        if (d->CtlID == ID_RULE_ENABLE) selected = g->ruleEnabled;
        if (variableChip) {
            auto keys = expressionKeys(g->variableTemplateDraft);
            selected = std::find(keys.begin(), keys.end(), g->variableTokens[d->CtlID - ID_VARIABLE_TOKEN_FIRST]) != keys.end();
        }
        if (d->CtlID == ID_VARIABLE_ADVANCED) selected = g->variableAdvanced;
        bool primary = d->CtlID == ID_SUBMIT || d->CtlID == ID_PRESET_SAVE || d->CtlID == ID_VARIABLE_SAVE;
        bool disabled = (d->itemState & ODS_DISABLED) != 0;
        hover = hover && !disabled;
        bool locationTab = (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY) || d->CtlID == ID_LOC_RECENT;
        bool panelButton = GetParent(d->hwndItem) == g->panel;
        bool locationButton = isPill && actions.at(static_cast<int>(d->CtlID)).kind == Action::Location;
        bool subjectButton = d->CtlID >= ID_QUICK_EDIT && d->CtlID < ID_QUICK_EDIT + 8;
        bool framed = namingView || quickPick || subjectButton || structuredField || d->CtlID == ID_NAME_MANAGER;
        bool quiet = (!isPill || d->CtlID == ID_TEMPLATE_APPLY) && !variableChip && !primary;
        bool menuOpen = g->openPanelMenu == static_cast<int>(d->CtlID);
        bool keyboardFocus = !disabled && (dropdownFeedback & 2) && (d->itemState & ODS_FOCUS);
        if(d->CtlID==ID_CAPABILITIES){
            auto font=SelectObject(d->hDC,g->smallFont);SetBkMode(d->hDC,TRANSPARENT);
            SetTextColor(d->hDC,RGB(124,130,140));
            RECT caption=r;caption.left+=px(2);
            DrawTextW(d->hDC,text.c_str(),-1,&caption,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
            if(keyboardFocus){RECT focus=r;InflateRect(&focus,-px(1),-px(1));DrawFocusRect(d->hDC,&focus);}
            SelectObject(d->hDC,font);return TRUE;
        }
        int panelHover=panelButton&&!disabled?hoverAmount(d->hwndItem,hover):0;
        auto appearance=panelButton?panelAppearance(static_cast<int>(d->CtlID),menuOpen,!disabled,panelHover,(d->itemState&ODS_SELECTED)!=0):std::nullopt;
        COLORREF fill = controlFill(d->CtlID == ID_TEMPLATE_APPLY ? g->bg : primary ? g->surface : framed ? g->field : isPill || variableChip ? g->surface : g->bg,
                                   selected || menuOpen, hover, (d->itemState & ODS_SELECTED) != 0, primary, !disabled);
        if (panelButton) {
            COLORREF base = quickPick || structuredField || subjectButton || locationButton ? RGB(29,35,45) :
                g->bg;
            fill = controlFill(base, selected || menuOpen, false, (d->itemState & ODS_SELECTED) != 0, primary, !disabled);
            if(quickPick&&selected&&!disabled)fill=controlFill(RGB(39,64,99),false,false,(d->itemState&ODS_SELECTED)!=0,false,true);
            if (namingView) fill = controlFill(selected ? RGB(43,51,64) : g->bg, false, false,
                (d->itemState & ODS_SELECTED) != 0, false, !disabled);
            if (locationTab) {
                auto track = CreateSolidBrush(g->field);
                FillRect(d->hDC, &r, track); DeleteObject(track);
                fill = selected ? g->surface : g->field;
            }
            if (panelHover) fill = mixColor(fill, RGB(255,255,255), panelHover*(selected||primary?5:7)/100);
        }
        int color = isPill ? bubbleColorIndex(actions.at(static_cast<int>(d->CtlID))) : 0;
        COLORREF mark = bubbleColors[color].value;
        if (color && !selected && !hover && !disabled)
            fill = RGB((GetRValue(fill) * 9 + GetRValue(mark)) / 10,
                       (GetGValue(fill) * 9 + GetGValue(mark)) / 10,
                       (GetBValue(fill) * 9 + GetBValue(mark)) / 10);
        if(appearance)fill=appearance->fill;
        auto brush = CreateSolidBrush(fill);
        COLORREF edge = fill;
        bool nameSelected = !disabled && isPill && selected && actions.at(static_cast<int>(d->CtlID)).kind != Action::Location;
        if (panelButton && !disabled && ((quickPick && selected) || (namingView && selected)))
            edge = namingView ? RGB(61,71,87) : mixColor(g->selection, g->accent, 30);
        if (panelButton && locationTab && selected && !disabled) edge = mixColor(g->surface, g->muted, 18);
        if(panelButton&&!disabled){
            if(quickPick||structuredField)edge=selected?RGB(82,126,187):RGB(46,56,71);
            if(subjectButton)edge=RGB(59,74,94);
            if(locationButton&&!selected)edge=RGB(43,52,66);
        }
        if(appearance)edge=appearance->edge;
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : nameSelected ? mixColor(g->selection, g->accent, 50) : edge);
        auto ob = SelectObject(d->hDC, brush), op = SelectObject(d->hDC, pen);
        int radius = px(panelButton && locationTab ? 12 : panelButton ? 10 : 12);
        paintRoundedRect(d->hDC, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(d->hDC, op);
        SelectObject(d->hDC, ob);
        DeleteObject(pen);
        DeleteObject(brush);
        if (d->CtlID == ID_DIRECTORY) {
            auto font = SelectObject(d->hDC, g->smallFont); SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, hover || (d->itemState & ODS_FOCUS) ? g->fg : g->muted);
            RECT textRect = r; textRect.right -= px(4);
            auto caption=trailingPath(d->hDC,text,textRect.right-textRect.left);
            DrawTextW(d->hDC, caption.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, font); return TRUE;
        }
        if (d->CtlID == ID_PANEL_OPTIONS || d->CtlID == ID_DIRECTORY_EDIT || d->CtlID == ID_DIRECTORY_SEARCH ||
            d->CtlID == ID_CLOSE || d->CtlID == ID_MAIN_ADD) {
            UiIcon icon = d->CtlID == ID_PANEL_OPTIONS ? UiIcon::Settings : d->CtlID == ID_DIRECTORY_EDIT ? UiIcon::Edit :
                          d->CtlID == ID_DIRECTORY_SEARCH ? UiIcon::Search :
                          d->CtlID == ID_CLOSE ? UiIcon::Close : UiIcon::Plus;
            int size = px(d->CtlID == ID_CLOSE ? 13 : 14);
            drawUiIcon(d->hDC, icon, (r.left + r.right - size) / 2.f,
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size),
                       disabled?RGB(96,106,122):hover?g->fg:
                       d->CtlID==ID_DIRECTORY_EDIT||d->CtlID==ID_DIRECTORY_SEARCH?RGB(143,178,225):g->muted);
            return TRUE;
        }
        if (d->CtlID == ID_FOCUS_NAME) {
            auto documentPen = CreatePen(PS_SOLID, std::max(1, px(1)), g->muted);
            auto oldPen = SelectObject(d->hDC, documentPen);
            int x = (r.right - px(12)) / 2, y = (r.bottom - px(14)) / 2;
            POINT document[]{{x,y},{x+px(7),y},{x+px(12),y+px(5)},
                             {x+px(12),y+px(14)},{x,y+px(14)},{x,y}};
            Polyline(d->hDC, document, 6);
            POINT fold[]{{x+px(7),y},{x+px(7),y+px(5)},{x+px(12),y+px(5)}};
            Polyline(d->hDC, fold, 3);
            SelectObject(d->hDC, oldPen); DeleteObject(documentPen);
            return TRUE;
        }
        if (d->CtlID == ID_FAVORITE_CURRENT) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Star, (r.left + r.right - size) / 2.f,
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), RGB(230, 184, 72), text == L"已收藏");
            return TRUE;
        }
        bool mainBody = d->hwndItem && GetParent(d->hwndItem) == g->panel &&
                        d->CtlID != ID_MINIMIZE && d->CtlID != ID_CLOSE;
        auto of = SelectObject(d->hDC, primary || subjectButton ? g->boldFont : panelButton && (locationTab || namingView || locationButton || d->CtlID==ID_STRUCT_SCHEMES || d->CtlID==ID_STRUCT_NAME || d->CtlID>=ID_NUMBER_MENU&&d->CtlID<=ID_RULES_SUMMARY || d->CtlID==ID_COMBO_UNDO || d->CtlID==ID_COMBO_REDO || d->CtlID==ID_CLEAR_RECENT || d->CtlID==ID_CLEAR_HISTORY) ? g->smallFont : tool || d->CtlID == ID_CLOSE ? g->font : mainBody ? g->bodyFont :
                               primary ? g->boldFont : framed ? g->font : quiet ? g->smallFont : g->font);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, disabled ? g->muted : primary ? RGB(255, 255, 255) :
                     selected ? g->fg : tool || (quiet && !framed) ? g->muted : g->fg);
        if (d->CtlID == ID_FAVORITE_CURRENT) SetTextColor(d->hDC, RGB(230, 184, 72));
        if (locationTab && !selected) SetTextColor(d->hDC, g->muted);
        if(appearance)SetTextColor(d->hDC,appearance->ink);
        RECT t = r;
        t.left += px(6);
        t.right -= px(6);
        if(d->CtlID==ID_TARGET_INFO){
            auto split=text.find(L'\n');auto title=text.substr(0,split),detail=split==text.npos?L"":text.substr(split+1);
            RECT top=t,bottom=t;top.top+=px(5);top.bottom=top.top+px(21);bottom.top=top.bottom;bottom.bottom-=px(5);
            SelectObject(d->hDC,g->smallFont);SetTextColor(d->hDC,disabled?g->muted:RGB(203,182,145));
            DrawTextW(d->hDC,title.c_str(),-1,&top,DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER|DT_NOPREFIX);
            SetTextColor(d->hDC,g->muted);DrawTextW(d->hDC,detail.c_str(),-1,&bottom,DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER|DT_NOPREFIX);
            SelectObject(d->hDC,of);return TRUE;
        }
        if(appearance){
            bool compact=d->CtlID==ID_COMBO_UNDO||d->CtlID==ID_COMBO_REDO||d->CtlID==ID_STRUCT_SCHEMES;
            int size=px(compact?12:14);
            drawUiIcon(d->hDC,appearance->icon,static_cast<float>(r.left+px(compact?7:8)),
                (r.top+r.bottom-size)/2.f,static_cast<float>(size),appearance->iconInk);
            t.left=r.left+px(compact?24:28);
            if(appearance->dropdown){
                if(!text.empty()&&text.back()==L'▾')text=trim(text.substr(0,text.size()-1));
                if(d->CtlID==ID_STRUCT_SCHEMES)text=L"命名设置";
                drawUiIcon(d->hDC,menuOpen?UiIcon::ChevronUp:UiIcon::Chevron,static_cast<float>(r.right-px(19)),
                    (r.top+r.bottom-px(12))/2.f,static_cast<float>(px(12)),appearance->iconInk);
                t.right=r.right-px(23);
            }
        }
        if (d->CtlID == ID_CLOSE) t = r;
        if(d->CtlID>=ID_QUICK_CHOICE&&d->CtlID<ID_QUICK_CHOICE+8&&primarySubject(d->CtlID-ID_QUICK_CHOICE)){
            int size=px(14);drawUiIcon(d->hDC,menuOpen?UiIcon::ChevronUp:UiIcon::Chevron,
                (r.left+r.right-size)/2.f,(r.top+r.bottom-size)/2.f,static_cast<float>(size),
                disabled?g->muted:hover||menuOpen?g->fg:g->muted);
            SelectObject(d->hDC,of);return TRUE;
        }
        if (subjectButton) {
            t.left += px(6); t.right -= px(24);
            drawUiIcon(d->hDC, UiIcon::Edit, static_cast<float>(r.right-px(22)), (r.top+r.bottom-px(12))/2.f,
                static_cast<float>(px(12)), disabled?g->muted:hover?g->fg:RGB(143,178,225));
        }
        if(quickPick&&selected&&!disabled){
            drawUiIcon(d->hDC,UiIcon::Check,static_cast<float>(r.left+px(4)),(r.top+r.bottom-px(10))/2.f,
                static_cast<float>(px(10)),RGB(167,203,252));t.left+=px(10);
        }
        if (locationButton) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Folder, static_cast<float>(r.left + px(color ? 18 : 8)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), selected ? g->accent : g->muted);
            t.left = r.left + px(color ? 37 : 27);
        }
        if (color) {
            auto dot = CreateSolidBrush(mark);
            auto oldBrush = SelectObject(d->hDC, dot);
            auto oldPen = SelectObject(d->hDC, GetStockObject(NULL_PEN));
            int cy = (r.top + r.bottom) / 2;
            Ellipse(d->hDC, r.left + px(6), cy - px(3), r.left + px(12), cy + px(3));
            SelectObject(d->hDC, oldPen); SelectObject(d->hDC, oldBrush); DeleteObject(dot);
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
            (g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.find(L"已打开") != std::wstring::npos||
             g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(L"命名推荐"))) {
            RECT nameRect = t, openedRect = t;
            nameRect.bottom = t.top + px(15);
            openedRect.top = nameRect.bottom - px(1);
            DrawTextW(d->hDC, text.c_str(), -1, &nameRect, DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(d->hDC, g->muted);
            auto badge=g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(L"命名推荐")?L"命名推荐":L"已打开";
            DrawTextW(d->hDC, badge, -1, &openedRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if(structuredField&&!g->structure.compact){
            auto split=text.find(L'\n');auto caption=text.substr(0,split),value=split==text.npos?L"":text.substr(split+1);
            RECT top=t,bottom=t;top.bottom=t.top+px(17);bottom.top=top.bottom;
            SelectObject(d->hDC,g->smallFont);SetTextColor(d->hDC,g->muted);
            DrawTextW(d->hDC,caption.c_str(),-1,&top,DT_SINGLELINE|DT_END_ELLIPSIS|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
            SelectObject(d->hDC,g->font);SetTextColor(d->hDC,g->fg);
            DrawTextW(d->hDC,value.c_str(),-1,&bottom,DT_SINGLELINE|DT_END_ELLIPSIS|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
        }else DrawTextW(d->hDC, text.c_str(), -1, &t,
                  DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX | (locationButton || subjectButton || appearance ? DT_LEFT : DT_CENTER));
        SelectObject(d->hDC, of);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (!g->building && code == BN_CLICKED && !insideTransientEditor(GetDlgItem(hwnd, id)))
            requestEditorDismiss(true);
        if(!g->building && structuredCommand(id,code))return 0;
        if (id >= 5000 && static_cast<size_t>(id - 5000) < g->cfg.projects.size() && code == BN_CLICKED) {
            handleProjectChoice(id - 5000 + 100); return 0;
        }
        if (id >= 4000 && static_cast<size_t>(id - 4000) < g->cfg.recipes.size() && code == BN_CLICKED) {
            perform({Action::Recipe, static_cast<size_t>(id - 4000)}, false); return 0;
        }

        if (g->building)
            return 0;
        if (code == BN_CLICKED && id != ID_DIRECTORY &&
            id != ID_PRESET_SAVE && id != ID_PRESET_CANCEL) closeFlyout(true);
        auto action = g->actions.find(id);
        if (action != g->actions.end() && (code == BN_CLICKED || code == BN_DOUBLECLICKED)) {

            if (code == BN_DOUBLECLICKED && (action->second.kind == Action::Prefix ||
                action->second.kind == Action::Suffix)) return 0;

            bool quick = code == BN_DOUBLECLICKED || (GetKeyState(VK_CONTROL) & 0x8000);
            perform(action->second, quick);
            return 0;
        }
        if (code == EN_SETFOCUS && (id == ID_RULE_PATTERN || id == ID_RULE_REPLACEMENT ||
            id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX)) checkpointNaming();
        if (id == ID_FREE_NAME_INPUT && code == EN_CHANGE) {
            if(g->structure.enabled&&!g->structure.free)enterFreeNaming();
            g->stemName = windowText(GetDlgItem(hwnd, ID_FREE_NAME_INPUT));
         g->ruleEnabled = false; g->ruleError.clear();
            resetSmartNaming();
            if(!g->structure.enabled)g->namingHistory.clear();
            syncNameParts();
            g->dirtyName = true;
            g->localNamePending = true;
            if (g->busy) g->applyFocus = GetDlgItem(hwnd, ID_FREE_NAME_INPUT);
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
            if (g->rulePattern.empty()) g->ruleEnabled = false;
            else if (wasEmpty) g->ruleEnabled = true;
            updateInlineRule(); return 0;
        }
        if ((id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX) && code == EN_CHANGE) {
            auto value = windowText(GetDlgItem(hwnd, id));
            bool prefix = id == ID_AFFIX_PREFIX;
            if (!value.empty() && !validAffix(value, prefix)) {
                invalidateNaming();g->statusError=true;
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
                g->ruleIndex = index; const auto &rule = renameRules()[index];
                g->rulePattern = rule.pattern; g->ruleReplacement = rule.replacement; g->ruleRegex = rule.regex;
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
            case ID_ROUTE_SUGGEST:showSuggestedLocations();break;
            case ID_TARGET_INFO:showTargetActions();break;
            case ID_CLEAR_RECENT:
                if (g->locationTab == 4 && !g->busy) { closeFlyout(); removeRecentLocations(); }
                break;
            case ID_CLEAR_HISTORY:
                if (g->locationTab == 2 && !g->busy) { closeFlyout(); removeHistoryLocation(); }
                break;
            case ID_PANEL_OPTIONS: showPanelMenu(ID_PANEL_OPTIONS); break;
            case ID_CAPABILITIES: showCapabilityMenu(); break;
            case ID_RULES_SUMMARY: showRuleSummaryMenu(); break;
            case ID_NUMBER_MENU: showNumberMenu(); break;
            case ID_AFFIX_SUMMARY: SendMessageW(hwnd,WM_COMMAND,ID_NAMING_MENU,0); break;
            case ID_REPLACE_SUMMARY: SendMessageW(hwnd,WM_COMMAND,ID_RENAME,0); break;
            case ID_PANEL_SETTINGS: g->managerTab = 3; showManager(); break;
            case ID_BIND_CONTROL: startBinding(); break;
            case ID_COPY_NAME:
                if (g->ruleError.empty() && !g->namingPending && structuredInputError().empty() && validName(g->stemName)) {
                    if (copyWorkflowText(g->stemName + g->dialog.extension)) {
                        setTextIfChanged(GetDlgItem(hwnd,ID_FINAL_NAME_LABEL),L"最终文件名 · 已复制");
                        SetTimer(hwnd,T_COPY_FEEDBACK,1600,nullptr);
                    } else workflowNotice(L"剪贴板暂时不可用，请重试。");
                }
                break;
            case ID_GENERIC_SUBMIT:
                if (g->dialog.generic && g->dialog.genericSubmit) apply(g->dialog.directory, g->stemName, true, true);
                break;
            case ID_FOCUS_NAME:
                if(!g->structure.free)structuredCommand(ID_STRUCT_FREE,0);
                SetFocus(GetDlgItem(hwnd,ID_FREE_NAME_INPUT));break;
            case ID_DIRECTORY: beginPathEdit(); break;
            case ID_DIRECTORY_EDIT:
                if (!g->editingPath) beginPathEdit();
                else if (IsWindowVisible(g->panel)) SetFocus(GetDlgItem(hwnd, ID_DIRECTORY_INPUT));
                break;
            case ID_DIRECTORY_CANCEL: cancelPathEdit(); break;
            case ID_DIRECTORY_SEARCH: cancelPathEdit(); showLocationFlyout(); break;
            case ID_LOCATION_PAGE: cancelPathEdit(); showLocationFlyout(true); break;
            case ID_NATIVE_FORMAT:
                if (g->attached && !g->busy) {
                    SetForegroundWindow(g->dialog.hwnd);
                    g->status = L"请在原软件中选择保存格式"; updatePanelContent();
                    enqueue({JobType::FocusFormat, g->dialog.hwnd, g->dialog.pid, g->generation.load()});
                }
                break;
            case ID_LOCATION_MENU:
            case ID_FILENAME_MENU:
                showPanelMenu(id);
                break;
            case IDOK:
                SendMessageW(hwnd, WM_COMMAND, ID_SUBMIT, 0);
                break;
            case IDCANCEL:
                SetForegroundWindow(g->dialog.hwnd);
                break;
            case ID_MINIMIZE:
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
                g->managerTab=0;showManager();break;
            case ID_ADD_PREFIX:
            case ID_ADD_SUFFIX:
                g->namingMode = id == ID_ADD_PREFIX ? 1 : 2; openInlinePreset(NEW_PRESET, GetDlgItem(hwnd, id)); break;
            case ID_PRESET_SAVE: saveInlinePreset(); break;
            case ID_PRESET_CANCEL:
                closeFlyout(true);
                break;
            case ID_NAMING_MENU:
            case ID_RENAME: {
                if(!structuredInputError().empty()||g->structuredIme)break;
                resetStructuredInput();int tool=id==ID_NAMING_MENU?2:3;
                g->inlineTool=g->inlineTool==tool?0:tool;closeFlyout();rebuildPanel();break;
            }
            case ID_RULE_ENABLE:
                checkpointNaming(); g->ruleEnabled = !g->ruleEnabled && !g->rulePattern.empty(); updateInlineRule(); break;
            case ID_RULE_CLEAR:
                checkpointNaming(); g->rulePattern.clear(); g->ruleReplacement.clear(); g->ruleEnabled = false;
                g->ruleIndex = 1; g->ruleRegex = false;
                updateInlineRule(); rebuildPanel(); break;
            case ID_VARIABLE_EDIT: openVariableEditor(); break;
            case ID_FAVORITE_CURRENT: {
                if (!g->dialog.directory.empty()) {
                    auto name = std::filesystem::path(g->dialog.directory).filename().wstring();
                    toggleFavoriteLocation({name.empty() ? g->dialog.directory : name, g->dialog.directory, L"收藏"});
                }
                break;
            }
            case ID_LOC_PREVIOUS:
            case ID_LOC_NEXT:
                g->locationPage += id == ID_LOC_NEXT ? 1 : -1;
                updatePanelContent();
                break;
            case 720: handleProjectChoice(2); break;
            case 721: handleProjectChoice(3); break;
            case 722: handleProjectChoice(4); break;
            case 723: handleProjectChoice(5); break;
            case 724: handleProjectChoice(1); break;
            case 730: transferPresets(false); break;
            case 731: transferPresets(true); break;
            case ID_REFRESH_NUMBER: updateInlineRule(); break;
            case ID_PROJECTS: showProjects(); break;
            case ID_RECORDS: showSavedRecords(); break;
            case ID_EXPLAIN: showNameExplanation(); break;
            case ID_TRANSFER: showTransfer(); break;
            case ID_DIAGNOSTICS: showDiagnostics(); break;
            case ID_CAPTURE_RECIPE: saveCurrentRecipe(); break;
            case ID_PLANS: showRecipeMenu(); break;
            case ID_SUBMIT:
                if (!g->ruleError.empty()) { g->status = g->ruleError; updatePanelContent(); break; }
                if (g->editingPath) { commitPathEdit(); break; }
                KillTimer(g->control, T_ACTION);
                apply(g->dialog.directory, g->stemName, !g->dialog.generic);
                break;
            case ID_COMBO_UNDO: moveNamingHistory(false); break;
            case ID_COMBO_REDO: moveNamingHistory(true); break;
            case ID_UNDO:
                if(g->structuredIme)break;
                checkpointNaming();  g->ruleEnabled = false; g->ruleError.clear();
                {
                    bool structured=g->structure.enabled;
                    g->freeRecovery.reset();resetStructuredInput();g->inlineTool=0;g->structuredAdvanced=false;
                    if(structured){
                        for(auto &f:g->structure.fields){f.value.clear();f.source=0;f.omitted=false;if(f.id==g->structure.primaryField)f.value=g->originalStem;}
                        g->structure.free=false;g->structure.literal=true;g->structure.literalName=g->originalStem;
                    }else g->structure={};
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

constexpr int M_TAB=200,M_LIST=202,M_NAME=203,M_VALUE=204,
              M_SAVE=207,M_DELETE=208,M_UP=209,M_ADD=210,M_FOLDER=218,
              M_CLEAR_HISTORY=219,M_AUTO=220,M_QUICK=221,M_LAYOUT=222,
              M_OPTIONS_SAVE=223,M_STARTUP=224,M_CAPTURE=228,M_AFFIX_PREVIEW=229,
              M_AFFIX_STATUS=230,M_AFFIX_CATEGORY=252,M_COPY_SCHEME=254,M_APPLY_SCHEME=255,M_SCHEME_DEFAULT=256,
              M_INFO=280,M_PROJECT_USE=281,M_PROJECT_NONE=282,M_PROJECT_WORK=283,M_PROJECT_DELIVERY=284,
              M_EXPORT=285,M_IMPORT=286,M_BACKUP=287,M_RESTORE=288,M_COPY_DIAGNOSTICS=289,M_BIND=290,M_UNBIND=291,
              M_FEEDBACK=292;
constexpr int MANAGER_WIDTH = 520, MANAGER_HEIGHT = 400;
LRESULT CALLBACK ManagerCheckProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    DWORD_PTR original = feedback;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000))) feedback |= 2;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS) feedback &= ~DWORD_PTR(2);
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp); auto dc = paint.dc();
        RECT r{}; GetClientRect(h, &r); FillRect(dc, &r, g->brush);
        bool checked = SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
        bool enabled = IsWindowEnabled(h) != FALSE;
        bool hover = enabled && (feedback & 1);
        bool focus = enabled && (feedback & 2) && GetFocus() == h;
        auto fill = CreateSolidBrush(controlFill(g->field, checked, hover, false, false, enabled));
        auto pen = CreatePen(PS_SOLID, 1, focus ? g->accent : checked ? mixColor(g->selection, g->accent, 50) :
                             hover ? mixColor(g->line, g->muted, 35) : RGB(52, 64, 82));
        auto ob = SelectObject(dc, fill), op = SelectObject(dc, pen);
        int top = (r.bottom - px(14)) / 2;
        paintRoundedRect(dc, 0, top, px(14), top + px(14), px(5), px(5));
        SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen); DeleteObject(fill);
        if (checked) drawUiIcon(dc, UiIcon::Check, 0, static_cast<float>(top), static_cast<float>(px(14)), enabled ? g->fg : g->muted);
        auto font = SelectObject(dc, g->font); SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, enabled ? g->fg : g->muted); r.left += px(22);
        auto text = windowText(h); DrawTextW(dc, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    auto check = msg == BM_SETCHECK ? SendMessageW(h, BM_GETCHECK, 0, 0) : 0;
    auto result = DefSubclassProc(h, msg, wp, lp);
    bool changed = false;
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
        feedback |= 1; changed = true;
    }
    if (msg == WM_MOUSELEAVE && (feedback & 1)) { feedback &= ~DWORD_PTR(1); changed = true; }
    if (changed || original != feedback) SetWindowSubclass(h, ManagerCheckProc, 1, feedback);
    if ((msg == BM_SETCHECK && check != static_cast<LRESULT>(wp)) || original != feedback || changed)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
LRESULT CALLBACK ManagerTabProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR hovered) {
    DWORD_PTR previous = hovered;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000))) hovered |= 256;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS) hovered &= ~DWORD_PTR(256);
    if (previous != hovered) {
        SetWindowSubclass(h, ManagerTabProc, 1, hovered);
        InvalidateRect(h, nullptr, FALSE);
    }
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp); auto dc = paint.dc();
        RECT r{}; GetClientRect(h, &r); FillRect(dc, &r, g->brush);
        auto font = SelectObject(dc, g->font); SetBkMode(dc, TRANSPARENT);
        int selected = TabCtrl_GetCurSel(h);
        for (int i = 0; i < TabCtrl_GetItemCount(h); ++i) {
            RECT item{}; TabCtrl_GetItemRect(h, i, &item);
            bool focus = i == selected && (hovered & 256) && GetFocus() == h;
            COLORREF color = controlFill(g->bg, i == selected, (hovered & 255) == static_cast<DWORD_PTR>(i + 1),
                                        false, false, IsWindowEnabled(h));
            auto fill = CreateSolidBrush(color); auto pen = CreatePen(PS_SOLID, 1, focus ? g->accent : color);
            auto ob = SelectObject(dc, fill), op = SelectObject(dc, pen);
            paintRoundedRect(dc, item.left, item.top, item.right, item.bottom, px(8), px(8));
            SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen); DeleteObject(fill);
            wchar_t text[80]{}; TCITEMW value{}; value.mask = TCIF_TEXT; value.pszText = text; value.cchTextMax = 80;
            bool advanced=i==MANAGER_TEMPLATES||i==MANAGER_BACKUP||i==MANAGER_CONNECTIONS;
            TabCtrl_GetItem(h, i, &value); SetTextColor(dc, i == selected || !advanced ? g->fg : g->muted);
            item.left+=px(12);
            if(advanced){
                RECT badge=item;badge.left=badge.right-px(30);badge.right-=px(4);
                auto previousFont=SelectObject(dc,g->smallFont);SetTextColor(dc,g->muted);
                DrawTextW(dc,L"高级",-1,&badge,DT_SINGLELINE|DT_RIGHT|DT_VCENTER);
                SelectObject(dc,previousFont);SetTextColor(dc,i==selected?g->fg:g->muted);item.right-=px(31);
            }
            DrawTextW(dc, text, -1, &item, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
        }
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    auto result = DefSubclassProc(h, msg, wp, lp);
    bool changed = false;
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        TCHITTESTINFO hit{}; hit.pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        DWORD_PTR next = msg == WM_MOUSELEAVE ? 0 : static_cast<DWORD_PTR>(TabCtrl_HitTest(h, &hit) + 1);
        if (next != (hovered & 255)) {
            SetWindowSubclass(h, ManagerTabProc, 1, (hovered & ~DWORD_PTR(255)) | next); changed = true;
            if (msg == WM_MOUSEMOVE && !(hovered & 255)) {
                TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0}; TrackMouseEvent(&tracking);
            }
        }
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_KEYDOWN || changed || msg == TCM_SETCURSEL)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
void resizeManager(int height) {
    RECT frame{0, 0, px(MANAGER_SHELL_WIDTH), px(std::max(430, height))};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(GetWindowLongPtrW(g->manager, GWL_STYLE)), FALSE,
                           WS_EX_APPWINDOW, g->dpi);
    RECT current{}; GetWindowRect(g->manager, &current);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(g->manager, MONITOR_DEFAULTTONEAREST), &monitor);
    int width = frame.right - frame.left, total = frame.bottom - frame.top;
    total=std::min<int>(total,monitor.rcWork.bottom-monitor.rcWork.top);
    int left = std::clamp<int>(current.left, monitor.rcWork.left, std::max<int>(monitor.rcWork.left, monitor.rcWork.right - width));
    int top = std::clamp<int>(current.top, monitor.rcWork.top, std::max<int>(monitor.rcWork.top, monitor.rcWork.bottom - total));
    if (current.left != left || current.top != top || current.right - current.left != width || current.bottom - current.top != total)
        SetWindowPos(g->manager, nullptr, left, top, width, total, SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(g->manager, nullptr, TRUE);
}
bool managingAffixes() { return g->managerTab==MANAGER_AFFIXES; }

bool managingPrefixes() { return g->managerTab == MANAGER_AFFIXES && g->affixCategory == 0; }
std::vector<std::wstring> &managerAffixes() {
    return managingPrefixes() ? g->cfg.prefixes : g->cfg.suffixes;
}
void updateAffixManagerPreview() {
    if (!g->manager || !managingAffixes()) return;
    auto value = windowText(GetDlgItem(g->manager, M_VALUE));
    bool prefix = managingPrefixes();
    bool valid = validAffix(value, prefix);
    auto preview = valid ? L"示例：" + (prefix ? expandAffix(value, g->cfg.dateFormat) + L"文件名" : L"文件名" + expandAffix(value, g->cfg.dateFormat)) :
        L"请输入有效内容，不能包含路径字符。";
    SetDlgItemTextW(g->manager, M_AFFIX_PREVIEW, preview.c_str());
    const auto &values = managerAffixes();
    EnableWindow(GetDlgItem(g->manager, M_SAVE), valid);
    EnableWindow(GetDlgItem(g->manager, M_DELETE), !g->addingAffix && g->editing < values.size());
    EnableWindow(GetDlgItem(g->manager, M_UP), !g->addingAffix && g->editing > 0 && g->editing < values.size());
}
UINT dragMessage = 0;
HWND managerChild(HWND parent, const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x,
                  int y, int w, int h) {
    if(parent==g->manager&&id!=M_TAB)x+=MANAGER_CONTENT_LEFT+60;
    const std::wstring kind(cls);
    bool combo = kind == L"COMBOBOX", edit = kind == L"EDIT", list = kind == L"LISTBOX";
    bool button = kind == L"BUTTON", check = button && (style & BS_TYPEMASK) == BS_AUTOCHECKBOX;
    if (combo) style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    if (button && !check) style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
    if (edit || list) {
        g->managerFields.push_back({px(x), px(y), px(x + w), px(y + h)});
        style &= ~WS_BORDER;
        if (list) style |= LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT;
        x += edit ? 8 : 3; y += 5; w -= edit ? 16 : 6; h -= 10;
    }
    auto hWnd = createChild(parent, cls, text, style, id, x, y, w, h, kind == L"STATIC" ? g->smallFont : g->font);
    if (list) SendMessageW(hWnd, LB_SETITEMHEIGHT, 0, px(26));
    if (edit || list) SetWindowSubclass(hWnd, InputFeedbackProc, 1, 0);
    if (button) SetWindowSubclass(hWnd, check ? ManagerCheckProc : PillProc, 1, 0);
    if (kind == WC_TABCONTROLW) {
        TabCtrl_SetPadding(hWnd, 0, 0);
        TabCtrl_SetItemSize(hWnd, px(120), px(32));
        SetWindowSubclass(hWnd, ManagerTabProc, 1, 0);
    }
    if (combo) {
        SendMessageW(hWnd, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), px(24));
        SendMessageW(hWnd, CB_SETITEMHEIGHT, 0, px(30));
        styleDropdown(hWnd);
    }
    return hWnd;
}
void managerLabel(HWND p, const std::wstring &text, int x, int y, int w = 336) {
    managerChild(p, L"STATIC", text, SS_ENDELLIPSIS, 0, x, y, w, 18);
}
std::wstring pickFolder(HWND owner) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    if (FAILED(dialog->Show(owner)))
        return {};
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item)))
        return {};
    PWSTR path = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)))
        return {};
    std::wstring result = path;
    CoTaskMemFree(path);
    return result;
}
std::filesystem::path startupLink() {
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &path))) {
        result = std::filesystem::path(path) / L"Jump.lnk";
        CoTaskMemFree(path);
    }
    return result;
}
bool enableStartup(bool enable) {
    auto link = startupLink();
    if (link.empty())
        return false;
    if (!enable)
        return !std::filesystem::exists(link) || DeleteFileW(link.c_str()) != 0;
    wchar_t exe[32768]{};
    auto length = GetModuleFileNameW(nullptr, exe, 32768);
    if (!length || length >= 32768)
        return false;
    ComPtr<IShellLinkW> shortcut;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shortcut))))
        return false;
    if (FAILED(shortcut->SetPath(exe)) ||
        FAILED(shortcut->SetWorkingDirectory(std::filesystem::path(exe).parent_path().c_str())) ||
        FAILED(shortcut->SetDescription(L"Jump 后台保存助手")) ||
        FAILED(shortcut->SetShowCmd(SW_HIDE)))
        return false;
    ComPtr<IPersistFile> file;
    shortcut.As(&file);
    auto temporary = link.parent_path() / (L"Jump-" + std::to_wstring(GetCurrentProcessId()) + L".tmp.lnk");
    bool saved = file && SUCCEEDED(file->Save(temporary.c_str(), TRUE)) &&
        MoveFileExW(temporary.c_str(), link.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!saved) DeleteFileW(temporary.c_str());
    return saved;
}
#include "settings_draft.h"
StructuredNaming managedNamingScheme(size_t row){
    auto index=SendDlgItemMessageW(g->manager,M_LIST,LB_GETITEMDATA,row,0);
    const auto &schemes=namingSchemeCatalog(g->cfg);
    return index>=0&&static_cast<size_t>(index)<schemes.size()?schemes[index]:StructuredNaming{};
}
size_t namingRecipeIndex(const std::wstring &id){
    const auto &indices=namingCatalog(g->cfg).recipeIndices;
    auto found=indices.find(id);return found==indices.end()?g->cfg.recipes.size():found->second;
}
void managerSelect() {
    if (!g->manager)
        return;
    g->managerLoading = true;
    for (int id : {M_NAME, M_VALUE})
        SetDlgItemTextW(g->manager, id, L"");
    auto &cfg = g->cfg;
    int selected = static_cast<int>(SendDlgItemMessageW(g->manager, M_LIST, LB_GETCURSEL, 0, 0));
    if (selected < 0)
        selected = 0;
    g->addingAffix = false;
    g->editing = selected;
    if (g->managerTab == 1) {
        if (g->editing < cfg.favorites.size()) {
            auto &l = cfg.favorites[g->editing];
            SetDlgItemTextW(g->manager, M_NAME, l.name.c_str());
            SetDlgItemTextW(g->manager, M_VALUE, l.path.c_str());
        }
    } else if (g->managerTab == 2) {
        auto index=SendDlgItemMessageW(g->manager,M_LIST,LB_GETITEMDATA,g->editing,0);
        if (index>=0&&static_cast<size_t>(index)<cfg.recipes.size()) {
            auto &r = cfg.recipes[index];
            SetDlgItemTextW(g->manager, M_NAME, r.name.c_str());
            SetDlgItemTextW(g->manager, M_VALUE, r.directory.c_str());
        }
    } else if (managingAffixes()) {
        const auto &values = managerAffixes();
        if (g->editing < values.size()) SetDlgItemTextW(g->manager, M_VALUE, values[g->editing].c_str());
    }
    g->managerLoading = false;
    if(g->managerTab==0){auto scheme=managedNamingScheme(g->editing);bool hasScheme=scheme.enabled;
        bool saved=namingRecipeIndex(scheme.id)<cfg.recipes.size();
        bool canMove=saved&&g->editing>0&&namingRecipeIndex(managedNamingScheme(g->editing-1).id)<cfg.recipes.size();
        for(int id:{M_SAVE,M_COPY_SCHEME,M_DELETE,M_UP,M_APPLY_SCHEME,M_SCHEME_DEFAULT})EnableWindow(GetDlgItem(g->manager,id),
            hasScheme && (id!=M_APPLY_SCHEME||g->attached&&g->dialog.save) && (id!=M_DELETE||saved) && (id!=M_UP||canMove));
        SetDlgItemTextW(g->manager,M_DELETE,builtinNamingScheme(scheme.id)?L"还原初始规则":L"删除方案");
        std::wstring detail;
        if (hasScheme) {
            std::wstring fields;
            for (const auto &field : scheme.fields) { if (!fields.empty()) fields += L" · "; fields += field.label; }
            auto sample = composeStructure(namingSession(scheme, L"Asset"), cfg);
            if (sample.error.empty() && scheme.counter)
                sample.name = numberingExample(sample.name, {scheme.counter, false, L"", L"", scheme.dateFormat, scheme.digits, scheme.upper});
            detail = namingDefaultsSummary(scheme.id)+L"\r\n字段：" + fields + L"\r\n" + (sample.error.empty() ? L"示例：" + sample.name : L"使用时填写 · " + sample.error);
        }
        if(hasScheme)detail+=g->attached&&g->dialog.save?L"\r\n使用方案会填入当前保存窗口。":L"\r\n“使用方案”需先在原软件中打开保存窗口。";
        SetDlgItemTextW(g->manager, M_INFO, detail.c_str());
    }
    if (g->managerTab == 1 || g->managerTab == 2) {
        auto count = SendDlgItemMessageW(g->manager, M_LIST, LB_GETCOUNT, 0, 0);
        EnableWindow(GetDlgItem(g->manager, M_DELETE), count > 0);
        EnableWindow(GetDlgItem(g->manager, M_UP), count > 0 && g->editing > 0);
    }
    updateAffixManagerPreview();
    if (g->managerTab != MANAGER_TEMPLATES) rememberManagerDraft();
}
void managerList() {
    auto list = GetDlgItem(g->manager, M_LIST);
    if (!list)
        return;
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    if (g->managerTab == 1) {
        for (auto &p : g->cfg.favorites)
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    } else if (g->managerTab == 0) {
        const auto &schemes=namingSchemeCatalog(g->cfg);
        for(size_t i=0;i<schemes.size();++i){
            auto row=SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(schemes[i].name.c_str()));
            SendMessageW(list,LB_SETITEMDATA,row,i);
        }
    } else if (g->managerTab == 2) {
        for(size_t i=0;i<g->cfg.recipes.size();++i){const auto &recipe=g->cfg.recipes[i];
            if(recipe.namingOnly)continue;
            auto row=SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(recipe.name.c_str()));
            SendMessageW(list,LB_SETITEMDATA,row,i);
        }
    } else if (managingAffixes()) {
        for (const auto &value : managerAffixes()) {
            auto name = affixLabel(value);
            SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        }
    }
    auto count = static_cast<size_t>(SendMessageW(list, LB_GETCOUNT, 0, 0));
    g->editing = count ? std::min(g->editing, count - 1) : 0;
    SendMessageW(list, LB_SETCURSEL, g->editing, 0);
    managerSelect();
}
int buildTemplateManager(HWND parent) {
    initializeVariableDraft();
    int y = 52, available = MANAGER_WIDTH - 32, cell = (available - 16) / 3;
    auto make = [&](const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int top, int width, int height) {
        return managerChild(parent, cls, text, style, id, x, top, width, height);
    };
    auto caption = [&](int x, int top, int width, const std::wstring &text) {
        managerLabel(parent, text, x, top, width);
    };
    caption(16, y, available, L"点选字段组合 · 再点移除，新字段加在末尾"); y += 26;
    g->variableTokens = {L"项目", L"name", L"作者", L"folder", L"date"};
    auto selectedKeys = expressionKeys(g->variableTemplateDraft);
    for (const auto &key : selectedKeys)
        if (std::find(g->variableTokens.begin(), g->variableTokens.end(), key) == g->variableTokens.end() && g->variableTokens.size() < 9)
            g->variableTokens.push_back(key);
    for (const auto &[key, value] : variableValues(g->variableValuesDraft))
        if (!key.empty() && !builtInVariable(variableKey(key)) &&
            std::find(g->variableTokens.begin(), g->variableTokens.end(), key) == g->variableTokens.end() && g->variableTokens.size() < 9)
            g->variableTokens.push_back(key);
    for (size_t i = 0; i < g->variableTokens.size(); ++i) {
        const auto &key = g->variableTokens[i];
        auto button = make(L"BUTTON", variableCaption(key), WS_TABSTOP, ID_VARIABLE_TOKEN_FIRST + static_cast<int>(i),
                            16 + static_cast<int>(i % 3) * (cell + 8), y + static_cast<int>(i / 3) * 32, cell, 26);
        tip(button, L"点击加入组合，再次点击移除：" + variableCaption(key));
    }
    y += static_cast<int>((g->variableTokens.size() + 2) / 3) * 32;
    g->variableInputKeys.clear();
    if (!g->variableAdvanced) {
        auto values = variableValues(g->variableValuesDraft);
        for (const auto &key : selectedKeys) {
            if (builtInVariable(key) || g->variableInputKeys.size() >= 4) continue;
            int i = static_cast<int>(g->variableInputKeys.size()), left = 16 + i % 2 * ((available - 8) / 2 + 8);
            int top = y + i / 2 * 54, width = (available - 8) / 2;
            caption(left, top, width, key);
            auto edit = make(L"EDIT", values[key], ES_AUTOHSCROLL | WS_TABSTOP, ID_VARIABLE_FIELD_FIRST + i,
                              left, top + 22, width, 28);
            SetWindowSubclass(edit, VariableEditProc, 1, 0); SendMessageW(edit, EM_SETLIMITTEXT, 120, 0);
            auto cue = L"填写" + key;
            SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue.c_str()));
            if (key == L"项目") tip(edit, L"首次默认带入当前文件夹名，可修改；保存后复用");
            if (key == L"作者") tip(edit, L"首次默认带入系统用户名，可修改；保存后复用");
            g->variableInputKeys.push_back(key);
        }
        y += static_cast<int>((g->variableInputKeys.size() + 1) / 2) * 54;
    }
    make(L"BUTTON", g->variableAdvanced ? L"收起表达式" : L"编辑表达式…", WS_TABSTOP,
          ID_VARIABLE_ADVANCED, 16, y, 100, 24); y += 28;
    if (g->variableAdvanced) {
        auto expression = make(L"EDIT", g->variableTemplateDraft, ES_AUTOHSCROLL | WS_TABSTOP,
                                ID_VARIABLE_TEMPLATE, 16, y, available, 28);
        SetWindowSubclass(expression, VariableEditProc, 1, 0);
        SendMessageW(expression, EM_SETLIMITTEXT, 160, 0); y += 32;
        auto values = make(L"EDIT", g->variableValuesDraft, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
                            ID_VARIABLE_VALUES, 16, y, available, 62);
        SetWindowSubclass(values, VariableEditProc, 1, 0);
        SendMessageW(values, EM_SETLIMITTEXT, 1200, 0); y += 66;
    }
    make(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, ID_VARIABLE_HINT, 16, y, available, 22); y += 26;
    make(L"BUTTON", L"撤销修改", WS_TABSTOP, ID_VARIABLE_CANCEL, 16, y, 80, 26);
    make(L"BUTTON", L"保存模板", WS_TABSTOP, ID_VARIABLE_SAVE, MANAGER_WIDTH - 108, y, 92, 26); y += 34;
    updateVariableDraftPreview();
    return std::max(MANAGER_HEIGHT, y + 16);
}
#include "settings_pages.h"
void buildManagerPage(bool preserveDraft) {
    HWND parent = g->manager;
    bool visible = IsWindowVisible(parent) != FALSE;
    if (visible) SendMessageW(parent, WM_SETREDRAW, FALSE, 0);
    std::vector<HWND> children;
    EnumChildWindows(parent, [](HWND h, LPARAM data) -> BOOL {
        if (GetDlgCtrlID(h) != M_TAB && GetParent(h) == g->manager)
            reinterpret_cast<std::vector<HWND> *>(data)->push_back(h);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&children));
    for (auto h : children) DestroyWindow(h);
    g->managerFields.clear();
    g->managerLoading = true; g->addingAffix = false;
    constexpr int left = 12, listWidth = 148, x = 172, width = 336;
    auto button = [&](const wchar_t *text, int id, int bx, int y, int w) {
        return managerChild(parent, L"BUTTON", text, BS_PUSHBUTTON | WS_TABSTOP, id, bx, y, w, 28);
    };
    auto field = [&](const wchar_t *title, int id, int y) {
        managerLabel(parent, title, x, y, width);
        return managerChild(parent, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, id, x, y + 20, width, 28);
    };
    auto actions = [&](int y) {
        button(L"保存修改", M_SAVE, x, y, 96);
        if(g->managerTab!=2)button(L"新增", M_ADD, x + 104, y, 72);
        button(L"移除", M_DELETE, x + 184, y, 72);
        button(L"上移", M_UP, x + 264, y, 72);
    };
    auto makeList = [&](int y, int bottom, bool draggable) {
        auto list = managerChild(parent, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP,
                                 M_LIST, left, y, listWidth, bottom - y);
        if (draggable) MakeDragList(list);
    };
    int height = MANAGER_HEIGHT;
    if (g->managerTab == MANAGER_AFFIXES) {
        auto category = managerChild(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
                                     M_AFFIX_CATEGORY, left, 52, 148, 140);
        for (auto name : {L"前缀预设", L"后缀预设"})
            SendMessageW(category, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
        SendMessageW(category, CB_SETCURSEL, g->affixCategory, 0);
        managerLabel(parent, g->affixCategory == 0 ? L"添加在文件名之前" : L"添加在文件名之后、扩展名之前", x, 58, width);
    }
    if(g->managerTab>=MANAGER_PROJECTS){
        height=buildSettingsPage(parent);
    } else if (g->managerTab == MANAGER_TEMPLATES) {
        height = buildTemplateManager(parent);
    } else if (g->managerTab == 3) {
        const std::pair<int, const wchar_t *> checks[] = {
            {M_AUTO, L"检测到保存窗口时自动显示助手"},
            {M_QUICK, L"允许快捷提交保存（Enter / 双击 / Ctrl + 单击）"},
            {M_STARTUP, L"开机后台运行"}
        };
        int y = 52;
        for (auto [id, text] : checks) {
            managerChild(parent, L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, id, left, y, 496, 26);
            y += 36;
        }
        managerLabel(parent, L"助手位置", left, 170, 496);
        auto combo = managerChild(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
                                  M_LAYOUT, left, 194, 336, 160);
        for (auto text : {L"自动：优先侧边，空间不足时调整", L"优先侧边", L"优先底部"})
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        SendMessageW(combo, CB_SETCURSEL, g->cfg.layout, 0);
        CheckDlgButton(parent, M_AUTO, g->cfg.automatic ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(parent, M_QUICK, g->cfg.quickSave ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(parent, M_STARTUP, std::filesystem::exists(startupLink()) ? BST_CHECKED : BST_UNCHECKED);
        button(L"保存选项", M_OPTIONS_SAVE, left, 242, 96);
        managerLabel(parent, L"后台等待窗口事件，不联网、不安装系统服务。", left, 300, 496);
        managerLabel(parent, L"配置文件", left, 336, 496);
        auto location = managerChild(parent, L"EDIT", g->configFile.wstring(), ES_READONLY | ES_AUTOHSCROLL,
                                    0, left, 358, 496, 28);
        SendMessageW(location, EM_SETSEL, 0, 0);
    } else if (managingAffixes()) {
        bool prefix = managingPrefixes();
        int top = 94;
        makeList(top, 340, true);
        auto value = field(prefix ? L"前缀内容" : L"后缀内容（放在扩展名前）", M_VALUE, top);
        SendMessageW(value, EM_SETLIMITTEXT, 80, 0);
        managerChild(parent, L"STATIC", L"", SS_LEFT, M_AFFIX_PREVIEW, x, top + 62, width, 34);
        managerChild(parent, L"STATIC", L"前 3 项常驻显示，其余点「＋」查找。\n拖动或上移调整顺序；支持 {date}。",
                     SS_LEFT, 0, x, top + 108, width, 44);
        managerChild(parent, L"STATIC", L"选择预设修改，或点击新增。", SS_LEFT,
                     M_AFFIX_STATUS, x, 268, width, 32);
        actions(312);
    } else {
        int y=52;
        if(g->managerTab!=0){field(L"显示名称",M_NAME,y);field(L"目录路径",M_VALUE,y+56);}
        if(g->managerTab==0){
            makeList(52,380,false);
            managerChild(parent,L"STATIC",L"字段、常用选项、格式、目录推荐和软件关联，统一在方案设置中修改和保存。",SS_LEFT,0,x,58,width,40);
            button(L"方案设置",M_SAVE,x,108,156);
            button(L"复制方案",M_COPY_SCHEME,x+168,108,168);
            button(L"新建方案",M_ADD,x,156,156);
            button(L"使用方案",M_APPLY_SCHEME,x+168,156,168);
            auto defaults=button(L"软件关联与默认",M_SCHEME_DEFAULT,x,200,width);
            tip(defaults,L"打开方案设置的“软件关联”，与其他规则一起保存。");
            managerChild(parent,L"STATIC",L"",SS_LEFT,M_INFO,x,240,width,96);
            button(L"删除方案",M_DELETE,x,348,156);button(L"上移",M_UP,x+168,348,168);
        } else if (g->managerTab == 1) {
            makeList(y, 340, false);
            button(L"选择文件夹…", M_FOLDER, x, 180, 160);
            button(L"清空历史位置", M_CLEAR_HISTORY, x + 168, 180, 168);
            managerChild(parent, L"STATIC", L"收藏名称和路径可直接编辑。历史仅表示浏览过的目录，不等同于成功保存。",
                         SS_LEFT, 0, x, 228, width, 56);
            actions(312);
        } else {
            makeList(y, 376, false);
            managerChild(parent,L"STATIC",L"保存目录、命名规则及本次修饰设置。规则在命名方案编辑器中维护。",SS_LEFT,0,x,174,width,64);
            button(L"保存当前设置为新流程", M_CAPTURE, x, 286, width);
            managerLabel(parent, L"流程沿用原软件的保存格式。", x, 322, width);
            actions(348);
        }
    }
    if (g->managerTab == 1 || g->managerTab == 2 || g->managerTab == 3)
        managerChild(parent, L"STATIC", L"", SS_LEFT, M_FEEDBACK, 12,
                     g->managerTab == 3 ? 274 : g->managerTab == 2 ? 388 : 356, 496, 32);
    for (int id : {M_SAVE, M_OPTIONS_SAVE, ID_VARIABLE_SAVE})
        if (auto save = GetDlgItem(parent, id); save && (g->managerTab == 1 || g->managerTab == 2 || g->managerTab == 3 ||
            g->managerTab == MANAGER_AFFIXES || g->managerTab == MANAGER_TEMPLATES)) tip(save, L"保存当前修改（Ctrl + S）");
    g->managerLoading = false;
    if (g->managerTab == MANAGER_AFFIXES) g->builtAffixCategory = g->affixCategory;
    if(g->managerTab<MANAGER_PROJECTS)managerList();
    if (!preserveDraft) rememberManagerDraft();
    resizeManager(height);
    if (visible) {
        SendMessageW(parent, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    }
}
void managerMessage(const std::wstring &text) {
    MessageBoxW(g->manager, text.c_str(), L"Jump", MB_OK | MB_ICONINFORMATION);
}
size_t managerRecipeIndex(){
    if(g->managerTab==0)return namingRecipeIndex(managedNamingScheme(g->editing).id);
    auto index=SendDlgItemMessageW(g->manager,M_LIST,LB_GETITEMDATA,g->editing,0);
    return index<0?g->cfg.recipes.size():static_cast<size_t>(index);
}
void editManagedScheme(bool create,bool copy,int initialPage=0){
    auto draft=create?namingDefinition(basicStructure(L"Asset",&g->cfg)):
        managedNamingScheme(g->editing);
    if(create)for(const auto &s:namingSchemeCatalog(g->cfg))if(s.id==L"asset-v2"){draft=s;break;}
    if(!draft.enabled)return;
    auto previous=namingDefinition(draft);
    if(create||copy){draft.id=newNamingIdentity();draft.name=create?L"新命名方案":draft.name+L" 副本";}
    auto applications=namingSchemeApplications(g->cfg,draft.id);
    CompactFormStyle style{g->smallFont,g->inputBrush,g->field,g->muted,g->line,g->accent,g->primary,g->surface,paintRoundedRect,styleDropdown};
    style.popupMenu=trackPopupMenuWithHelp;
    auto theme=workflowForm(L"命名方案",L"",{});theme.compact=&style;
    bool globalDefault=defaultNamingScheme(g->cfg).id==draft.id;
    while(showNamingDesigner(g->manager,draft,g->cfg,theme,g->attached?schemeSubject():L"Asset",applications,false,create?9:initialPage,-1,&globalDefault)){
        if(!storeNamingScheme(draft,false,&applications,&globalDefault))continue;
        if(g->attached&&!g->structure.free&&g->structure.id==draft.id)
            applySchemeRevision(draft,previous,schemeSubject());
        const auto &schemes=namingSchemeCatalog(g->cfg);
        auto selected=std::find_if(schemes.begin(),schemes.end(),[&](const auto &s){return s.id==draft.id;});
        if(selected!=schemes.end())g->editing=static_cast<size_t>(selected-schemes.begin());
        managerList();return;
    }
}
bool templateManagerCommand(HWND hwnd, int id, int code) {
    if (g->managerTab != MANAGER_TEMPLATES) return false;
    if ((id == ID_VARIABLE_TEMPLATE || id == ID_VARIABLE_VALUES ||
         (id >= ID_VARIABLE_FIELD_FIRST && id < ID_VARIABLE_FIELD_FIRST + 4)) && code == EN_CHANGE) {
        updateVariableDraftPreview(); invalidateManagerHeading(); return true;
    }
    if (code != BN_CLICKED) return false;
    if (id >= ID_VARIABLE_TOKEN_FIRST && id < ID_VARIABLE_TOKEN_FIRST + static_cast<int>(g->variableTokens.size())) {
        updateVariableDraftPreview();
        auto key = g->variableTokens[id - ID_VARIABLE_TOKEN_FIRST];
        toggleVariableToken(key); buildManagerPage(true); return true;
    }
    if (id == ID_VARIABLE_ADVANCED) {
        updateVariableDraftPreview(); g->variableAdvanced = !g->variableAdvanced; buildManagerPage(true); return true;
    }
    if (id == ID_VARIABLE_CANCEL) {
        g->variableEditor = false; buildManagerPage(); return true;
    }
    if (id != ID_VARIABLE_SAVE) return false;
    updateVariableDraftPreview();
    auto preview = smartNamingPreview(g->parts.base.empty() ? L"文件名" : compose(g->parts), g->dialog.directory, g->dialog.extension,
                                     {0, false, g->variableTemplateDraft, g->variableValuesDraft, g->cfg.dateFormat});
    if (trim(g->variableTemplateDraft).empty() || !preview.error.empty()) return true;
    if (g->attached && !g->sessionExpression.empty()) {
        if (auto project = activeProject(g->cfg)) {
            auto previous = g->cfg;
            auto found = std::find_if(g->cfg.projects.begin(), g->cfg.projects.end(), [&](const auto &p) { return p.id == project->id; });
            auto values = variableValues(found->variables), draft = variableValues(g->variableValuesDraft);
            std::wstring updated;
            for (auto &[key, value] : values) updated += key + L"=" + (draft.contains(key) ? draft[key] : value) + L"\r\n";
            found->variables = updated;
            if (!persist()) {
                g->cfg = std::move(previous);
                SetDlgItemTextW(hwnd, ID_VARIABLE_HINT, L"项目变量未保存，请检查配置写入权限后重试。");
                return true;
            }
        }
        checkpointNaming();
        g->sessionExpression = trim(g->variableTemplateDraft); g->sessionVariables = g->variableValuesDraft;
        rememberManagerDraft();
        updateInlineRule(); SetDlgItemTextW(hwnd, ID_VARIABLE_HINT, L"已应用到本次命名；可将当前设置另存为方案。"); return true;
    }
    auto oldTemplate = g->cfg.variableTemplate, oldValues = g->cfg.variableValues;
    g->cfg.variableTemplate = trim(g->variableTemplateDraft); g->cfg.variableValues = g->variableValuesDraft;
    if (!persist()) {
        g->cfg.variableTemplate = oldTemplate; g->cfg.variableValues = oldValues;
        setTextIfChanged(GetDlgItem(hwnd, ID_VARIABLE_HINT), L"模板未保存，请检查配置写入权限。"); return true;
    }
    // Configuration does not turn the template on or submit the host's save dialog.
    if (g->attached && g->variablesEnabled) updateInlineRule();
    if (g->attached) updatePanelContent();
    rememberManagerDraft();
    setTextIfChanged(GetDlgItem(hwnd, ID_VARIABLE_HINT), L"已保存 · 在助手中点击“变量表达式”即可套用");
    return true;
}
LRESULT CALLBACK ManagerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == dragMessage) {
        auto d = reinterpret_cast<DRAGLISTINFO *>(lp);
        static int from = -1;
        switch (d->uNotification) {
        case DL_BEGINDRAG:
            if (!discardManagerDraft()) return FALSE;
            from = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            return from >= 0;
        case DL_DRAGGING: {
            int to = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            DrawInsert(hwnd, d->hWnd, to);
            return DL_MOVECURSOR;
        }
        case DL_DROPPED: {
            int to = LBItemFromPt(d->hWnd, d->ptCursor, TRUE);
            if (managingAffixes() && from >= 0 && to >= 0 && from != to) {
                auto &values = managerAffixes();
                if (static_cast<size_t>(from) < values.size() && static_cast<size_t>(to) < values.size()) {
                    auto previous = g->cfg;
                    auto selected = g->editing;
                    auto value = values[from];
                    values.erase(values.begin() + from);
                    values.insert(values.begin() + to, value);
                    g->editing = to;
                    if (!commitManagerConfig(std::move(previous))) g->editing = selected;
                    managerList();
                }
            }
            DrawInsert(hwnd, d->hWnd, -1);
            from = -1;
            return 0;
        }
        case DL_CANCELDRAG:
            DrawInsert(hwnd, d->hWnd, -1);
            from = -1;
            return 0;
        }
    }
    switch (msg) {
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(lp);
        RECT minimum{0,0,px(MANAGER_SHELL_WIDTH),px(380)};
        AdjustWindowRectExForDpi(&minimum,static_cast<DWORD>(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,WS_EX_APPWINDOW,g->dpi);
        info->ptMinTrackSize={minimum.right-minimum.left,minimum.bottom-minimum.top};return 0;
    }
    case WM_SIZE: {
        if(wp==SIZE_MINIMIZED||settingsDialogs.empty())break;
        auto host=settingsDialogs.back();if(!host->window||!host->contentWidth)break;
        RECT client{};GetClientRect(hwnd,&client);int available=std::max(1L,client.bottom-px(56));
        int height=std::min(host->contentHeight,available),maximum=std::max(0,host->contentHeight-height);
        int next=std::clamp(host->scroll,0,maximum);
        if(next!=host->scroll){ScrollWindowEx(host->window,0,host->scroll-next,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE);host->scroll=next;}
        SetWindowLongPtrW(host->window,GWL_STYLE,(GetWindowLongPtrW(host->window,GWL_STYLE)&~WS_VSCROLL)|(maximum?WS_VSCROLL:0));
        SCROLLINFO scroll{sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS};scroll.nMax=std::max(0,host->contentHeight-host->footerHeight-1);scroll.nPage=std::max(1,height-host->footerHeight);scroll.nPos=next;SetScrollInfo(host->window,SB_VERT,&scroll,TRUE);
        int space=std::max(1L,client.right-px(MANAGER_CONTENT_LEFT));
        int width=std::min(space,host->contentWidth+(maximum?GetSystemMetricsForDpi(SM_CXVSCROLL,g->dpi):0));
        SetWindowPos(host->window,nullptr,px(MANAGER_CONTENT_LEFT)+(space-width)/2,px(48),width,height,SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
        if(host->footerHeight)host->procedure(host->window,WM_SETTINGS_VIEWPORT_CHANGED,0,0);
        InvalidateRect(hwnd,nullptr,TRUE);return 0;
    }
    case WM_DPICHANGED: {
        int before=g->dpi;g->dpi=HIWORD(wp);fonts();
        for(auto child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            if(settingsDialog(child))continue;
            RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,hwnd,reinterpret_cast<POINT*>(&r),2);
            MoveWindow(child,MulDiv(r.left,g->dpi,before),MulDiv(r.top,g->dpi,before),MulDiv(r.right-r.left,g->dpi,before),MulDiv(r.bottom-r.top,g->dpi,before),FALSE);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(g->font),FALSE);
        }
        for(auto &r:g->managerFields){r={MulDiv(r.left,g->dpi,before),MulDiv(r.top,g->dpi,before),MulDiv(r.right,g->dpi,before),MulDiv(r.bottom,g->dpi,before)};}
        auto suggested=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOACTIVATE|SWP_NOZORDER);
        NamingDesignerDpi scale{g->dpi,g->font,g->smallFont};
        for(auto host:settingsDialogs)if(host->procedure==namingDesignerProcedure)SendMessageW(host->window,WM_NAMING_DESIGNER_DPI,0,reinterpret_cast<LPARAM>(&scale));
        RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_FRAME);return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(hwnd, msg, wp); auto dc = paint.dc();
        RECT client{}; GetClientRect(hwnd, &client); FillRect(dc, &client, g->brush);
        auto oldFont=SelectObject(dc,g->boldFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,g->fg);
        RECT brand{px(24),px(16),px(136),px(40)};DrawTextW(dc,L"设置中心",-1,&brand,DT_SINGLELINE|DT_VCENTER);
        auto title=std::wstring(managerTitles[g->managerTab]);
        if(!settingsDialogs.empty()){
            auto editor=windowText(settingsDialogs.back()->window);
            title+=L"  /  "+(editor==title?L"编辑":editor);
        } else if (managerDraftDirty()) title += L" · 未保存";
        RECT heading{px(MANAGER_CONTENT_LEFT+20),px(16),client.right-px(20),px(40)};
        DrawTextW(dc,title.c_str(),-1,&heading,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(dc,oldFont);
        if(settingsDialogs.empty())for (const auto &r : g->managerFields)paintInputFrame(dc, hwnd, r, px(8));
        return 0;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC: {
        bool field = msg != WM_CTLCOLORSTATIC || windowClass(reinterpret_cast<HWND>(lp)) == L"Edit";
        SetTextColor(reinterpret_cast<HDC>(wp), field ? g->fg : g->muted);
        SetBkColor(reinterpret_cast<HDC>(wp), field ? g->field : g->bg);
        return reinterpret_cast<LRESULT>(field ? g->inputBrush : g->brush);
    }
    case WM_DRAWITEM: {
        auto item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (item.CtlType == ODT_COMBOBOX) return drawDropdownItem(item);
        bool list = item.CtlType == ODT_LISTBOX;
        if (!list && item.CtlType != ODT_BUTTON) return FALSE;
        PaintBuffer buffer(item.hDC, item.rcItem); item.hDC = buffer.dc;
        RECT r = item.rcItem; FillRect(item.hDC, &r, list ? g->inputBrush : g->brush);
        if (list && item.itemID == static_cast<UINT>(-1)) return TRUE;
        bool disabled = (item.itemState & ODS_DISABLED) != 0;
        POINT mouse{}; GetCursorPos(&mouse); ScreenToClient(item.hwndItem, &mouse);
        DWORD_PTR feedback = 0; GetWindowSubclass(item.hwndItem, PillProc, 1, &feedback);
        bool hover = !disabled && (list ? PtInRect(&r, mouse) : (feedback & 1));
        bool primary = item.CtlID == M_SAVE || item.CtlID == M_OPTIONS_SAVE || item.CtlID == ID_VARIABLE_SAVE;
        bool fold = item.CtlID == ID_VARIABLE_ADVANCED;
        bool expanded = g->variableAdvanced;
        bool templateChip = item.CtlID >= ID_VARIABLE_TOKEN_FIRST && item.CtlID < ID_VARIABLE_TOKEN_FIRST + g->variableTokens.size();
        bool chosen = false;
        if (templateChip) {
            auto keys = expressionKeys(g->variableTemplateDraft);
            chosen = std::find(keys.begin(), keys.end(), g->variableTokens[item.CtlID - ID_VARIABLE_TOKEN_FIRST]) != keys.end();
        }
        bool keyboardFocus = !disabled && !list && (item.itemState & ODS_FOCUS) && (feedback & 2);
        COLORREF color = list ? (item.itemState & ODS_SELECTED ? g->selection : g->field) :
                         controlFill(fold ? g->bg : g->surface, (fold && expanded) || chosen, hover,
                                     (item.itemState & ODS_SELECTED) != 0, primary, !disabled);
        auto fill = CreateSolidBrush(color);
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : color);
        auto ob = SelectObject(item.hDC, fill), op = SelectObject(item.hDC, pen);
        paintRoundedRect(item.hDC, r.left, r.top, r.right, r.bottom, px(8), px(8));
        SelectObject(item.hDC, op); SelectObject(item.hDC, ob); DeleteObject(pen); DeleteObject(fill);
        std::wstring text;
        if (list) {
            auto length = SendMessageW(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0);
            if (length == LB_ERR) return TRUE;
            text.resize(static_cast<size_t>(length) + 1);
            SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
            text.resize(static_cast<size_t>(length));
        } else text = windowText(item.hwndItem);
        auto font = SelectObject(item.hDC, primary ? g->boldFont : g->font);
        SetBkMode(item.hDC, TRANSPARENT); SetTextColor(item.hDC, disabled || (fold && !expanded && !hover) ? g->muted : g->fg);
        r.left += px(8); r.right -= px(8);
        if (fold) {
            int size = px(14);
            drawUiIcon(item.hDC, expanded ? UiIcon::ChevronUp : UiIcon::Chevron, static_cast<float>(r.right - size),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), expanded ? g->accent : g->muted);
            r.right -= px(22);
        }
        DrawTextW(item.hDC, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (list || fold ? DT_LEFT : DT_CENTER));
        SelectObject(item.hDC, font);
        return TRUE;
    }
    case WM_EXITSIZEMOVE:
        rememberManagerPosition(); return 0;
    case WM_CLOSE:
        if(!settingsDialogs.empty()){requestSettingsNavigation(-1);return 0;}
        if (!discardManagerDraft()) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_SETTINGS_NAVIGATE:
        if(!settingsDialogs.empty())return 0;
        if(pendingSettingsClose){pendingSettingsClose=false;pendingSettingsPage=-1;DestroyWindow(hwnd);return 0;}
        if(pendingSettingsPage>=0){g->managerTab=pendingSettingsPage;pendingSettingsPage=-1;
            TabCtrl_SetCurSel(GetDlgItem(hwnd,M_TAB),g->managerTab);g->editing=0;buildManagerPage();}
        return 0;
    case WM_DESTROY:
        g->manager = nullptr;
        g->variableEditor = false;
        if (g->attached) {
            if (g->managerTab == MANAGER_AFFIXES) g->namingMode = g->affixCategory + 1;
            g->paths = rankLocations(g->cfg, g->dialog.source, {});
            rebuildPanel();
            if (GetForegroundWindow() == g->dialog.hwnd)
                reposition(true);
        }
        return 0;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR *>(lp)->idFrom == M_TAB &&
            reinterpret_cast<NMHDR *>(lp)->code == TCN_SELCHANGE) {
            int next=TabCtrl_GetCurSel(GetDlgItem(hwnd,M_TAB));
            if(!settingsDialogs.empty()){TabCtrl_SetCurSel(GetDlgItem(hwnd,M_TAB),g->managerTab);requestSettingsNavigation(next);return 0;}
            if (!discardManagerDraft()) { TabCtrl_SetCurSel(GetDlgItem(hwnd, M_TAB), g->managerTab); return 0; }
            if (g->managerTab == MANAGER_TEMPLATES) g->variableEditor = false;
            g->managerTab = next;
            g->editing = 0;
            buildManagerPage();
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (g->managerLoading)
            return 0;
        if (settingsDialogs.empty() &&
            (((id == M_NAME || id == M_VALUE) && code == EN_CHANGE) ||
             ((id == M_AUTO || id == M_QUICK || id == M_STARTUP) && code == BN_CLICKED) ||
             (id == M_LAYOUT && code == CBN_SELCHANGE))) {
            managerFeedback(L""); invalidateManagerHeading();
        }
        if(settingsPageCommand(id,code))return 0;
        if (templateManagerCommand(hwnd, id, code)) return 0;
        if (id == M_AFFIX_CATEGORY && code == CBN_SELCHANGE) {
            int selected = static_cast<int>(SendDlgItemMessageW(hwnd, id, CB_GETCURSEL, 0, 0));
            if (selected >= 0 && selected <= 1) {
                if (!discardManagerDraft()) { SendDlgItemMessageW(hwnd, id, CB_SETCURSEL, g->affixCategory, 0); return 0; }
                g->affixCategory = selected;
                g->editing = 0; buildManagerPage();
            }
            return 0;
        }


        if (id == M_LIST && code == LBN_SELCHANGE) {
            if (!discardManagerDraft()) { SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, g->editing, 0); return 0; }
            managerSelect();
            return 0;
        }
        if (id == M_VALUE && code == EN_CHANGE && managingAffixes()) {
            updateAffixManagerPreview();
            SetDlgItemTextW(hwnd, M_AFFIX_STATUS, L"修改后点击「保存修改」。");
            return 0;
        }
        if (code != BN_CLICKED)
            return 0;
        auto text = [&](int control) { return trim(windowText(GetDlgItem(hwnd, control))); };
        auto &cfg = g->cfg;

        if(g->managerTab==0){
            auto scheme=managedNamingScheme(g->editing);auto index=managerRecipeIndex();
            if(id==M_ADD||id==M_SAVE||id==M_COPY_SCHEME){editManagedScheme(id==M_ADD,id==M_COPY_SCHEME);return 0;}
            if(id==M_SCHEME_DEFAULT){editManagedScheme(false,false,5);return 0;}
            if(id==M_APPLY_SCHEME&&g->attached){
                useNamingScheme(scheme);
                DestroyWindow(hwnd);SetForegroundWindow(g->dialog.hwnd);reposition(true);return 0;
            }
            if(index>=cfg.recipes.size())return 0;
            if(id==M_DELETE||id==M_UP){
                auto previous=cfg;
                if(id==M_DELETE){
                    if(!builtinNamingScheme(scheme.id))removeNamingSchemeDefaults(cfg,scheme.id);
                    cfg.recipes.erase(cfg.recipes.begin()+index);
                    if(builtinNamingScheme(scheme.id))std::erase_if(cfg.candidateRenames,[&](const auto &r){return r.schema==scheme.id;});
                }else if(g->editing>0){auto before=namingRecipeIndex(managedNamingScheme(g->editing-1).id);
                    if(before<cfg.recipes.size()){std::swap(cfg.recipes[index],cfg.recipes[before]);--g->editing;}}
                if(!persist()){cfg=std::move(previous);managerMessage(L"保存失败，原方案已保留。");}
                else if(id==M_DELETE&&builtinNamingScheme(scheme.id)&&g->attached&&g->structure.id==scheme.id){
                    for(const auto &s:namingSchemeCatalog(cfg))if(s.id==scheme.id){applySchemeRevision(s,scheme,schemeSubject());break;}}
                managerList();return 0;
            }
            return 0;
        }

        if (managingAffixes()) {
            auto &values = managerAffixes();
            bool prefix = managingPrefixes();
            if (id == M_ADD) {
                if (!discardManagerDraft()) return 0;
                if (values.size() >= 100u) { managerMessage(L"最多保存 100 个预设，请先移除不用的内容。"); return 0; }
                g->addingAffix = true;
                g->editing = values.size();
                SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                SetDlgItemTextW(hwnd, M_VALUE, L"");
                SetDlgItemTextW(hwnd, M_AFFIX_STATUS, prefix ? L"新增前缀：填写内容后保存，即可成为泡泡。" :
                                                            L"新增后缀：填写内容后保存，即可成为泡泡。");
                updateAffixManagerPreview();
                rememberManagerDraft();
                if (IsWindowVisible(hwnd)) SetFocus(GetDlgItem(hwnd, M_VALUE));
                return 0;
            }
            if (id == M_SAVE) {
                auto value = windowText(GetDlgItem(hwnd, M_VALUE));
                if (!validAffix(value, prefix)) { managerMessage(L"请输入有效内容，不能包含路径字符。"); return 0; }
                auto duplicate = std::find(values.begin(), values.end(), value);
                if (duplicate != values.end() && (g->addingAffix || static_cast<size_t>(duplicate - values.begin()) != g->editing)) {
                    managerMessage(L"已有相同内容的预设，请修改内容或直接使用已有预设。"); return 0;
                }
                auto previous = cfg;
                bool adding = g->addingAffix || g->editing >= values.size();
                if (adding) {
                    if (values.size() >= 100u) return 0;
                    values.insert(values.begin(), value);
                } else {
                    moveAffixColor(prefix, values[g->editing], value);
                    values[g->editing] = value;
                }
                if (!commitManagerConfig(std::move(previous))) return 0;
                if (adding) g->editing = 0;
                g->addingAffix = false;
                managerList();
                managerFeedback(L"已保存，返回助手即可点选泡泡。");
                return 0;
            }
            if (id == M_DELETE || id == M_UP) {
                if (g->addingAffix || g->editing >= values.size()) return 0;
                if (!discardManagerDraft()) return 0;
                auto previous = cfg;
                auto selected = g->editing;
                if (id == M_DELETE) {
                    moveAffixColor(prefix, values[g->editing], L"");
                    values.erase(values.begin() + g->editing);
                }
                else if (g->editing > 0) {
                    std::swap(values[g->editing], values[g->editing - 1]); --g->editing;
                }
                bool saved = commitManagerConfig(std::move(previous));
                if (!saved) g->editing = selected;
                managerList();
                managerFeedback(!saved ? L"保存失败，原预设已保留。" :
                    id == M_DELETE ? L"已移除预设，当前文件名保持不变。" : L"已调整泡泡顺序。");
                return 0;
            }
            return 0;
        }
        if(id==M_ADD){
            if(g->managerTab!=1){SendMessageW(hwnd,WM_COMMAND,M_CAPTURE,0);return 0;}
            if (!discardManagerDraft()) return 0;
            if (cfg.favorites.size() >= 100) { managerFeedback(L"最多收藏 100 个位置，请先移除不用的位置。"); return 0; }
            g->editing = cfg.favorites.size();
            SendDlgItemMessageW(hwnd, M_LIST, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
            SetDlgItemTextW(hwnd, M_NAME, L"");
            SetDlgItemTextW(hwnd, M_VALUE, g->dialog.directory.c_str());
            for (int action : {M_DELETE, M_UP}) EnableWindow(GetDlgItem(hwnd, action), FALSE);
            managerFeedback(L"填写名称和目录后点击「保存修改」。");
            rememberManagerDraft(); SetFocus(GetDlgItem(hwnd, M_NAME)); return 0;
        }
        if (id == M_SAVE) {
            auto name = text(M_NAME), value = text(M_VALUE);
            if (name.empty() || (g->managerTab == 1 && value.empty())) {
                managerMessage(L"请输入名称和内容。");
                return 0;
            }
            auto previous=cfg;
            if (g->managerTab == 1) {
                value = pathEditValue(value);
                if (name.size() > 80 || value.empty()) { managerFeedback(L"请填写 80 字以内的名称和完整文件夹路径。"); return 0; }
                value = std::filesystem::path(value).lexically_normal().wstring();
                for (size_t i = 0; i < cfg.favorites.size(); ++i)
                    if (i != g->editing && normalize(cfg.favorites[i].path) == normalize(value)) {
                        managerFeedback(L"此目录已经收藏，请选择已有位置修改名称。"); return 0;
                    }
                if (g->editing < cfg.favorites.size())
                    cfg.favorites[g->editing] = {name, value, L"收藏"};
                else {
                    cfg.favorites.push_back({name, value, L"收藏"});
                }
            } else {
                auto index=managerRecipeIndex();
                if(index>=cfg.recipes.size())return 0;
                for(size_t i=0;i<cfg.recipes.size();++i)if(i!=index&&lower(cfg.recipes[i].name)==lower(name)){
                    managerMessage(L"同名方案或流程已存在，请使用其他名称。");return 0;
                }
                Recipe recipe=cfg.recipes[index];recipe.name=name;recipe.directory=value;
                if (std::filesystem::path(value).is_absolute()) recipe.relativeDirectory = false;
                cfg.recipes[index] = recipe;
            }
            if (!commitManagerConfig(std::move(previous))) return 0;
            managerList();
            managerFeedback(L"已保存。");
            return 0;
        }
        if (id == M_DELETE || id == M_UP) {
            if (!discardManagerDraft()) return 0;
            auto previous = cfg;
            auto selected = g->editing;
            auto index = g->editing;
            if (g->managerTab == 1) {
                auto &p = cfg.favorites;
                if (index < p.size()) {
                    if (id == M_DELETE)
                        p.erase(p.begin() + index);
                    else if (index > 0) {
                        std::swap(p[index],p[index-1]);
                        --g->editing;
                    }
                }
            } else {
                auto &p = cfg.recipes;index=managerRecipeIndex();
                if (index < p.size()) {
                    if (id == M_DELETE)
                        p.erase(p.begin() + index);
                    else if (g->editing > 0) {
                        auto before=SendDlgItemMessageW(hwnd,M_LIST,LB_GETITEMDATA,g->editing-1,0);
                        if(before>=0)std::swap(p[index],p[before]);
                        --g->editing;
                    }
                }
            }
            bool saved = commitManagerConfig(std::move(previous));
            if (!saved) g->editing = selected;
            managerList();
            managerFeedback(saved ? (id == M_DELETE ? L"已移除。" : L"已调整顺序。") : L"保存失败，原设置已保留。");
            return 0;
        }
        if (id == M_FOLDER) {
            auto path = pickFolder(hwnd);
            if (!path.empty())
                SetDlgItemTextW(hwnd, M_VALUE, path.c_str());
            return 0;
        }
        if (id == M_CLEAR_HISTORY) {
            managerMessage(removeHistoryLocation() ? L"历史位置已清空。" : g->status);
            return 0;
        }
        if(id==M_CAPTURE){
            if(!g->attached||g->dialog.directory.empty()||!validName(g->stemName)||!g->ruleError.empty()||!structuredInputError().empty()){
                managerMessage(L"请先在保存窗口中选择目录并组成有效名称。");return 0;
            }
            if(cfg.recipes.size()>=100){managerMessage(L"最多保存 100 套方案与流程。");return 0;}
            auto name=text(M_NAME);if(name.empty())name=L"保存流程";
            auto base=name;int suffix=2;
            while(std::any_of(cfg.recipes.begin(),cfg.recipes.end(),[&](const auto &r){return lower(r.name)==lower(name);}))
                name=base+L" "+std::to_wstring(suffix++);
            auto previous=cfg;cfg.recipes.push_back(captureRecipe(name));
            if(!persist()){cfg=std::move(previous);managerMessage(L"保存失败，原设置已保留。");return 0;}
            g->editing=std::count_if(cfg.recipes.begin(),cfg.recipes.end(),[](const auto &r){return !r.namingOnly;})-1;
            managerList();return 0;
        }
        if (id == M_OPTIONS_SAVE) {
            bool start = IsDlgButtonChecked(hwnd, M_STARTUP) == BST_CHECKED;
            bool previousStart = std::filesystem::exists(startupLink());
            auto previous = cfg;
            cfg.automatic = IsDlgButtonChecked(hwnd, M_AUTO) == BST_CHECKED;
            cfg.quickSave = IsDlgButtonChecked(hwnd, M_QUICK) == BST_CHECKED;
            cfg.layout = static_cast<int>(SendDlgItemMessageW(hwnd, M_LAYOUT, CB_GETCURSEL, 0, 0));
            if (!commitManagerConfig(previous)) return 0;
            // Re-save an enabled shortcut so moving the portable folder repairs its target.
            if ((start || previousStart) && !enableStartup(start)) {
                cfg = std::move(previous);
                bool restored = persist();
                managerFeedback(restored ? L"开机运行设置失败，原选项已恢复。请检查启动目录写入权限。" :
                    L"开机运行设置失败，原选项尚未恢复到磁盘，请重试保存。");
                return 0;
            }
            if (!cfg.automatic)
                hidePanel();
            rememberManagerDraft(); managerFeedback(L"选项已保存。");
            return 0;
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
POINT fitWindowPoint(POINT point, int width, int height, const RECT &work) {
    point.x = std::clamp<LONG>(point.x, work.left, std::max<LONG>(work.left, work.right - width));
    point.y = std::clamp<LONG>(point.y, work.top, std::max<LONG>(work.top, work.bottom - height));
    return point;
}
POINT managerOpenPoint(int width, int height) {
    MONITORINFO monitor{sizeof(monitor)};
    if (g->cfg.managerPositionSaved) {
        RECT saved{g->cfg.managerX, g->cfg.managerY, g->cfg.managerX + width, g->cfg.managerY + height};
        GetMonitorInfoW(MonitorFromRect(&saved, MONITOR_DEFAULTTONEAREST), &monitor);
        return fitWindowPoint({saved.left, saved.top}, width, height, monitor.rcWork);
    }
    HWND anchor = g->attached && IsWindow(g->dialog.hwnd) && !IsIconic(g->dialog.hwnd) ? g->dialog.hwnd :
                  IsWindowVisible(g->panel) && !IsIconic(g->panel) ? g->panel : nullptr;
    POINT mouse{}; GetCursorPos(&mouse);
    auto screen = anchor ? MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST) :
                  MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTONULL);
    if (!screen) screen = MonitorFromPoint(mouse, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfoW(screen, &monitor);
    RECT reference = anchor ? visibleWindowRect(anchor) : monitor.rcWork;
    return fitWindowPoint({reference.left + (reference.right - reference.left - width) / 2,
                           reference.top + (reference.bottom - reference.top - height) / 2}, width, height, monitor.rcWork);
}
void positionManager(bool opening) {
    RECT current{}; if (!GetWindowRect(g->manager, &current)) return;
    int width = current.right - current.left, height = current.bottom - current.top;
    POINT point{current.left, current.top};
    if (opening) point = managerOpenPoint(width, height);
    else {
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST), &monitor);
        point = fitWindowPoint(point, width, height, monitor.rcWork);
    }
    if (point.x != current.left || point.y != current.top)
        SetWindowPos(g->manager, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
void rememberManagerPosition() {
    RECT current{};
    if (IsIconic(g->manager) || IsZoomed(g->manager) || !GetWindowRect(g->manager, &current)) return;
    if (g->cfg.managerPositionSaved && g->cfg.managerX == current.left && g->cfg.managerY == current.top) return;
    g->cfg.managerPositionSaved = true; g->cfg.managerX = current.left; g->cfg.managerY = current.top;
    persist();
}
void presentManager() {
    // dev.ps1 and the startup shortcut launch with STARTF_USESHOWWINDOW/SW_HIDE.
    // Windows may override the first ShowWindow(SW_SHOW) with that startup state.
    // An explicit management action must make the window visible on this call.
    ShowWindow(g->manager, IsIconic(g->manager) ? SW_RESTORE : SW_SHOW);
    if (IsIconic(g->manager)) ShowWindow(g->manager, SW_RESTORE);
    positionManager(false);
    SetWindowPos(g->manager, HWND_TOP, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!IsWindowVisible(g->manager) || IsIconic(g->manager)) {
        g->status = L"管理窗口未能显示，请重试。";
        if (g->attached) updatePanelContent();
        return;
    }
    SetForegroundWindow(g->manager);
    // Hand focus to the visible editor before hiding the clicked helper.
    hidePanel();
}
void showManager(bool present) {
    g->presetEditor = false;
    KillTimer(g->control, T_ACTION);
    if (g->manager) {
        auto tab = GetDlgItem(g->manager, M_TAB);
        if(!settingsDialogs.empty()){
            int next=g->managerTab;g->managerTab=TabCtrl_GetCurSel(tab);
            if(next!=g->managerTab)requestSettingsNavigation(next);
            if(present)presentManager();return;
        }
        if (TabCtrl_GetCurSel(tab) != g->managerTab ||
            (g->managerTab == MANAGER_AFFIXES && g->builtAffixCategory != g->affixCategory)) {
            int next = g->managerTab, category = g->affixCategory;
            g->managerTab = TabCtrl_GetCurSel(tab);
            if (g->managerTab == MANAGER_AFFIXES) g->affixCategory = g->builtAffixCategory;
            if (!discardManagerDraft()) { if (present) presentManager(); return; }
            if (g->managerTab == MANAGER_TEMPLATES) g->variableEditor = false;
            g->managerTab = next; g->affixCategory = category;
            TabCtrl_SetCurSel(tab, g->managerTab);
            g->editing = 0;
            buildManagerPage();
        }
        if (present) presentManager();
        return;
    }
    int w = px(MANAGER_WIDTH), h = px(MANAGER_HEIGHT);
    auto point = managerOpenPoint(w, h);
    g->manager =
        CreateWindowExW(WS_EX_APPWINDOW|WS_EX_CONTROLPARENT, L"Jump.Manager", L"Jump · 设置中心",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN, point.x,
                        point.y, w, h, nullptr, nullptr, g->instance, nullptr);
    if (!g->manager) {
        g->status = L"无法创建管理窗口，请重试。";
        if (g->attached) updatePanelContent();
        return;
    }
    int managerDpi=static_cast<int>(GetDpiForWindow(g->manager));
    if(managerDpi!=g->dpi){g->dpi=managerDpi;fonts();}
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g->manager, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(g->manager, 35, &g->bg, sizeof(g->bg));
    DwmSetWindowAttribute(g->manager, 36, &g->fg, sizeof(g->fg));
    auto tab = managerChild(g->manager, WC_TABCONTROLW, L"", WS_TABSTOP | TCS_FIXEDWIDTH | TCS_BUTTONS | TCS_FLATBUTTONS | TCS_MULTILINE,
                            M_TAB, 12, 54, 124, 340);
    for (auto name : managerTitles) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(name);
        TabCtrl_InsertItem(tab, TabCtrl_GetItemCount(tab), &item);
    }
    // Inserting captions recalculates the native button-tab widths. Apply the
    // compact size after insertion so selecting the final page cannot scroll
    // the first page out of view.
    TabCtrl_SetMinTabWidth(tab, 0);
    TabCtrl_SetPadding(tab, 0, 0);
    TabCtrl_SetItemSize(tab, px(120), px(32));
    TabCtrl_SetCurSel(tab, g->managerTab);
    buildManagerPage();
    positionManager(true);
    if (present) presentManager();
}
#include "settings_center.h"
bool handleAppShortcut(const MSG &message) {
    if (message.message != WM_KEYDOWN || !(GetKeyState(VK_CONTROL) & 0x8000) ||
        (GetKeyState(VK_MENU) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000) ||
        GetPropW(GetFocus(), L"Jump.IME") || g->structuredIme) return false;
    HWND root = GetAncestor(message.hwnd, GA_ROOT);
    HWND owner = nullptr;
    int id = 0;
    if (root == g->manager && message.wParam == 'S') {
        owner = settingsDialogs.empty() ? g->manager : settingsDialogs.back()->window;
        if (!settingsDialogs.empty()) id = IDOK;
        else if (g->managerTab == 3) id = M_OPTIONS_SAVE;
        else if (g->managerTab == MANAGER_TEMPLATES) id = ID_VARIABLE_SAVE;
        else if (g->managerTab == 1 || g->managerTab == 2 || managingAffixes()) id = M_SAVE;
    } else if (root == g->panel && (message.wParam == 'F' || message.wParam == 'L')) {
        owner = g->panel;
        id = message.wParam == 'F' ? ID_DIRECTORY_SEARCH : ID_DIRECTORY_EDIT;
    }
    auto button = id ? GetDlgItem(owner, id) : nullptr;
    if (!button || !IsWindowVisible(button) || !IsWindowEnabled(button)) return false;
    PostMessageW(owner, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(button));
    return true;
}
bool selfTestManagerPosition() {
    auto originalDialog = g->dialog; bool originalAttached = g->attached;
    bool saved = g->cfg.managerPositionSaved; int savedX = g->cfg.managerX, savedY = g->cfg.managerY;
    auto foreground = GetForegroundWindow();
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST), &monitor);
    const auto &work = monitor.rcWork;
    int centerX = (work.left + work.right) / 2 + std::max(0L, (work.right - work.left - px(MANAGER_WIDTH)) / 8);
    int centerY = (work.top + work.bottom) / 2 + std::max(0L, (work.bottom - work.top - px(MANAGER_HEIGHT + 40)) / 8);
    auto anchor = CreateWindowExW(0, L"STATIC", L"Jump placement check", WS_POPUP,
        centerX - px(150), centerY - px(100), px(300), px(200), nullptr, nullptr, g->instance, nullptr);
    g->dialog.hwnd = anchor; g->attached = true; g->cfg.managerPositionSaved = false;
    bool ok = [&]() {
        showManager(false);
        RECT r{}; GetWindowRect(g->manager, &r);
        if (std::abs((r.left + r.right) / 2 - centerX) > 1 || std::abs((r.top + r.bottom) / 2 - centerY) > 1) return false;
        auto point = fitWindowPoint({r.left + px(24), r.top + px(16)}, r.right - r.left, r.bottom - r.top, work);
        SetWindowPos(g->manager, nullptr, point.x, point.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(g->manager, WM_EXITSIZEMOVE, 0, 0);
        auto stored = load(g->configFile);
        if (!stored.managerPositionSaved || stored.managerX != point.x || stored.managerY != point.y) return false;
        g->attached = false; DestroyWindow(g->manager); g->attached = true;
        showManager(false); GetWindowRect(g->manager, &r);
        if (r.left != point.x || r.top != point.y) return false;
        g->attached = false; DestroyWindow(g->manager); g->attached = true;
        g->cfg.managerX = 100000; g->cfg.managerY = -100000;
        showManager(false); GetWindowRect(g->manager, &r);
        MONITORINFO nearest{sizeof(nearest)};
        GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &nearest);
        return r.left >= nearest.rcWork.left && r.top >= nearest.rcWork.top &&
               r.right <= nearest.rcWork.right && r.bottom <= nearest.rcWork.bottom &&
               !IsWindowVisible(g->manager) && GetForegroundWindow() == foreground;
    }();
    g->attached = false; if (g->manager) DestroyWindow(g->manager);
    DestroyWindow(anchor); g->dialog = originalDialog; g->attached = originalAttached;
    g->cfg.managerPositionSaved = saved; g->cfg.managerX = savedX; g->cfg.managerY = savedY;
    persist();
    return ok || hiddenCheckFailure("selfTestManagerPosition", __LINE__);
}
bool selfTestCurrentManager(const std::filesystem::path &directory){
    auto foreground=GetForegroundWindow();g->attached=false;
    const wchar_t *pages[]{L"manager-schemes.bmp",L"manager-locations.bmp",L"manager-workflows.bmp",L"manager-options.bmp",L"manager-prefixes.bmp",L"manager-expressions.bmp",L"manager-projects.bmp",L"manager-backups.bmp",L"manager-connections.bmp"};
    for(int page=0;page<MANAGER_PAGES;++page){
        g->managerTab=page;showManager(false);
        if(TabCtrl_GetItemCount(GetDlgItem(g->manager,M_TAB))!=MANAGER_PAGES)return hiddenCheckFailure("currentManager",__LINE__);
        RECT client{};GetClientRect(g->manager,&client);
        for(HWND child=GetWindow(g->manager,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
            if(!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE))continue;
            RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,g->manager,reinterpret_cast<POINT*>(&r),2);
            if(r.left<0||r.top<0||r.right>client.right||r.bottom>client.bottom)return hiddenCheckFailure("currentManagerBounds",__LINE__);
        }
        if(page==0&&(GetDlgItem(g->manager,201)||GetDlgItem(g->manager,205)||!GetDlgItem(g->manager,M_COPY_SCHEME)))return hiddenCheckFailure("currentSchemePage",__LINE__);
        if(page==2&&(GetDlgItem(g->manager,226)||GetDlgItem(g->manager,227)||GetDlgItem(g->manager,225)))return hiddenCheckFailure("currentWorkflowPage",__LINE__);
        if(!saveHiddenPanelPreview(directory/pages[page],g->manager))return false;
        if(IsWindowVisible(g->manager)||GetForegroundWindow()!=foreground)return false;
    }
    DestroyWindow(g->manager);return true;
}


void showPresetManager(bool add, size_t index, bool present) {
    if (g->manager && settingsDialogs.empty() && managerDraftDirty()) {
        if (!discardManagerDraft()) { if (present) presentManager(); return; }
        if (g->managerTab == MANAGER_TEMPLATES) g->variableEditor = false;
        buildManagerPage();
    }
    g->managerTab = MANAGER_AFFIXES;
    if (g->namingMode) g->affixCategory = g->namingMode - 1;
    showManager(false);
    if (!g->manager || g->managerTab != MANAGER_AFFIXES || !settingsDialogs.empty()) return;
    g->editing = index; managerList();
    if (add) SendMessageW(g->manager, WM_COMMAND, M_ADD, reinterpret_cast<LPARAM>(GetDlgItem(g->manager, M_ADD)));
    if (present) { showManager(); SetFocus(GetDlgItem(g->manager, M_VALUE)); }
}
bool checkManagerShow(const std::filesystem::path &directory, const std::wstring &entry) {
    // Run before any other top-level ShowWindow call, including panel hiding.
    // STARTF_USESHOWWINDOW / SW_HIDE must not swallow this first explicit user action.
    g->managerTab = entry == L"locations" ? 1 : (entry == L"prefixes" || entry == L"suffixes") ? MANAGER_AFFIXES : 0;
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
    report << "{\"firstVisible\":" << (first ? "true" : "false") << ",\"reused\":" << (reused ? "true" : "false")
        << ",\"restored\":" << (restored ? "true" : "false") << "}";
    if (g->manager) DestroyWindow(g->manager);
    return first && reused && restored;
}
#include "save_notice.h"
void tray(bool add) {
    NOTIFYICONDATAW n{};
    n.cbSize = sizeof(n);
    n.hWnd = g->control;
    n.uID = 1;
    n.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = static_cast<HICON>(LoadImageW(g->instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    wcscpy_s(n.szTip, g->paused ? L"Jump · 已暂停" : L"Jump · 后台保存助手");
    Shell_NotifyIconW(add ? NIM_ADD : NIM_MODIFY, &n);
    if (add) {
        n.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &n);
    }
}
void bindingNotice(const std::wstring &text) {
    NOTIFYICONDATAW n{}; n.cbSize = sizeof(n); n.hWnd = g->control; n.uID = 1;
    n.uFlags = NIF_INFO; n.dwInfoFlags = NIIF_INFO;
    wcscpy_s(n.szInfoTitle, L"Jump · 文件名关联");
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE); Shell_NotifyIconW(NIM_MODIFY, &n);
}
void cancelBinding() {
    KillTimer(g->control, T_BIND);
    UnregisterHotKey(g->control, 2);
    if (g->bindingHook) UnhookWindowsHookEx(g->bindingHook);
    g->bindingHook = nullptr;
    if (g->bindingHint) DestroyWindow(g->bindingHint);
    g->bindingHint = nullptr;
}
LRESULT CALLBACK bindingMouseHook(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && message == WM_LBUTTONUP && g && g->bindingHint) {
        g->bindingPoint = reinterpret_cast<MSLLHOOKSTRUCT *>(data)->pt;
        PostMessageW(g->control, WM_BIND_CAPTURE, 0, 0);
    }
    return CallNextHookEx(nullptr, code, message, data);
}
void bindTarget(HWND target, POINT point, bool focused) {
    cancelBinding();
    DWORD pid = 0; GetWindowThreadProcessId(target, &pid);
    if (!target || !pid || pid == g->pid || !IsWindowVisible(target) || !IsWindowEnabled(target)) {
        bindingNotice(L"请在原软件的保存窗口中，选中文件名输入框后重试。"); return;
    }
    detach();
    g->dialog = {}; g->dialog.hwnd = target; g->dialog.pid = pid;
    g->suppressed = nullptr;
    Job job{JobType::BindControl, target, pid, ++g->generation};
    job.point = point; job.focusedBinding = focused; enqueue(std::move(job));
}
void startBinding() {
    cancelBinding(); hidePanel();
    MONITORINFO monitor{sizeof(monitor)};
    POINT cursor{}; GetCursorPos(&cursor);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
    bool escape = RegisterHotKey(g->control, 2, MOD_NOREPEAT, VK_ESCAPE) != 0;
    g->bindingHint = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"STATIC", escape ? L"点击原软件的文件名输入框以记住关联\nEsc 取消 · 20 秒内有效" :
        L"点击原软件的文件名输入框以记住关联\n20 秒后自动退出",
        WS_POPUP | WS_BORDER | SS_CENTER, monitor.rcWork.right - 400, monitor.rcWork.top + 24,
        380, 62, nullptr, nullptr, g->instance, nullptr);
    SendMessageW(g->bindingHint, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
    ShowWindow(g->bindingHint, SW_SHOWNOACTIVATE);
    g->bindingDeadline = GetTickCount64() + 20000;
    // Observe the next click without consuming it. A polling-only picker can miss short clicks.
    g->bindingHook = SetWindowsHookExW(WH_MOUSE_LL, bindingMouseHook, g->instance, 0);
    if (!g->bindingHook) {
        cancelBinding(); bindingNotice(L"请选中文件名输入框后按 Ctrl+Alt+N 建立关联。"); return;
    }
    SetTimer(g->control, T_BIND, 25, nullptr);
}
void pollBinding() {
    if (GetTickCount64() >= g->bindingDeadline) { cancelBinding(); inspectForeground(); }
}
void trayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 1, L"设置中心…");
    AppendMenuW(menu, MF_STRING, 2, g->paused ? L"恢复自动显示" : L"暂停自动显示");
    HMENU tools = CreatePopupMenu();
    AppendMenuW(tools, MF_STRING, ID_TRANSFER, L"迁移、备份与恢复…");
    AppendMenuW(tools, MF_STRING, ID_DIAGNOSTICS, L"当前接入与诊断…");
    AppendMenuW(tools, MF_STRING, 3, L"打开配置目录");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"更多工具");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 4, L"退出 Jump");
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(g->control);
    int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, g->control);
    DestroyMenu(menu);
    if (command == ID_TRANSFER) showTransfer();
    else if (command == ID_DIAGNOSTICS) showDiagnostics();
    else if (command == 1)
        showManager();
    else if (command == 2) {
        g->paused = !g->paused;
        if (g->paused)
            hidePanel();
        else
            inspectForeground();
        tray(false);
    } else if (command == 3)
        ShellExecuteW(nullptr, L"open", g->configFile.parent_path().c_str(), nullptr, nullptr, SW_SHOW);
    else if (command == 4)
        PostMessageW(g->control, WM_CLOSE, 0, 0);
}
LRESULT CALLBACK ControlProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g->taskbarCreated) {
        tray(true);
        return 0;
    }
    switch (msg) {
    case WM_COMMAND:
        return 0;
    case WM_TARGET_RESULT:{
        if(!g->targetWorker)return 0;auto result=g->targetWorker->take();
        if(!result||!g->attached||result->request.generation!=g->generation||result->request.path!=g->targetPath||result->request.path!=currentSaveTarget())return 0;
        g->targetInfo=std::move(*result);updatePanelContent();return 0;
    }
    case WM_NAMING_RESULT: {
        if (!g->namingWorker) return 0;
        auto result = g->readyNaming ? std::move(g->readyNaming) : g->namingWorker->take();
        g->readyNaming.reset();
        if (!result || !g->attached || result->request.id != g->namingId ||
            result->request.generation != g->generation || result->request.key != g->smartCacheKey) return 0;
        if (g->busy) { g->readyNaming = std::move(result); return 0; }
        g->namingPending = g->namingSubmit = false;
        g->smartPreview = result->preview; g->ruleError = result->preview.error;
        if (!g->ruleError.empty()) {
            g->status = g->ruleError; g->statusError = true; updatePanelContent(); return 0;
        }
        g->stemName = result->preview.name; g->dirtyName = g->localNamePending = true;
        g->statusError = false; g->status = L"编号已更新；通过 Jump 保存时将再次核验";
        updatePanelContent();
        bool submit = result->request.submit;
        if (submit && result->request.displayed != g->stemName) {
            auto text = L"编号核验结果发生变化：\n\n原预览：" + result->request.displayed +
                result->request.extension + L"\n新名称：" + g->stemName + result->request.extension +
                L"\n\n是否使用新名称继续保存？原软件的覆盖确认仍会保留。";
            submit = MessageBoxW(g->panel, text.c_str(), L"确认编号变化", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
            if (!g->attached || result->request.generation != g->generation || result->request.id != g->namingId) return 0;
        }
        g->numberingApproved = true;
        apply(result->request.directory, g->stemName, submit, true);
        g->numberingApproved = false;
        return 0;
    }
    case WM_BIND_CAPTURE:
        if (g->bindingHint) {
            auto target = GetAncestor(WindowFromPoint(g->bindingPoint), GA_ROOT);
            DWORD pid = 0; GetWindowThreadProcessId(target, &pid);
            if (pid && pid != g->pid) bindTarget(target, g->bindingPoint, false);
        }
        return 0;
    case WM_HOTKEY:
        if (wp == 1) bindTarget(GetForegroundWindow(), {}, true);
        else if (wp == 2) { cancelBinding(); inspectForeground(); }
        else if (wp == 3) { cancelBinding(); inspectForeground(true); }
        return 0;
    case WM_BLENDER_BROWSER:
    case WM_HOUDINI_BROWSER: {
        HWND target = reinterpret_cast<HWND>(wp);
        DWORD pid = 0; GetWindowThreadProcessId(target, &pid);
        if (pid != static_cast<DWORD>(lp) ||
            !(msg == WM_HOUDINI_BROWSER ? houdiniBrowserWindow(target) : blenderBrowserWindow(target))) return 0;
        if (g->attached && g->dialog.hwnd == target) {
            if (!g->busy) SetTimer(hwnd, T_REFRESH, 50, nullptr);
        } else if (GetForegroundWindow() == target) SetTimer(hwnd, T_INSPECT, 30, nullptr);
        return 0;
    }
    case WM_SAVE_BRIDGE: {
        HWND target = reinterpret_cast<HWND>(wp);
        DWORD pid = 0; GetWindowThreadProcessId(target, &pid);
        if (!pid || pid == g->pid || pid != static_cast<DWORD>(lp) ||
            (g->testTargetPid && pid != g->testTargetPid) ||
            (!g->testTargetImage.empty() && processImage(target) != g->testTargetImage)) return 0;
        if (reinterpret_cast<ULONG_PTR>(GetPropW(target, L"Jump.SaveBridge.v1")) != pid) {
            if (g->attached && g->dialog.bridge && g->dialog.hwnd == target) detach();
            return 0;
        }
        g->inspectAttempts = 0;
        if (g->attached && g->dialog.hwnd == target) {
            if (!g->busy) SetTimer(hwnd, T_REFRESH, 50, nullptr);
        } else if (GetForegroundWindow() == target) SetTimer(hwnd, T_INSPECT, 30, nullptr);
        return 0;
    }
    case WM_PANEL_REFRESH:
        if (g->attached && g->rebuildPending)
            rebuildPanel();
        return 0;
    case WM_EVENT: {
        DWORD event = static_cast<DWORD>(wp);
        HWND target = reinterpret_cast<HWND>(lp);
        if (event == EVENT_OBJECT_DESTROY && target == g->suppressed)
            g->suppressed = nullptr;
        if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_OBJECT_SHOW) g->inspectAttempts = 0;
        if (event == EVENT_SYSTEM_FOREGROUND) {
            if (g->attached && !attachedDialogAlive())
                detach();
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        } else if (g->attached && target == g->dialog.hwnd &&
                   (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE)) {
            detach();
        } else if (event == EVENT_OBJECT_SHOW && (windowClass(target) == L"#32770" ||
                   windowClass(target).starts_with(L"GHOST_WindowClass") || houdiniBrowserWindow(target) || genericCandidate(target)))
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        else if (g->attached && event == EVENT_OBJECT_LOCATIONCHANGE && target == g->dialog.hwnd)
            reposition();
        else if (g->attached && !g->busy && GetAncestor(target, GA_ROOT) == g->dialog.hwnd &&
                 (event == EVENT_OBJECT_VALUECHANGE || event == EVENT_OBJECT_NAMECHANGE))
            SetTimer(hwnd, T_REFRESH, 120, nullptr);
        else if (event == EVENT_OBJECT_NAMECHANGE && target == GetForegroundWindow() && looksLikeFileDialog(target)) {
            g->inspectAttempts = 0;
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        }
        return 0;
    }
    case WM_TIMER:
        if(wp==T_TARGET_CHECK){KillTimer(hwnd,T_TARGET_CHECK);
            auto path=currentSaveTarget();if(g->targetWorker&&!path.empty()&&path==g->targetPath)
                g->targetWorker->request({0,g->generation.load(),path});return 0;}
        if (wp == T_BIND) { pollBinding(); return 0; }
        if (wp == T_GENERIC_REFRESH) {
            if (g->attached && g->dialog.generic && !g->busy && !g->localNamePending && !g->paused &&
                (GetForegroundWindow() == g->dialog.hwnd || GetForegroundWindow() == g->panel))
                enqueue({JobType::Refresh, g->dialog.hwnd, g->dialog.pid, g->generation.load()});
            return 0;
        }
        KillTimer(hwnd, wp);
        if (wp == T_INSPECT)
            inspectForeground();
        else if (wp == T_REFRESH) {
            if (g->attached && !g->busy &&
                (GetForegroundWindow() == g->dialog.hwnd || GetForegroundWindow() == g->panel))
                enqueue({JobType::Refresh, g->dialog.hwnd, g->dialog.pid, g->generation.load()});
        } else if (wp == T_SEARCH) {
            if (g->attached && !g->busy && GetForegroundWindow() == g->panel && IsWindowVisible(g->panel))
                rebuildPanel();
        } else if (wp == T_ACTION) {
            // Naming chips do not activate Jump. The attached native dialog is
            // also a valid foreground owner for their delayed, fill-only update.
            if (g->attached && !g->busy && !g->editingPath && g->dirtyName && g->localNamePending &&
                focusAllowsAction({JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load()}))
                apply(g->dialog.directory, g->stemName, false);
        } else if (wp == T_SAVE_VERIFY) {
            auto now = GetTickCount64();
            for (auto it = g->saveChecks.begin(); it != g->saveChecks.end();) {
                if (it->verifyAt <= now) { enqueue(*it); it = g->saveChecks.erase(it); }
                else ++it;
            }
            armSaveChecks();
        }
        return 0;
    case WM_RESULT: {
        std::unique_ptr<Result> result(reinterpret_cast<Result *>(lp));
        if (result->job.type == JobType::ResolvePaste) { finishPastedTarget(*result); return 0; }
        // Some custom exporters keep their window open after saving. Start the
        // file watch on accepted invocation as well as on dialog closure.
        if (result->job.generic && result->job.type == JobType::Apply && result->submitted && result->error.empty())
            verifySavedFile(result->probe);
        if (result->job.type == JobType::VerifySave) {
            if (result->saved) {
                std::erase_if(g->saveChecks, [&](const Job &pending) { return pending.probe.file == result->job.probe.file; });
                armSaveChecks();
                if (!result->verifiedFile.empty()) {
                    SYSTEMTIME now{}; GetLocalTime(&now); wchar_t stamp[40]{};
                    swprintf_s(stamp, L"%04d-%02d-%02d %02d:%02d:%02d", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
                    SavedFile record{result->verifiedFile, result->job.probe.process, stamp,
                        result->job.probe.namingStructure,result->job.probe.namingProject};
                    rememberSavedFile(g->cfg,record);
                    rememberSubject(g->cfg,record.projectId,recentSavedSubject(record));
                    g->settingsDirty = true;
                    showVerifiedSaveNotice(*result);
                }
                rememberSavedDirectory(result->job.probe.directory);
            }
            else if (result->job.verifyAttempt < 4) {
                // Brief, event-scoped retries allow the host to write after closing its dialog,
                // without occupying the UIA worker or polling while the app is idle.
                constexpr UINT delays[] = {50, 100, 250, 400};
                auto retry = result->job;
                retry.verifyAt = GetTickCount64() + delays[retry.verifyAttempt++];
                std::erase_if(g->saveChecks, [&](const Job &pending) { return pending.probe.file == retry.probe.file; });
                g->saveChecks.push_back(std::move(retry));
                armSaveChecks();
            }
            return 0;
        }
        if (result->job.generation != g->generation.load()) {
            if (result->error.empty() && (!IsWindow(result->job.hwnd) || !IsWindowVisible(result->job.hwnd)))
                verifySavedFile(result->probe);
            return 0;
        }
        if (result->job.type == JobType::Apply && result->submitted &&
            (!attachedDialogAlive() || !IsWindowVisible(result->job.hwnd))) {
            g->saveProbe = preserveSaveBaseline(g->saveProbe, result->probe);
            detach(); return 0;
        }
        if ((result->job.type == JobType::Inspect || result->job.type == JobType::BindControl) && !sameTarget(result->job))
            return 0;
        if (!result->error.empty()) {
            if (result->job.type == JobType::BindControl) { bindingNotice(result->error); return 0; }
            if (result->job.requestedInspect) {
                bindingNotice(L"当前窗口没有唯一可读写的文件名框。请先打开原软件的保存窗口；若已打开，可点选文件名框后按 Ctrl+Alt+N 建立直接关联。");
                return 0;
            }
            g->statusError = true;
            if (result->job.type == JobType::Refresh && result->job.generic) { detach(); return 0; }
            if (result->job.type == JobType::Inspect && g->inspectAttempts < 4 && GetForegroundWindow() == result->job.hwnd)
                SetTimer(hwnd, T_INSPECT, 180 * std::max(1, g->inspectAttempts), nullptr);
            g->busy = false;
            g->busySubmitting=false;g->saveAfterFill.reset();
            if (g->readyNaming) PostMessageW(g->control, WM_NAMING_RESULT, 0, 0);
            if (g->pathPending) { g->pathPending = false; g->editingPath = true; }
            g->pendingQuick = false;
            g->status = result->error;
            if (g->attached) {
                for (auto h : g->children) EnableWindow(h, TRUE);
                updatePanelContent();
                if (result->data.generic && result->job.type == JobType::Apply) {
                    g->localNamePending = false;
                    enqueue({JobType::Refresh, g->dialog.hwnd, g->dialog.pid, g->generation.load()});
                }
            }
            return 0;
        }
        g->statusError = false;
        if (result->job.type == JobType::BindControl) {
            const auto &binding = result->binding;
            std::erase_if(g->cfg.filenameBindings, [&](const auto &previous) {
                return filename_controls::scope(previous, binding.executable, binding.windowClass, binding.windowTitle);
            });
            if (g->cfg.filenameBindings.size() >= 100) g->cfg.filenameBindings.erase(g->cfg.filenameBindings.begin());
            g->cfg.filenameBindings.push_back(binding);
            persist(); result->job.type = JobType::Inspect;
        }
        if (result->job.type == JobType::Inspect) {
            g->inspectAttempts = 0;
            g->lastHistoryDirectory.clear();
            g->dialog = result->data;
            g->saveProbe = result->probe;
            g->saveProbe.namingProject = g->cfg.activeProject;
            g->nativeSubjectPending.reset();
            g->originalName = g->dialog.name;
            g->originalStem = stripExtension(g->dialog.name, g->dialog.extension);
            g->originalDirectory = g->dialog.directory;
            g->stemName = stripExtension(g->dialog.name, g->dialog.extension);
            g->parts = splitName(g->stemName, g->cfg.modifiers);
         g->inlineTool = 0;
            resetSmartNaming();
            g->namingHistory.clear();
            g->structure={};g->freeRecovery.reset();g->structuredField=-1;g->structuredDraft.clear();
            g->ruleEnabled = false; g->ruleError.clear();
            g->namingMode = 1;

            rememberCurrentDirectory();
            g->openedLocations = result->opened;
            g->paths = rankLocations(g->cfg, g->dialog.source, result->opened);
            g->locationTab = !g->cfg.favorites.empty() ? 1 : !g->cfg.recentSaves.empty() ? 4 : 0;
            g->locationPage = 0;
            g->dirtyName = false;
            g->localNamePending = false;
            g->attached = true;
            resumeNamingDraft();
            g->editingPath = g->pathPending = false;
            g->pathDraft.clear();
            g->suppressed = nullptr;
            g->status = g->dialog.generic ? L"单击泡泡填入，在原窗口确认保存" :
                g->dialog.save ? L"单击填入，Enter 或原窗口保存" : L"单击位置，跳转到文件夹";
            g->dpi = static_cast<int>(GetDpiForWindow(g->dialog.hwnd));
            fonts();
            paintTheme();
            targetHooks();
            rebuildPanel();
            if(g->structuredResumeNumber){g->structuredResumeNumber=false;updateInlineRule();}
            if (g->dialog.generic) SetTimer(hwnd, T_GENERIC_REFRESH, 700, nullptr);
            if (GetForegroundWindow() == g->dialog.hwnd)
                reposition(true);
        } else if (result->job.type == JobType::Refresh) {
            if (g->busy)
                return 0;
            auto previousStem = g->stemName, previousDirectory = g->dialog.directory,
                 previousExtension = g->dialog.extension, previousFormat = g->dialog.format;
            bool previousDirty = g->dirtyName, previousSettingsDirty = g->settingsDirty;
            bool needsLocations = result->data.genericNavigate && !GetDlgItem(g->panel, ID_DIRECTORY);
            g->saveProbe = preserveSaveBaseline(g->saveProbe, result->probe);
            auto observedStem = stripExtension(result->data.name, result->data.extension);
            bool nativeEdit = GetForegroundWindow() == g->dialog.hwnd && observedStem !=
                stripExtension(g->dialog.name, g->dialog.extension);
            if (!g->localNamePending || nativeEdit) {
                bool changed = observedStem != g->stemName;
                g->dialog = result->data;
                g->stemName = stripExtension(g->dialog.name, g->dialog.extension);
                if (changed) {
                    adoptNativeName();
                }
                g->localNamePending = false;
                // Associate native readback with the same naming/project data
                // as Jump fills, so native Save can record and restore it too.
                if(!g->saveProbe.file.empty()&&lower(stripExtension(std::filesystem::path(g->saveProbe.file).filename().wstring(),g->dialog.extension))==lower(g->stemName)){
                    g->saveProbe.namingStructure=savedStructure();g->saveProbe.namingProject=g->cfg.activeProject;g->saveProbe.namingKnown=true;
                }
                if (changed) g->dirtyName = g->stemName != g->originalStem;
                KillTimer(g->control, T_ACTION);
            } else {
                g->dialog.directory = result->data.directory;
                g->dialog.extension = result->data.extension;
                g->dialog.format = result->data.format;
            }
            rememberCurrentDirectory();
            if ((g->numberMode || g->folderName || g->variablesEnabled) &&
                (g->dialog.directory != previousDirectory || g->dialog.extension != previousExtension)) {
                updateInlineRule();
            }
            if (needsLocations) rebuildPanel();
            else if (g->stemName != previousStem || g->dialog.directory != previousDirectory ||
                g->dialog.extension != previousExtension || g->dialog.format != previousFormat ||
                g->dirtyName != previousDirty || g->settingsDirty != previousSettingsDirty)
                updatePanelContent();
        } else {
            g->busy = false;
            g->busySubmitting=false;
            if (g->readyNaming) PostMessageW(g->control, WM_NAMING_RESULT, 0, 0);
            g->pathPending = false;
            if (result->navigated) { g->editingPath = false; g->pathDraft.clear(); }
            g->saveProbe = preserveSaveBaseline(g->saveProbe, result->probe);
            bool pendingEdit = g->stemName != result->job.name;
            if (!pendingEdit) g->localNamePending = false;
            g->dialog = result->data;
            if (!pendingEdit)
                g->stemName = stripExtension(g->dialog.name, g->dialog.extension);
            if (result->navigated || result->submitted) rememberCurrentDirectory();
            g->status =
                result->submitted ? L"已提交给原软件，等待保存结果" :
                result->navigated && g->dialog.generic ?
                    (g->dialog.genericDestination ? L"已更新输出位置和名称，请在原窗口确认保存" :
                     g->dialog.genericAddress ? L"已切换目录，文件名已保留" : L"已切换目录，名称与格式已核验") :
                g->dialog.generic ? L"已填入，请在原窗口确认保存" : L"已同步到原窗口，可继续编辑";
            if (!result->submitted && g->applyFocus && IsWindow(g->applyFocus) &&
                GetAncestor(g->applyFocus, GA_ROOT) == g->panel &&
                GetForegroundWindow() == g->dialog.hwnd) {
                SetForegroundWindow(g->panel);
                SetFocus(g->applyFocus);
            }
            if (result->submitted) {
                g->saveAfterFill.reset();
                g->pendingQuick = false;
                hidePanel();
            } else if (g->pendingQuick) {
                g->pendingQuick = false;
                apply(g->dialog.directory, g->stemName, g->cfg.quickSave && g->dialog.save);
            } else {
                for (auto h : g->children)
                    enableIfChanged(h, TRUE);
                updatePanelContent();
                if(g->saveAfterFill){
                    auto save=*g->saveAfterFill;g->saveAfterFill.reset();
                    if(sameTarget(save)&&save.name==g->stemName){apply(save.directory,save.name,true,true);return 0;}
                }
                if (pendingEdit) {
                    // Keep the user's newer text when a worker finishes an older edit.
                    if (GetForegroundWindow() == g->panel || GetForegroundWindow() == g->dialog.hwnd)
                        apply(g->dialog.directory, g->stemName, false);
                }
            }
        }
        return 0;
    }
    case WM_TRAY:
        if (LOWORD(lp) == WM_CONTEXTMENU || LOWORD(lp) == WM_RBUTTONUP)
            trayMenu();
        else if (LOWORD(lp) == NIN_SELECT || LOWORD(lp) == WM_LBUTTONDBLCLK)
            showManager();
        return 0;
    case WM_CONFIG_SAVED:
        if(g->configWriter&&wp==g->configWriter->revision()){
            g->settingsDirty=!lp;g->configPending=false;
            if(!lp){g->status=L"自动记录未能保存，请检查配置目录权限；下次保存时会重试。";g->statusError=true;}
            if(g->attached)updatePanelContent();
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (g->attached) {
            paintTheme();
            rebuildPanel();
        }
        return 0;
    case WM_CLOSE:
        rememberEditedSubject();
        if(!flushPersistence())MessageBoxW(hwnd,L"最新自动记录未能保存，请检查配置目录权限。已有配置仍保留。",L"Jump",MB_OK|MB_ICONWARNING);
        g->namingWorker.reset();
        g->targetWorker.reset();KillTimer(hwnd,T_TARGET_CHECK);
        if (g->savedNotice) DestroyWindow(g->savedNotice);
        cancelBinding(); UnregisterHotKey(hwnd, 1); UnregisterHotKey(hwnd, 3);
        if (g->manager)
            DestroyWindow(g->manager);
        detach();
        for (auto hook : g->globalHooks)
            UnhookWinEvent(hook);
        g->globalHooks.clear();
        g->configWriter.reset();
        {
            NOTIFYICONDATAW n{};
            n.cbSize = sizeof(n);
            n.hWnd = hwnd;
            n.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &n);
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
std::filesystem::path configPath(const std::wstring &explicitPath) {
    if (!explicitPath.empty())
        return std::filesystem::absolute(explicitPath);
    wchar_t exe[32768];
    GetModuleFileNameW(nullptr, exe, 32768);
    auto folder = std::filesystem::path(exe).parent_path() / L"data";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (!error) {
        auto probe = folder / L".write-probe";
        HANDLE f = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            CloseHandle(f);
            return folder / L"settings.ini";
        }
    }
    PWSTR local = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
        auto out = std::filesystem::path(local) / L"Jump" / L"settings.ini";
        CoTaskMemFree(local);
        return out;
    }
    return folder / L"settings.ini";
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int argc = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc == 5 && std::wstring(args[1]) == L"--blender-probe") {
        auto reply = blender_bridge::request(reinterpret_cast<HWND>(_wcstoui64(args[3], nullptr, 10)),
            static_cast<DWORD>(wcstoul(args[2], nullptr, 10)), nullptr, L"INSPECT");
        auto report = reply.ok ? L"OK\n" + reply.name + L"\n" + reply.directory + L"\n" + reply.extension : L"ERROR\n" + reply.error;
        HANDLE file = CreateFileW(args[4], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD written = 0;
        if (file != INVALID_HANDLE_VALUE) {
            WriteFile(file, report.data(), static_cast<DWORD>(report.size() * sizeof(wchar_t)), &written, nullptr);
            CloseHandle(file);
        }
        LocalFree(args); CoUninitialize(); return reply.ok ? 0 : 1;
    }
    std::wstring config;
    bool manager = false, testUi = false;
    bool testHostOnly = false, testSolidMaterial = false;
    DWORD testTargetPid = 0;
    DWORD testFillDelayMs = 0;
    std::filesystem::path managerChecks;
    std::wstring managerEntry = L"locations";
    std::filesystem::path checks;
    std::filesystem::path assistChecks;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = args[i];
        if (a == L"--config" && i + 1 < argc)
            config = args[++i];
        else if (a == L"--manage")
            manager = true;
        else if (a == L"--test-ui")
            testUi = true;
        else if (a == L"--test-host-only")
            testHostOnly = true;
        else if (a == L"--test-target-pid" && i + 1 < argc)
            testTargetPid = wcstoul(args[++i], nullptr, 10);
        else if (a == L"--test-fill-delay-ms" && i + 1 < argc)
            testFillDelayMs = std::min<DWORD>(1000, wcstoul(args[++i], nullptr, 10));
        else if (a == L"--test-solid-material")
            testSolidMaterial = true;
        else if (a == L"--self-test" && i + 1 < argc)
            checks = args[++i];
        else if (a == L"--save-assist-check" && i + 1 < argc)
            assistChecks = std::filesystem::absolute(args[++i]);
        else if (a == L"--manager-show-check" && i + 1 < argc)
            managerChecks = std::filesystem::absolute(args[++i]);
        else if (a == L"--manager-entry" && i + 1 < argc)
            managerEntry = args[++i];
    }
    LocalFree(args);
    if(!assistChecks.empty()){bool ok=saveAssistChecks(assistChecks);CoUninitialize();return ok?0:1;}
    if (!checks.empty() && !(selfTest(checks) && workflowModelChecks(checks) && selfTestShellAddresses() && filename_controls::selfTest())) {
        CoUninitialize();
        return 1;
    }
    g = new State;
    runSettingsDialog=hostSettingsDialog;sizeSettingsDialog=layoutSettingsDialog;
    g->solidMaterial = !checks.empty() || (testUi && testSolidMaterial);
    g->testFillDelayMs = testUi && testHostOnly ? testFillDelayMs : 0;
    g->testTargetPid = testUi ? testTargetPid : 0;
    if (testHostOnly) {
        wchar_t executable[32768]{}; GetModuleFileNameW(nullptr, executable, 32768);
        g->testTargetImage = lower((std::filesystem::path(executable).parent_path() / L"Jump.TestHost.exe").wstring());
    }
    g->instance = instance;
    g->configFile = checks.empty() ? configPath(config) : std::filesystem::absolute(checks / L"inline-presets.ini");
    if (!managerChecks.empty()) g->configFile = managerChecks / L"settings.ini";
    std::wstring key =
        L"Local\\Jump.Native." + std::to_wstring(std::hash<std::wstring>{}(lower(g->configFile.wstring())));
    g->singleton = CreateMutexW(nullptr, FALSE, key.c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        auto existing = FindWindowW(L"Jump.Control", nullptr);
        if (existing && manager)
            PostMessageW(existing, WM_TRAY, 0, NIN_SELECT);
        CloseHandle(g->singleton);
        CoUninitialize();
        return 0;
    }
    g->cfg = load(g->configFile);
    if (!std::filesystem::exists(g->configFile))
        persist();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES | ICC_TAB_CLASSES};
    InitCommonControlsEx(&controls);
    dragMessage = RegisterWindowMessageW(DRAGLISTMSGSTRING);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(instance, MAKEINTRESOURCEW(101));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
    wc.lpszClassName = L"Jump.Control";
    wc.lpfnWndProc = ControlProc;
    RegisterClassExW(&wc);
    wc.lpszClassName = L"Jump.Panel";
    wc.lpfnWndProc = PanelProc;
    RegisterClassExW(&wc);
    wc.lpszClassName = L"Jump.Manager";
    wc.lpfnWndProc = ManagerProc;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"Jump.Flyout";
    wc.lpfnWndProc = FlyoutProc;
    RegisterClassExW(&wc);
    g->control = CreateWindowW(L"Jump.Control", L"Jump Background", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                               nullptr, instance, nullptr);
    g->panel = CreateWindowExW(WS_EX_TOPMOST | (testUi ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW) | WS_EX_NOACTIVATE, L"Jump.Panel", L"Jump 保存助手",
                               WS_POPUP | WS_CLIPCHILDREN, 0, 0, PANEL_WIDTH, 382, nullptr, nullptr, instance, nullptr);
    g->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    fonts();
    paintTheme();
    if (!managerChecks.empty()) {
        bool ok = checkManagerShow(managerChecks, managerEntry);
        DestroyWindow(g->panel); DestroyWindow(g->control); CloseHandle(g->singleton);
        CoUninitialize(); return ok ? 0 : 1;
    }
    if (!checks.empty()) {
        DeleteFileW((checks / L"failure.txt").c_str());
        bool ok = selfTestPanelLayer() &&
                  saveReferencePreview(checks) && selfTestPaintStability() && selfTestManagerPosition() && selfTestCurrentManager(checks) && selfTestStructuredLayout(checks) && selfTestRecommendedLocations(checks);
        DestroyWindow(g->panel);
        DestroyWindow(g->control);
        CloseHandle(g->singleton);
        CoUninitialize();
        return ok ? 0 : 1;
    }
    g->configWriter=std::make_unique<ConfigWriter>(g->configFile,g->control,WM_CONFIG_SAVED);
    g->namingWorker = std::make_unique<NamingWorker>(g->control, WM_NAMING_RESULT);
    g->targetWorker = std::make_unique<SaveTargetWorker>(g->control, WM_TARGET_RESULT);
    g->workEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g->worker = CreateThread(nullptr, 0, workerMain, nullptr, 0, nullptr);
    RegisterHotKey(g->control, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
    RegisterHotKey(g->control, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'J');
    for (auto event : {EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, EVENT_OBJECT_DESTROY, EVENT_OBJECT_NAMECHANGE}) {
        auto hook = SetWinEventHook(event, event, nullptr, winEvent, 0, 0,
                                    WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        if (hook)
            g->globalHooks.push_back(hook);
    }
    tray(true);
    if (manager)
        showManager();
    inspectForeground();
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (handleAppShortcut(message)) continue;
        if (g->flyout && IsDialogMessageW(g->flyout, &message))
            continue;
        if (g->manager && IsDialogMessageW(g->manager, &message))
            continue;
        if (IsWindowVisible(g->panel) && IsDialogMessageW(g->panel, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g->stopWorker = true;
    SetEvent(g->workEvent);
    if (WaitForSingleObject(g->worker, 500) == WAIT_OBJECT_0) {
        CloseHandle(g->worker);
        CloseHandle(g->workEvent);
    }
    CloseHandle(g->singleton);
    CoUninitialize();
    return 0;
}

