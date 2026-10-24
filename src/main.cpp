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
    GetClientRect(h, &layout.client);
    GetScrollInfo(h, SB_CTL, &layout.info);
    auto &info = layout.info;
    int extent = std::max(1, info.nMax - info.nMin + 1);
    layout.maximum = std::max(info.nMin, info.nMax - static_cast<int>(info.nPage ? info.nPage - 1 : 0));
    int height = std::max(0L, layout.client.bottom - px(4));
    int thumbHeight = std::min(height, std::max(px(20), MulDiv(height, info.nPage, extent)));
    layout.travel = height - thumbHeight;
    int top = px(2) + (layout.maximum > info.nMin
                           ? MulDiv(layout.travel, info.nPos - info.nMin, layout.maximum - info.nMin)
                           : 0);
    layout.thumb = {0, top, layout.client.right, top + thumbHeight};
    return layout;
}
LRESULT CALLBACK LocationScrollProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    auto &state = *reinterpret_cast<LocationScrollState *>(data);
    auto notify = [&](int code) {
        SendMessageW(GetParent(h), WM_VSCROLL, MAKEWPARAM(code, state.trackPosition),
                     reinterpret_cast<LPARAM>(h));
    };
    auto stop = [&]() {
        state.dragging = false;
        state.repeatDirection = -1;
        KillTimer(h, 1);
        InvalidateRect(h, nullptr, FALSE);
    };
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(h, msg, wp);
        auto dc = paint.dc();
        auto layout = locationScrollGeometry(h);
        FillRect(dc, &layout.client, g->brush);
        int center = layout.client.right / 2;
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        auto trackBrush = CreateSolidBrush(g->line);
        auto oldBrush = SelectObject(dc, trackBrush);
        paintRoundedRect(dc, center - px(1), px(2), center + px(1), layout.client.bottom - px(2), px(2),
                         px(2));
        bool active = state.hover || state.dragging || GetFocus() == h;
        int width = px(active ? 8 : 6);
        auto color =
            state.dragging || GetFocus() == h ? g->accent : mixColor(g->bg, g->muted, active ? 80 : 55);
        auto thumbBrush = CreateSolidBrush(IsWindowEnabled(h) ? color : g->line);
        SelectObject(dc, thumbBrush);
        paintRoundedRect(dc, center - width / 2, layout.thumb.top, center - width / 2 + width,
                         layout.thumb.bottom, width, width);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(trackBrush);
        DeleteObject(thumbBrush);
        return 0;
    }
    case SBM_SETSCROLLINFO: {
        // Keep native range/accessibility storage, but never invoke its light-theme painting.
        SCROLLINFO before{sizeof(before), SIF_RANGE | SIF_PAGE | SIF_POS}, after = before;
        GetScrollInfo(h, SB_CTL, &before);
        auto result = DefSubclassProc(h, msg, FALSE, lp);
        GetScrollInfo(h, SB_CTL, &after);
        if (before.nMin != after.nMin || before.nMax != after.nMax || before.nPage != after.nPage ||
            before.nPos != after.nPos)
            InvalidateRect(h, nullptr, FALSE);
        return result;
    }
    case SBM_GETSCROLLINFO: {
        auto result = DefSubclassProc(h, msg, wp, lp);
        auto info = reinterpret_cast<SCROLLINFO *>(lp);
        if (result && state.dragging && (info->fMask & SIF_TRACKPOS))
            info->nTrackPos = state.trackPosition;
        return result;
    }
    case WM_MOUSEMOVE: {
        if (!state.hover) {
            state.hover = true;
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
            TrackMouseEvent(&tracking);
            InvalidateRect(h, nullptr, FALSE);
        }
        if (state.dragging && GetCapture() == h) {
            auto layout = locationScrollGeometry(h);
            int offset = std::clamp(GET_Y_LPARAM(lp) - state.grabOffset - px(2), 0, layout.travel);
            int position =
                layout.info.nMin +
                (layout.travel ? MulDiv(offset, layout.maximum - layout.info.nMin, layout.travel) : 0);
            if (position != state.trackPosition) {
                state.trackPosition = position;
                notify(SB_THUMBTRACK);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        state.hover = false;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        if (!IsWindowEnabled(h))
            return 0;
        auto layout = locationScrollGeometry(h);
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        state.trackPosition = layout.info.nPos;
        SetCapture(h);
        if (PtInRect(&layout.thumb, point)) {
            state.dragging = true;
            state.grabOffset = point.y - layout.thumb.top;
        } else {
            state.repeatDirection = point.y < layout.thumb.top ? SB_PAGEUP : SB_PAGEDOWN;
            SetTimer(h, 1, 400, nullptr);
            notify(state.repeatDirection);
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1 && state.repeatDirection != -1 && GetCapture() == h) {
            SetTimer(h, 1, 70, nullptr);
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(h, &point);
            auto layout = locationScrollGeometry(h);
            if (PtInRect(&layout.client, point) &&
                ((state.repeatDirection == SB_PAGEUP && point.y < layout.thumb.top) ||
                 (state.repeatDirection == SB_PAGEDOWN && point.y >= layout.thumb.bottom)))
                notify(state.repeatDirection);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state.dragging)
            notify(SB_THUMBPOSITION);
        stop();
        if (GetCapture() == h)
            ReleaseCapture();
        return 0;
    case WM_CANCELMODE:
        stop();
        if (GetCapture() == h)
            ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        stop();
        return 0;
    case WM_ENABLE:
        if (!wp) {
            stop();
            if (GetCapture() == h)
                ReleaseCapture();
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        return SendMessageW(GetParent(h), msg, wp, lp);
    case WM_KEYDOWN:
        if (!IsWindowEnabled(h))
            return 0;
        switch (wp) {
        case VK_UP:
            notify(SB_LINEUP);
            return 0;
        case VK_DOWN:
            notify(SB_LINEDOWN);
            return 0;
        case VK_PRIOR:
            notify(SB_PAGEUP);
            return 0;
        case VK_NEXT:
            notify(SB_PAGEDOWN);
            return 0;
        case VK_HOME:
            notify(SB_TOP);
            return 0;
        case VK_END:
            notify(SB_BOTTOM);
            return 0;
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
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE || msg == WM_ENABLE)
        feedback &= ~DWORD_PTR(1);
    if (before != feedback)
        SetWindowSubclass(h, InputFeedbackProc, 1, feedback);
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (before != feedback || msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE) {
        RECT frame{};
        GetWindowRect(h, &frame);
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
    if (h && GetPropW(h, L"Jump.InputError"))
        return RGB(191, 106, 110);
    if (h && h != parent && GetWindowSubclass(h, InputFeedbackProc, 1, &feedback) && IsWindowEnabled(h)) {
        if (GetFocus() == h)
            return g->accent;
        if (feedback & 1)
            return mixColor(g->line, g->muted, 35);
    }
    return g->line;
}
void paintInputFrame(HDC dc, HWND parent, const RECT &frame, int radius) {
    auto pen = CreatePen(PS_SOLID, 1, inputBorder(parent, frame));
    auto op = SelectObject(dc, pen), ob = SelectObject(dc, g->inputBrush);
    paintRoundedRect(dc, frame.left, frame.top, frame.right, frame.bottom, radius, radius);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
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
void buildNamingHint(int id, int x, int y, int width);
void updateInlineControls();
HWND createChild(HWND parent, const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y,
                 int width, int height, HFONT font) {
    auto control = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, px(x), px(y),
                                   px(width), px(height), parent,
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
    if (staticText)
        style = (style & ~SS_TYPEMASK) | SS_OWNERDRAW;
    if (edit)
        g->fields.push_back({px(x), px(y), px(x + w), px(y + h)});
    HWND control = createChild(g->panel, cls, text, style, id, x + (edit ? 10 : 0), y + (edit ? 7 : 0),
                               w - (edit ? 20 : 0), h - (edit ? 12 : 0), g->bodyFont);
    g->children.push_back(control);
    SetWindowSubclass(control, TransientEditProc, 4, 0);
    if (cls == std::wstring(L"BUTTON"))
        SetWindowSubclass(control, PillProc, 1, 0);
    if (edit)
        SetWindowSubclass(control, InputFeedbackProc, 1, 0);
    if (staticText)
        SetWindowSubclass(control, MaterialControlProc, 2, textStyle);
    if (edit || cls == std::wstring(MSFTEDIT_CLASS))
        configureMaterialEditor(control);
    if (id == ID_FOLDER_SCROLL) {
        auto state = new LocationScrollState;
        if (!SetWindowSubclass(control, LocationScrollProc, 1, reinterpret_cast<DWORD_PTR>(state)))
            delete state;
    }
    return control;
}
void label(int x, int y, int w, const std::wstring &text, bool heading = false) {
    g->labels.push_back({{px(x), px(y), px(x + w), px(y + 22)}, text, heading});
}
void tip(HWND control, const std::wstring &text) {
    bool exists = g->tipStrings.contains(control);
    if (exists && g->tipStrings.at(control) == text)
        return;
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
    int controlId = GetDlgCtrlID(h);
    // BS_NOTIFY turns the second fast press into BN_DOUBLECLICKED. Candidate
    // chips toggle per completed click, so keep native press/release handling
    // for that press too; location buttons retain their double-click Save.
    if (msg == WM_LBUTTONDBLCLK && GetParent(h) == g->panel &&
        ((controlId >= ID_QUICK_PICK && controlId < ID_QUICK_PICK + 48) ||
         (controlId >= ID_STRUCT_CHOICE && controlId < ID_STRUCT_CHOICE + 6)))
        msg = WM_LBUTTONDOWN;
    if (msg == WM_ERASEBKGND)
        return 1; // Owner drawing paints the complete button atomically.
    DWORD_PTR originalFeedback = feedback;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000)))
        feedback |= 2;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS)
        feedback &= ~DWORD_PTR(2);
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        feedback |= 1;
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE)
        feedback &= ~DWORD_PTR(1);
    if (feedback != originalFeedback) {
        if (((feedback ^ originalFeedback) & 1) && GetParent(h) == g->panel && g->animations &&
            IsWindowVisible(h)) {
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
        if (motion == g->hoverMotions.end() || !g->animations ||
            GetTickCount64() - motion->second.started >= 140) {
            KillTimer(h, HOVER_TIMER);
            g->hoverMotions.erase(h);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        KillTimer(h, HOVER_TIMER);
        g->hoverMotions.erase(h);
    }
    if (msg == WM_MOUSEWHEEL && GetParent(h) != g->manager)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN && GetParent(h) == g->panel) {
        int id = GetDlgCtrlID(h);
        if (g->editingPath && id != ID_DIRECTORY && id != ID_DIRECTORY_EDIT)
            cancelPathEdit(false);
        // These commands open an editing window. Activate on mouse-down, before
        // the button's focus/capture handling and the later window hand-off.
        if (id == ID_MANAGER || id == ID_NAME_MANAGER || id == ID_RENAME || id == ID_DIRECTORY ||
            id == ID_DIRECTORY_EDIT)
            SetForegroundWindow(g->panel);
    }
    if (msg == WM_CONTEXTMENU) {
        int id = GetDlgCtrlID(h);
        if (GetParent(h) == g->panel && ((id >= ID_QUICK_PICK && id < ID_QUICK_PICK + 48) ||
                                         (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6)))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
        auto action = g->actions.find(id);
        if (GetParent(h) == g->panel && action != g->actions.end() &&
            (action->second.kind == Action::Prefix || action->second.kind == Action::Suffix ||
             action->second.kind == Action::Location || action->second.kind == Action::Variables))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    if (feedback != originalFeedback || msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        invalidateSubjectGroup(h);
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK VariableEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL)
        return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) {
        SetForegroundWindow(GetAncestor(h, GA_ROOT));
        SetFocus(h);
    }
    bool submit = GetDlgCtrlID(h) != ID_VARIABLE_VALUES;
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_ESCAPE || (submit && key->wParam == VK_RETURN)))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || (submit && wp == VK_RETURN))) {
        SendMessageW(GetParent(h), WM_COMMAND, wp == VK_ESCAPE ? ID_VARIABLE_CANCEL : ID_VARIABLE_SAVE, 0);
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_ESCAPE || (submit && wp == VK_RETURN)))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
bool pasteTarget(HWND editor);
LRESULT CALLBACK PathEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_PASTE && pasteTarget(h))
        return 0;
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_KILLFOCUS)
        PostMessageW(g->panel, WM_PATH_BLUR, 0, 0);
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_RETURN)
            commitPathEdit();
        else
            cancelPathEdit();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK PreviewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_IME_STARTCOMPOSITION)
        SetPropW(h, L"Jump.IME", reinterpret_cast<HANDLE>(1));
    if (msg == WM_IME_ENDCOMPOSITION)
        RemovePropW(h, L"Jump.IME");
    if (GetPropW(h, L"Jump.IME") && (msg == WM_KEYDOWN || msg == WM_CHAR))
        return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_PASTE && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT && pasteTarget(h))
        return 0;
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
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
        if (g->editingPath)
            cancelPathEdit(false);
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
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) {
        cancelPathEdit(false);
        closeFlyout();
        SetForegroundWindow(g->panel);
        SetFocus(h);
    }
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_ESCAPE) {
            if (!structuredInputError().empty()) {
                g->building = true;
                setTextIfChanged(GetDlgItem(g->panel, ID_AFFIX_PREFIX), g->parts.prefix);
                setTextIfChanged(GetDlgItem(g->panel, ID_AFFIX_SUFFIX), g->parts.suffix);
                g->building = false;
                g->statusError = false;
            }
            g->inlineTool = 0;
            rebuildPanel();
        } else if (GetDlgCtrlID(h) == ID_AFFIX_PREFIX || GetDlgCtrlID(h) == ID_AFFIX_SUFFIX)
            SendMessageW(g->panel, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), EN_CHANGE),
                         reinterpret_cast<LPARAM>(h));
        else
            updateInlineRule();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK DropdownProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    auto setFeedback = [&](DWORD_PTR value) {
        if (feedback == value)
            return;
        feedback = value;
        SetWindowSubclass(h, DropdownProc, 1, value);
        InvalidateRect(h, nullptr, FALSE);
    };
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
        setFeedback(feedback | 1);
    }
    if (msg == WM_MOUSELEAVE)
        setFeedback(feedback & ~DWORD_PTR(1));
    if (msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000))
        setFeedback(feedback | 2);
    if (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000))
        setFeedback(feedback | 2);
    if (msg == WM_KILLFOCUS)
        setFeedback(feedback & ~DWORD_PTR(2));
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp);
        HDC dc = paint.dc();
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, g->brush);
        const bool expanded = (feedback & 4) != 0;
        const bool hover = (feedback & 1) != 0;
        const bool keyboardFocus = (feedback & 2) && GetFocus() == h;
        COLORREF fill =
            controlFill(expanded ? g->surface : g->field, false, hover, false, false, IsWindowEnabled(h));
        auto brush = CreateSolidBrush(fill);
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : fill);
        auto ob = SelectObject(dc, brush), op = SelectObject(dc, pen);
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(10), px(10));
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen);
        DeleteObject(brush);
        auto font = SelectObject(dc, g->bodyFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g->fg);
        r.left += px(6);
        r.right -= px(22);
        auto text = windowText(h);
        DrawTextW(dc, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        r.left = r.right;
        r.right += px(22);
        drawUiIcon(dc, expanded ? UiIcon::ChevronUp : UiIcon::Chevron, (r.left + r.right - px(14)) / 2.f,
                   (r.top + r.bottom - px(14)) / 2.f, static_cast<float>(px(14)),
                   expanded                 ? g->accent
                   : hover || keyboardFocus ? g->fg
                                            : g->muted);
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_LBUTTONDOWN) {
        setFeedback(feedback & ~DWORD_PTR(2));
        if (g->editingPath)
            cancelPathEdit(false);
        if (GetParent(h) == g->panel)
            closeFlyout();
        SetForegroundWindow(GetAncestor(h, GA_ROOT));
        SetFocus(h);
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
LRESULT drawDropdownItem(DRAWITEMSTRUCT item) {
    PaintBuffer buffer(item.hDC, item.rcItem);
    item.hDC = buffer.dc;
    auto brush = CreateSolidBrush(g->surface);
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    if (item.itemID == static_cast<UINT>(-1))
        return TRUE;
    auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
    if (length == CB_ERR)
        return TRUE;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(length));
    DWORD_PTR feedback = 0;
    GetWindowSubclass(item.hwndItem, DropdownProc, 1, &feedback);
    auto currentIndex = (feedback & 4) && (feedback >> 8) ? static_cast<LRESULT>((feedback >> 8) - 1)
                                                          : SendMessageW(item.hwndItem, CB_GETCURSEL, 0, 0);
    const bool current = currentIndex == item.itemID;
    bool hovered = false;
    COMBOBOXINFO combo{sizeof(combo)};
    if (GetComboBoxInfo(item.hwndItem, &combo) && IsWindowVisible(combo.hwndList)) {
        POINT mouse{};
        GetCursorPos(&mouse);
        ScreenToClient(combo.hwndList, &mouse);
        hovered = PtInRect(&item.rcItem, mouse) != FALSE;
    }
    if ((item.itemState & ODS_SELECTED) && (!current || hovered)) {
        RECT hover = item.rcItem;
        InflateRect(&hover, -px(4), -px(2));
        auto fill = CreateSolidBrush(RGB(45, 56, 72));
        auto pen = CreatePen(PS_SOLID, 1, RGB(45, 56, 72));
        auto ob = SelectObject(item.hDC, fill), op = SelectObject(item.hDC, pen);
        paintRoundedRect(item.hDC, hover.left, hover.top, hover.right, hover.bottom, px(8), px(8));
        SelectObject(item.hDC, op);
        SelectObject(item.hDC, ob);
        DeleteObject(pen);
        DeleteObject(fill);
    }

    auto old = SelectObject(item.hDC, g->font);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, item.itemState & ODS_DISABLED ? g->muted : g->fg);
    RECT textRect = item.rcItem;
    textRect.left += px(12);
    textRect.right -= px(30);
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
        RECT r{};
        GetClientRect(h, &r);
        auto pen = CreatePen(PS_SOLID, 1, g->line);
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(12), px(12));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
    };
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT) {
        ClientPaint paint(h, msg, wp);
        RECT r{};
        GetClientRect(h, &r);
        auto fill = CreateSolidBrush(g->surface);
        FillRect(paint.dc(), &r, fill);
        DeleteObject(fill);
        DefSubclassProc(h, WM_PRINT, reinterpret_cast<WPARAM>(paint.dc()), PRF_CLIENT | PRF_ERASEBKGND);
        border(paint.dc());
        return 0;
    }
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        int previous = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(h, L"Jump.PopupHotRow"))) - 1;
        auto hit = msg == WM_MOUSEMOVE ? SendMessageW(h, LB_ITEMFROMPOINT, 0, lp) : -1;
        int next = hit != -1 && !HIWORD(hit) ? LOWORD(hit) : -1;
        if (next != previous) {
            if (next < 0)
                RemovePropW(h, L"Jump.PopupHotRow");
            else
                SetPropW(h, L"Jump.PopupHotRow", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(next + 1)));
            for (int row : {previous, next}) {
                RECT r{};
                if (row >= 0 && SendMessageW(h, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&r)) != LB_ERR)
                    InvalidateRect(h, &r, FALSE);
            }
        }
        if (msg == WM_MOUSEMOVE && previous < 0) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
            TrackMouseEvent(&tracking);
        }
    }
    if (msg == WM_NCDESTROY)
        RemovePropW(h, L"Jump.PopupHotRow");
    if (msg == WM_WINDOWPOSCHANGED) {
        RECT r{};
        GetWindowRect(h, &r);
        int width = r.right - r.left, height = r.bottom - r.top;
        DWORD_PTR size = static_cast<DWORD_PTR>(MAKELONG(width, height));
        if (size != dimensions && width > 0 && height > 0) {
            SetWindowSubclass(h, DropdownListProc, 1, size);
            // Window owns the region after a successful SetWindowRgn call.
            auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, px(12), px(12));
            if (!SetWindowRgn(h, region, TRUE))
                DeleteObject(region);
        }
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_PRINT || msg == WM_PRINTCLIENT)
        border(reinterpret_cast<HDC>(wp));
    return result;
}
void styleDropdownList(HWND combo) {
    COMBOBOXINFO info{sizeof(info)};
    if (!GetComboBoxInfo(combo, &info) || !info.hwndList)
        return;
    DWORD_PTR previous = 0;
    if (GetWindowSubclass(info.hwndList, DropdownListProc, 1, &previous))
        return;
    SetWindowSubclass(info.hwndList, DropdownListProc, 1, 0);
    SetWindowTheme(info.hwndList, L"", L"");
    SetWindowLongPtrW(info.hwndList, GWL_STYLE, GetWindowLongPtrW(info.hwndList, GWL_STYLE) & ~WS_BORDER);
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE,
                      GetWindowLongPtrW(info.hwndList, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    SetWindowPos(info.hwndList, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
LRESULT CALLBACK DropdownParentProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_DROPDOWN_COMMIT) {
        auto control = reinterpret_cast<HWND>(lp);
        DWORD_PTR feedback = 0;
        if (!IsWindow(control) || GetParent(control) != h ||
            !GetWindowSubclass(control, DropdownProc, 1, &feedback) || !(feedback & 32))
            return 0;
        auto original = static_cast<LRESULT>(feedback >> 8) - 1;
        SetWindowSubclass(control, DropdownProc, 1, feedback & 3);
        InvalidateRect(control, nullptr, FALSE);
        if (!(feedback & 8) || (feedback & 16)) {
            if (original >= 0)
                SendMessageW(control, CB_SETCURSEL, original, 0);
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
                if (GetKeyState(VK_LBUTTON) & 0x8000)
                    feedback &= ~DWORD_PTR(2);
            } else if (code == CBN_SELCHANGE && (feedback & (4 | 32)))
                return 0;
            else if (code == CBN_SELENDOK && (feedback & (4 | 32)))
                feedback = (feedback | 8) & ~DWORD_PTR(16);
            else if (code == CBN_SELENDCANCEL && (feedback & (4 | 32)))
                feedback = (feedback | 16) & ~DWORD_PTR(8);
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
        if (item.CtlType == ODT_COMBOBOX)
            return drawDropdownItem(item);
    }
    if (msg == WM_CTLCOLORLISTBOX && windowClass(reinterpret_cast<HWND>(lp)) == L"ComboLBox") {
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->surface);
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
    const auto &source = g->locationTab == 4   ? g->cfg.recentSaves
                         : g->locationTab == 1 ? g->cfg.favorites
                         : g->locationTab == 2 ? g->cfg.history
                                               : g->paths;
    std::vector<Location> locations;
    std::set<std::wstring> seen;
    if (g->locationTab == 0 && g->dialog.save)
        for (const auto &location : namingRecommendedLocations(g->structure, g->cfg))
            if (seen.insert(normalize(location.path)).second)
                locations.push_back(location);
    for (const auto &location : source)
        if (!location.path.empty() && seen.insert(normalize(location.path)).second)
            locations.push_back(location);
    return locations;
}
int locationColumnCount(size_t count) {
    return count == 4 ? 2 : static_cast<int>(std::clamp<size_t>(count, 1, 3));
}
int locationRowCount(size_t count) {
    if (g->locationTab == 2)
        return static_cast<int>(std::clamp<size_t>(count, 1, 3));
    int columns = locationColumnCount(count);
    return std::clamp((static_cast<int>(count) + columns - 1) / columns, 1, 3);
}
void resizeFolderRows(int rows) {
    if (rows == g->folderRows)
        return;
    int cutoff = px(g->folderTop + g->folderRows * FOLDER_ROW_STEP),
        delta = px((rows - g->folderRows) * FOLDER_ROW_STEP);
    for (auto h : g->children) {
        int id = GetDlgCtrlID(h);
        if ((id >= ID_LOCATION_FIRST && id < ID_LOCATION_FIRST + LOCATION_PAGE_SIZE) ||
            id == ID_FOLDER_SCROLL || id == ID_LOCATION_EMPTY || id == ID_LOCATION_PAGE)
            continue;
        RECT r{};
        GetWindowRect(h, &r);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
        if (r.top >= cutoff)
            SetWindowPos(h, nullptr, r.left, r.top + delta, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
    OffsetRect(&g->pathEditRect, 0, delta);
    g->height += (rows - g->folderRows) * FOLDER_ROW_STEP;
    g->folderRows = rows;
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    reposition(false);
    InvalidateRect(g->panel, nullptr, TRUE);
}
void updateLocationShortcuts() {
    if (g->dialog.generic && !g->dialog.genericNavigate)
        return;
    auto locations = locationChoices();
    bool history = g->locationTab == 2;
    int columns = locationColumnCount(locations.size());
    int totalRows = (static_cast<int>(locations.size()) + columns - 1) / columns;
    int rows = locationRowCount(locations.size());
    resizeFolderRows(rows);
    g->locationPage = history ? 0 : std::clamp(g->locationPage, 0, std::max(0, totalRows - rows));
    size_t first = static_cast<size_t>(g->locationPage * columns);
    size_t count = history ? 2 : rows * columns;
    std::vector<Location> items(locations.begin() + first,
                                locations.begin() + std::min(first + count, locations.size()));
    bool changed =
        items.size() != g->displayPaths.size() ||
        !std::equal(items.begin(), items.end(), g->displayPaths.begin(), [](const auto &a, const auto &b) {
            return a.name == b.name && a.path == b.path && a.sources == b.sources;
        });
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
        if (!button)
            continue;
        bool present = static_cast<size_t>(i) < g->displayPaths.size();
        if (present) {
            const auto &location = g->displayPaths[i];
            auto name = location.name.empty() ? location.path : location.name;
            if (windowText(button) != name)
                setTextIfChanged(button, name.c_str());
            int left = history ? PANEL_MARGIN : PANEL_MARGIN + (available + 8) * (i % columns) / columns;
            int right = history ? PANEL_MARGIN + available
                                : PANEL_MARGIN + (available + 8) * (i % columns + 1) / columns - 8;
            positionChild(button, px(left), px(g->folderTop + (history ? i : i / columns) * FOLDER_ROW_STEP),
                          px(right - left), px(FOLDER_BUTTON_HEIGHT));
            g->actions[id] = {Action::Location, static_cast<size_t>(i)};
            tip(button, location.path +
                            (location.sources.starts_with(L"命名推荐") ? L"\n" + location.sources : L"") +
                            (g->locationTab == 0 && location.sources.find(L"已打开") != std::wstring::npos
                                 ? L"\n已打开 · 资源管理器窗口中的文件夹"
                                 : L"") +
                            L"\n单击切换目录；右键设置颜色；Ctrl + 单击直接保存");
        } else
            g->actions.erase(id);
        showIfChanged(button, present ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
    positionChild(scroll, px(g->width - PANEL_MARGIN - 16), px(g->folderTop), px(16),
                  px(rows * FOLDER_ROW_STEP - 8));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0;
    info.nMax = std::max(0, totalRows - 1);
    info.nPage = rows;
    info.nPos = g->locationPage;
    SetScrollInfo(scroll, SB_CTL, &info, TRUE);
    enableIfChanged(scroll, !g->busy);
    showIfChanged(scroll, !history && totalRows > 3 ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto more = GetDlgItem(g->panel, ID_LOCATION_PAGE);
    positionChild(more, px(PANEL_MARGIN), px(g->folderTop + 2 * FOLDER_ROW_STEP), px(available),
                  px(FOLDER_BUTTON_HEIGHT));
    setTextIfChanged(more, L"查看全部历史（" + std::to_wstring(locations.size()) + L"）…");
    showIfChanged(more, history && locations.size() > 2 ? SW_SHOWNOACTIVATE : SW_HIDE);
    const wchar_t *empty = g->locationTab == 1   ? L"暂无收藏 · 点击路径旁的 ☆ 添加"
                           : g->locationTab == 4 ? L"暂无最近保存记录"
                           : g->locationTab == 2 ? L"暂无浏览历史"
                                                 : L"暂无位置，可点击下方路径输入目录";
    setTextIfChanged(GetDlgItem(g->panel, ID_LOCATION_EMPTY), empty);
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
void apply(const std::wstring &, const std::wstring &, bool, bool);
void checkpointNaming();
#include "save_assist_ui.h"
void updatePanelContent() {
    g->building = true;
    if (g->dialog.generic) {
        auto directory = GetDlgItem(g->panel, ID_GENERIC_DIRECTORY);
        setTextIfChanged(directory, g->dialog.directory.empty() ? L"目录未确认 · 自动编号不可用"
                                                                : L"目录：" + g->dialog.directory);
        if (directory)
            tip(directory, g->dialog.directory.empty() ? L"请在原软件选择目录；Jump 不猜测保存位置"
                                                       : g->dialog.directory + L"\n切换目录请在原软件操作");
    }
    auto preview = GetDlgItem(g->panel, ID_PREVIEW);
    setTextIfChanged(preview, g->stemName + g->dialog.extension);
    if (preview)
        tip(preview, L"最终文件名：" + g->stemName + g->dialog.extension +
                         L"\n结果预览；修改名称请使用上方字段或“命名设置”。");
    setTextIfChanged(GetDlgItem(g->panel, ID_FREE_NAME_INPUT), g->stemName);
    setTextIfChanged(GetDlgItem(g->panel, ID_DIRECTORY),
                     g->dialog.directory.empty()
                         ? (g->dialog.genericNavigate ? L"选择快捷位置或输入目录" : L"使用原软件当前目录")
                         : g->dialog.directory.c_str());
    if (auto directory = GetDlgItem(g->panel, ID_DIRECTORY))
        tip(directory, g->dialog.directory.empty() ? L"使用原软件当前目录" : g->dialog.directory);
    showIfChanged(GetDlgItem(g->panel, ID_DIRECTORY), g->editingPath ? SW_HIDE : SW_SHOWNOACTIVATE);
    auto pathInput = GetDlgItem(g->panel, ID_DIRECTORY_INPUT);
    if (pathInput && windowText(pathInput) != g->pathDraft)
        setTextIfChanged(pathInput, g->pathDraft.c_str());
    showIfChanged(pathInput, g->editingPath ? SW_SHOWNOACTIVATE : SW_HIDE);
    setTextIfChanged(GetDlgItem(g->panel, ID_DIRECTORY_EDIT), L"编辑路径");
    if (auto edit = GetDlgItem(g->panel, ID_DIRECTORY_EDIT)) {
        tip(edit, g->editingPath ? L"Enter 切换目录；点击别处或 Esc 退出编辑" : L"输入或粘贴其他文件夹路径");
        enableIfChanged(edit, !g->busy);
    }
    enableIfChanged(GetDlgItem(g->panel, ID_DIRECTORY_SEARCH), !g->busy && !g->pastePending);
    InvalidateRect(g->panel, &g->pathEditRect, FALSE);
    bool saveError = g->settingsDirty && !g->configPending;
    auto status = g->status + (saveError ? L" · 配置未保存" : L"");
    setTextIfChanged(GetDlgItem(g->panel, ID_STATUS), status.c_str());
    showIfChanged(GetDlgItem(g->panel, ID_STATUS),
                  g->statusError || saveError || g->namingPending || g->pastePending ? SW_SHOWNOACTIVATE
                                                                                     : SW_HIDE);
    if (auto control = GetDlgItem(g->panel, ID_STATUS))
        tip(control, saveError
                         ? g->status + L"\n预设和最近记录尚未写入配置；恢复权限后会在下一次操作重试。\n" +
                               g->configFile.wstring()
                         : g->status);
    bool favorite = std::any_of(g->cfg.favorites.begin(), g->cfg.favorites.end(), [](const auto &p) {
        return normalize(p.path) == normalize(g->dialog.directory);
    });
    setTextIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), favorite ? L"已收藏" : L"＋收藏");
    enableIfChanged(GetDlgItem(g->panel, ID_FAVORITE_CURRENT), !g->busy && !g->dialog.directory.empty());
    if (auto control = GetDlgItem(g->panel, ID_FAVORITE_CURRENT))
        tip(control, favorite ? L"取消收藏当前目录" : L"收藏当前目录");
    updateLocationShortcuts();
    updateInlineControls();
    updateStructuredContent();
    updateExperienceContent();
    updateSaveAssist();
    updatePanelStatusLayout();
    enableIfChanged(GetDlgItem(g->panel, ID_SUBMIT),
                    (!g->busy || (!g->busySubmitting && !g->dialog.generic)) && !g->pastePending &&
                        !g->structuredIme && structuredInputError().empty() && g->ruleError.empty() &&
                        validName(g->stemName));
    enableIfChanged(GetDlgItem(g->panel, ID_UNDO), !g->busy && g->dirtyName);
    if (auto submit = GetDlgItem(g->panel, ID_SUBMIT))
        tip(submit, g->status + (g->dialog.generic ? L"\n填入名称后，在原软件中确认保存"
                                                   : L"\n确认文件名与位置后，提交原软件保存"));
    setTextIfChanged(g->panel, (L"Jump 保存助手 · " + std::wstring(status)).c_str());
    g->building = false;
    for (auto h : g->children)
        if (windowClass(h) == L"Button")
            InvalidateRect(h, nullptr, FALSE);
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
    RECT current{};
    GetWindowRect(g->panel, &current);
    bool moved = current.left != x || current.top != y;
    bool resized = current.right - current.left != width || current.bottom - current.top != height;
    bool topmost = (GetWindowLongPtrW(g->panel, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (moved || resized || !topmost || (show && !IsWindowVisible(g->panel)))
        SetWindowPos(g->panel, HWND_TOPMOST, x, y, width, height,
                     SWP_NOACTIVATE | (moved ? 0 : SWP_NOMOVE) | (resized ? 0 : SWP_NOSIZE) |
                         (show ? SWP_SHOWWINDOW : 0));
    g->lastTargetRect = r;
}
void updateInlineControls() {
    if (g->inlineTool == 2) {
        for (int id : {ID_AFFIX_PREFIX, ID_AFFIX_SUFFIX}) {
            auto input = GetDlgItem(g->panel, id);
            const auto &value = id == ID_AFFIX_PREFIX ? g->parts.prefix : g->parts.suffix;
            auto draft = windowText(input);
            if (input && GetFocus() != input && draft != value &&
                (draft.empty() || validAffix(draft, id == ID_AFFIX_PREFIX)))
                SetWindowTextW(input, value.c_str());
        }
    }
    if (g->inlineTool != 3)
        return;
    auto sourceText = compose(g->parts);
    g->rulePreview = renamePreview(sourceText, L"", L"", g->rulePattern, g->ruleReplacement, g->ruleRegex);
    auto source = GetDlgItem(g->panel, ID_RULE_SOURCE);
    setHighlightedText(source, sourceText, g->rulePreview.matches);
    SetDlgItemTextW(g->panel, ID_RULE_ENABLE, g->ruleEnabled ? L"已启用" : L"启用");
    auto message = !g->rulePreview.error.empty()
                       ? g->rulePreview.error
                       : L"匹配 " + std::to_wstring(g->rulePreview.matches.size()) + L" 处 · " +
                             (g->ruleEnabled ? L"规则已启用，收起后保留" : L"规则未启用");
    SetDlgItemTextW(g->panel, ID_RULE_HINT, message.c_str());
    showIfChanged(GetDlgItem(g->panel, ID_RULE_HINT),
                  g->rulePreview.error.empty() ? SW_SHOWNOACTIVATE : SW_HIDE);
    SetDlgItemTextW(g->panel, ID_RULE_RESULT, (L"→ " + g->stemName + g->dialog.extension).c_str());
}
void buildInlineTools(int &y, int available) {
    g->dividers.push_back(px(y - 4));
    int cell = (available - 16) / 3;
    if (g->inlineTool == 2) {
        for (bool prefix : {true, false}) {
            label(16, y + 4, 36, prefix ? L"前缀" : L"后缀");
            auto input =
                child(L"EDIT", prefix ? g->parts.prefix : g->parts.suffix, ES_AUTOHSCROLL | WS_TABSTOP,
                      prefix ? ID_AFFIX_PREFIX : ID_AFFIX_SUFFIX, 52, y, available - 72, 32);
            SetWindowSubclass(input, InlineEditProc, 1, 0);
            SendMessageW(input, EM_SETLIMITTEXT, 80, 0);
            SendMessageW(input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"自由输入，留空停用"));
            auto add = child(L"BUTTON", L"＋", WS_TABSTOP, prefix ? ID_ADD_PREFIX : ID_ADD_SUFFIX,
                             g->width - 44, y, 28, 28);
            tip(add, L"搜索全部或添加预设");
            y += 36;
            buildNamingHint(prefix ? ID_PREFIX_ERROR : ID_SUFFIX_ERROR, 52, y, available - 36);
            int x = 16, row = y;
            const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
            for (size_t i = 0; i < values.size() && i < 3; ++i)
                pill(affixLabel(values[i]), prefix ? Action::Prefix : Action::Suffix, i, x, row, available,
                     L"单击启用，再点取消；右键编辑或换色", 16, cell);
            y += 40;
        }
    } else if (g->inlineTool == 3) {
        auto combo = child(L"COMBOBOX", L"",
                           CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP | WS_VSCROLL,
                           ID_RULE_MODE, 16, y, available - 124, 240);
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
        auto pattern = child(L"EDIT", g->rulePattern, ES_AUTOHSCROLL | WS_TABSTOP, ID_RULE_PATTERN, 16, y,
                             (available - 8) / 2, 32);
        auto replacement = child(L"EDIT", g->ruleReplacement, ES_AUTOHSCROLL | WS_TABSTOP,
                                 ID_RULE_REPLACEMENT, 24 + (available - 8) / 2, y, (available - 8) / 2, 32);
        SendMessageW(pattern, EM_SETLIMITTEXT, 160, 0);
        SendMessageW(replacement, EM_SETLIMITTEXT, 240, 0);
        SetWindowSubclass(pattern, InlineEditProc, 1, 0);
        SetWindowSubclass(replacement, InlineEditProc, 1, 0);
        SendMessageW(pattern, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"查找文字 / 正则表达式"));
        SendMessageW(replacement, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"替换为，留空即删除"));
        y += 40;
        buildNamingHint(ID_RULE_ERROR, 16, y, available);
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
        if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT + 8) || focusId == ID_STRUCT_INPUT ||
            focusId == ID_FREE_NAME_INPUT || focusId == ID_DIRECTORY_INPUT || focusId == ID_RULE_PATTERN ||
            focusId == ID_RULE_REPLACEMENT || focusId == ID_VARIABLE_TEMPLATE ||
            (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
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
        g->width =
            g->bottom ? std::clamp(MulDiv(target.right - target.left, 96, g->dpi), 648, 760) : PANEL_WIDTH;
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
        const std::pair<int, const wchar_t *> tabs[] = {{ID_LOC_RECOMMENDED, L"推荐"},
                                                        {ID_LOC_FAVORITES, L"收藏"},
                                                        {ID_LOC_RECENT, L"最近"},
                                                        {ID_LOC_HISTORY, L"历史"}};
        constexpr int tabCount = static_cast<int>(std::size(tabs));
        g->locationTabsRect = {px(m), px(y), px(m + avail), px(y + 28)};
        for (int i = 0; i < tabCount; ++i) {
            int left = m + 2 + (avail - 4) * i / tabCount;
            int right = m + 2 + (avail - 4) * (i + 1) / tabCount;
            auto tab = child(L"BUTTON", tabs[i].second, WS_TABSTOP, tabs[i].first, left + 1, y + 2,
                             right - left - 2, 24);
            if (tabs[i].first == ID_LOC_RECOMMENDED)
                tip(tab, L"推荐位置与资源管理器中已打开的文件夹；相同路径只显示一次");
            if (tabs[i].first == ID_LOC_RECENT)
                tip(tab, L"已确认文件保存成功的位置；仅浏览或取消保存不会计入");
            if (tabs[i].first == ID_LOC_HISTORY)
                tip(tab, L"在文件窗口浏览或跳转过的目录；未保存或取消保存也会记录");
        }
        y = 68;
        g->folderTop = y;
        size_t locationCount = locationChoices().size();
        int columns = locationColumnCount(locationCount);
        g->folderRows = locationRowCount(locationCount);
        for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
            int left = m + (avail + 8) * (i % columns) / columns,
                right = m + (avail + 8) * (i % columns + 1) / columns - 8;
            child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_FIRST + i, left, y + i / columns * FOLDER_ROW_STEP,
                  right - left, FOLDER_BUTTON_HEIGHT);
        }
        child(L"SCROLLBAR", L"", SBS_VERT | WS_TABSTOP, ID_FOLDER_SCROLL, w - m - 16, y, 16,
              g->folderRows * FOLDER_ROW_STEP - 8);
        auto historyMore =
            child(L"BUTTON", L"", WS_TABSTOP, ID_LOCATION_PAGE, m, y, avail, FOLDER_BUTTON_HEIGHT);
        tip(historyMore, L"按名称或完整路径搜索浏览历史；记录按最近访问排序");
        auto empty = child(L"STATIC", L"", SS_LEFT, ID_LOCATION_EMPTY, m, y, avail, FOLDER_BUTTON_HEIGHT);
        SendMessageW(empty, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
        y += g->folderRows * FOLDER_ROW_STEP + 2;
        auto directory = child(L"BUTTON", L"", WS_TABSTOP, ID_DIRECTORY, m, y, avail - 82, 24);
        tip(directory, L"当前路径；点击编辑（Ctrl + L）");
        g->pathEditRect = {px(m), px(y), px(m + avail - 82), px(y + 24)};
        auto input = CreateWindowExW(0, L"EDIT", g->pathDraft.c_str(), WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
                                     px(m + 4), px(y + 2), px(avail - 90), px(20), g->panel,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DIRECTORY_INPUT)),
                                     g->instance, nullptr);
        g->children.push_back(input);
        SetWindowSubclass(input, TransientEditProc, 4, 0);
        SetWindowSubclass(input, InputFeedbackProc, 1, 0);
        configureMaterialEditor(input);
        SendMessageW(input, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), TRUE);
        SendMessageW(input, EM_SETLIMITTEXT, 32767, 0);
        SendMessageW(input, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"输入或粘贴文件夹路径，Enter 切换"));
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
    if (g->dialog.save && (!g->dialog.generic || g->dialog.genericNavigate)) {
        auto route = child(L"BUTTON", L"", WS_TABSTOP, ID_ROUTE_SUGGEST, m, y, avail, 0);
        showIfChanged(route, SW_HIDE);
    }
    g->dividers.push_back(px(y));
    y += 8;
    if (g->dialog.save) {
        if (!g->structure.enabled) {
            g->structure = basicStructure(g->stemName, &g->cfg);
            g->structure.literal = true;
            g->structure.literalName = g->stemName;
        }
        buildStructuredControls(y, avail);
        buildNamingActions(y, avail);
        g->dividers.push_back(px(y + 2));
        y += 10;
        auto finalLabel = child(L"STATIC", L"最终文件名", SS_LEFT, ID_FINAL_NAME_LABEL, m, y, avail, 18);
        SendMessageW(finalLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
        y += 20;
        g->structuredFullHeight = structuredPreviewHeight(g->stemName + g->dialog.extension, avail);
        child(L"STATIC", g->stemName + g->dialog.extension, SS_LEFT, ID_PREVIEW, m, y, avail,
              g->structuredFullHeight);
        y += g->structuredFullHeight + 6;
        auto targetInfo = child(L"BUTTON", L"", WS_TABSTOP, ID_TARGET_INFO, m, y, avail, 0);
        showIfChanged(targetInfo, SW_HIDE);
        auto status = child(L"STATIC", g->status, SS_LEFT | SS_ENDELLIPSIS, ID_STATUS, m, y + 3, avail, 16);
        SendMessageW(status, WM_SETFONT, reinterpret_cast<WPARAM>(g->smallFont), TRUE);
    } else
        y += 8;
    auto capabilities = child(L"BUTTON", L"", WS_TABSTOP, ID_CAPABILITIES, m, y, avail, 24);
    tip(capabilities, L"查看当前窗口可以操作的内容、限制及下一步。");
    y += 28;
    g->height = y;
    g->building = false;
    updatePanelContent();
    SendMessageW(g->panel, WM_SETREDRAW, TRUE, 0);
    if (!wasVisible)
        ShowWindow(g->panel, SW_HIDE);
    // WM_SETREDRAW suppresses child painting during reconstruction too. Refresh
    // the complete subtree so old field highlights and candidate captions cannot remain.
    RedrawWindow(g->panel, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    reposition();
    if (focusId) {
        auto edit = GetDlgItem(g->panel, focusId);
        if (edit) {
            SetFocus(edit);
            if ((focusId >= ID_QUICK_INPUT && focusId < ID_QUICK_INPUT + 8) || focusId == ID_STRUCT_INPUT ||
                focusId == ID_FREE_NAME_INPUT || focusId == ID_PRESET_VALUE ||
                focusId == ID_VARIABLE_TEMPLATE ||
                (focusId >= ID_VARIABLE_FIELD_FIRST && focusId < ID_VARIABLE_FIELD_FIRST + 4))
                SendMessageW(edit, EM_SETSEL, selectionStart, selectionEnd);
        }
    }
}
bool persist() {
    g->settingsDirty =
        !(g->configWriter ? g->configWriter->saveNow(g->cfg) : jump::save(g->cfg, g->configFile));
    g->configPending = false;
    return !g->settingsDirty;
}
void persistDeferred() {
    if (!g->configWriter) {
        persist();
        return;
    }
    g->settingsDirty = true;
    g->configPending = true;
    g->configWriter->request(g->cfg);
}
bool flushPersistence() {
    if (g->settingsDirty)
        return persist();
    return !g->configWriter || g->configWriter->flush();
}
bool setBubbleColor(const ButtonAction &action, int color) {
    if (color < 0 || color > PRESET_COLOR_COUNT)
        return false;
    auto previous = g->cfg;
    {
        auto entry = bubbleColorEntry(action);
        if (!entry.colors)
            return false;
        if (color)
            (*entry.colors)[entry.key] = color;
        else
            entry.colors->erase(entry.key);
    }
    bool saved = persist();
    if (!saved)
        g->cfg = std::move(previous);
    g->status = !saved ? L"颜色未保存，请检查配置写入权限" : color ? L"已设置颜色标记" : L"已清除颜色标记";
    rebuildPanel();
    return saved;
}
void openInlinePreset(size_t index = NEW_PRESET, HWND anchor = nullptr) {
    closeFlyout();
    std::wstring value;
    if (index != NEW_PRESET) {
        const auto &values = g->namingMode == 1 ? g->cfg.prefixes : g->cfg.suffixes;
        if (index >= values.size())
            return;
        value = values[index];
    }
    g->presetEditor = true;
    g->presetIndex = index;
    g->presetDraft = value;
    KillTimer(g->control, T_ACTION);
    showPresetFlyout(anchor);
}
void saveInlinePreset() {
    if (!g->presetEditor)
        return;
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
        if (duplicate != values.end() &&
            (added || static_cast<size_t>(duplicate - values.begin()) != g->presetIndex)) {
            error(L"已有相同预设");
            return;
        }
        if (added) {
            if (values.size() >= 100) {
                error(L"最多 100 个预设");
                return;
            }
            values.insert(values.begin(), value);
        } else {
            if (g->presetIndex >= values.size()) {
                error(L"预设已变化，请重新选择");
                return;
            }
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

    g->status = added ? L"已新增预设，点选即可使用" : L"已修改预设，当前文件名保持不变";
    rebuildPanel();
    if (IsWindowVisible(g->panel))
        SetFocus(GetDlgItem(g->panel, g->namingMode == 1 ? ID_ADD_PREFIX : ID_ADD_SUFFIX));
}
// Exercise the actual panel commands with a hidden window and isolated settings.
// This does not create a host dialog, start the worker, or change foreground focus.
bool hiddenCheckFailure(const char *name, int line) {
    std::fprintf(stderr, "Hidden check failed: %s, main.cpp:%d\n", name, line);
    auto message = std::string(name) + ", main.cpp:" + std::to_string(line);
    auto file = CreateFileW((g->configFile.parent_path() / L"failure.txt").c_str(), GENERIC_WRITE,
                            FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written{};
        WriteFile(file, message.data(), static_cast<DWORD>(message.size()), &written, nullptr);
        CloseHandle(file);
    }
    return false;
}

void rememberCurrentDirectory() {
    const auto &directory = g->dialog.directory;
    if (directory.empty())
        return;
    // Polling the same directory is not a new visit, especially after removal.
    if (normalize(g->lastHistoryDirectory) == normalize(directory))
        return;
    g->lastHistoryDirectory = directory;
    if (!g->cfg.history.empty() && normalize(g->cfg.history.front().path) == normalize(directory)) {
        if (g->settingsDirty)
            persistDeferred();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberLocation(g->cfg, {name.empty() ? directory : name, directory, L"历史"});
    persistDeferred();
}
void verifySavedFile(const SaveProbe &probe) {
    if (probe.file.empty())
        return;
    Job job;
    job.type = JobType::VerifySave;
    job.probe = probe;
    enqueue(std::move(job));
}
void armSaveChecks() {
    KillTimer(g->control, T_SAVE_VERIFY);
    if (g->saveChecks.empty())
        return;
    auto earliest =
        std::min_element(g->saveChecks.begin(), g->saveChecks.end(), [](const Job &a, const Job &b) {
            return a.verifyAt < b.verifyAt;
        })->verifyAt;
    auto now = GetTickCount64();
    SetTimer(g->control, T_SAVE_VERIFY, static_cast<UINT>(earliest > now ? earliest - now : 1), nullptr);
}
void rememberSavedDirectory(const std::wstring &directory) {
    if (directory.empty())
        return;
    if (!g->cfg.recentSaves.empty() && normalize(g->cfg.recentSaves.front().path) == normalize(directory)) {
        if (g->settingsDirty)
            persistDeferred();
        if (g->attached)
            updatePanelContent();
        return;
    }
    auto name = std::filesystem::path(directory).filename().wstring();
    rememberSavedLocation(g->cfg, {name.empty() ? directory : name, directory, L"最近保存"});
    persistDeferred();
    g->paths = rankLocations(g->cfg, g->dialog.source, g->openedLocations);
    if (g->attached && !g->busy && GetForegroundWindow() != g->panel)
        rebuildPanel();
}
void apply(const std::wstring &path, const std::wstring &name, bool submit, bool explicitGeneric = false) {
    if (g->structuredIme || !structuredInputError().empty())
        return;
    if (g->pastePending)
        return;
    // Generic label clicks and Enter remain fill-only. Invoking Save requires its explicit menu command.
    if (g->dialog.generic && !explicitGeneric)
        submit = false;
    if (!g->attached)
        return;
    if (g->busy) {
        // Explicit Save during a name fill is serialized behind that fill.
        // Further edits invalidate this intent; background fills never create it.
        if (submit && !g->busySubmitting && g->ruleError.empty() && validName(name))
            g->saveAfterFill =
                Job{JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path, name, true};
        return;
    }
    if (g->numberMode && g->namingWorker && !g->numberingApproved) {
        if (submit) {
            auto displayed = g->stemName;
            invalidateNaming();
            composedName(&path);
            if (!g->ruleError.empty()) {
                g->status = g->ruleError;
                g->statusError = true;
                updatePanelContent();
                return;
            }
            g->namingRequest.submit = true;
            g->namingRequest.displayed = displayed;
            g->namingId = g->namingWorker->request(g->namingRequest);
            g->namingPending = g->namingSubmit = true;
            g->status = L"保存前重新核验编号…修改命名会取消本次提交";
            updatePanelContent();
            return;
        }
        if (g->namingPending) {
            updatePanelContent();
            return;
        }
    }
    if (!g->ruleError.empty()) {
        g->status = g->ruleError;
        updatePanelContent();
        return;
    }
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
    g->busySubmitting = submit;
    g->localNamePending = true;
    g->applyFocus = GetForegroundWindow() == g->panel ? GetFocus() : nullptr;
    g->status = submit ? L"正在核验并提交保存…" : L"正在填入…";
    updatePanelContent();
    Job job{JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path, name, submit};
    job.namingStructure = savedStructure();
    job.namingProject = g->cfg.activeProject;
    // Transfer focus from the foreground UI thread, before the UIA worker needs
    // to press Enter in a custom address editor.
    if (g->dialog.genericAddress && !path.empty() && normalize(path) != normalize(g->dialog.directory) &&
        focusAllowsAction(job))
        SetForegroundWindow(job.hwnd);
    enqueue(job);
    for (auto h : g->children)
        if ((windowClass(h) == L"Button" && GetDlgCtrlID(h) != ID_CLOSE &&
             !liveStructuredControl(GetDlgCtrlID(h)) &&
             !((GetDlgCtrlID(h) >= ID_LOC_RECOMMENDED && GetDlgCtrlID(h) <= ID_LOC_HISTORY) ||
               GetDlgCtrlID(h) == ID_LOC_RECENT) &&
             !(GetDlgCtrlID(h) == ID_SUBMIT && !submit && !g->dialog.generic) &&
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
    if ((!drive && !network) || value.find_first_of(L"\r\n\t\"<>|*?") != value.npos)
        return {};
    return value;
}
void beginPathEdit() {
    if (g->busy)
        return;
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
        SetFocus(input);
        SendMessageW(input, EM_SETSEL, 0, -1);
    }
}
void cancelPathEdit(bool restoreFocus) {
    if (!g->editingPath)
        return;
    if (g->pastePending)
        invalidateNaming();
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
    if (!g->editingPath || g->busy)
        return;
    g->pathDraft = windowText(GetDlgItem(g->panel, ID_DIRECTORY_INPUT));
    auto path = pathEditValue(g->pathDraft);
    if (path.empty()) {
        g->status = L"请输入完整文件夹路径，例如 C:\\Users\\用户名\\Downloads";
        updatePanelContent();
        return;
    }
    g->editingPath = false;
    if (normalize(path) == normalize(g->dialog.directory)) {
        g->status = L"已在该目录";
        updatePanelContent();
        if (g->attached && g->dirtyName && g->localNamePending)
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
        return;
    }
    if (g->numberMode || g->folderName || g->variablesEnabled) {
        g->stemName = composedName(&path);
        if (!g->ruleError.empty()) {
            g->status = g->ruleError;
            g->statusError = true;
            updatePanelContent();
            return;
        }
    }
    apply(path, g->stemName, false);
    g->pathPending = g->busy;
    g->applyFocus = nullptr;
    // A counter scan continues this navigation asynchronously. Reopening the
    // editor here races its queued blur and can replace the result with "cancelled".
    if (!g->busy && !g->namingPending) {
        g->editingPath = true;
        updatePanelContent();
    }
}
#include "paste_target_ui.h"
NamingSnapshot captureNaming() {
    auto options = namingOptions();
    auto parts = g->parts;
    // Derived output is recomputed on restore and must not create phantom undo steps.
    if (g->structure.enabled && !g->structure.free) {
        parts.base.clear();
        parts.stage.clear();
        parts.date.clear();
    }
    return {parts,
            options.expression,
            options.variables,
            g->rulePattern,
            g->ruleReplacement,
            g->numberMode,
            options.digits,
            options.dateFormat,
            g->ruleIndex,
            g->folderName,
            g->variablesEnabled,
            options.upperVersion,
            g->ruleEnabled,
            g->ruleRegex,
            g->structure,
            g->freeRecovery ? std::make_shared<NamingSnapshot>(*g->freeRecovery) : nullptr};
}
void checkpointNaming() {
    g->namingHistory.checkpoint(captureNaming());
    g->structuredCheckpoint = false;
}
void moveNamingHistory(bool redo) {
    if (!g->attached || g->busy || composingNaming())
        return;
    auto value = g->namingHistory.move(captureNaming(), redo);
    if (!value)
        return;
    restoreNamingState(*value);
    g->stemName = composedName();
    g->dirtyName = g->localNamePending = true;
    g->statusError = !g->ruleError.empty();
    g->status = g->statusError ? g->ruleError : redo ? L"已重做命名组合" : L"已撤销命名组合";
    rebuildPanel();
    if (g->ruleError.empty())
        apply(g->dialog.directory, g->stemName, false);
}
Recipe captureRecipe(const std::wstring &name) {
    Recipe recipe{name, g->dialog.directory, g->parts.base, false, !g->parts.date.empty()};
    recipe.complete = true;
    if (g->structure.enabled && !g->structure.free) {
        auto structure = g->structure;
        structure.counter = g->numberMode;
        structure.digits = namingOptions().digits;
        structure.dateFormat = namingOptions().dateFormat;
        structure.upper = namingOptions().upperVersion;
        recipe.structure = serializeStructure(structure);
    }
    recipe.prefix = g->parts.prefix;
    recipe.suffix = g->parts.suffix;
    recipe.stageValue = g->parts.stage;
    for (const auto &preset : g->cfg.prefixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.prefix) {
            recipe.prefix = preset;
            break;
        }
    for (const auto &preset : g->cfg.suffixes)
        if (expandAffix(preset, namingOptions().dateFormat) == recipe.suffix) {
            recipe.suffix = preset;
            break;
        }
    auto options = namingOptions();
    recipe.counter = options.counter;
    recipe.folder = options.folder;
    recipe.variables = g->variablesEnabled;
    recipe.expression = options.expression;
    // Project values stay with their project. A recipe stores only the session/default values.
    recipe.variableValues = g->sessionExpression.empty() ? g->cfg.variableValues : g->sessionVariables;
    recipe.digits = options.digits;
    recipe.upperVersion = options.upperVersion;
    recipe.dateFormat = options.dateFormat;
    recipe.ruleEnabled = g->ruleEnabled;
    recipe.ruleRegex = g->ruleRegex;
    recipe.pattern = g->rulePattern;
    recipe.replacement = g->ruleReplacement;
    if (const auto *project = activeProject(g->cfg)) {
        auto relative = std::filesystem::path(recipe.directory).lexically_relative(project->root).wstring();
        if (!relative.empty() && !projectPath(*project, relative).empty()) {
            recipe.directory = relative;
            recipe.relativeDirectory = true;
        }
    }
    return recipe;
}
void perform(ButtonAction action, bool quick) {
    if (!g->attached)
        return;
    bool affix = action.kind == Action::Prefix || action.kind == Action::Suffix;
    bool smart = action.kind == Action::Folder || action.kind == Action::Variables;
    if (affix || smart)
        quick = false;
    bool localNaming = affix || smart;
    if (g->busy && (quick || !localNaming)) {
        if (quick && action.kind == g->lastAction.kind && action.index == g->lastAction.index)
            g->pendingQuick = true;
        return;
    }
    // Compose local clicks even while an older fill is in flight. WM_RESULT
    // already keeps the newer stem and sends the latest name after that fill.
    if (g->busy && localNaming)
        g->pendingQuick = false;
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
        g->folderName = !g->folderName;
        updateName = true;
        quick = false;
        break;
    case Action::Variables:
        if (!g->variablesEnabled &&
            (trim(g->cfg.variableTemplate).empty() ||
             !smartNamingPreview(
                  compose(g->parts), path, g->dialog.extension,
                  {0, false, g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression,
                   effectiveVariables(), g->cfg.dateFormat})
                  .error.empty())) {
            openVariableEditor();
            return;
        }
        g->variablesEnabled = !g->variablesEnabled;
        updateName = true;
        quick = false;
        break;
    case Action::Prefix:
    case Action::Suffix: {
        bool prefix = action.kind == Action::Prefix;
        const auto &values = prefix ? g->cfg.prefixes : g->cfg.suffixes;
        if (action.index >= values.size())
            return;
        auto value = expandAffix(values[action.index], namingOptions().dateFormat);
        if (!prefix)
            if (auto field = structuredSuffixField(g->structure, g->cfg, value)) {
                auto next = namingFieldCanOmit(g->structure,
                                               static_cast<size_t>(field - g->structure.fields.data())) &&
                                    fieldValue(*field, g->cfg, g->structure.dateFormat) == value.substr(1)
                                ? L""
                                : value.substr(1);
                field->value = next;
                field->source = 0;
                field->omitted = next.empty();
                g->parts.suffix.clear();
                if (g->structuredField == static_cast<int>(field - g->structure.fields.data()))
                    resetStructuredInput();
                updateName = true;
                g->dirtyName = true;
                break;
            }
        auto &selected = prefix ? g->parts.prefix : g->parts.suffix;
        if (selected == value)
            selected.clear();
        else {
            // Adopt an identical literal affix already in the name rather than duplicating it.
            auto body = composeBody(g->parts);
            if (selected.empty() && (prefix ? body.starts_with(value) : body.ends_with(value))) {
                if (prefix)
                    body.erase(0, value.size());
                else
                    body.resize(body.size() - value.size());
                auto oldPrefix = g->parts.prefix, oldSuffix = g->parts.suffix;
                g->parts = splitName(body, g->cfg.modifiers);
                g->parts.prefix = oldPrefix;
                g->parts.suffix = oldSuffix;
            }
            if (!prefix && std::any_of(g->cfg.modifiers.begin(), g->cfg.modifiers.end(),
                                       [&](const auto &m) { return value == L"_" + m; }))
                g->parts.stage.clear();
            if (!prefix && value == L"_" + g->parts.date)
                g->parts.date.clear();
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
        if (r.namingOnly) {
            useNamingScheme(deserializeStructure(r.structure));
            return;
        }

        path = r.directory.empty() ? g->dialog.directory : r.directory;
        if (r.relativeDirectory) {
            const auto *project = activeProject(g->cfg);
            path = project ? projectPath(*project, r.directory) : L"";
            if (path.empty()) {
                g->status = L"此方案使用项目相对目录，请先选择项目并设置根目录。";
                g->statusError = true;
                updatePanelContent();
                return;
            }
        }
        resetStructuredInput();
        g->inlineTool = 0;
        g->structure = {};
        g->structuredField = -1;
        g->freeRecovery.reset();
        if (r.complete) {
            g->structure = deserializeStructure(r.structure);
            g->structuredField = -1;
            g->freeRecovery.reset();
            g->parts = {r.value, r.stageValue, r.date ? today(r.dateFormat) : L"",
                        expandAffix(r.prefix, r.dateFormat), expandAffix(r.suffix, r.dateFormat)};
            g->numberMode = r.counter;
            g->folderName = r.folder;
            g->variablesEnabled = r.variables;
            g->sessionExpression = r.expression;
            g->sessionVariables = r.variableValues;
            g->sessionDigits = r.digits;
            g->sessionDateFormat = r.dateFormat;
            g->sessionUpper = r.upperVersion;
            g->ruleEnabled = r.ruleEnabled;
            g->ruleRegex = r.ruleRegex;
            g->rulePattern = r.pattern;
            g->ruleReplacement = r.replacement;
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
        g->rebuildPending = true;
        PostMessageW(g->control, WM_PANEL_REFRESH, 0, 0);
        break;
    }
    }
    if (updateName) {
        name = composedName(&path);
        g->stemName = name;
        g->dirtyName = true;
        g->statusError = !g->ruleError.empty();
        if (g->statusError)
            g->status = g->ruleError;
        g->localNamePending = true;
        g->building = true;
        setTextIfChanged(GetDlgItem(g->panel, ID_PREVIEW), name + g->dialog.extension);
        g->building = false;
        updatePanelContent();
    }
    if (g->ruleError.empty())
        apply(path, name, quick && g->cfg.quickSave && g->dialog.save);
}
void updateInlineRule(bool immediate) {
    invalidateNaming();
    KillTimer(g->control, T_ACTION);
    g->stemName = composedName();
    g->dirtyName = true;
    g->localNamePending = true;
    g->status = g->ruleError.empty() ? L"命名预览已更新，规则收起后继续生效" : g->ruleError;
    g->statusError = !g->ruleError.empty();
    updatePanelContent();
    if (g->ruleError.empty()) {
        if (immediate && g->attached && !g->editingPath &&
            focusAllowsAction({JobType::Apply, g->dialog.hwnd, g->dialog.pid, g->generation.load()}))
            apply(g->dialog.directory, g->stemName, false);
        else
            SetTimer(g->control, T_ACTION, NAME_INPUT_DELAY, nullptr);
    }
}
void updateVariableDraftPreview() {
    if (!g->manager || g->managerTab != MANAGER_TEMPLATES)
        return;
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_TEMPLATE))
        g->variableTemplateDraft = windowText(edit);
    if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_VALUES))
        g->variableValuesDraft = windowText(edit);
    if (!g->variableAdvanced) {
        for (size_t i = 0; i < g->variableInputKeys.size(); ++i)
            if (auto edit = GetDlgItem(g->manager, ID_VARIABLE_FIELD_FIRST + static_cast<int>(i)))
                setVariableDraftValue(g->variableInputKeys[i], trim(windowText(edit)));
    }
    auto preview = smartNamingPreview(
        g->parts.base.empty() ? L"文件名" : compose(g->parts), g->dialog.directory, g->dialog.extension,
        {0, false, g->variableTemplateDraft, g->variableValuesDraft, g->cfg.dateFormat});
    if (trim(g->variableTemplateDraft).empty())
        preview.error = L"至少点选一个字段来组成文件名";
    auto hint = GetDlgItem(g->manager, ID_VARIABLE_HINT);
    auto text = preview.error.empty() ? L"预览：" + preview.name : preview.error;
    setTextIfChanged(hint, text);
    enableIfChanged(GetDlgItem(g->manager, ID_VARIABLE_SAVE), preview.error.empty());
}
void initializeVariableDraft() {
    if (!g->variableEditor) {
        g->variableTemplateDraft =
            g->sessionExpression.empty() ? g->cfg.variableTemplate : g->sessionExpression;
        g->variableValuesDraft = effectiveVariables();
        g->variableAdvanced = false;
        auto values = variableValues(g->variableValuesDraft);
        if (!values.contains(L"项目")) {
            auto folder = g->dialog.directory;
            while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/'))
                folder.pop_back();
            folder = std::filesystem::path(folder).filename().wstring();
            if (validName(folder))
                setVariableDraftValue(L"项目", folder);
        }
        if (!values.contains(L"作者")) {
            wchar_t user[256]{};
            DWORD length = static_cast<DWORD>(std::size(user));
            if (GetUserNameW(user, &length) && validName(user))
                setVariableDraftValue(L"作者", user);
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
    invalidateNaming();
    g->namingHistory.clear();
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
    bool windowName = event == EVENT_OBJECT_NAMECHANGE && object == OBJID_WINDOW && child == CHILDID_SELF &&
                      GetAncestor(hwnd, GA_ROOT) == hwnd;
    if (event != EVENT_SYSTEM_FOREGROUND && !value && (object != OBJID_WINDOW || child != CHILDID_SELF))
        return;
    if (value && !windowName && (!g->attached || GetAncestor(hwnd, GA_ROOT) != g->dialog.hwnd))
        return;
    if ((event == EVENT_OBJECT_HIDE || event == EVENT_OBJECT_DESTROY) && hwnd != g->dialog.hwnd)
        return;
    if (event == EVENT_OBJECT_SHOW &&
        (GetAncestor(hwnd, GA_ROOT) != hwnd ||
         (windowClass(hwnd) != L"#32770" && !windowClass(hwnd).starts_with(L"GHOST_WindowClass") &&
          !houdiniBrowserWindow(hwnd) && !genericCandidate(hwnd))))
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
    if (g->testTargetPid && pid != g->testTargetPid) {
        if (g->attached)
            hidePanel();
        return;
    }
    if (!g->testTargetImage.empty() && processImage(foreground) != g->testTargetImage) {
        if (g->attached)
            hidePanel();
        return;
    }
    if (requested) {
        if (!IsWindowVisible(foreground) || !IsWindowEnabled(foreground) || IsHungAppWindow(foreground))
            return;
        g->suppressed = nullptr;
        g->inspectAttempts = 0;
    }
    if (g->attached && !attachedDialogAlive())
        detach();
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
    if (g->bindingHint)
        return;
    if (g->inspectCandidate != foreground) {
        g->inspectCandidate = foreground;
        g->inspectAttempts = 0;
    }
    if (g->inspectAttempts++ >= 4)
        return;
    if (g->attached)
        detach();
    ULONGLONG gen = ++g->generation;
    g->dialog.hwnd = foreground;
    GetWindowThreadProcessId(foreground, &g->dialog.pid);
    Job job{JobType::Inspect, foreground, g->dialog.pid, gen};
    job.requestedInspect = requested;
    enqueue(std::move(job));
}

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

void showRecipeMenu() {
    if (!IsWindowVisible(g->panel))
        return;
    auto menu = CreatePopupMenu();
    for (size_t i = 0; i < g->cfg.recipes.size(); ++i)
        if (!g->cfg.recipes[i].namingOnly)
            AppendMenuW(menu, MF_STRING, 4000 + i, g->cfg.recipes[i].name.c_str());
    if (GetMenuItemCount(menu))
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_CAPTURE_RECIPE, L"将当前整套设置保存为流程…");
    AppendMenuW(menu, MF_STRING, 2, L"添加可填写变量的示例方案");
    AppendMenuW(menu, MF_STRING, 1, L"管理保存流程…");
    auto anchor = GetDlgItem(g->panel, g->structure.enabled ? ID_STRUCT_SCHEMES : ID_NAMING_MENU);
    RECT rect{};
    GetWindowRect(anchor ? anchor : g->panel, &rect);
    SetForegroundWindow(g->panel);
    int selected = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, rect.left, rect.bottom, g->panel);
    DestroyMenu(menu);
    if (selected == 2)
        addExampleRecipes();
    else if (selected == ID_CAPTURE_RECIPE)
        SendMessageW(g->panel, WM_COMMAND, ID_CAPTURE_RECIPE, 0);
    else if (selected == 1) {
        g->managerTab = 2;
        showManager();
    } else if (selected >= 4000 && static_cast<size_t>(selected - 4000) < g->cfg.recipes.size())
        SendMessageW(g->panel, WM_COMMAND, selected, 0);
}
LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g && !g->testTargetImage.empty()) {
        static LRESULT paintSerial = 0, accessibilityQueries = 0;
        if (msg == WM_APP + 200)
            return wp ? accessibilityQueries : paintSerial;
        if (msg == WM_PAINT || msg == WM_DRAWITEM)
            ++paintSerial;
        if (msg == WM_GETOBJECT)
            ++accessibilityQueries;
    }
    switch (msg) {
    case WM_VSCROLL:
        if (reinterpret_cast<HWND>(lp) == GetDlgItem(hwnd, ID_FOLDER_SCROLL) && !g->busy) {
            SCROLLINFO info{sizeof(info), SIF_TRACKPOS};
            GetScrollInfo(reinterpret_cast<HWND>(lp), SB_CTL, &info);
            int code = LOWORD(wp);
            if (code == SB_LINEUP)
                --g->locationPage;
            else if (code == SB_LINEDOWN)
                ++g->locationPage;
            else if (code == SB_PAGEUP)
                g->locationPage -= 3;
            else if (code == SB_PAGEDOWN)
                g->locationPage += 3;
            else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION)
                g->locationPage = info.nTrackPos;
            else if (code == SB_TOP)
                g->locationPage = 0;
            else if (code == SB_BOTTOM)
                g->locationPage = INT_MAX;
            updatePanelContent();
            return 0;
        }
        break;
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(hwnd, &point);
        RECT folders{px(PANEL_MARGIN), px(g->folderTop), px(g->width - PANEL_MARGIN),
                     px(g->folderTop + g->folderRows * FOLDER_ROW_STEP - 8)};
        if (PtInRect(&folders, point) && !g->busy) {
            g->folderWheel += GET_WHEEL_DELTA_WPARAM(wp);
            int rows = g->folderWheel / WHEEL_DELTA;
            g->folderWheel %= WHEEL_DELTA;
            if (rows) {
                g->locationPage -= rows;
                updatePanelContent();
            }
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN:
        cancelPathEdit(false);
        closeFlyout();
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        requestEditorDismiss(true);
        return 0;
    case WM_EDITOR_DISMISS:
        if ((static_cast<ULONG_PTR>(lp) >> 1) == g->editorRevision)
            finishEditorDismiss(reinterpret_cast<HWND>(wp), (lp & 1) != 0);
        return 0;
    case WM_TIMER:
        if (wp == T_COPY_FEEDBACK) {
            KillTimer(hwnd, T_COPY_FEEDBACK);
            setTextIfChanged(GetDlgItem(hwnd, ID_FINAL_NAME_LABEL), L"最终文件名");
            return 0;
        }
        if (wp == T_EDITOR_DISMISS) {
            KillTimer(hwnd, T_EDITOR_DISMISS);
            finishEditorDismiss(g->pendingEditorDismiss, g->explicitEditorDismiss);
            return 0;
        }
        break;
    case WM_PATH_BLUR:
        if (g->editingPath &&
            (GetForegroundWindow() != hwnd || (GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_INPUT) &&
                                               GetFocus() != GetDlgItem(hwnd, ID_DIRECTORY_EDIT))))
            cancelPathEdit(false);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && g->editingPath)
            PostMessageW(hwnd, WM_PATH_BLUR, 0, 0);
        if (LOWORD(wp) != WA_INACTIVE)
            rememberNativeSubject();
        if (LOWORD(wp) == WA_INACTIVE)
            requestEditorDismiss((GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
                                 (GetAsyncKeyState(VK_RBUTTON) & 0x8000));
        break;
    case WM_CONTEXTMENU: {
        closeFlyout();
        auto h = reinterpret_cast<HWND>(wp);
        if (GetParent(h) == hwnd && GetDlgCtrlID(h) == ID_PREVIEW) {
            bool copyable = g->ruleError.empty() && !g->namingPending && structuredInputError().empty() &&
                            validName(g->stemName);
            auto menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | (copyable ? 0 : MF_GRAYED), ID_COPY_NAME, L"复制完整文件名");
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (point.x == -1 && point.y == -1) {
                RECT r{};
                GetWindowRect(h, &r);
                point = {r.left, r.bottom};
            }
            SetForegroundWindow(hwnd);
            int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
            DestroyMenu(menu);
            if (command)
                SendMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        if (g->busy)
            return 0;
        if (renameStructuredCandidate(h))
            return 0;
        if (GetDlgCtrlID(h) == ID_DIRECTORY) {
            auto menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_EDIT, L"编辑路径");
            AppendMenuW(menu, MF_STRING, ID_DIRECTORY_SEARCH, L"搜索位置…");
            POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (point.x == -1 && point.y == -1) {
                RECT r{};
                GetWindowRect(h, &r);
                point = {r.left, r.bottom};
            }
            SetForegroundWindow(hwnd);
            int command = trackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd);
            DestroyMenu(menu);
            if (command)
                SendMessageW(hwnd, WM_COMMAND, command, 0);
            return 0;
        }
        auto found = g->actions.find(GetDlgCtrlID(h));
        if (GetParent(h) != hwnd || found == g->actions.end())
            return 0;
        auto action = found->second;
        if (action.kind == Action::Variables) {
            openVariableEditor();
            return 0;
        }
        bool folder = action.kind == Action::Location;
        if (!folder && action.kind != Action::Prefix && action.kind != Action::Suffix)
            return 0;
        if (folder && action.index >= g->displayPaths.size())
            return 0;
        HMENU menu = CreatePopupMenu();
        if (!folder)
            AppendMenuW(menu, MF_STRING, 3, L"修改预设");
        HMENU colors = CreatePopupMenu();
        int marked = bubbleColorIndex(action);
        for (int i = 0; i <= PRESET_COLOR_COUNT; ++i)
            AppendMenuW(colors, MF_STRING | (i == marked ? MF_CHECKED : 0), 100 + i, bubbleColors[i].name);
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colors), L"颜色标记");
        if (folder)
            AppendMenuW(menu, MF_STRING, 4,
                        isFavoritePath(g->displayPaths[action.index].path) ? L"取消收藏" : L"收藏文件夹");
        else
            AppendMenuW(menu, MF_STRING, 1, L"移除此预设");
        if (folder && g->locationTab == 2)
            AppendMenuW(menu, MF_STRING, 6, L"从历史移除");
        if (folder && g->locationTab == 4)
            AppendMenuW(menu, MF_STRING, 7, L"从最近移除");
        AppendMenuW(menu, MF_STRING, 2,
                    folder                          ? L"管理收藏…"
                    : action.kind == Action::Prefix ? L"管理前缀预设…"
                    : action.kind == Action::Suffix ? L"管理后缀预设…"
                                                    : L"管理命名预设…");
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (point.x == -1 && point.y == -1) {
            RECT r{};
            GetWindowRect(h, &r);
            point = {r.left, r.bottom};
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
                if (!saved)
                    g->cfg = std::move(previous);
                g->status = saved ? L"已移除预设，当前文件名保持不变" : L"移除未保存，原预设已保留";
                g->statusError = !saved;
                rebuildPanel();
            }
        } else if (command == 2) {
            if (folder) {
                g->managerTab = 1;
                showManager();
            } else {
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
        ClientPaint paint(hwnd, msg, wp);
        HDC dc = paint.dc();
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
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(track);
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
        SelectObject(dc, op);
        DeleteObject(pen);
        for (auto &[rect, text, heading] : g->labels) {
            auto copy = rect;
            SelectObject(dc, heading ? g->boldFont : g->smallFont);
            SetTextColor(dc, heading ? g->fg : g->muted);
            auto separator = text.find(L" · ");
            if (heading && rect.top < px(12) && separator != text.npos) {
                auto brand = text.substr(0, separator), caption = text.substr(separator + 3);
                DrawTextW(dc, brand.c_str(), -1, &copy, DT_SINGLELINE | DT_VCENTER);
                SIZE size{};
                GetTextExtentPoint32W(dc, brand.c_str(), static_cast<int>(brand.size()), &size);
                copy.left += size.cx + px(10);
                SelectObject(dc, g->smallFont);
                SetTextColor(dc, g->muted);
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
        if (windowClass(reinterpret_cast<HWND>(lp)) == L"Edit") {
            SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
            SetBkColor(reinterpret_cast<HDC>(wp), g->field);
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
        if (d->CtlType == ODT_COMBOBOX)
            return drawDropdownItem(*d);
        if (d->CtlType == ODT_STATIC) {
            DWORD_PTR textStyle = 0;
            if (!GetWindowSubclass(d->hwndItem, MaterialControlProc, 2, &textStyle))
                return FALSE;
            PaintBuffer buffer(d->hDC, d->rcItem, panelComposited(d->hwndItem) && IsWindowVisible(g->panel));
            paintMaterialText(d->hwndItem, buffer.dc, d->rcItem, static_cast<DWORD>(textStyle));
            return TRUE;
        }
        if (d->CtlType != ODT_BUTTON)
            return FALSE;
        DRAWITEMSTRUCT item = *d;
        PaintBuffer buffer(item.hDC, item.rcItem,
                           panelComposited(item.hwndItem) && IsWindowVisible(g->panel));
        item.hDC = buffer.dc;
        d = &item;
        if (drawSubjectGroup(*d))
            return TRUE;
        auto text = windowText(d->hwndItem);
        const auto &actions = GetParent(d->hwndItem) == g->flyout ? g->flyoutActions : g->actions;
        bool isPill = actions.contains(static_cast<int>(d->CtlID));
        bool variableChip = GetParent(d->hwndItem) == g->panel && g->variableEditor &&
                            d->CtlID >= ID_VARIABLE_TOKEN_FIRST &&
                            d->CtlID < ID_VARIABLE_TOKEN_FIRST + g->variableTokens.size();
        RECT r = d->rcItem;
        FillRect(d->hDC, &r, g->brush);
        DWORD_PTR dropdownFeedback = 0;
        GetWindowSubclass(d->hwndItem, PillProc, 1, &dropdownFeedback);
        bool hover = (dropdownFeedback & 1) != 0;
        bool selected = false;
        bool namingView = d->CtlID == ID_STRUCT_QUICK_VIEW || d->CtlID == ID_STRUCT_FIELDS_VIEW;
        if (namingView)
            selected = d->CtlID == ID_STRUCT_QUICK_VIEW ? quickNamingView() : !quickNamingView();
        bool quickPick = d->CtlID >= ID_QUICK_PICK && d->CtlID < ID_QUICK_PICK + 48;
        if (quickPick) {
            int index = (d->CtlID - ID_QUICK_PICK) / 6, slot = (d->CtlID - ID_QUICK_PICK) % 6;
            if (static_cast<size_t>(index) < g->structure.fields.size() && g->quickPicks.contains(index) &&
                static_cast<size_t>(slot) < g->quickPicks[index].size())
                selected = g->quickPicks[index][slot].value ==
                           fieldValue(g->structure.fields[index], g->cfg, g->structure.dateFormat);
        }
        bool structuredField =
            d->CtlID >= ID_STRUCT_FIELD && d->CtlID < ID_STRUCT_FIELD + g->structure.fields.size();
        if (structuredField)
            selected = static_cast<int>(d->CtlID) - ID_STRUCT_FIELD == g->structuredField;
        if (d->CtlID >= ID_STRUCT_CHOICE && d->CtlID < ID_STRUCT_CHOICE + g->structuredChoices.size() &&
            g->structuredField >= 0 && static_cast<size_t>(g->structuredField) < g->structure.fields.size())
            selected = g->structuredChoices[d->CtlID - ID_STRUCT_CHOICE].value ==
                       fieldValue(g->structure.fields[g->structuredField], g->cfg, g->structure.dateFormat);
        if (isPill) {
            auto action = actions.at(static_cast<int>(d->CtlID));
            if (action.kind == Action::Location)
                selected = action.index < g->displayPaths.size() &&
                           normalize(g->displayPaths[action.index].path) == normalize(g->dialog.directory);
            else if (action.kind == Action::Folder)
                selected = g->folderName;
            else if (action.kind == Action::Variables)
                selected = g->variablesEnabled;
            else if (action.kind == Action::Prefix)
                selected =
                    g->parts.prefix == expandAffix(g->cfg.prefixes[action.index], namingOptions().dateFormat);
            else if (action.kind == Action::Suffix) {
                selected =
                    g->parts.suffix == expandAffix(g->cfg.suffixes[action.index], namingOptions().dateFormat);
                auto value = expandAffix(g->cfg.suffixes[action.index], namingOptions().dateFormat);
                if (auto field = structuredSuffixField(g->structure, g->cfg, value))
                    selected =
                        selected || fieldValue(*field, g->cfg, g->structure.dateFormat) == value.substr(1);
            }
        }
        if (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY)
            selected = static_cast<int>(d->CtlID) - ID_LOC_RECOMMENDED == g->locationTab;
        if (d->CtlID == ID_LOC_RECENT)
            selected = g->locationTab == 4;
        bool affixTool = d->CtlID == ID_NAMING_MENU || d->CtlID == ID_AFFIX_SUMMARY;
        bool replaceTool = d->CtlID == ID_RENAME || d->CtlID == ID_REPLACE_SUMMARY;
        bool tool = affixTool || replaceTool;
        bool active = affixTool     ? !g->parts.prefix.empty() || !g->parts.suffix.empty()
                      : replaceTool ? g->ruleEnabled && !g->rulePattern.empty()
                                    : false;
        int toolIndex = affixTool ? 2 : 3;
        if (tool)
            selected = g->inlineTool == toolIndex || active;
        if (d->CtlID == ID_RULE_ENABLE)
            selected = g->ruleEnabled;
        if (variableChip) {
            auto keys = expressionKeys(g->variableTemplateDraft);
            selected = std::find(keys.begin(), keys.end(),
                                 g->variableTokens[d->CtlID - ID_VARIABLE_TOKEN_FIRST]) != keys.end();
        }
        if (d->CtlID == ID_VARIABLE_ADVANCED)
            selected = g->variableAdvanced;
        bool primary = d->CtlID == ID_SUBMIT || d->CtlID == ID_PRESET_SAVE || d->CtlID == ID_VARIABLE_SAVE;
        bool disabled = (d->itemState & ODS_DISABLED) != 0;
        hover = hover && !disabled;
        bool locationTab =
            (d->CtlID >= ID_LOC_RECOMMENDED && d->CtlID <= ID_LOC_HISTORY) || d->CtlID == ID_LOC_RECENT;
        bool panelButton = GetParent(d->hwndItem) == g->panel;
        bool locationButton = isPill && actions.at(static_cast<int>(d->CtlID)).kind == Action::Location;
        bool subjectButton = d->CtlID >= ID_QUICK_EDIT && d->CtlID < ID_QUICK_EDIT + 8;
        bool framed =
            namingView || quickPick || subjectButton || structuredField || d->CtlID == ID_NAME_MANAGER;
        bool quiet = !isPill && !variableChip && !primary;
        bool menuOpen = g->openPanelMenu == static_cast<int>(d->CtlID);
        bool keyboardFocus = !disabled && (dropdownFeedback & 2) && (d->itemState & ODS_FOCUS);
        if (d->CtlID == ID_CAPABILITIES) {
            auto font = SelectObject(d->hDC, g->smallFont);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, RGB(124, 130, 140));
            RECT caption = r;
            caption.left += px(2);
            DrawTextW(d->hDC, text.c_str(), -1, &caption,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (keyboardFocus) {
                RECT focus = r;
                InflateRect(&focus, -px(1), -px(1));
                DrawFocusRect(d->hDC, &focus);
            }
            SelectObject(d->hDC, font);
            return TRUE;
        }
        int panelHover = panelButton && !disabled ? hoverAmount(d->hwndItem, hover) : 0;
        auto appearance = panelButton ? panelAppearance(static_cast<int>(d->CtlID), menuOpen, !disabled,
                                                        panelHover, (d->itemState & ODS_SELECTED) != 0)
                                      : std::nullopt;
        COLORREF fill =
            controlFill(primary                  ? g->surface
                        : framed                 ? g->field
                        : isPill || variableChip ? g->surface
                                                 : g->bg,
                        selected || menuOpen, hover, (d->itemState & ODS_SELECTED) != 0, primary, !disabled);
        if (panelButton) {
            COLORREF base =
                quickPick || structuredField || subjectButton || locationButton ? RGB(29, 35, 45) : g->bg;
            fill = controlFill(base, selected || menuOpen, false, (d->itemState & ODS_SELECTED) != 0, primary,
                               !disabled);
            if (quickPick && selected && !disabled)
                fill = controlFill(RGB(39, 64, 99), false, false, (d->itemState & ODS_SELECTED) != 0, false,
                                   true);
            if (namingView)
                fill = controlFill(selected ? RGB(43, 51, 64) : g->bg, false, false,
                                   (d->itemState & ODS_SELECTED) != 0, false, !disabled);
            if (locationTab) {
                auto track = CreateSolidBrush(g->field);
                FillRect(d->hDC, &r, track);
                DeleteObject(track);
                fill = selected ? g->surface : g->field;
            }
            if (panelHover)
                fill = mixColor(fill, RGB(255, 255, 255), panelHover * (selected || primary ? 5 : 7) / 100);
        }
        int color = isPill ? bubbleColorIndex(actions.at(static_cast<int>(d->CtlID))) : 0;
        COLORREF mark = bubbleColors[color].value;
        if (color && !selected && !hover && !disabled)
            fill = RGB((GetRValue(fill) * 9 + GetRValue(mark)) / 10,
                       (GetGValue(fill) * 9 + GetGValue(mark)) / 10,
                       (GetBValue(fill) * 9 + GetBValue(mark)) / 10);
        if (appearance)
            fill = appearance->fill;
        auto brush = CreateSolidBrush(fill);
        COLORREF edge = fill;
        bool nameSelected = !disabled && isPill && selected &&
                            actions.at(static_cast<int>(d->CtlID)).kind != Action::Location;
        if (panelButton && !disabled && ((quickPick && selected) || (namingView && selected)))
            edge = namingView ? RGB(61, 71, 87) : mixColor(g->selection, g->accent, 30);
        if (panelButton && locationTab && selected && !disabled)
            edge = mixColor(g->surface, g->muted, 18);
        if (panelButton && !disabled) {
            if (quickPick || structuredField)
                edge = selected ? RGB(82, 126, 187) : RGB(46, 56, 71);
            if (subjectButton)
                edge = RGB(59, 74, 94);
            if (locationButton && !selected)
                edge = RGB(43, 52, 66);
        }
        if (appearance)
            edge = appearance->edge;
        auto pen = CreatePen(PS_SOLID, 1,
                             keyboardFocus  ? g->accent
                             : nameSelected ? mixColor(g->selection, g->accent, 50)
                                            : edge);
        auto ob = SelectObject(d->hDC, brush), op = SelectObject(d->hDC, pen);
        int radius = px(panelButton && locationTab ? 12 : panelButton ? 10 : 12);
        paintRoundedRect(d->hDC, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(d->hDC, op);
        SelectObject(d->hDC, ob);
        DeleteObject(pen);
        DeleteObject(brush);
        if (d->CtlID == ID_DIRECTORY) {
            auto font = SelectObject(d->hDC, g->smallFont);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, hover || (d->itemState & ODS_FOCUS) ? g->fg : g->muted);
            RECT textRect = r;
            textRect.right -= px(4);
            auto caption = trailingPath(d->hDC, text, textRect.right - textRect.left);
            DrawTextW(d->hDC, caption.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, font);
            return TRUE;
        }
        if (d->CtlID == ID_PANEL_OPTIONS || d->CtlID == ID_DIRECTORY_EDIT ||
            d->CtlID == ID_DIRECTORY_SEARCH || d->CtlID == ID_CLOSE) {
            UiIcon icon = d->CtlID == ID_PANEL_OPTIONS      ? UiIcon::Settings
                          : d->CtlID == ID_DIRECTORY_EDIT   ? UiIcon::Edit
                          : d->CtlID == ID_DIRECTORY_SEARCH ? UiIcon::Search
                          : d->CtlID == ID_CLOSE            ? UiIcon::Close
                                                            : UiIcon::Plus;
            int size = px(d->CtlID == ID_CLOSE ? 13 : 14);
            drawUiIcon(d->hDC, icon, (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size),
                       disabled                                                           ? RGB(96, 106, 122)
                       : hover                                                            ? g->fg
                       : d->CtlID == ID_DIRECTORY_EDIT || d->CtlID == ID_DIRECTORY_SEARCH ? RGB(143, 178, 225)
                                                                                          : g->muted);
            return TRUE;
        }
        if (d->CtlID == ID_FAVORITE_CURRENT) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Star, (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size), RGB(230, 184, 72), text == L"已收藏");
            return TRUE;
        }
        bool mainBody = d->hwndItem && GetParent(d->hwndItem) == g->panel && d->CtlID != ID_CLOSE;
        auto of = SelectObject(d->hDC, primary || subjectButton ? g->boldFont
                                       : panelButton &&
                                               (locationTab || namingView || locationButton ||
                                                d->CtlID == ID_STRUCT_SCHEMES || d->CtlID == ID_STRUCT_NAME ||
                                                d->CtlID >= ID_NUMBER_MENU && d->CtlID <= ID_RULES_SUMMARY ||
                                                d->CtlID == ID_COMBO_UNDO || d->CtlID == ID_COMBO_REDO ||
                                                d->CtlID == ID_CLEAR_RECENT || d->CtlID == ID_CLEAR_HISTORY)
                                           ? g->smallFont
                                       : tool || d->CtlID == ID_CLOSE ? g->font
                                       : mainBody                     ? g->bodyFont
                                       : primary                      ? g->boldFont
                                       : framed                       ? g->font
                                       : quiet                        ? g->smallFont
                                                                      : g->font);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, disabled                     ? g->muted
                             : primary                    ? RGB(255, 255, 255)
                             : selected                   ? g->fg
                             : tool || (quiet && !framed) ? g->muted
                                                          : g->fg);
        if (d->CtlID == ID_FAVORITE_CURRENT)
            SetTextColor(d->hDC, RGB(230, 184, 72));
        if (locationTab && !selected)
            SetTextColor(d->hDC, g->muted);
        if (appearance)
            SetTextColor(d->hDC, appearance->ink);
        RECT t = r;
        t.left += px(6);
        t.right -= px(6);
        if (d->CtlID == ID_TARGET_INFO) {
            auto split = text.find(L'\n');
            auto title = text.substr(0, split), detail = split == text.npos ? L"" : text.substr(split + 1);
            RECT top = t, bottom = t;
            top.top += px(5);
            top.bottom = top.top + px(21);
            bottom.top = top.bottom;
            bottom.bottom -= px(5);
            SelectObject(d->hDC, g->smallFont);
            SetTextColor(d->hDC, disabled ? g->muted : RGB(203, 182, 145));
            DrawTextW(d->hDC, title.c_str(), -1, &top,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, detail.c_str(), -1, &bottom,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (appearance) {
            bool compact =
                d->CtlID == ID_COMBO_UNDO || d->CtlID == ID_COMBO_REDO || d->CtlID == ID_STRUCT_SCHEMES;
            int size = px(compact ? 12 : 14);
            drawUiIcon(d->hDC, appearance->icon, static_cast<float>(r.left + px(compact ? 7 : 8)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size), appearance->iconInk);
            t.left = r.left + px(compact ? 24 : 28);
            if (appearance->dropdown) {
                if (!text.empty() && text.back() == L'▾')
                    text = trim(text.substr(0, text.size() - 1));
                if (d->CtlID == ID_STRUCT_SCHEMES)
                    text = L"命名设置";
                drawUiIcon(d->hDC, menuOpen ? UiIcon::ChevronUp : UiIcon::Chevron,
                           static_cast<float>(r.right - px(19)), (r.top + r.bottom - px(12)) / 2.f,
                           static_cast<float>(px(12)), appearance->iconInk);
                t.right = r.right - px(23);
            }
        }
        if (d->CtlID == ID_CLOSE)
            t = r;
        if (d->CtlID >= ID_QUICK_CHOICE && d->CtlID < ID_QUICK_CHOICE + 8 &&
            primarySubject(d->CtlID - ID_QUICK_CHOICE)) {
            int size = px(14);
            drawUiIcon(d->hDC, menuOpen ? UiIcon::ChevronUp : UiIcon::Chevron,
                       (r.left + r.right - size) / 2.f, (r.top + r.bottom - size) / 2.f,
                       static_cast<float>(size),
                       disabled            ? g->muted
                       : hover || menuOpen ? g->fg
                                           : g->muted);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (subjectButton) {
            t.left += px(6);
            t.right -= px(24);
            drawUiIcon(d->hDC, UiIcon::Edit, static_cast<float>(r.right - px(22)),
                       (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)),
                       disabled ? g->muted
                       : hover  ? g->fg
                                : RGB(143, 178, 225));
        }
        if (quickPick && selected && !disabled) {
            drawUiIcon(d->hDC, UiIcon::Check, static_cast<float>(r.left + px(4)),
                       (r.top + r.bottom - px(10)) / 2.f, static_cast<float>(px(10)), RGB(167, 203, 252));
            t.left += px(10);
        }
        if (locationButton) {
            int size = px(14);
            drawUiIcon(d->hDC, UiIcon::Folder, static_cast<float>(r.left + px(color ? 18 : 8)),
                       (r.top + r.bottom - size) / 2.f, static_cast<float>(size),
                       selected ? g->accent : g->muted);
            t.left = r.left + px(color ? 37 : 27);
        }
        if (color) {
            auto dot = CreateSolidBrush(mark);
            auto oldBrush = SelectObject(d->hDC, dot);
            auto oldPen = SelectObject(d->hDC, GetStockObject(NULL_PEN));
            int cy = (r.top + r.bottom) / 2;
            Ellipse(d->hDC, r.left + px(6), cy - px(3), r.left + px(12), cy + px(3));
            SelectObject(d->hDC, oldPen);
            SelectObject(d->hDC, oldBrush);
            DeleteObject(dot);
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
            (g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.find(L"已打开") !=
                 std::wstring::npos ||
             g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(
                 L"命名推荐"))) {
            RECT nameRect = t, openedRect = t;
            nameRect.bottom = t.top + px(15);
            openedRect.top = nameRect.bottom - px(1);
            DrawTextW(d->hDC, text.c_str(), -1, &nameRect,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX);
            SetTextColor(d->hDC, g->muted);
            auto badge =
                g->displayPaths[actions.at(static_cast<int>(d->CtlID)).index].sources.starts_with(L"命名推荐")
                    ? L"命名推荐"
                    : L"已打开";
            DrawTextW(d->hDC, badge, -1, &openedRect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, of);
            return TRUE;
        }
        if (structuredField && !g->structure.compact) {
            auto split = text.find(L'\n');
            auto caption = text.substr(0, split), value = split == text.npos ? L"" : text.substr(split + 1);
            RECT top = t, bottom = t;
            top.bottom = t.top + px(17);
            bottom.top = top.bottom;
            SelectObject(d->hDC, g->smallFont);
            SetTextColor(d->hDC, g->muted);
            DrawTextW(d->hDC, caption.c_str(), -1, &top,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
            SelectObject(d->hDC, g->font);
            SetTextColor(d->hDC, g->fg);
            DrawTextW(d->hDC, value.c_str(), -1, &bottom,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        } else
            DrawTextW(d->hDC, text.c_str(), -1, &t,
                      DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER | DT_NOPREFIX |
                          (locationButton || subjectButton || appearance ? DT_LEFT : DT_CENTER));
        SelectObject(d->hDC, of);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (!g->building && code == BN_CLICKED && !insideTransientEditor(GetDlgItem(hwnd, id)))
            requestEditorDismiss(true);
        if (!g->building && structuredCommand(id, code))
            return 0;
        if (id >= 5000 && static_cast<size_t>(id - 5000) < g->cfg.projects.size() && code == BN_CLICKED) {
            handleProjectChoice(id - 5000 + 100);
            return 0;
        }
        if (id >= 4000 && static_cast<size_t>(id - 4000) < g->cfg.recipes.size() && code == BN_CLICKED) {
            perform({Action::Recipe, static_cast<size_t>(id - 4000)}, false);
            return 0;
        }

        if (g->building)
            return 0;
        if (code == BN_CLICKED && id != ID_DIRECTORY && id != ID_PRESET_SAVE && id != ID_PRESET_CANCEL)
            closeFlyout(true);
        auto action = g->actions.find(id);
        if (action != g->actions.end() && (code == BN_CLICKED || code == BN_DOUBLECLICKED)) {

            if (code == BN_DOUBLECLICKED &&
                (action->second.kind == Action::Prefix || action->second.kind == Action::Suffix))
                return 0;

            bool quick = code == BN_DOUBLECLICKED || (GetKeyState(VK_CONTROL) & 0x8000);
            perform(action->second, quick);
            return 0;
        }
        if (code == EN_SETFOCUS && (id == ID_RULE_PATTERN || id == ID_RULE_REPLACEMENT ||
                                    id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX))
            checkpointNaming();
        if (id == ID_FREE_NAME_INPUT && code == EN_CHANGE) {
            if (g->structure.enabled && !g->structure.free)
                enterFreeNaming();
            g->stemName = windowText(GetDlgItem(hwnd, ID_FREE_NAME_INPUT));
            g->ruleEnabled = false;
            g->ruleError.clear();
            resetSmartNaming();
            if (!g->structure.enabled)
                g->namingHistory.clear();
            syncNameParts();
            g->dirtyName = true;
            g->localNamePending = true;
            if (g->busy)
                g->applyFocus = GetDlgItem(hwnd, ID_FREE_NAME_INPUT);
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
            if (g->rulePattern.empty())
                g->ruleEnabled = false;
            else if (wasEmpty)
                g->ruleEnabled = true;
            updateInlineRule();
            return 0;
        }
        if ((id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX) && code == EN_CHANGE) {
            auto value = windowText(GetDlgItem(hwnd, id));
            bool prefix = id == ID_AFFIX_PREFIX;
            if (!value.empty() && !validAffix(value, prefix)) {
                invalidateNaming();
                g->statusError = true;
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
                g->ruleIndex = index;
                const auto &rule = renameRules()[index];
                g->rulePattern = rule.pattern;
                g->ruleReplacement = rule.replacement;
                g->ruleRegex = rule.regex;
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
            case ID_ROUTE_SUGGEST:
                showSuggestedLocations();
                break;
            case ID_TARGET_INFO:
                showTargetActions();
                break;
            case ID_CLEAR_RECENT:
                if (g->locationTab == 4 && !g->busy) {
                    closeFlyout();
                    removeRecentLocations();
                }
                break;
            case ID_CLEAR_HISTORY:
                if (g->locationTab == 2 && !g->busy) {
                    closeFlyout();
                    removeHistoryLocation();
                }
                break;
            case ID_PANEL_OPTIONS:
                showPanelMenu(ID_PANEL_OPTIONS);
                break;
            case ID_CAPABILITIES:
                showCapabilityMenu();
                break;
            case ID_RULES_SUMMARY:
                showRuleSummaryMenu();
                break;
            case ID_NUMBER_MENU:
                showNumberMenu();
                break;
            case ID_AFFIX_SUMMARY:
                SendMessageW(hwnd, WM_COMMAND, ID_NAMING_MENU, 0);
                break;
            case ID_REPLACE_SUMMARY:
                SendMessageW(hwnd, WM_COMMAND, ID_RENAME, 0);
                break;
            case ID_PANEL_SETTINGS:
                g->managerTab = 3;
                showManager();
                break;
            case ID_BIND_CONTROL:
                startBinding();
                break;
            case ID_COPY_NAME:
                if (g->ruleError.empty() && !g->namingPending && structuredInputError().empty() &&
                    validName(g->stemName)) {
                    if (copyWorkflowText(g->stemName + g->dialog.extension)) {
                        setTextIfChanged(GetDlgItem(hwnd, ID_FINAL_NAME_LABEL), L"最终文件名 · 已复制");
                        SetTimer(hwnd, T_COPY_FEEDBACK, 1600, nullptr);
                    } else
                        workflowNotice(L"剪贴板暂时不可用，请重试。");
                }
                break;
            case ID_GENERIC_SUBMIT:
                if (g->dialog.generic && g->dialog.genericSubmit)
                    apply(g->dialog.directory, g->stemName, true, true);
                break;
            case ID_FOCUS_NAME:
                if (!g->structure.free)
                    structuredCommand(ID_STRUCT_FREE, 0);
                SetFocus(GetDlgItem(hwnd, ID_FREE_NAME_INPUT));
                break;
            case ID_DIRECTORY:
                beginPathEdit();
                break;
            case ID_DIRECTORY_EDIT:
                if (!g->editingPath)
                    beginPathEdit();
                else if (IsWindowVisible(g->panel))
                    SetFocus(GetDlgItem(hwnd, ID_DIRECTORY_INPUT));
                break;
            case ID_DIRECTORY_SEARCH:
                cancelPathEdit();
                showLocationFlyout();
                break;
            case ID_LOCATION_PAGE:
                cancelPathEdit();
                showLocationFlyout(true);
                break;
            case IDOK:
                SendMessageW(hwnd, WM_COMMAND, ID_SUBMIT, 0);
                break;
            case IDCANCEL:
                SetForegroundWindow(g->dialog.hwnd);
                break;
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
                g->managerTab = 0;
                showManager();
                break;
            case ID_ADD_PREFIX:
            case ID_ADD_SUFFIX:
                g->namingMode = id == ID_ADD_PREFIX ? 1 : 2;
                openInlinePreset(NEW_PRESET, GetDlgItem(hwnd, id));
                break;
            case ID_PRESET_SAVE:
                saveInlinePreset();
                break;
            case ID_PRESET_CANCEL:
                closeFlyout(true);
                break;
            case ID_NAMING_MENU:
            case ID_RENAME: {
                if (!structuredInputError().empty() || g->structuredIme)
                    break;
                resetStructuredInput();
                int tool = id == ID_NAMING_MENU ? 2 : 3;
                g->inlineTool = g->inlineTool == tool ? 0 : tool;
                closeFlyout();
                rebuildPanel();
                break;
            }
            case ID_RULE_ENABLE:
                checkpointNaming();
                g->ruleEnabled = !g->ruleEnabled && !g->rulePattern.empty();
                updateInlineRule();
                break;
            case ID_RULE_CLEAR:
                checkpointNaming();
                g->rulePattern.clear();
                g->ruleReplacement.clear();
                g->ruleEnabled = false;
                g->ruleIndex = 1;
                g->ruleRegex = false;
                updateInlineRule();
                rebuildPanel();
                break;
            case ID_VARIABLE_EDIT:
                openVariableEditor();
                break;
            case ID_FAVORITE_CURRENT: {
                if (!g->dialog.directory.empty()) {
                    auto name = std::filesystem::path(g->dialog.directory).filename().wstring();
                    toggleFavoriteLocation(
                        {name.empty() ? g->dialog.directory : name, g->dialog.directory, L"收藏"});
                }
                break;
            }
            case 720:
                handleProjectChoice(2);
                break;
            case 721:
                handleProjectChoice(3);
                break;
            case 722:
                handleProjectChoice(4);
                break;
            case 723:
                handleProjectChoice(5);
                break;
            case 724:
                handleProjectChoice(1);
                break;
            case 730:
                transferPresets(false);
                break;
            case 731:
                transferPresets(true);
                break;
            case ID_REFRESH_NUMBER:
                updateInlineRule();
                break;
            case ID_PROJECTS:
                showProjects();
                break;
            case ID_RECORDS:
                showSavedRecords();
                break;
            case ID_EXPLAIN:
                showNameExplanation();
                break;
            case ID_TRANSFER:
                showTransfer();
                break;
            case ID_DIAGNOSTICS:
                showDiagnostics();
                break;
            case ID_CAPTURE_RECIPE:
                saveCurrentRecipe();
                break;
            case ID_PLANS:
                showRecipeMenu();
                break;
            case ID_SUBMIT:
                if (!g->ruleError.empty()) {
                    g->status = g->ruleError;
                    updatePanelContent();
                    break;
                }
                if (g->editingPath) {
                    commitPathEdit();
                    break;
                }
                KillTimer(g->control, T_ACTION);
                apply(g->dialog.directory, g->stemName, !g->dialog.generic);
                break;
            case ID_COMBO_UNDO:
                moveNamingHistory(false);
                break;
            case ID_COMBO_REDO:
                moveNamingHistory(true);
                break;
            case ID_UNDO:
                if (g->structuredIme)
                    break;
                checkpointNaming();
                g->ruleEnabled = false;
                g->ruleError.clear();
                {
                    bool structured = g->structure.enabled;
                    g->freeRecovery.reset();
                    resetStructuredInput();
                    g->inlineTool = 0;
                    g->structuredAdvanced = false;
                    if (structured) {
                        for (auto &f : g->structure.fields) {
                            f.value.clear();
                            f.source = 0;
                            f.omitted = false;
                            if (f.id == g->structure.primaryField)
                                f.value = g->originalStem;
                        }
                        g->structure.free = false;
                        g->structure.literal = true;
                        g->structure.literalName = g->originalStem;
                    } else
                        g->structure = {};
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
