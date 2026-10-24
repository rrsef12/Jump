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
constexpr int ID_COMBO_UNDO = 700, ID_COMBO_REDO = 701, ID_PROJECTS = 702, ID_RECORDS = 703, ID_EXPLAIN = 704,
              ID_TRANSFER = 705, ID_DIAGNOSTICS = 706, ID_CAPTURE_RECIPE = 707, ID_REFRESH_NUMBER = 708,
              ID_GENERIC_SUBMIT = 709, ID_COPY_NAME = 710, ID_GENERIC_DIRECTORY = 712;
constexpr UINT T_INSPECT = 1, T_REFRESH = 2, T_ACTION = 3, T_SEARCH = 4, T_SAVE_VERIFY = 5, T_BIND = 6,
               T_GENERIC_REFRESH = 7;
constexpr UINT NAME_INPUT_DELAY = 150;
constexpr int ID_BIND_CONTROL = 188;
constexpr int ID_STRUCT_DELIVERY = 753, ID_STRUCT_FREE = 755, ID_STRUCT_EXAMPLE = 757, ID_STRUCT_INPUT = 760,
              ID_STRUCT_USE = 761, ID_STRUCT_CLEAR = 762, ID_STRUCT_CLOSE = 763, ID_STRUCT_LIBRARY = 764,
              ID_STRUCT_SUMMARY = 766, ID_STRUCT_SOURCE = 767, ID_STRUCT_INHERIT = 770, ID_STRUCT_TODAY = 771,
              ID_STRUCT_LIVE = 772, ID_STRUCT_CANCEL = 773, ID_STRUCT_TITLE = 774, ID_STRUCT_EMPTY = 775,
              ID_QUICK_CANCEL = 778, ID_STRUCT_RULE_CLEAR = 779, ID_STRUCT_FIELD = 780,
              ID_STRUCT_QUICK_VIEW = 788, ID_STRUCT_FIELDS_VIEW = 789, ID_STRUCT_SCHEMES = 790,
              ID_STRUCT_DESIGN = 791, ID_STRUCT_NAME = 792, ID_STRUCT_NUMBER_NONE = 793,
              ID_STRUCT_NUMBER_VERSION = 794, ID_STRUCT_NUMBER_SEQUENCE = 795, ID_STRUCT_CHOICE = 800,
              ID_QUICK_INPUT = 810, ID_QUICK_CHOICE = 820, ID_QUICK_EDIT = 830, ID_QUICK_PICK = 840,
              ID_QUICK_DONE = 888;
constexpr int ID_NUMBER_MENU = 889, ID_AFFIX_SUMMARY = 890, ID_REPLACE_SUMMARY = 891, ID_CAPABILITIES = 892,
              ID_RULES_SUMMARY = 894, ID_FINAL_NAME_LABEL = 895, ID_FREE_NAME_INPUT = 896;
constexpr int ID_FIELD_ERROR = 900, ID_PREFIX_ERROR = 908, ID_SUFFIX_ERROR = 909, ID_FREE_NAME_ERROR = 910,
              ID_RULE_ERROR = 911;
constexpr int ID_ROUTE_SUGGEST = 912, ID_TARGET_INFO = 913;
std::wstring savedStructure();
constexpr int ID_VARIABLE_TEMPLATE = 240, ID_VARIABLE_VALUES = 241, ID_VARIABLE_HINT = 242,
              ID_VARIABLE_SAVE = 243, ID_VARIABLE_CANCEL = 244, ID_VARIABLE_EDIT = 245,
              ID_VARIABLE_ADVANCED = 246, ID_VARIABLE_FIELD_FIRST = 260, ID_VARIABLE_TOKEN_FIRST = 600;
constexpr int PANEL_GAP = 3;
constexpr int PANEL_WIDTH = 320, PANEL_MARGIN = 12, FOLDER_ROW_STEP = 34, FOLDER_BUTTON_HEIGHT = 28;
constexpr int ID_PANEL_OPTIONS = 189, ID_PANEL_SETTINGS = 190;
constexpr int LOCATION_PAGE_SIZE = 9, ID_LOCATION_FIRST = 160, ID_LOCATION_PAGE = 153,
              ID_LOCATION_EMPTY = 154;
constexpr int ID_FOLDER_SCROLL = 169, ID_RULE_PATTERN = 172, ID_RULE_REPLACEMENT = 173, ID_RULE_MODE = 174,
              ID_RULE_SOURCE = 175, ID_RULE_HINT = 176, ID_RULE_ENABLE = 177, ID_RULE_CLEAR = 178,
              ID_ADD_PREFIX = 179, ID_ADD_SUFFIX = 180, ID_RULE_RESULT = 185, ID_AFFIX_PREFIX = 186,
              ID_AFFIX_SUFFIX = 187;
constexpr int ID_DIRECTORY_EDIT = 155, ID_DIRECTORY_INPUT = 156, ID_DIRECTORY_SEARCH = 158;
constexpr int ID_PREVIEW = 101, ID_MANAGER = 105, ID_SUBMIT = 106, ID_UNDO = 107, ID_CLOSE = 108,
              ID_PLANS = 110, ID_NAME_MANAGER = 112;
constexpr int ID_DIRECTORY = 113, ID_STATUS = 114;
constexpr int ID_RENAME = 116, ID_LOC_RECOMMENDED = 120, ID_LOC_FAVORITES = 121, ID_LOC_HISTORY = 122,
              ID_FAVORITE_CURRENT = 124, ID_LOC_RECENT = 127, ID_CLEAR_RECENT = 128, ID_CLEAR_HISTORY = 129;
constexpr int ID_PRESET_VALUE = 137, ID_PRESET_SAVE = 138, ID_PRESET_CANCEL = 139, ID_PRESET_HINT = 140;
constexpr int ID_NAMING_MENU = 142;
constexpr int ID_FOLDER_SEARCH = 145, ID_FOLDER_LIST = 146, ID_FLYOUT_MANAGE = 149, ID_FOLDER_SELECT = 150,
              ID_FOLDER_STAR = 151, ID_FOCUS_NAME = 152;
constexpr size_t NEW_PRESET = static_cast<size_t>(-1);
constexpr int MANAGER_AFFIXES = 4, MANAGER_TEMPLATES = 5, MANAGER_PROJECTS = 6, MANAGER_BACKUP = 7,
              MANAGER_CONNECTIONS = 8;
constexpr int MANAGER_PAGES = 9, MANAGER_CONTENT_LEFT = 148, MANAGER_SHELL_WIDTH = 792;
constexpr UINT WM_SETTINGS_NAVIGATE = WM_APP + 13;
const wchar_t *managerTitles[]{L"命名方案",   L"收藏位置", L"保存流程",   L"常规",    L"前后缀",
                               L"变量表达式", L"项目",     L"备份与迁移", L"软件接入"};
std::vector<SettingsDialog *> settingsDialogs;
int pendingSettingsPage = -1;
bool pendingSettingsClose = false;
void openSettingsPage(int page);
void refreshSettingsPage();
bool requestSettingsNavigation(int page);
bool managerDraftDirty();
bool discardManagerDraft();
bool handleAppShortcut(const MSG &message);
State *g;
#include "dialog_detect.h"
#include "panel_graphics.h"
std::wstring previewName() {
    return g->stemName + (g->dialog.extension.empty() ? L"" : g->dialog.extension);
}
void syncNameParts() {
    auto prefix = g->parts.prefix, suffix = g->parts.suffix, body = g->stemName;
    if (!prefix.empty() && body.starts_with(prefix))
        body.erase(0, prefix.size());
    else
        prefix.clear();
    if (!suffix.empty() && body.ends_with(suffix))
        body.resize(body.size() - suffix.size());
    else
        suffix.clear();
    g->parts = splitName(body, g->cfg.modifiers);
    g->parts.prefix = prefix;
    g->parts.suffix = suffix;
}
void invalidateNaming() {
    g->saveAfterFill.reset();
    ++g->pasteId;
    g->pastePending = false;
    if (g->namingWorker)
        g->namingId = g->namingWorker->cancel();
    g->namingPending = g->namingSubmit = false;
    g->readyNaming.reset();
    g->smartCacheKey.clear();
}
std::wstring effectiveVariables() {
    const auto *project = activeProject(g->cfg);
    return mergedVariables(g->sessionExpression.empty() ? g->cfg.variableValues : g->sessionVariables,
                           project ? project->variables : L"");
}
SmartNaming namingOptions() {
    SmartNaming options{g->numberMode, g->folderName,
                        g->variablesEnabled
                            ? (g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression)
                            : L"",
                        g->variablesEnabled ? effectiveVariables() : L""};
    options.dateFormat = g->sessionDateFormat < 0 ? g->cfg.dateFormat : g->sessionDateFormat;
    options.digits = g->sessionDigits ? g->sessionDigits : g->cfg.numberDigits;
    options.upperVersion = g->sessionDigits ? g->sessionUpper : g->cfg.upperVersion;
    return options;
}
std::wstring composedName(const std::wstring *targetDirectory = nullptr) {
    if (g->structure.enabled && !g->structure.free) {
        g->structure.dateFormat = namingOptions().dateFormat;
        auto composed = composeStructure(g->structure, g->cfg);
        if (!composed.error.empty()) {
            g->ruleError = composed.error;
            return g->stemName;
        }
        g->parts.base = composed.name;
        // A structured field owns stage/date; legacy modifiers cannot add them again.
        g->parts.stage.clear();
        g->parts.date.clear();
    }
    auto source = compose(g->parts);
    g->rulePreview = renamePreview(source, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex);
    g->ruleError = g->ruleEnabled ? g->rulePreview.error : L"";
    auto name = g->ruleEnabled && g->ruleError.empty() ? g->rulePreview.name : source;
    if (!g->ruleError.empty() || (!g->numberMode && !g->folderName && !g->variablesEnabled))
        return name;
    auto options = namingOptions();
    const auto &directory = targetDirectory ? *targetDirectory : g->dialog.directory;
    auto key = name + L"\n" + directory + L"\n" + g->dialog.extension + L"\n" +
               std::to_wstring(options.counter) + (options.folder ? L"+" : L"-") + L"\n" +
               options.expression + L"\n" + options.variables + L"\n" + today(options.dateFormat) +
               std::to_wstring(options.digits) + (options.upperVersion ? L"V" : L"v");
    if (key != g->smartCacheKey) {
        g->smartCacheKey = key;
        if (options.counter && g->namingWorker) {
            auto lightweight = options;
            lightweight.counter = 0;
            auto preview = smartNamingPreview(name, directory, g->dialog.extension, lightweight);
            if (!preview.error.empty()) {
                g->smartPreview = preview;
                return g->ruleError = preview.error, name;
            }
            g->namingRequest = {0,         g->generation.load(), key,         name,
                                directory, g->dialog.extension,  g->stemName, options,
                                false};
            g->namingId = g->namingWorker->request(g->namingRequest);
            g->namingPending = true;
            g->ruleError.clear();
            g->status = L"正在计算编号，可继续编辑…";
            return g->stemName;
        }
        g->smartPreview = smartNamingPreview(name, directory, g->dialog.extension, options);
    }
    if (g->namingPending)
        return g->stemName;
    g->ruleError = g->smartPreview.error;
    return g->ruleError.empty() ? g->smartPreview.name : name;
}
void resetSmartNaming() {
    invalidateNaming();
    g->numberMode = 0;
    g->folderName = g->variablesEnabled = false;
    g->sessionExpression.clear();
    g->sessionVariables.clear();
    g->sessionDigits = 0;
    g->sessionDateFormat = -1;
}
std::wstring variableKey(std::wstring key) {
    if (key == L"名称")
        return L"name";
    if (key == L"文件夹")
        return L"folder";
    if (key == L"日期")
        return L"date";
    return key;
}
std::wstring variableCaption(const std::wstring &key) {
    return key == L"name" ? L"原文件名" : key == L"folder" ? L"文件夹" : key == L"date" ? L"日期" : key;
}
bool builtInVariable(const std::wstring &key) {
    return key == L"name" || key == L"folder" || key == L"date";
}
std::vector<std::wstring> expressionKeys(const std::wstring &expression) {
    std::vector<std::wstring> keys;
    for (size_t at = 0; (at = expression.find(L'{', at)) != expression.npos;) {
        auto end = expression.find(L'}', at + 1);
        if (end == expression.npos)
            break;
        auto key = variableKey(expression.substr(at + 1, end - at - 1));
        if (!key.empty() && std::find(keys.begin(), keys.end(), key) == keys.end())
            keys.push_back(key);
        at = end + 1;
    }
    return keys;
}
std::map<std::wstring, std::wstring> variableValues(const std::wstring &text) {
    std::map<std::wstring, std::wstring> values;
    size_t at = 0;
    while (at < text.size()) {
        auto end = text.find(L'\n', at);
        if (end == text.npos)
            end = text.size();
        auto line = trim(text.substr(at, end - at));
        auto equal = line.find(L'=');
        if (equal != line.npos)
            values[trim(line.substr(0, equal))] = trim(line.substr(equal + 1));
        at = end + 1;
    }
    return values;
}
void setVariableDraftValue(const std::wstring &key, const std::wstring &value) {
    // Replace only this field, retaining other values and any advanced draft text.
    auto &text = g->variableValuesDraft;
    for (size_t at = 0; at < text.size();) {
        auto end = text.find(L'\n', at);
        if (end == text.npos)
            end = text.size();
        auto line = trim(text.substr(at, end - at));
        auto equal = line.find(L'=');
        if (equal != line.npos && trim(line.substr(0, equal)) == key) {
            bool cr = end > at && text[end - 1] == L'\r';
            text.replace(at, end - at, key + L"=" + value + (cr ? L"\r" : L""));
            return;
        }
        at = end + 1;
    }
    if (!text.empty() && text.back() != L'\n')
        text += L"\r\n";
    text += key + L"=" + value;
}
void toggleVariableToken(const std::wstring &key) {
    auto &expression = g->variableTemplateDraft;
    bool removed = false;
    for (size_t at = 0; (at = expression.find(L'{', at)) != expression.npos;) {
        auto end = expression.find(L'}', at + 1);
        if (end == expression.npos)
            break;
        if (variableKey(expression.substr(at + 1, end - at - 1)) != key) {
            at = end + 1;
            continue;
        }
        size_t first = at, last = end + 1;
        if (last < expression.size() && expression[last] == L'_')
            ++last;
        else if (first && expression[first - 1] == L'_')
            --first;
        expression.erase(first, last - first);
        at = first;
        removed = true;
    }
    if (!removed) {
        if (!expression.empty() && expression.back() != L'_')
            expression += L"_";
        expression += L"{" + key + L"}";
    }
}

std::wstring affixLabel(std::wstring value) {
    size_t at = 0;
    while ((at = value.find(L"{date}", at)) != value.npos) {
        value.replace(at, 6, L"日期");
        at += 2;
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
    if (!entry.colors)
        return 0;
    auto found = entry.colors->find(entry.key);
    return found == entry.colors->end() ? 0 : std::clamp(found->second, 0, PRESET_COLOR_COUNT);
}
void moveAffixColor(bool prefix, const std::wstring &before, const std::wstring &after = L"") {
    if (before == after)
        return;
    auto &colors = prefix ? g->cfg.prefixColors : g->cfg.suffixColors;
    auto found = colors.find(before);
    if (found == colors.end())
        return;
    int color = found->second;
    colors.erase(found);
    if (!after.empty())
        colors[after] = color;
}
LRESULT CALLBACK PillProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
void invalidateSubjectGroup(HWND control);
COLORREF mixColor(COLORREF base, COLORREF tint, int percent) {
    return RGB((GetRValue(base) * (100 - percent) + GetRValue(tint) * percent) / 100,
               (GetGValue(base) * (100 - percent) + GetGValue(tint) * percent) / 100,
               (GetBValue(base) * (100 - percent) + GetBValue(tint) * percent) / 100);
}
COLORREF controlFill(COLORREF base, bool selected, bool hover, bool pressed, bool primary, bool enabled) {
    if (!enabled)
        return base;
    auto fill = primary ? g->primary : selected ? g->selection : base;
    if (pressed)
        return mixColor(fill, RGB(0, 0, 0), 12);
    return hover ? mixColor(fill, RGB(255, 255, 255), selected || primary ? 5 : 7) : fill;
}
constexpr UINT_PTR HOVER_TIMER = 0x4a50;
int hoverAmount(HWND h, bool hover) {
    auto motion = g->hoverMotions.find(h);
    if (motion == g->hoverMotions.end() || !g->animations)
        return hover ? 100 : 0;
    const auto &m = motion->second;
    double t = std::min(1.0, (GetTickCount64() - m.started) / 140.0);
    return m.from + static_cast<int>((m.to - m.from) * t * (2 - t));
}
#include "panel_controls.h"
#include "popup_menu.h"
#include "naming_feedback.h"
#include "experience_ui.h"
void updatePanelContent();
void apply(const std::wstring &, const std::wstring &, bool, bool);
void checkpointNaming();
#include "save_assist_ui.h"
#include "panel_session.h"
void showManager(bool present);
void startBinding();
void showPresetManager(bool add, size_t index = 0, bool present = true);
LRESULT CALLBACK PanelProc(HWND, UINT, WPARAM, LPARAM);
#include "flyout_ui.h"
void showPanelMenu(int id) {
    RECT rect{};
    HWND control = GetDlgItem(g->panel, id);
    if (!control)
        control = GetDlgItem(g->panel, ID_STRUCT_SCHEMES);
    if (!control)
        control = g->panel;
    GetWindowRect(control, &rect);
    auto menu = buildPanelMenu(id);
    SetForegroundWindow(g->panel);
    g->openPanelMenu = id;
    InvalidateRect(control, nullptr, FALSE);
    UpdateWindow(control);
    int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_RIGHTBUTTON, rect.right,
                                 rect.bottom, g->panel);
    g->openPanelMenu = 0;
    if (IsWindow(control)) {
        InvalidateRect(control, nullptr, FALSE);
        UpdateWindow(control);
    }
    DestroyMenu(menu);
    if (command)
        SendMessageW(g->panel, WM_COMMAND, command, 0);
}

#include "workflow_ui.h"
#include "naming_defaults_ui.h"
#include "structured_ui.h"
#include "transient_edit.h"
#include "panel_appearance.h"

#include "panel_proc.h"
constexpr int M_TAB = 200, M_LIST = 202, M_NAME = 203, M_VALUE = 204, M_SAVE = 207, M_DELETE = 208,
              M_UP = 209, M_ADD = 210, M_FOLDER = 218, M_CLEAR_HISTORY = 219, M_AUTO = 220, M_QUICK = 221,
              M_LAYOUT = 222, M_OPTIONS_SAVE = 223, M_STARTUP = 224, M_CAPTURE = 228, M_AFFIX_PREVIEW = 229,
              M_AFFIX_STATUS = 230, M_AFFIX_CATEGORY = 252, M_COPY_SCHEME = 254, M_APPLY_SCHEME = 255,
              M_SCHEME_DEFAULT = 256, M_INFO = 280, M_PROJECT_USE = 281, M_PROJECT_NONE = 282,
              M_PROJECT_WORK = 283, M_PROJECT_DELIVERY = 284, M_EXPORT = 285, M_IMPORT = 286, M_BACKUP = 287,
              M_RESTORE = 288, M_COPY_DIAGNOSTICS = 289, M_BIND = 290, M_UNBIND = 291, M_FEEDBACK = 292;
constexpr int MANAGER_WIDTH = 520, MANAGER_HEIGHT = 400;
#include "manager_ui.h"
#include "ui_checks.h"
#include "save_notice.h"
void tray(bool add) {
    NOTIFYICONDATAW n{};
    n.cbSize = sizeof(n);
    n.hWnd = g->control;
    n.uID = 1;
    n.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    n.uCallbackMessage = WM_TRAY;
    n.hIcon = static_cast<HICON>(LoadImageW(g->instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                            LR_SHARED));
    wcscpy_s(n.szTip, g->paused ? L"Jump · 已暂停" : L"Jump · 后台保存助手");
    Shell_NotifyIconW(add ? NIM_ADD : NIM_MODIFY, &n);
    if (add) {
        n.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &n);
    }
}
void bindingNotice(const std::wstring &text) {
    NOTIFYICONDATAW n{};
    n.cbSize = sizeof(n);
    n.hWnd = g->control;
    n.uID = 1;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_INFO;
    wcscpy_s(n.szInfoTitle, L"Jump · 文件名关联");
    wcsncpy_s(n.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}
void cancelBinding() {
    KillTimer(g->control, T_BIND);
    UnregisterHotKey(g->control, 2);
    if (g->bindingHook)
        UnhookWindowsHookEx(g->bindingHook);
    g->bindingHook = nullptr;
    if (g->bindingHint)
        DestroyWindow(g->bindingHint);
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
    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    if (!target || !pid || pid == g->pid || !IsWindowVisible(target) || !IsWindowEnabled(target)) {
        bindingNotice(L"请在原软件的保存窗口中，选中文件名输入框后重试。");
        return;
    }
    detach();
    g->dialog = {};
    g->dialog.hwnd = target;
    g->dialog.pid = pid;
    g->suppressed = nullptr;
    Job job{JobType::BindControl, target, pid, ++g->generation};
    job.point = point;
    job.focusedBinding = focused;
    enqueue(std::move(job));
}
void startBinding() {
    cancelBinding();
    hidePanel();
    MONITORINFO monitor{sizeof(monitor)};
    POINT cursor{};
    GetCursorPos(&cursor);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
    bool escape = RegisterHotKey(g->control, 2, MOD_NOREPEAT, VK_ESCAPE) != 0;
    g->bindingHint =
        CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC",
                        escape ? L"点击原软件的文件名输入框以记住关联\nEsc 取消 · 20 秒内有效"
                               : L"点击原软件的文件名输入框以记住关联\n20 秒后自动退出",
                        WS_POPUP | WS_BORDER | SS_CENTER, monitor.rcWork.right - 400, monitor.rcWork.top + 24,
                        380, 62, nullptr, nullptr, g->instance, nullptr);
    SendMessageW(g->bindingHint, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
    ShowWindow(g->bindingHint, SW_SHOWNOACTIVATE);
    g->bindingDeadline = GetTickCount64() + 20000;
    // Observe the next click without consuming it. A polling-only picker can miss short clicks.
    g->bindingHook = SetWindowsHookExW(WH_MOUSE_LL, bindingMouseHook, g->instance, 0);
    if (!g->bindingHook) {
        cancelBinding();
        bindingNotice(L"请选中文件名输入框后按 Ctrl+Alt+N 建立关联。");
        return;
    }
    SetTimer(g->control, T_BIND, 25, nullptr);
}
void pollBinding() {
    if (GetTickCount64() >= g->bindingDeadline) {
        cancelBinding();
        inspectForeground();
    }
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
    if (command == ID_TRANSFER)
        showTransfer();
    else if (command == ID_DIAGNOSTICS)
        showDiagnostics();
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
    case WM_TARGET_RESULT: {
        if (!g->targetWorker)
            return 0;
        auto result = g->targetWorker->take();
        if (!result || !g->attached || result->request.generation != g->generation ||
            result->request.path != g->targetPath || result->request.path != currentSaveTarget())
            return 0;
        g->targetInfo = std::move(*result);
        updatePanelContent();
        return 0;
    }
    case WM_NAMING_RESULT: {
        if (!g->namingWorker)
            return 0;
        auto result = g->readyNaming ? std::move(g->readyNaming) : g->namingWorker->take();
        g->readyNaming.reset();
        if (!result || !g->attached || result->request.id != g->namingId ||
            result->request.generation != g->generation || result->request.key != g->smartCacheKey)
            return 0;
        if (g->busy) {
            g->readyNaming = std::move(result);
            return 0;
        }
        g->namingPending = g->namingSubmit = false;
        g->smartPreview = result->preview;
        g->ruleError = result->preview.error;
        if (!g->ruleError.empty()) {
            g->status = g->ruleError;
            g->statusError = true;
            updatePanelContent();
            return 0;
        }
        g->stemName = result->preview.name;
        g->dirtyName = g->localNamePending = true;
        g->statusError = false;
        g->status = L"编号已更新；通过 Jump 保存时将再次核验";
        updatePanelContent();
        bool submit = result->request.submit;
        if (submit && result->request.displayed != g->stemName) {
            auto text = L"编号核验结果发生变化：\n\n原预览：" + result->request.displayed +
                        result->request.extension + L"\n新名称：" + g->stemName + result->request.extension +
                        L"\n\n是否使用新名称继续保存？原软件的覆盖确认仍会保留。";
            submit = MessageBoxW(g->panel, text.c_str(), L"确认编号变化",
                                 MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
            if (!g->attached || result->request.generation != g->generation ||
                result->request.id != g->namingId)
                return 0;
        }
        g->numberingApproved = true;
        apply(result->request.directory, g->stemName, submit, true);
        g->numberingApproved = false;
        return 0;
    }
    case WM_BIND_CAPTURE:
        if (g->bindingHint) {
            auto target = GetAncestor(WindowFromPoint(g->bindingPoint), GA_ROOT);
            DWORD pid = 0;
            GetWindowThreadProcessId(target, &pid);
            if (pid && pid != g->pid)
                bindTarget(target, g->bindingPoint, false);
        }
        return 0;
    case WM_HOTKEY:
        if (wp == 1)
            bindTarget(GetForegroundWindow(), {}, true);
        else if (wp == 2) {
            cancelBinding();
            inspectForeground();
        } else if (wp == 3) {
            cancelBinding();
            inspectForeground(true);
        }
        return 0;
    case WM_BLENDER_BROWSER:
    case WM_HOUDINI_BROWSER: {
        HWND target = reinterpret_cast<HWND>(wp);
        DWORD pid = 0;
        GetWindowThreadProcessId(target, &pid);
        if (pid != static_cast<DWORD>(lp) ||
            !(msg == WM_HOUDINI_BROWSER ? houdiniBrowserWindow(target) : blenderBrowserWindow(target)))
            return 0;
        if (g->attached && g->dialog.hwnd == target) {
            if (!g->busy)
                SetTimer(hwnd, T_REFRESH, 50, nullptr);
        } else if (GetForegroundWindow() == target)
            SetTimer(hwnd, T_INSPECT, 30, nullptr);
        return 0;
    }
    case WM_SAVE_BRIDGE: {
        HWND target = reinterpret_cast<HWND>(wp);
        DWORD pid = 0;
        GetWindowThreadProcessId(target, &pid);
        if (!pid || pid == g->pid || pid != static_cast<DWORD>(lp) ||
            (g->testTargetPid && pid != g->testTargetPid) ||
            (!g->testTargetImage.empty() && processImage(target) != g->testTargetImage))
            return 0;
        if (reinterpret_cast<ULONG_PTR>(GetPropW(target, L"Jump.SaveBridge.v1")) != pid) {
            if (g->attached && g->dialog.bridge && g->dialog.hwnd == target)
                detach();
            return 0;
        }
        g->inspectAttempts = 0;
        if (g->attached && g->dialog.hwnd == target) {
            if (!g->busy)
                SetTimer(hwnd, T_REFRESH, 50, nullptr);
        } else if (GetForegroundWindow() == target)
            SetTimer(hwnd, T_INSPECT, 30, nullptr);
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
        if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_OBJECT_SHOW)
            g->inspectAttempts = 0;
        if (event == EVENT_SYSTEM_FOREGROUND) {
            if (g->attached && !attachedDialogAlive())
                detach();
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        } else if (g->attached && target == g->dialog.hwnd &&
                   (event == EVENT_OBJECT_DESTROY || event == EVENT_OBJECT_HIDE)) {
            detach();
        } else if (event == EVENT_OBJECT_SHOW && (windowClass(target) == L"#32770" ||
                                                  windowClass(target).starts_with(L"GHOST_WindowClass") ||
                                                  houdiniBrowserWindow(target) || genericCandidate(target)))
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        else if (g->attached && event == EVENT_OBJECT_LOCATIONCHANGE && target == g->dialog.hwnd)
            reposition();
        else if (g->attached && !g->busy && GetAncestor(target, GA_ROOT) == g->dialog.hwnd &&
                 (event == EVENT_OBJECT_VALUECHANGE || event == EVENT_OBJECT_NAMECHANGE))
            SetTimer(hwnd, T_REFRESH, 120, nullptr);
        else if (event == EVENT_OBJECT_NAMECHANGE && target == GetForegroundWindow() &&
                 looksLikeFileDialog(target)) {
            g->inspectAttempts = 0;
            SetTimer(hwnd, T_INSPECT, 85, nullptr);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == T_TARGET_CHECK) {
            KillTimer(hwnd, T_TARGET_CHECK);
            auto path = currentSaveTarget();
            if (g->targetWorker && !path.empty() && path == g->targetPath)
                g->targetWorker->request({0, g->generation.load(), path});
            return 0;
        }
        if (wp == T_BIND) {
            pollBinding();
            return 0;
        }
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
                if (it->verifyAt <= now) {
                    enqueue(*it);
                    it = g->saveChecks.erase(it);
                } else
                    ++it;
            }
            armSaveChecks();
        }
        return 0;
    case WM_RESULT: {
        std::unique_ptr<Result> result(reinterpret_cast<Result *>(lp));
        if (result->job.type == JobType::ResolvePaste) {
            finishPastedTarget(*result);
            return 0;
        }
        // Some custom exporters keep their window open after saving. Start the
        // file watch on accepted invocation as well as on dialog closure.
        if (result->job.generic && result->job.type == JobType::Apply && result->submitted &&
            result->error.empty())
            verifySavedFile(result->probe);
        if (result->job.type == JobType::VerifySave) {
            if (result->saved) {
                std::erase_if(g->saveChecks, [&](const Job &pending) {
                    return pending.probe.file == result->job.probe.file;
                });
                armSaveChecks();
                if (!result->verifiedFile.empty()) {
                    SYSTEMTIME now{};
                    GetLocalTime(&now);
                    wchar_t stamp[40]{};
                    swprintf_s(stamp, L"%04d-%02d-%02d %02d:%02d:%02d", now.wYear, now.wMonth, now.wDay,
                               now.wHour, now.wMinute, now.wSecond);
                    SavedFile record{result->verifiedFile, result->job.probe.process, stamp,
                                     result->job.probe.namingStructure, result->job.probe.namingProject};
                    rememberSavedFile(g->cfg, record);
                    rememberSubject(g->cfg, record.projectId, recentSavedSubject(record));
                    g->settingsDirty = true;
                    showVerifiedSaveNotice(*result);
                }
                rememberSavedDirectory(result->job.probe.directory);
            } else if (result->job.verifyAttempt < 4) {
                // Brief, event-scoped retries allow the host to write after closing its dialog,
                // without occupying the UIA worker or polling while the app is idle.
                constexpr UINT delays[] = {50, 100, 250, 400};
                auto retry = result->job;
                retry.verifyAt = GetTickCount64() + delays[retry.verifyAttempt++];
                std::erase_if(g->saveChecks,
                              [&](const Job &pending) { return pending.probe.file == retry.probe.file; });
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
            detach();
            return 0;
        }
        if ((result->job.type == JobType::Inspect || result->job.type == JobType::BindControl) &&
            !sameTarget(result->job))
            return 0;
        if (!result->error.empty()) {
            if (result->job.type == JobType::BindControl) {
                bindingNotice(result->error);
                return 0;
            }
            if (result->job.requestedInspect) {
                bindingNotice(L"当前窗口没有唯一可读写的文件名框。请先打开原软件的保存窗口；若已打开，可点选"
                              L"文件名框后按 Ctrl+Alt+N 建立直接关联。");
                return 0;
            }
            g->statusError = true;
            if (result->job.type == JobType::Refresh && result->job.generic) {
                detach();
                return 0;
            }
            if (result->job.type == JobType::Inspect && g->inspectAttempts < 4 &&
                GetForegroundWindow() == result->job.hwnd)
                SetTimer(hwnd, T_INSPECT, 180 * std::max(1, g->inspectAttempts), nullptr);
            g->busy = false;
            g->busySubmitting = false;
            g->saveAfterFill.reset();
            if (g->readyNaming)
                PostMessageW(g->control, WM_NAMING_RESULT, 0, 0);
            if (g->pathPending) {
                g->pathPending = false;
                g->editingPath = true;
            }
            g->pendingQuick = false;
            g->status = result->error;
            if (g->attached) {
                for (auto h : g->children)
                    EnableWindow(h, TRUE);
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
                return filename_controls::scope(previous, binding.executable, binding.windowClass,
                                                binding.windowTitle);
            });
            if (g->cfg.filenameBindings.size() >= 100)
                g->cfg.filenameBindings.erase(g->cfg.filenameBindings.begin());
            g->cfg.filenameBindings.push_back(binding);
            persist();
            result->job.type = JobType::Inspect;
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
            g->structure = {};
            g->freeRecovery.reset();
            g->structuredField = -1;
            g->structuredDraft.clear();
            g->ruleEnabled = false;
            g->ruleError.clear();
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
            g->status = g->dialog.generic ? L"单击选项填入，在原窗口确认保存"
                        : g->dialog.save  ? L"单击填入，Enter 或原窗口保存"
                                          : L"单击位置，跳转到文件夹";
            g->dpi = static_cast<int>(GetDpiForWindow(g->dialog.hwnd));
            fonts();
            paintTheme();
            targetHooks();
            rebuildPanel();
            if (g->structuredResumeNumber) {
                g->structuredResumeNumber = false;
                updateInlineRule();
            }
            if (g->dialog.generic)
                SetTimer(hwnd, T_GENERIC_REFRESH, 700, nullptr);
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
            bool nativeEdit = GetForegroundWindow() == g->dialog.hwnd &&
                              observedStem != stripExtension(g->dialog.name, g->dialog.extension);
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
                if (!g->saveProbe.file.empty() &&
                    lower(stripExtension(std::filesystem::path(g->saveProbe.file).filename().wstring(),
                                         g->dialog.extension)) == lower(g->stemName)) {
                    g->saveProbe.namingStructure = savedStructure();
                    g->saveProbe.namingProject = g->cfg.activeProject;
                    g->saveProbe.namingKnown = true;
                }
                if (changed)
                    g->dirtyName = g->stemName != g->originalStem;
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
            if (needsLocations)
                rebuildPanel();
            else if (g->stemName != previousStem || g->dialog.directory != previousDirectory ||
                     g->dialog.extension != previousExtension || g->dialog.format != previousFormat ||
                     g->dirtyName != previousDirty || g->settingsDirty != previousSettingsDirty)
                updatePanelContent();
        } else {
            g->busy = false;
            g->busySubmitting = false;
            if (g->readyNaming)
                PostMessageW(g->control, WM_NAMING_RESULT, 0, 0);
            g->pathPending = false;
            if (result->navigated) {
                g->editingPath = false;
                g->pathDraft.clear();
            }
            g->saveProbe = preserveSaveBaseline(g->saveProbe, result->probe);
            bool pendingEdit = g->stemName != result->job.name;
            if (!pendingEdit)
                g->localNamePending = false;
            g->dialog = result->data;
            if (!pendingEdit)
                g->stemName = stripExtension(g->dialog.name, g->dialog.extension);
            if (result->navigated || result->submitted)
                rememberCurrentDirectory();
            g->status = result->submitted ? L"已提交给原软件，等待保存结果"
                        : result->navigated && g->dialog.generic
                            ? (g->dialog.genericDestination ? L"已更新输出位置和名称，请在原窗口确认保存"
                               : g->dialog.genericAddress   ? L"已切换目录，文件名已保留"
                                                            : L"已切换目录，名称与格式已核验")
                        : g->dialog.generic ? L"已填入，请在原窗口确认保存"
                                            : L"已同步到原窗口，可继续编辑";
            if (!result->submitted && g->applyFocus && IsWindow(g->applyFocus) &&
                GetAncestor(g->applyFocus, GA_ROOT) == g->panel && GetForegroundWindow() == g->dialog.hwnd) {
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
                if (g->saveAfterFill) {
                    auto save = *g->saveAfterFill;
                    g->saveAfterFill.reset();
                    if (sameTarget(save) && save.name == g->stemName) {
                        apply(save.directory, save.name, true, true);
                        return 0;
                    }
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
        if (g->configWriter && wp == g->configWriter->revision()) {
            g->settingsDirty = !lp;
            g->configPending = false;
            if (!lp) {
                g->status = L"自动记录未能保存，请检查配置目录权限；下次保存时会重试。";
                g->statusError = true;
            }
            if (g->attached)
                updatePanelContent();
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
        if (!flushPersistence())
            MessageBoxW(hwnd, L"最新自动记录未能保存，请检查配置目录权限。已有配置仍保留。", L"Jump",
                        MB_OK | MB_ICONWARNING);
        g->namingWorker.reset();
        g->targetWorker.reset();
        KillTimer(hwnd, T_TARGET_CHECK);
        if (g->savedNotice)
            DestroyWindow(g->savedNotice);
        cancelBinding();
        UnregisterHotKey(hwnd, 1);
        UnregisterHotKey(hwnd, 3);
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
        auto reply =
            blender_bridge::request(reinterpret_cast<HWND>(_wcstoui64(args[3], nullptr, 10)),
                                    static_cast<DWORD>(wcstoul(args[2], nullptr, 10)), nullptr, L"INSPECT");
        auto report = reply.ok ? L"OK\n" + reply.name + L"\n" + reply.directory + L"\n" + reply.extension
                               : L"ERROR\n" + reply.error;
        HANDLE file =
            CreateFileW(args[4], GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        DWORD written = 0;
        if (file != INVALID_HANDLE_VALUE) {
            WriteFile(file, report.data(), static_cast<DWORD>(report.size() * sizeof(wchar_t)), &written,
                      nullptr);
            CloseHandle(file);
        }
        LocalFree(args);
        CoUninitialize();
        return reply.ok ? 0 : 1;
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
    if (!assistChecks.empty()) {
        bool ok = saveAssistChecks(assistChecks);
        CoUninitialize();
        return ok ? 0 : 1;
    }
    if (!checks.empty() && !(selfTest(checks) && workflowModelChecks(checks) && selfTestShellAddresses() &&
                             filename_controls::selfTest())) {
        CoUninitialize();
        return 1;
    }
    g = new State;
    runSettingsDialog = hostSettingsDialog;
    sizeSettingsDialog = layoutSettingsDialog;
    g->solidMaterial = !checks.empty() || (testUi && testSolidMaterial);
    g->testFillDelayMs = testUi && testHostOnly ? testFillDelayMs : 0;
    g->testTargetPid = testUi ? testTargetPid : 0;
    if (testHostOnly) {
        wchar_t executable[32768]{};
        GetModuleFileNameW(nullptr, executable, 32768);
        g->testTargetImage =
            lower((std::filesystem::path(executable).parent_path() / L"Jump.TestHost.exe").wstring());
    }
    g->instance = instance;
    g->configFile =
        checks.empty() ? configPath(config) : std::filesystem::absolute(checks / L"inline-presets.ini");
    if (!managerChecks.empty())
        g->configFile = managerChecks / L"settings.ini";
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
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                               LR_SHARED));
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
    g->panel =
        CreateWindowExW(WS_EX_TOPMOST | (testUi ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW) | WS_EX_NOACTIVATE,
                        L"Jump.Panel", L"Jump 保存助手", WS_POPUP | WS_CLIPCHILDREN, 0, 0, PANEL_WIDTH, 382,
                        nullptr, nullptr, instance, nullptr);
    g->taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    fonts();
    paintTheme();
    if (!managerChecks.empty()) {
        bool ok = checkManagerShow(managerChecks, managerEntry);
        DestroyWindow(g->panel);
        DestroyWindow(g->control);
        CloseHandle(g->singleton);
        CoUninitialize();
        return ok ? 0 : 1;
    }
    if (!checks.empty()) {
        DeleteFileW((checks / L"failure.txt").c_str());
        bool ok = selfTestPanelLayer() && saveReferencePreview(checks) && selfTestPaintStability() &&
                  selfTestManagerPosition() && selfTestCurrentManager(checks) &&
                  selfTestStructuredLayout(checks) && selfTestRecommendedLocations(checks);
        DestroyWindow(g->panel);
        DestroyWindow(g->control);
        CloseHandle(g->singleton);
        CoUninitialize();
        return ok ? 0 : 1;
    }
    g->configWriter = std::make_unique<ConfigWriter>(g->configFile, g->control, WM_CONFIG_SAVED);
    g->namingWorker = std::make_unique<NamingWorker>(g->control, WM_NAMING_RESULT);
    g->targetWorker = std::make_unique<SaveTargetWorker>(g->control, WM_TARGET_RESULT);
    g->workEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g->worker = CreateThread(nullptr, 0, workerMain, nullptr, 0, nullptr);
    RegisterHotKey(g->control, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'N');
    RegisterHotKey(g->control, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'J');
    for (auto event : {EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_SHOW, EVENT_OBJECT_HIDE, EVENT_OBJECT_DESTROY,
                       EVENT_OBJECT_NAMECHANGE}) {
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
        if (handleAppShortcut(message))
            continue;
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
