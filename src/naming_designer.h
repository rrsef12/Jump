#pragma once
#include "form_dialog.h"
#include "structured_naming.h"
#include <uxtheme.h>
#include <objbase.h>
#include <commdlg.h>
#include "naming_choices.h"
#include "naming_tools.h"

namespace jump {
inline std::wstring newNamingIdentity() {
    GUID guid{};
    CoCreateGuid(&guid);
    wchar_t text[40]{};
    StringFromGUID2(guid, text, 40);
    return text;
}
// All edits stay in this draft; Cancel cannot modify the active scheme.
struct NamingDesignerExample {
    StructuredNaming draft;
    std::map<std::wstring, std::wstring> simulated;
    std::wstring originalName;
    int selected = 0, page = 0, fieldPage = 0;
    std::vector<std::wstring> applications;
    bool applicationSettings = false;
    bool globalDefault = false;
};
struct NamingDesigner {
    FormData theme;
    CompactFormStyle style;
    StructuredNaming draft;
    const Config *config{};
    std::wstring originalName, error;
    std::wstring trialInput;
    bool trialInitialized = false;
    std::wstring baseline;
    bool baselineReady = false, initialSaved = true;
    std::map<std::wstring, std::wstring>
        simulated; // Temporary input; never serialized or applied to the real session.
    int adoptedExample = 0, errorField = -1, errorPage = 0, errorControl = 0;
    std::vector<StructuredNaming> starters;
    std::vector<std::wstring> applications;
    std::wstring applicationNotice;
    int selected = 0, page = 0, fieldPage = 0, scroll = 0, scrollMax = 0, example = 0;
    bool loading = false, applyOnSave = true, advanced = false, commonCreated = false,
         advancedCreated = false, applicationSettings = false;
    bool advancedPages[10]{};
    bool globalDefault = false;
    std::vector<NamingChoiceRow> choiceRows;
    int choicePage = 0, locationSelected = -1;
    RECT previewFrame{};
    RECT tableFrame{}, tableHeader{}, formatFrame{};
    std::vector<RECT> rowFrames;
    std::vector<std::wstring> conditions, primaryFields;
    std::vector<std::pair<HWND, int>> pages;
    std::vector<std::pair<HWND, int>> contentY;
    std::vector<HWND> frames;
    std::vector<NamingDesignerExample> examples;
    std::shared_ptr<const NamingCatalog> catalog;
    std::vector<const StructuredNaming *> templates;
    StructuredNaming previewSchema;
    HWND tooltip{};
    std::map<int, std::wstring> tooltips;
    std::vector<std::pair<size_t, size_t>> previewMatches;
    std::vector<size_t> previewFields;
};
constexpr UINT WM_NAMING_DESIGNER_DPI = WM_APP + 41;
struct NamingDesignerDpi {
    int dpi;
    HFONT font, hint;
};
enum DesignerId {
    ND_NAME = 4000,
    ND_TEMPLATE,
    ND_SEPARATOR,
    ND_NUMBERING,
    ND_FIELDS,
    ND_ADD,
    ND_REMOVE,
    ND_UP,
    ND_DOWN,
    ND_COPY,
    ND_BASE = 4020,
    ND_OUTPUT,
    ND_CHOICES,
    ND_SCHEME,
    ND_LABEL = 4040,
    ND_SOURCE,
    ND_DEFAULT,
    ND_REQUIRED,
    ND_QUICK,
    ND_PRIMARY,
    ND_DEFAULT_LABEL,
    ND_SOURCE_VALUE,
    ND_SOURCE_HINT,
    ND_CASING = 4060,
    ND_VALIDATION,
    ND_WIDTH,
    ND_OWN_JOINER,
    ND_JOINER,
    ND_PREFIX,
    ND_SUFFIX,
    ND_CANDIDATES = 4080,
    ND_CONDITION_FIELD,
    ND_CONDITION_VALUE,
    ND_PREVIEW,
    ND_ERROR,
    ND_DIGITS = 4090,
    ND_DATE_FORMAT,
    ND_UPPER,
    ND_EXAMPLE,
    ND_ADVANCED = 4100,
    ND_NAME_LABEL,
    ND_EXAMPLE_LABEL,
    ND_TEMPLATE_LABEL,
    ND_INTRO,
    ND_PREVIEW_LABEL,
    ND_ORIGINAL = 4120,
    ND_SCOPE,
    ND_ORDER_TITLE,
    ND_ORDER_HINT,
    ND_PREVIEW_PARTS,
    ND_PREVIEW_NOTE,
    ND_BASIC_HINT,
    ND_APPLICATION_LABEL = 4140,
    ND_APPLICATIONS,
    ND_APPLICATION_HINT,
    ND_APPLICATION_ADD,
    ND_APPLICATION_REMOVE,
    ND_APPLICATION_DELETE,
    ND_TRIAL = 4150,
    ND_ENABLED = 4160,
    ND_CLEAR_FORMAT,
    ND_RECOMMEND,
    ND_WARN_EXISTING,
    ND_EDIT_LOCATIONS,
    ND_EDIT_APPLICATIONS,
    ND_GLOBAL_HINT,
    ND_SECTION_FIELDS = 4170,
    ND_SECTION_LOCATIONS,
    ND_SECTION_APPS,
    ND_GLOBAL_DEFAULT,
    ND_CHOICE_PREV = 4180,
    ND_CHOICE_NEXT,
    ND_CHOICE_ADD,
    ND_CHOICE_PAGE,
    ND_MORE = 4190,
    ND_QUICK_SEPARATOR,
    ND_QUICK_COUNTER,
    ND_QUICK_FORMAT_LABEL,
    ND_QUICK_COUNTER_LABEL,
    ND_DETAIL_TITLE,
    ND_DETAIL_NOTE,
    ND_RESULT_LABEL,
    ND_SIMULATE = 4530,
    ND_TEMPLATES,
    ND_STRUCTURE,
    ND_RESET_SIM,
    ND_SIM_EXPLANATION,
    ND_USAGE_SUMMARY,
    ND_ADOPT,
    ND_RESTORE,
    ND_TEMPLATE_EFFECT,
    ND_BEHAVIOR,
    ND_CONDITION,
    ND_FIX,
    ND_ROW_UP = 4545,
    ND_ROW_DOWN = 4555,
    ND_SIM_LABEL = 4570,
    ND_SIM_VALUE = 4800,
    ND_SIM_AUTO = 4820,
    ND_ROW_SOURCE = 4850,
    ND_USAGE_STATUS = 4860,
    ND_EXTRA_TITLE,
    ND_TOOLS_LABEL,
    ND_TEST_ARROW,
    ND_COL_PART,
    ND_COL_CONTENT,
    ND_COL_ACTIONS,
    ND_FORMAT_TITLE,
    ND_DIRTY,
    ND_FOOTER_SURFACE,
    ND_CHOICE_NAME = 4500,
    ND_CHOICE_VALUE = 4510,
    ND_CHOICE_DELETE = 4520,
    ND_DATE_LABEL = 4880,
    ND_ROW_DATE = 4890,
    ND_SCHEME_MENU = 4870,
    ND_SIMPLE_VALUE = 4200,
    ND_SIMPLE_LABEL = 4220,
    ND_SIMPLE_HINT = 4240,
    ND_SIMPLE_CHOICES = 4260,
    ND_SIMPLE_AUTO = 4280,
    ND_SIMPLE_TEXT = 4300,
    ND_SIMPLE_REMOVE = 4320,
    ND_SIMPLE_SETTINGS = 4340
};
constexpr int DESIGNER_APPLICATIONS_HEIGHT = 76;
enum DesignerAddId {
    NDA_PRIMARY = 4400,
    NDA_PROJECT,
    NDA_DATE,
    NDA_FIXED,
    NDA_CUSTOM,
    NDA_APPLICATION,
    NDA_TYPE,
    NDA_DETAIL,
    NDA_STATUS,
    NDA_MANUAL_NUMBER,
    NDA_FORMAT = 4410,
    NDA_ORDER,
    NDA_COPY,
    NDA_LOCATIONS,
    NDA_FIELD_BASE = 4460
};
inline std::vector<std::pair<int, NamingField>> designerContentPresets(const Config &config) {
    // Presets only initialize the shared field parameters; they add no scheme-specific behavior.
    auto common = basicStructure(L"", &config);
    NamingField number;
    number.label = L"手动编号";
    number.defaultSource = 0;
    number.defaultValue = number.value = L"1";
    number.validation = 2;
    number.width = 3;
    std::vector<std::pair<int, NamingField>> presets{{NDA_TYPE, common.fields[2]},
                                                     {NDA_DETAIL, common.fields[3]},
                                                     {NDA_STATUS, common.fields[4]},
                                                     {NDA_MANUAL_NUMBER, std::move(number)}};
    for (auto &[command, field] : presets) {
        field.id.clear();
        field.quick = true;
    }
    return presets;
}
inline int designerExistingContent(const StructuredNaming &draft, int command,
                                   const std::vector<std::pair<int, NamingField>> &presets) {
    for (size_t i = 0; i < draft.fields.size(); ++i) {
        const auto &field = draft.fields[i];
        if ((command == NDA_PRIMARY && field.id == draft.primaryField) ||
            (command == NDA_PROJECT && namingFieldDefaultSource(field) == 1) ||
            (command == NDA_DATE && namingFieldDefaultSource(field) == 2))
            return static_cast<int>(i);
        for (const auto &[id, preset] : presets)
            if (id == command &&
                (field.label == preset.label ||
                 (command == NDA_MANUAL_NUMBER && field.label == L"编号" && field.validation == 2)))
                return static_cast<int>(i);
    }
    // Older schemes may keep these as editable text. Open them without changing their source.
    for (size_t i = 0; i < draft.fields.size(); ++i) {
        const auto &field = draft.fields[i];
        if ((command == NDA_PRIMARY && field.label == L"主要名称") ||
            (command == NDA_PROJECT && field.label == L"项目名称") ||
            (command == NDA_DATE && (field.label == L"日期" || field.label == L"当天日期")))
            return static_cast<int>(i);
    }
    return -1;
}
inline std::wstring designerExistingHelp(const StructuredNaming &draft, size_t index) {
    const auto &field = draft.fields[index];
    std::wstring text = L"点击编辑第 " + std::to_wstring(index + 1) + L" 项“" + field.label +
                        L"”，不会重复添加。\n当前来源：";
    int source = namingFieldDefaultSource(field);
    text += source == 1                      ? L"自动使用当前项目名称"
            : source == 2                    ? L"自动使用当天日期"
            : source == 3                    ? L"固定文字"
            : field.id == draft.primaryField ? L"自动带入原文件名，可在保存时修改"
                                             : L"保存时手动填写或选择";
    text += source == 3      ? L"；每次输出相同文字，固定内容不能为空。"
            : field.required ? L"；必填。"
                             : L"；可留空。";
    if (!field.candidates.empty())
        text += L"\n已有 " + std::to_wstring(field.candidates.size()) + L" 个常用选项，可继续修改。";
    text += L"\n可设置启用开关、内容来源、输入限制、输出格式、常用选项与出现条件。";
    return text;
}
inline std::wstring designerContentHelp(int command, const StructuredNaming &draft,
                                        const std::vector<std::pair<int, NamingField>> &presets) {
    std::wstring purpose, effect;
    switch (command) {
    case NDA_PRIMARY:
        purpose = L"主要名称\n文件名里的核心名称，如“守卫”“Rock”。";
        effect = L"新保存窗口自动带入原文件名，可在主面板修改；默认必填。\n例如：守卫_模型_工作。";
        break;
    case NDA_PROJECT:
        purpose = L"项目名称\n把所属项目写入文件名，便于区分不同项目的文件。";
        effect = L"自动跟随当前项目，无需重复填写；未选择项目时不输出这一项。\n例如：Demo_守卫。";
        break;
    case NDA_DATE:
        purpose = L"日期\n把日期写入文件名，便于按时间辨认文件。";
        effect = L"新增时自动取当天日期。日期样式在本行“日期格式”中调整。\n例如：守卫_20261020。";
        break;
    case NDA_TYPE:
        purpose = L"文件类别\n说明文件是什么，如模型、材质、角色设定。";
        effect = L"添加后可直接选择常用类别；可编辑显示名称与实际输出，如“静态模型”输出 SM。";
        break;
    case NDA_DETAIL:
        purpose = L"补充说明\n区分同一对象的不同内容，如重甲、夜景、侧面。";
        effect = L"添加后可填写或选择常用说明，留空时不占文件名。\n例如：守卫_重甲。";
        break;
    case NDA_STATUS:
        purpose = L"当前状态\n标明文件进展，如工作、审核、交付、定稿。";
        effect = L"添加后可选择或填写状态，常用选项可以自行调整。\n例如：守卫_审核。";
        break;
    case NDA_MANUAL_NUMBER:
        purpose = L"手动编号\n添加由你填写的编号，用来区分同类文件。";
        effect = L"默认 1，输出三位数字：1 → "
                 L"001；只接受数字，可修改补零位数。\n不会自动递增；需要按保存目录续号，请使用“文件名格式 → "
                 L"末尾自动编号”。";
        break;
    case NDA_FIXED:
        purpose = L"固定文字\n每次都输出相同的文字，如 SM、T、Final。";
        effect = L"先填写固定内容；保存文件时不必重复输入。之后可在方案中编辑，可添加多项。\n例如：SM_守卫。";
        break;
    case NDA_CUSTOM:
        purpose = L"自定义内容\n新增一个自己命名的输入项，如作者、LOD、镜头号。";
        effect = L"先填写名称，再设置默认值、常用选项、输出格式或出现条件；可添加多项。\n例如：作者填 "
                 L"LJ，文件名为守卫_LJ。";
        break;
    default:
        return {};
    }
    int existing = designerExistingContent(draft, command, presets);
    if (existing >= 0)
        return purpose + L"\n\n" + designerExistingHelp(draft, existing);
    if (draft.fields.size() >= 8)
        return purpose + L"\n" + effect +
               L"\n\n当前已有 8 项，暂时无法新增。请先删除不需要的命名内容；软件关联不占名额。";
    return purpose + L"\n" + effect + L"\n新增到命名顺序末尾，可用每行的 ↑ / ↓ 调整顺序。";
}
inline std::wstring designerText(HWND window, int id) {
    auto control = GetDlgItem(window, id);
    int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(length);
    return text;
}
inline void designerSet(HWND window, int id, const std::wstring &text) {
    if (designerText(window, id) != text)
        SetDlgItemTextW(window, id, text.c_str());
}
inline int designerChoice(HWND window, int id) {
    return static_cast<int>(SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0));
}
inline bool designerChecked(HWND window, int id) {
    return IsDlgButtonChecked(window, id) == BST_CHECKED;
}
inline void designerTip(HWND window, NamingDesigner &data, int id, const std::wstring &text) {
    auto control = GetDlgItem(window, id);
    if (!data.tooltip || !control)
        return;
    bool exists = data.tooltips.contains(id);
    if (exists && data.tooltips[id] == text)
        return;
    data.tooltips[id] = text;
    TOOLINFOW info{sizeof(info)};
    info.hwnd = window;
    info.uId = reinterpret_cast<UINT_PTR>(control);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.lpszText = data.tooltips[id].data();
    SendMessageW(data.tooltip, exists ? TTM_UPDATETIPTEXTW : TTM_ADDTOOLW, 0,
                 reinterpret_cast<LPARAM>(&info));
}
inline std::wstring designerFieldRules(const StructuredNaming &draft, size_t index) {
    const auto &field = draft.fields[index];
    std::wstring text = field.enabled ? L"已启用" : L"已关闭（保留设置）";
    text += field.required ? L" · 必填" : L" · 可留空";
    if (field.width)
        text += L" · 仅数字（补零 " + std::to_wstring(field.width) + L" 位）";
    else
        text += field.validation == 1   ? L" · 仅英文/数字/下划线"
                : field.validation == 2 ? L" · 仅数字"
                : field.validation == 3 ? L" · 不能有空格"
                                        : L" · 允许中文";
    if (field.casing)
        text += field.casing == 1 ? L" · 转大写" : L" · 转小写";
    if (field.ownJoiner)
        text += L" · 独立连接符“" + field.joiner + L"”";
    if (!field.prefix.empty())
        text += L" · 前加“" + field.prefix + L"”";
    if (!field.suffix.empty())
        text += L" · 后加“" + field.suffix + L"”";
    for (const auto &other : draft.fields)
        if (other.id == field.conditionField)
            text += L" · 仅当“" + other.label + L"”为“" + field.conditionValue + L"”时加入";
    text += field.quick ? L" · 主面板显示" : L" · 全部命名部分中显示";
    return text;
}
inline std::wstring designerLocationStatus(bool enabled, size_t count) {
    if (!count)
        return L"目录推荐：尚未配置";
    return std::wstring(L"目录推荐：") + (enabled ? L"已启用 · " : L"已关闭 · 保留 ") +
           std::to_wstring(count) + L" 条规则";
}
inline void designerGlobalHint(HWND window, NamingDesigner &data) {
    auto flags = designerLocationStatus(data.advancedPages[4] ? designerChecked(window, ND_RECOMMEND)
                                                              : data.draft.recommendLocations,
                                        data.draft.locations.size()) +
                 L"；同名提醒：" + (designerChecked(window, ND_WARN_EXISTING) ? L"开启" : L"关闭");
    designerSet(window, ND_GLOBAL_HINT, flags);
    designerTip(window, data, ND_GLOBAL_HINT,
                flags + L"\n关闭目录推荐时，保留所有规则；重新勾选可恢复。\n软件关联可添加 "
                        L"exe，也可移除单个或删除整个关联项。");
    designerSet(window, ND_EDIT_LOCATIONS, L"编辑目录规则 · " + std::to_wstring(data.draft.locations.size()));
    designerSet(window, ND_EDIT_APPLICATIONS, L"关联软件 · " + std::to_wstring(data.applications.size()));
}
inline void designerApplicationHint(HWND window, NamingDesigner &data) {
    std::wstring hint = data.applicationNotice;
    int selected = designerChoice(window, ND_APPLICATIONS);
    if (hint.empty() && selected >= 0 && static_cast<size_t>(selected) < data.applications.size()) {
        const auto &application = data.applications[selected];
        auto assigned = data.config->applicationNamingSchemes.find(application);
        if (assigned != data.config->applicationNamingSchemes.end() && assigned->second != data.draft.id) {
            std::wstring scheme = L"其他方案";
            for (const auto &item : data.catalog->schemes)
                if (item.id == assigned->second) {
                    scheme = item.name;
                    break;
                }
            hint = application + L" 当前使用“" + scheme + L"”，保存后改用此方案。";
        }
    }
    if (hint.empty())
        hint = data.applications.empty() ? L"选择软件的 .exe；软件关联不参与文件名拼接。"
                                         : L"保存方案后生效；该软件新打开保存窗口时自动使用。";
    designerSet(window, ND_APPLICATION_HINT, hint);
    EnableWindow(GetDlgItem(window, ND_APPLICATION_REMOVE), selected >= 0 && !data.applications.empty());
    if (data.advancedPages[3])
        designerGlobalHint(window, data);
}
inline void designerLoadApplications(HWND window, NamingDesigner &data, int selected = -1) {
    if (selected < 0)
        selected = std::max(0, designerChoice(window, ND_APPLICATIONS));
    auto combo = GetDlgItem(window, ND_APPLICATIONS);
    bool loading = data.loading;
    data.loading = true;
    SendMessageW(combo, WM_SETREDRAW, FALSE, 0);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    if (data.applications.empty())
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"未关联软件"));
    else
        for (const auto &application : data.applications)
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(application.c_str()));
    SendMessageW(combo, CB_SETCURSEL,
                 std::min(selected, std::max(0, static_cast<int>(data.applications.size()) - 1)), 0);
    SendMessageW(combo, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(combo, nullptr, FALSE);
    data.loading = loading;
    designerApplicationHint(window, data);
}
inline void designerAddApplication(HWND window, NamingDesigner &data) {
    wchar_t path[32768]{};
    OPENFILENAMEW picker{sizeof(picker)};
    picker.hwndOwner = GetAncestor(window, GA_ROOT);
    picker.lpstrTitle = L"选择自动使用此方案的软件";
    picker.lpstrFilter = L"应用程序 (*.exe)\0*.exe\0\0";
    picker.lpstrFile = path;
    picker.nMaxFile = 32768;
    picker.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&picker))
        return;
    auto application = namingApplicationKey(path);
    data.applicationNotice.clear();
    if (application.empty()) {
        data.applicationNotice = L"请选择软件的 .exe 文件。";
        designerApplicationHint(window, data);
        return;
    }
    auto found = std::find(data.applications.begin(), data.applications.end(), application);
    if (found != data.applications.end()) {
        designerLoadApplications(window, data, static_cast<int>(found - data.applications.begin()));
        return;
    }
    size_t count = data.applications.size() + 1;
    for (const auto &[executable, scheme] : data.config->applicationNamingSchemes)
        if (scheme != data.draft.id && executable != application &&
            std::find(data.applications.begin(), data.applications.end(), executable) ==
                data.applications.end())
            ++count;
    if (count > 100) {
        data.applicationNotice = L"最多关联 100 个软件，请先移除不再使用的关联。";
        designerApplicationHint(window, data);
        return;
    }
    data.applications.push_back(application);
    designerLoadApplications(window, data, static_cast<int>(data.applications.size() - 1));
}
#include "naming_designer_layout.h"
inline void designerReadChoices(HWND window, NamingDesigner &data) {
    for (int row = 0; row < 3; ++row) {
        size_t index = data.choicePage * 3 + row;
        if (index >= data.choiceRows.size())
            break;
        data.choiceRows[index].preset.name = trim(designerText(window, ND_CHOICE_NAME + row));
        data.choiceRows[index].preset.value = trim(designerText(window, ND_CHOICE_VALUE + row));
    }
}
inline void designerLoadChoices(HWND window, NamingDesigner &data) {
    bool loading = data.loading;
    data.loading = true;
    for (int row = 0; row < 3; ++row) {
        size_t index = data.choicePage * 3 + row;
        bool exists = index < data.choiceRows.size();
        for (int base : {ND_CHOICE_NAME, ND_CHOICE_VALUE, ND_CHOICE_DELETE})
            ShowWindow(GetDlgItem(window, base + row), exists ? SW_SHOW : SW_HIDE);
        if (exists) {
            designerSet(window, ND_CHOICE_NAME + row, data.choiceRows[index].preset.name);
            designerSet(window, ND_CHOICE_VALUE + row, data.choiceRows[index].preset.value);
        }
    }
    designerSet(window, ND_CHOICE_PAGE,
                std::to_wstring(data.choicePage + 1) + L" / " +
                    std::to_wstring(std::max<size_t>(1, (data.choiceRows.size() + 2) / 3)));
    EnableWindow(GetDlgItem(window, ND_CHOICE_PREV), data.choicePage > 0);
    EnableWindow(GetDlgItem(window, ND_CHOICE_NEXT),
                 static_cast<size_t>((data.choicePage + 1) * 3) < data.choiceRows.size());
    EnableWindow(GetDlgItem(window, ND_CHOICE_ADD), data.choiceRows.size() < 50);
    data.loading = loading;
}
inline bool designerChoicesCommand(HWND window, NamingDesigner &data, int id, int code) {
    if (!data.advanced || data.page != 2 || code != BN_CLICKED)
        return false;
    if (id != ND_CHOICE_ADD && id != ND_CHOICE_PREV && id != ND_CHOICE_NEXT &&
        (id < ND_CHOICE_DELETE || id >= ND_CHOICE_DELETE + 3))
        return false;
    designerReadChoices(window, data);
    if (id == ND_CHOICE_ADD && data.choiceRows.size() < 50) {
        data.choiceRows.push_back({});
        data.choicePage = static_cast<int>((data.choiceRows.size() - 1) / 3);
    }
    if (id == ND_CHOICE_PREV && data.choicePage > 0)
        --data.choicePage;
    if (id == ND_CHOICE_NEXT && static_cast<size_t>((data.choicePage + 1) * 3) < data.choiceRows.size())
        ++data.choicePage;
    if (id >= ND_CHOICE_DELETE && id < ND_CHOICE_DELETE + 3) {
        size_t row = data.choicePage * 3 + id - ND_CHOICE_DELETE;
        if (row < data.choiceRows.size())
            data.choiceRows.erase(data.choiceRows.begin() + row);
        if (data.choiceRows.empty())
            data.choiceRows.push_back({});
        data.choicePage = std::min(data.choicePage, static_cast<int>((data.choiceRows.size() - 1) / 3));
    }
    designerLoadChoices(window, data);
    return true;
}
inline void designerLocationValues(HWND window, NamingDesigner &data) {
    int index = designerChoice(window, 4701);
    SendDlgItemMessageW(window, 4702, CB_RESETCONTENT, 0, 0);
    if (index >= 0 && static_cast<size_t>(index) < data.draft.fields.size())
        for (const auto &candidate : data.draft.fields[index].candidates) {
            auto value = namingFieldOutput(data.draft.fields[index], candidate.value);
            SendDlgItemMessageW(window, 4702, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
        }
}
inline bool designerReadLocation(HWND window, NamingDesigner &data, StructuredNaming &next) {
    auto value = trim(designerText(window, 4702)), path = trim(designerText(window, 4703));
    if (value.empty() && path.empty())
        return true;
    int field = designerChoice(window, 4701);
    if (field < 0 || static_cast<size_t>(field) >= next.fields.size() || value.empty() || path.empty()) {
        data.error = L"请填完目录规则的命名部分、匹配文字和文件夹；不需要的规则可删除。";
        return false;
    }
    NamingLocationRule rule{next.fields[field].id, value, path};
    if (data.locationSelected >= 0 && static_cast<size_t>(data.locationSelected) < next.locations.size())
        next.locations[data.locationSelected] = rule;
    else
        next.locations.push_back(rule);
    data.error = namingSchemaError(next);
    return data.error.empty();
}
inline void designerLoadLocations(HWND window, NamingDesigner &data) {
    bool loading = data.loading;
    data.loading = true;
    SendDlgItemMessageW(window, 4700, LB_RESETCONTENT, 0, 0);
    for (const auto &rule : data.draft.locations) {
        auto field = std::find_if(data.draft.fields.begin(), data.draft.fields.end(),
                                  [&](const auto &f) { return f.id == rule.field; });
        auto text = (field == data.draft.fields.end() ? rule.field : field->label) + L" = " + rule.value +
                    L" → " + rule.directory;
        SendDlgItemMessageW(window, 4700, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    SendDlgItemMessageW(window, 4701, CB_RESETCONTENT, 0, 0);
    for (const auto &field : data.draft.fields)
        SendDlgItemMessageW(window, 4701, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(field.label.c_str()));
    bool selected = data.locationSelected >= 0 &&
                    static_cast<size_t>(data.locationSelected) < data.draft.locations.size();
    int field = 0;
    if (selected) {
        const auto &rule = data.draft.locations[data.locationSelected];
        for (size_t i = 0; i < data.draft.fields.size(); ++i)
            if (data.draft.fields[i].id == rule.field)
                field = static_cast<int>(i);
    }
    SendDlgItemMessageW(window, 4701, CB_SETCURSEL, field, 0);
    designerLocationValues(window, data);
    SendDlgItemMessageW(window, 4700, LB_SETCURSEL, selected ? data.locationSelected : -1, 0);
    designerSet(window, 4702, selected ? data.draft.locations[data.locationSelected].value : L"");
    designerSet(window, 4703, selected ? data.draft.locations[data.locationSelected].directory : L"");
    EnableWindow(GetDlgItem(window, 4705), TRUE);
    data.loading = loading;
}
inline bool designerLocationsCommand(HWND window, NamingDesigner &data, int id, int code) {
    if (id == 4701 && code == CBN_SELCHANGE) {
        designerLocationValues(window, data);
        return true;
    }
    if (id == 4706 && code == BN_CLICKED) {
        IFileOpenDialog *picker = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&picker)))) {
            DWORD options{};
            picker->GetOptions(&options);
            picker->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
            if (SUCCEEDED(picker->Show(GetAncestor(window, GA_ROOT)))) {
                IShellItem *item = nullptr;
                if (SUCCEEDED(picker->GetResult(&item))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                        designerSet(window, 4703, path);
                        CoTaskMemFree(path);
                    }
                    item->Release();
                }
            }
            picker->Release();
        }
        return true;
    }
    bool select = id == 4700 && code == LBN_SELCHANGE;
    if (!select && (code != BN_CLICKED || (id != 4707 && id != 4705)))
        return false;
    int selected = select ? static_cast<int>(SendDlgItemMessageW(window, 4700, LB_GETCURSEL, 0, 0)) : -1;
    auto next = data.draft;
    if (id == 4705) {
        if (data.locationSelected >= 0 && static_cast<size_t>(data.locationSelected) < next.locations.size())
            next.locations.erase(next.locations.begin() + data.locationSelected);
    } else if (!designerReadLocation(window, data, next)) {
        designerSet(window, ND_ERROR, data.error);
        SendDlgItemMessageW(window, 4700, LB_SETCURSEL, data.locationSelected, 0);
        return true;
    }
    data.draft = std::move(next);
    data.locationSelected = selected;
    designerLoadLocations(window, data);
    return true;
}
inline bool readDesignerField(HWND window, NamingDesigner &data, StructuredNaming &next, int remove = -1,
                              bool allowInvalid = false) {
    next = data.draft;
    next.name = trim(designerText(window, ND_NAME));
    if (data.advanced && data.page == 3) {
        next.counter = std::max(0, designerChoice(window, ND_NUMBERING));
        next.digits = designerChoice(window, ND_DIGITS) + 2;
        next.upper = designerChecked(window, ND_UPPER);
    }
    if (!data.advanced) {
        next.separator = designerText(window, ND_QUICK_SEPARATOR);
        next.counter = std::max(0, designerChoice(window, ND_QUICK_COUNTER));
    }
    if (data.advanced && data.page == 4) {
        next.recommendLocations = designerChecked(window, ND_RECOMMEND);
        next.warnExisting = designerChecked(window, ND_WARN_EXISTING);
    }
    if (data.advanced && data.page == 0) {
        int primary = designerChoice(window, ND_PRIMARY);
        next.primaryField = primary >= 0 && static_cast<size_t>(primary) < data.primaryFields.size()
                                ? data.primaryFields[primary]
                                : L"";
    }
    auto &f = next.fields[data.selected];
    if (!data.advanced) {
        for (size_t i = 0; i < next.fields.size(); ++i) {
            auto &field = next.fields[i];
            if (static_cast<int>(i) == remove || field.defaultSource == 1 || field.defaultSource == 2 ||
                field.id == next.primaryField)
                continue;
            bool choices = !field.candidates.empty() && field.defaultSource != 3;
            field.defaultValue = trim(
                designerText(window, (choices ? ND_SIMPLE_VALUE : ND_SIMPLE_TEXT) + static_cast<int>(i)));
            field.value = field.defaultValue;
            if (!allowInvalid && field.enabled && field.defaultSource == 3 && field.defaultValue.empty()) {
                data.error = field.label + L"的固定文字不能为空";
                return false;
            }
        }
    } else if (data.page == 0 && data.selected != remove) {
        f.label = trim(designerText(window, ND_LABEL));
        f.defaultSource = designerChoice(window, ND_SOURCE);
        f.defaultValue = trim(designerText(window, ND_DEFAULT));
        if (f.defaultSource == 1 || f.defaultSource == 2 || f.id == next.primaryField)
            f.defaultValue.clear();
        f.source = f.defaultSource;
        f.value = f.defaultValue;
        if (f.defaultSource == 2)
            next.dateFormat = designerChoice(window, ND_DATE_FORMAT);
        if (!allowInvalid && f.enabled && f.defaultSource == 3 && f.defaultValue.empty()) {
            data.error = L"固定文字不能为空";
            return false;
        }
        if (next.primaryField == f.id && f.defaultSource != 0) {
            data.error = L"带入原名的命名部分应使用“可编辑文字”来源";
            return false;
        }
    } else if (data.page == 1 && data.selected != remove) {
        f.casing = designerChoice(window, ND_CASING);
        f.validation = designerChoice(window, ND_VALIDATION);
        auto width = trim(designerText(window, ND_WIDTH));
        if (width.size() != 1 || width[0] < L'0' || width[0] > L'8') {
            data.error = L"补零位数填写 0–8；0 表示不补零";
            return false;
        }
        f.width = width[0] - L'0';
        f.ownJoiner = designerChecked(window, ND_OWN_JOINER);
        f.joiner = designerText(window, ND_JOINER);
        f.prefix = designerText(window, ND_PREFIX);
        f.suffix = designerText(window, ND_SUFFIX);
    } else if (data.page == 6 && data.selected != remove) {
        f.required = designerChecked(window, ND_REQUIRED);
        f.quick = designerChecked(window, ND_QUICK);
        f.enabled = designerChecked(window, ND_ENABLED);
    } else if (data.page == 7 && data.selected != remove) {
        int condition = designerChoice(window, ND_CONDITION_FIELD);
        f.conditionField = condition >= 0 && static_cast<size_t>(condition) < data.conditions.size()
                               ? data.conditions[condition]
                               : L"";
        f.conditionValue = f.conditionField.empty() ? L"" : trim(designerText(window, ND_CONDITION_VALUE));
    } else if (data.page == 2 && data.selected != remove) {
        designerReadChoices(window, data);
        data.error = validateNamingChoices(f, data.choiceRows);
        if (!data.error.empty() && !allowInvalid)
            return false;
        applyNamingChoices(next, data.selected, data.choiceRows);
    }
    if (data.advanced && data.page == 4 && !designerReadLocation(window, data, next))
        return false;
    if (remove >= 0 && static_cast<size_t>(remove) < next.fields.size() && next.fields.size() > 1) {
        // Validate the remaining fields, so an invalid default cannot trap a field in the scheme.
        auto removed = next.fields[remove].id;
        next.fields.erase(next.fields.begin() + remove);
        if (next.primaryField == removed)
            next.primaryField.clear();
        for (auto &field : next.fields)
            if (field.conditionField == removed) {
                field.conditionField.clear();
                field.conditionValue.clear();
            }
        std::erase_if(next.locations, [&](const auto &rule) { return rule.field == removed; });
    }
    data.error = namingSchemaError(next);
    return allowInvalid || data.error.empty();
}
#include "naming_designer_preview.h"
inline void designerSourceUI(HWND window, NamingDesigner &data) {
    int source = data.advancedPages[0] ? designerChoice(window, ND_SOURCE)
                                       : namingFieldDefaultSource(data.draft.fields[data.selected]),
        primary = designerChoice(window, ND_PRIMARY);
    const auto &field = data.draft.fields[data.selected];
    bool original = primary >= 0 && static_cast<size_t>(primary) < data.primaryFields.size() &&
                    data.primaryFields[primary] == field.id;
    CheckDlgButton(window, ND_ORIGINAL, original ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(window, ND_ORIGINAL), source == 0);
    bool editable = source == 3 || (source == 0 && !original);
    ShowWindow(GetDlgItem(window, ND_DEFAULT),
               data.advanced && data.page == 0 && editable ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(window, ND_SOURCE_VALUE),
               data.advanced && data.page == 0 && !editable ? SW_SHOW : SW_HIDE);
    const auto *project = activeProject(*data.config);
    designerSet(window, ND_DEFAULT_LABEL,
                source == 3   ? L"固定输出文字"
                : source == 1 ? L"自动取当前项目"
                : source == 2 ? L"自动取当天日期"
                : original    ? L"使用原文件名"
                              : L"默认内容（每次保存时预填，可留空）");
    designerSet(window, ND_SOURCE_VALUE,
                source == 1   ? (project ? project->name : L"尚未选择项目")
                : source == 2 ? today(designerChoice(window, ND_DATE_FORMAT))
                              : L"使用保存窗口的原文件名");
    designerSet(window, ND_SOURCE_HINT,
                source == 1   ? L"新窗口取当前项目；本次手填或选择选项可替换，不改方案来源。"
                : source == 2 ? L"新窗口取当天日期；本次手填或选择选项可替换，不改方案来源。"
                : source == 3 ? L"每次固定输出，使用时只读；已存选项保留，但不替换固定文字。"
                : original    ? L"不设固定默认值。先带入原名，手填或选择选项仅替换本次内容。"
                              : L"新窗口先预填默认内容；本次手填或选择选项可替换，不改默认。");
    for (int id : {ND_DATE_LABEL, ND_DATE_FORMAT})
        ShowWindow(GetDlgItem(window, id),
                   data.advanced && data.page == 0 && source == 2 ? SW_SHOW : SW_HIDE);
    EnableWindow(GetDlgItem(window, ND_DIGITS), designerChoice(window, ND_NUMBERING) != 0);
    EnableWindow(GetDlgItem(window, ND_UPPER), designerChoice(window, ND_NUMBERING) == 1);
    if (data.advancedPages[3])
        designerGlobalHint(window, data);
    InvalidateRect(window, nullptr, TRUE);
}
inline void designerLoadField(HWND window, NamingDesigner &data) {
    data.loading = true;
    const auto &f = data.draft.fields[data.selected];
    designerSet(window, ND_LABEL, f.label);
    designerSet(window, ND_DEFAULT, f.defaultValue);
    SendDlgItemMessageW(window, ND_SOURCE, CB_SETCURSEL, f.defaultSource < 0 ? f.source : f.defaultSource, 0);
    CheckDlgButton(window, ND_REQUIRED, f.required ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, ND_QUICK, f.quick ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, ND_ENABLED, f.enabled ? BST_CHECKED : BST_UNCHECKED);
    designerSet(window, ND_SCOPE, data.page >= 3 ? L"方案设置 · 整个文件名" : L"当前命名部分 · " + f.label);
    SendDlgItemMessageW(window, ND_CASING, CB_SETCURSEL, f.casing, 0);
    SendDlgItemMessageW(window, ND_VALIDATION, CB_SETCURSEL, f.validation, 0);
    designerSet(window, ND_WIDTH, std::to_wstring(f.width));
    CheckDlgButton(window, ND_OWN_JOINER, f.ownJoiner ? BST_CHECKED : BST_UNCHECKED);
    designerSet(window, ND_JOINER, f.joiner);
    designerSet(window, ND_PREFIX, f.prefix);
    designerSet(window, ND_SUFFIX, f.suffix);
    designerSet(window, ND_CANDIDATES, L"编辑常用选项 · " + std::to_wstring(f.candidates.size()) + L" 项…");
    SendDlgItemMessageW(window, ND_CONDITION_FIELD, CB_RESETCONTENT, 0, 0);
    data.conditions = {L""};
    SendDlgItemMessageW(window, ND_CONDITION_FIELD, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"始终添加"));
    int selected = 0;
    for (const auto &other : data.draft.fields)
        if (other.id != f.id) {
            data.conditions.push_back(other.id);
            SendDlgItemMessageW(window, ND_CONDITION_FIELD, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(other.label.c_str()));
            if (other.id == f.conditionField)
                selected = static_cast<int>(data.conditions.size() - 1);
        }
    SendDlgItemMessageW(window, ND_CONDITION_FIELD, CB_SETCURSEL, selected, 0);
    SendDlgItemMessageW(window, ND_CONDITION_VALUE, CB_RESETCONTENT, 0, 0);
    for (const auto &other : data.draft.fields)
        if (other.id == f.conditionField)
            for (const auto &choice : other.candidates) {
                auto output = namingFieldOutput(other, choice.value);
                SendDlgItemMessageW(window, ND_CONDITION_VALUE, CB_ADDSTRING, 0,
                                    reinterpret_cast<LPARAM>(output.c_str()));
            }
    designerSet(window, ND_CONDITION_VALUE, f.conditionValue);
    EnableWindow(GetDlgItem(window, ND_JOINER), f.ownJoiner);
    EnableWindow(GetDlgItem(window, ND_CONDITION_VALUE), !f.conditionField.empty());
    EnableWindow(GetDlgItem(window, ND_REMOVE), data.draft.fields.size() > 1);
    EnableWindow(GetDlgItem(window, ND_UP), data.selected > 0);
    EnableWindow(GetDlgItem(window, ND_DOWN),
                 static_cast<size_t>(data.selected + 1) < data.draft.fields.size());
    for (int id = ND_BASE; id <= ND_SCHEME; ++id)
        InvalidateRect(GetDlgItem(window, id), nullptr, FALSE);
    if (data.advanced && data.page == 2) {
        data.choiceRows.clear();
        data.choicePage = 0;
        for (const auto &candidate : f.candidates)
            data.choiceRows.push_back({candidate, candidate.value});
        if (data.choiceRows.empty())
            data.choiceRows.push_back({});
        designerLoadChoices(window, data);
    }
    if (data.advanced && data.page == 4)
        designerLoadLocations(window, data);
    data.loading = false;
    designerSourceUI(window, data);
    designerPreview(window, data);
}
inline void designerLoadSimple(HWND window, NamingDesigner &data) {
    data.loading = true;
    for (size_t i = 0; i < data.draft.fields.size(); ++i) {
        const auto &f = data.draft.fields[i];
        int index = static_cast<int>(i);
        bool automatic = f.defaultSource == 1 || f.defaultSource == 2,
             original = f.id == data.draft.primaryField;
        auto value =
            original ? designerText(window, ND_EXAMPLE) : fieldValue(f, *data.config, data.draft.dateFormat);
        designerSet(window, ND_SIMPLE_LABEL + index, std::to_wstring(i + 1) + L". " + f.label);
        designerSet(window, ND_SIMPLE_SETTINGS + index, L"编辑规则…");
        designerTip(window, data, ND_SIMPLE_LABEL + index, f.label);
        EnableWindow(GetDlgItem(window, ND_ROW_UP + index), i > 0);
        EnableWindow(GetDlgItem(window, ND_ROW_DOWN + index), i + 1 < data.draft.fields.size());
        auto combo = GetDlgItem(window, ND_SIMPLE_VALUE + index);
        if (combo) {
            bool visible = (GetWindowLongPtrW(combo, GWL_STYLE) & WS_VISIBLE) != 0;
            SendMessageW(combo, WM_SETREDRAW, FALSE, 0);
            SendMessageW(combo, CB_RESETCONTENT, 0, 0);
            SendMessageW(combo, CB_INITSTORAGE, f.candidates.size(), f.candidates.size() * 128);
            for (const auto &choice : f.candidates)
                SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(choice.value.c_str()));
            SendMessageW(combo, WM_SETREDRAW, TRUE, 0);
            if (!visible)
                ShowWindow(combo, SW_HIDE);
        }
        designerSet(window, ND_SIMPLE_VALUE + index, value);
        designerSet(window, ND_SIMPLE_TEXT + index, value);
        designerSet(window, ND_SIMPLE_AUTO + index,
                    original                     ? L"使用保存窗口的原文件名"
                    : automatic && value.empty() ? L"未选择项目"
                                                 : value);
        designerTip(window, data, ND_SIMPLE_AUTO + index,
                    original ? L"此项不设固定默认值，也不使用 Asset "
                               L"作为备用值。新窗口先带入原文件名；手填或选择常用选项只替换本次内容，不改方案"
                               L"。顶部预览使用独立测试原名。"
                             : L"新窗口按此来源自动填入：" + value +
                                   L"。保存时手填或选择选项可替换本次内容，不改变自动来源。");
        auto cue = f.defaultSource == 3 ? L"每次保存时固定输出" : L"每次保存时预填（可留空）";
        SendDlgItemMessageW(window, ND_SIMPLE_TEXT + index, EM_SETCUEBANNER, FALSE,
                            reinterpret_cast<LPARAM>(cue));
        COMBOBOXINFO info{sizeof(info)};
        if (GetComboBoxInfo(combo, &info) && info.hwndItem)
            SendMessageW(info.hwndItem, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(cue));
        designerSet(window, ND_ROW_SOURCE + index,
                    original    ? L"原名带入 · 可改"
                    : automatic ? (f.defaultSource == 1 ? L"自动 · 当前项目，可改" : L"自动 · 当天日期，可改")
                    : f.defaultSource == 3 ? L"固定文字 · 只读"
                                           : L"默认预填 · 可改 / 可选");
        std::wstring hint;
        if (!f.enabled)
            hint = L"已停用 · 规则仍保留";
        else {
            hint = f.defaultSource == 3 ? L"每次输出" : f.required ? L"必填" : L"可留空";
            if (original)
                hint += L" · 手填或选项替换本次";
            if (!f.quick)
                hint += L" · 主面板收起";
            auto brief = [](const std::wstring &text, size_t limit) {
                return text.size() > limit ? text.substr(0, limit - 1) + L"…" : text;
            };
            if (!f.conditionField.empty())
                for (const auto &other : data.draft.fields)
                    if (other.id == f.conditionField)
                        hint += L" · 条件：" + brief(other.label, 10) + L" = " + brief(f.conditionValue, 12);
            if (f.width)
                hint += L" · 数字补至 " + std::to_wstring(f.width) + L" 位";
            else if (f.validation == 2)
                hint += L" · 仅数字";
            if (f.casing || f.validation == 1 || f.validation == 3 || f.ownJoiner || !f.prefix.empty() ||
                !f.suffix.empty())
                hint += L" · 有文字处理";
        }
        designerSet(window, ND_SIMPLE_HINT + index, hint);
        auto help = designerExistingHelp(data.draft, i) + L"\n\n当前规则：\n" +
                    designerFieldRules(data.draft, i) + L"\n点击本行“编辑规则”修改或关闭。";
        designerTip(window, data, ND_SIMPLE_HINT + index, help);
        designerTip(window, data, ND_SIMPLE_SETTINGS + index, help);
        designerSet(window, ND_SIMPLE_CHOICES + index,
                    f.candidates.empty() ? L"添加选项…" : std::to_wstring(f.candidates.size()) + L" 个选项…");
        designerTip(window, data, ND_ROW_SOURCE + index, help);
        designerTip(window, data, ND_SIMPLE_CHOICES + index,
                    f.defaultSource == 3 ? L"固定文字使用时只读；选项保留供切换来源后使用，不会替换固定内容。"
                                         : L"保存时手动选择，替换本次内容，不会自动成为默认值。可编辑显示名称"
                                           L"、输出文字、关键词与颜色。");
        designerTip(window, data, ND_SIMPLE_VALUE + index,
                    L"新保存窗口的默认内容。可以填写或选择选项的输出文字；这里的修改会随方案保存，顶部试填不"
                    L"改默认。");
        designerTip(window, data, ND_SIMPLE_TEXT + index,
                    f.defaultSource == 3 ? L"每次固定输出；保存时不能临时修改。"
                                         : L"新保存窗口先预填此内容；保存时手填或选择选项只替换本次内容。");
        designerTip(window, data, ND_ROW_DATE + index,
                    L"调整本方案所有自动日期部分的格式，不改变手动填写的日期。");
        designerTip(window, data, ND_ROW_UP + index, L"向前移动这个命名部分");
        designerTip(window, data, ND_ROW_DOWN + index, L"向后移动这个命名部分");
        designerTip(window, data, ND_SIMPLE_REMOVE + index, L"删除这个命名部分");
        EnableWindow(GetDlgItem(window, ND_SIMPLE_REMOVE + index), data.draft.fields.size() > 1);
        bool choices = !f.candidates.empty() && !original && f.defaultSource != 3;
        ShowWindow(combo, !data.advanced && !automatic && choices ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(window, ND_SIMPLE_TEXT + index),
                   !data.advanced && !automatic && !original && !choices ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(window, ND_SIMPLE_AUTO + index),
                   !data.advanced && (automatic || original) ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(window, ND_SIMPLE_CHOICES + index), !data.advanced ? SW_SHOW : SW_HIDE);
    }
    data.loading = false;
}
inline void designerCreateControls(HWND window, NamingDesigner &data);
inline void designerLoad(HWND window, NamingDesigner &data) {
    bool visible = IsWindowVisible(window) != FALSE;
    if (visible)
        SendMessageW(window, WM_SETREDRAW, FALSE, 0);
    data.loading = true;
    designerCreateControls(window, data);
    designerSet(window, ND_NAME, data.draft.name);
    designerSet(window, ND_SEPARATOR, data.draft.separator);
    designerLoadApplications(window, data);
    SendDlgItemMessageW(window, ND_TEMPLATE, CB_SETCURSEL, data.example, 0);
    designerSet(window, ND_QUICK_SEPARATOR, data.draft.separator);
    auto number = std::wstring(std::max(0, data.draft.digits - 1), L'0') + L"1";
    auto counter = GetDlgItem(window, ND_QUICK_COUNTER);
    for (int index : {1, 2}) {
        auto text = index == 1 ? std::wstring(L"版本 ") + (data.draft.upper ? L"V" : L"v") + number
                               : L"序号 " + number;
        SendMessageW(counter, CB_DELETESTRING, index, 0);
        SendMessageW(counter, CB_INSERTSTRING, index, reinterpret_cast<LPARAM>(text.c_str()));
    }
    SendDlgItemMessageW(window, ND_QUICK_COUNTER, CB_SETCURSEL, data.draft.counter, 0);
    SendDlgItemMessageW(window, ND_NUMBERING, CB_SETCURSEL, data.draft.counter, 0);
    SendDlgItemMessageW(window, ND_DIGITS, CB_SETCURSEL, data.draft.digits - 2, 0);
    SendDlgItemMessageW(window, ND_DATE_FORMAT, CB_SETCURSEL, data.draft.dateFormat, 0);
    CheckDlgButton(window, ND_UPPER, data.draft.upper ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, ND_RECOMMEND, data.draft.recommendLocations ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window, ND_WARN_EXISTING, data.draft.warnExisting ? BST_CHECKED : BST_UNCHECKED);
    if (data.advancedPages[3])
        designerGlobalHint(window, data);
    CheckDlgButton(window, ND_GLOBAL_DEFAULT, data.globalDefault ? BST_CHECKED : BST_UNCHECKED);
    EnableWindow(GetDlgItem(window, ND_GLOBAL_DEFAULT),
                 data.draft.id != L"asset-v2" || defaultNamingScheme(*data.config).id != data.draft.id);
    SendDlgItemMessageW(window, ND_PRIMARY, CB_RESETCONTENT, 0, 0);
    data.primaryFields = {L""};
    int primary = 0;
    SendDlgItemMessageW(window, ND_PRIMARY, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"不自动填入原名"));
    for (const auto &f : data.draft.fields)
        if ((f.defaultSource < 0 ? f.source : f.defaultSource) == 0) {
            data.primaryFields.push_back(f.id);
            SendDlgItemMessageW(window, ND_PRIMARY, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(f.label.c_str()));
            if (f.id == data.draft.primaryField)
                primary = static_cast<int>(data.primaryFields.size() - 1);
        }
    SendDlgItemMessageW(window, ND_PRIMARY, CB_SETCURSEL, primary, 0);
    SendDlgItemMessageW(window, ND_FIELDS, LB_RESETCONTENT, 0, 0);
    for (const auto &f : data.draft.fields)
        SendDlgItemMessageW(window, ND_FIELDS, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(f.label.c_str()));
    SendDlgItemMessageW(window, ND_FIELDS, LB_SETCURSEL, data.selected, 0);
    data.loading = false;
    designerLoadSimulation(window, data);
    designerUsageSummary(window, data);
    if (!data.advanced)
        designerLoadSimple(window, data);
    designerSet(window, ND_STRUCTURE, designerStructureText(data.draft));
    if (data.advanced && data.page == 0)
        SendDlgItemMessageW(window, ND_SOURCE, CB_SETCURSEL,
                            namingFieldDefaultSource(data.draft.fields[data.selected]), 0);
    designerLayout(window, data);
    // Lazy creation must preserve a predictable keyboard traversal order.
    auto tab = [&](int id) {
        if (auto control = GetDlgItem(window, id))
            SetWindowPos(control, HWND_BOTTOM, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
    };
    tab(ND_NAME);
    tab(ND_TEMPLATES);
    tab(ND_SCHEME_MENU);
    tab(ND_SECTION_APPS);
    if (!data.advanced || data.page != 8)
        tab(ND_EXAMPLE);
    tab(ND_SIMULATE);
    tab(ND_TRIAL);
    tab(ND_SECTION_FIELDS);
    tab(ND_TEMPLATE);
    tab(ND_ADOPT);
    tab(ND_RESTORE);
    for (int id : {ND_BASE, ND_CHOICES, ND_BEHAVIOR, ND_OUTPUT, ND_CONDITION})
        tab(id);
    if (data.advanced) {
        for (int id : {ND_FIELDS,
                       ND_ADD,
                       ND_REMOVE,
                       ND_UP,
                       ND_DOWN,
                       ND_LABEL,
                       ND_SOURCE,
                       ND_ORIGINAL,
                       ND_DEFAULT,
                       ND_REQUIRED,
                       ND_QUICK,
                       ND_CASING,
                       ND_VALIDATION,
                       ND_WIDTH,
                       ND_OWN_JOINER,
                       ND_JOINER,
                       ND_PREFIX,
                       ND_SUFFIX,
                       ND_CANDIDATES,
                       ND_CONDITION_FIELD,
                       ND_CONDITION_VALUE,
                       ND_SEPARATOR,
                       ND_DATE_FORMAT,
                       ND_NUMBERING,
                       ND_DIGITS,
                       ND_UPPER,
                       ND_ENABLED,
                       ND_CLEAR_FORMAT,
                       ND_RECOMMEND,
                       ND_WARN_EXISTING,
                       ND_EDIT_LOCATIONS,
                       ND_EDIT_APPLICATIONS})
            tab(id);
    } else {
        for (size_t i = 0; i < data.draft.fields.size(); ++i)
            for (int base : {ND_SIMPLE_VALUE, ND_SIMPLE_TEXT, ND_ROW_DATE, ND_SIMPLE_CHOICES,
                             ND_SIMPLE_SETTINGS, ND_ROW_UP, ND_ROW_DOWN, ND_SIMPLE_REMOVE})
                tab(base + static_cast<int>(i));
        tab(ND_ADD);
        tab(ND_QUICK_SEPARATOR);
        tab(ND_QUICK_COUNTER);
        tab(ND_SCHEME);
        tab(ND_SECTION_LOCATIONS);
    }
    for (int row = 0; row < 3; ++row)
        for (int base : {ND_CHOICE_NAME, ND_CHOICE_VALUE, ND_CHOICE_DELETE})
            tab(base + row);
    for (int id : std::initializer_list<int>{
             ND_CHOICE_ADD, ND_CHOICE_PREV, ND_CHOICE_NEXT, ND_CONDITION_FIELD, ND_CONDITION_VALUE, 4700,
             4701, 4702, 4703, 4706, 4707, 4705, ND_GLOBAL_DEFAULT, ND_APPLICATIONS, ND_APPLICATION_ADD,
             ND_APPLICATION_REMOVE, ND_APPLICATION_DELETE})
        tab(id);
    if (data.advanced && data.page == 8)
        tab(ND_EXAMPLE);
    for (size_t i = 0; i < data.draft.fields.size(); ++i)
        tab(ND_SIM_VALUE + static_cast<int>(i));
    for (int id :
         std::initializer_list<int>{ND_RESET_SIM, ND_SIM_EXPLANATION, ND_ADVANCED, ND_FIX, IDCANCEL, IDOK})
        tab(id);
    // The advanced editor reads its own controls. Repopulating every hidden
    // simple-view combo on each tab/field change only repeats unrelated work.
    if (data.advanced && data.advancedCreated)
        designerLoadField(window, data);
    else {
        data.loading = false;
        designerPreview(window, data);
    }
    designerFooter(window, data);
    if (visible) {
        SendMessageW(window, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
    }
}
inline void designerCreateControls(HWND window, NamingDesigner &state) {
    auto *data = &state;
    auto &theme = data->theme;
    bool initial = !data->commonCreated;
    auto px = [&](int v) { return MulDiv(v, theme.dpi, 96); };
    auto child = [&](const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y,
                     int width, int height, int page = -1) {
        // Field controls sit beneath the permanent section navigation.
        int baseY = y;
        bool content = y >= 76 && (id < ND_APPLICATION_LABEL || id > ND_APPLICATION_DELETE);
        if (page >= 0 && (page <= 2 || page == 6 || page == 7)) {
            y += 192;
            if (x >= 400) {
                x += -76;
            } else if (x >= 188)
                x -= 168;
            if (width == 436)
                width = 600;
            else if (width == 428)
                width = 592;
            else if (width == 188 || width == 184)
                width = 280;
            else if (width == 216 || width == 224)
                width = 296;
            if (id == ND_WIDTH) {
                x = 568;
                width = 52;
            }
            if (id == ND_JOINER) {
                x = 484;
                width = 136;
            }
            if (id == ND_ENABLED) {
                x = 20;
                width = 600;
            }
            if (id >= ND_CHOICE_DELETE && id < ND_CHOICE_DELETE + 3) {
                x = 588;
                width = 32;
            }
            if (id == ND_CHOICE_PREV)
                x = 466;
            if (id == ND_CHOICE_PAGE)
                x = 506;
            if (id == ND_CHOICE_NEXT)
                x = 584;
            if (id == ND_CONDITION_VALUE)
                x = 324;
            if (id >= ND_CHOICE_NAME && id < ND_CHOICE_NAME + 3) {
                x = 24;
                width = 276;
            }
            if (id >= ND_CHOICE_VALUE && id < ND_CHOICE_VALUE + 3) {
                x = 328;
                width = 248;
            }
        } else if (page >= 3 && page <= 5)
            y += 196;
        auto control = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style,
                                       px(x), px(y), px(width), px(height), window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleW(nullptr), nullptr);
        if (content)
            data->contentY.push_back({control, baseY});
        bool hint = id == ND_PREVIEW_NOTE || id == ND_USAGE_STATUS || id == ND_TOOLS_LABEL ||
                    id == ND_DIRTY || id >= ND_SIMPLE_HINT && id < ND_SIMPLE_HINT + 8 ||
                    id >= ND_ROW_SOURCE && id < ND_ROW_SOURCE + 8;
        SendMessageW(control, WM_SETFONT,
                     reinterpret_cast<WPARAM>(hint ? theme.compact->hintFont : theme.font), FALSE);
        if (std::wstring_view(cls) != L"STATIC" &&
            !(std::wstring_view(cls) == L"BUTTON" && (style & BS_TYPEMASK) == BS_OWNERDRAW))
            SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
        if (page != -1)
            data->pages.push_back({control, page});
        if (std::wstring(cls) == L"EDIT") {
            data->frames.push_back(control);
            SendMessageW(control, EM_SETLIMITTEXT, id == ND_CANDIDATES ? 8000 : 255, 0);
            SendMessageW(control, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(px(3), px(3)));
        }
        if (std::wstring(cls) == L"BUTTON" && (style & BS_TYPEMASK) == BS_OWNERDRAW)
            SetWindowSubclass(control, compactFormControl, 1, 0);
        return control;
    };
    auto label = [&](const wchar_t *text, int x, int y, int width, int page = -1) {
        auto h = child(L"STATIC", text, SS_NOPREFIX, -1, x, y, width, 18, page);
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(theme.compact->hintFont), TRUE);
    };
    auto button = [&](const wchar_t *text, int id, int x, int y, int width) {
        return child(L"BUTTON", text, WS_TABSTOP | BS_OWNERDRAW, id, x, y, width, 28);
    };
    auto combo = [&](int id, int x, int y, int width, std::initializer_list<const wchar_t *> items,
                     int page = -1, bool editable = false) {
        auto h = child(L"COMBOBOX", L"",
                       WS_TABSTOP | WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS |
                           (editable ? CBS_DROPDOWN : CBS_DROPDOWNLIST),
                       id, x, y, width, 240, page);
        for (auto item : items)
            SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(h, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), px(26));
        SendMessageW(h, CB_SETITEMHEIGHT, 0, px(28));
        SendMessageW(h, CB_SETCURSEL, 0, 0);
        if (theme.compact->dropdown)
            theme.compact->dropdown(h);
        return h;
    };
    if (initial) {
        child(L"STATIC", L"方案名称", SS_NOPREFIX | SS_CENTERIMAGE, ND_NAME_LABEL, 20, 18, 68, 22);
        child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_NAME, 24, 36, 286, 22);
        child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_USAGE_STATUS, 20, 66, 490, 22);
        child(L"STATIC", L"保存位置", SS_NOPREFIX, ND_EXTRA_TITLE, 20, 500, 90, 22, -2);
        child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS | SS_NOTIFY, ND_TOOLS_LABEL, 20, 540, 440, 22, -2);
        child(L"STATIC", L"→", SS_NOPREFIX | SS_CENTER | SS_CENTERIMAGE, ND_TEST_ARROW, 260, 124, 40, 28);
        child(L"STATIC", L"命名部分", SS_NOPREFIX | SS_CENTERIMAGE, ND_COL_PART, 32, 244, 164, 24, -2);
        child(L"STATIC", L"默认填入 / 自动内容", SS_NOPREFIX | SS_CENTERIMAGE, ND_COL_CONTENT, 204, 244, 218,
              24, -2);
        child(L"STATIC", L"选项与规则", SS_NOPREFIX | SS_CENTERIMAGE, ND_COL_ACTIONS, 440, 244, 168, 24, -2);
        child(L"STATIC", L"文件名格式", SS_NOPREFIX, ND_FORMAT_TITLE, 32, 500, 180, 20, -2);
        child(L"EDIT", data->originalName, WS_TABSTOP | ES_AUTOHSCROLL, ND_EXAMPLE, 344, 36, 276, 22, -3);
        auto templateLabel =
            child(L"STATIC", L"选择示例（先试用，再采用）", SS_NOPREFIX, ND_TEMPLATE_LABEL, 340, 12, 280, 18);
        SendMessageW(templateLabel, WM_SETFONT, reinterpret_cast<WPARAM>(theme.compact->hintFont), TRUE);
        std::vector<std::wstring> names{L"原方案草稿（保留修改）"};
        for (const auto *s : data->templates) {
            auto example = composeStructure(namingSession(*s, L"Rock"), *data->config);
            if (s->counter) {
                SmartNaming options{s->counter};
                options.digits = s->digits;
                options.upperVersion = s->upper;
                example.name = numberingExample(example.name, options);
            }
            names.push_back(s->name + L" · " + (example.name.empty() ? L"等待填写" : example.name));
        }
        auto templates = combo(ND_TEMPLATE, 340, 32, 280, {}, -2);
        SendMessageW(templates, WM_SETREDRAW, FALSE, 0);
        SendMessageW(templates, CB_INITSTORAGE, names.size(), names.size() * 160);
        for (const auto &name : names)
            SendMessageW(templates, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
        SendMessageW(templates, WM_SETREDRAW, TRUE, 0);
        child(L"STATIC", L"软件关联", SS_NOPREFIX, ND_APPLICATION_LABEL, 20, 82, 112, 20);
        combo(ND_APPLICATIONS, 140, 76, 264, {});
        button(L"添加 exe…", ND_APPLICATION_ADD, 420, 76, 100);
        button(L"移除", ND_APPLICATION_REMOVE, 528, 76, 52);
        auto applicationHint = child(L"STATIC", L"", SS_NOPREFIX, ND_APPLICATION_HINT, 140, 110, 440, 34);
        button(L"清空关联", ND_APPLICATION_DELETE, 508, 110, 72);
        SendMessageW(applicationHint, WM_SETFONT, reinterpret_cast<WPARAM>(theme.compact->hintFont), FALSE);
        child(L"STATIC", L"按顺序组成文件名；右侧填写的是以后默认内容，试填请用“模拟一次保存”。", SS_NOPREFIX,
              ND_INTRO, 20, 82, 560, 30, -2);
        child(L"STATIC", L"试填预览 · 原文件名", SS_NOPREFIX, ND_PREVIEW_LABEL, 16, 412, 100, 18);
        button(L"批量预览…", ND_TRIAL, 420, 412, 172);
        child(L"STATIC", L"", SS_OWNERDRAW, ND_PREVIEW, 112, 410, 512, 24);
        child(L"STATIC", L"生成文件名（模拟）", SS_NOPREFIX, ND_RESULT_LABEL, 32, 123, 120, 18);
        child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_PREVIEW_PARTS, 32, 470, 576, 18);
        child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_PREVIEW_NOTE, 32, 490, 576, 18);
        child(L"STATIC", L"", SS_NOPREFIX | SS_LEFT | SS_ENDELLIPSIS, ND_ERROR, 16, 442, 608, 32);
        child(L"STATIC", L"", SS_NOPREFIX | SS_CENTERIMAGE, ND_DIRTY, 20, 488, 244, 28);
        child(L"STATIC", L"", SS_NOPREFIX, ND_FOOTER_SURFACE, 0, 476, 640, 52);
        button(L"复制为新方案", ND_COPY, 16, 488, 124);
        button(L"···", ND_SCHEME_MENU, 584, 14, 36);
        button(L"＋ 添加命名部分", ND_ADD, 140, 488, 128);
        button(L"文件名结构", ND_SECTION_FIELDS, 20, 76, 142);
        button(L"编号设置…", ND_SCHEME, 172, 76, 142);
        button(L"目录推荐…", ND_SECTION_LOCATIONS, 324, 76, 142);
        button(L"使用范围", ND_SECTION_APPS, 476, 76, 144);
        button(L"‹ 返回方案", ND_ADVANCED, 20, 184, 124);
        combo(ND_QUICK_SEPARATOR, 176, 400, 148, {L"_", L"-", L""}, -2, true);
        combo(ND_QUICK_COUNTER, 460, 400, 160, {L"不加编号", L"版本 v001", L"序号 001"}, -2);
        child(L"STATIC", L"分隔符", SS_NOPREFIX, ND_QUICK_FORMAT_LABEL, 20, 400, 140, 18, -2);
        child(L"STATIC", L"末尾自动编号", SS_NOPREFIX, ND_QUICK_COUNTER_LABEL, 348, 400, 104, 18, -2);
        child(L"STATIC", L"日期格式（本方案所有自动日期部分）", SS_NOPREFIX, ND_DATE_LABEL, 20, 550, 600, 20,
              -3);
        combo(ND_DATE_FORMAT, 20, 578, 280, {L"YYYYMMDD", L"YYYY-MM-DD", L"YYYY_MM_DD"}, -3);
        child(L"STATIC", L"", SS_NOPREFIX, ND_DETAIL_TITLE, 164, 190, 456, 24);
        child(L"STATIC", L"", SS_NOPREFIX, ND_DETAIL_NOTE, 20, 224, 600, 30);
        button(L"取消", IDCANCEL, 408, 488, 88);
        button(data->applyOnSave ? L"保存并使用" : L"保存方案", IDOK, 508, 488, 116);
        for (int id :
             {ND_ORDER_HINT, ND_SOURCE_HINT, ND_BASIC_HINT, ND_PREVIEW_PARTS, ND_PREVIEW_NOTE, ND_ERROR})
            SendMessageW(GetDlgItem(window, id), WM_SETFONT,
                         reinterpret_cast<WPARAM>(theme.compact->hintFont), TRUE);
        button(L"模拟填写…", ND_SIMULATE, 224, 72, 194);
        button(L"选择命名示例…", ND_TEMPLATES, 416, 24, 204);
        button(L"填写与显示", ND_BEHAVIOR, 264, 308, 112);
        button(L"添加条件", ND_CONDITION, 508, 308, 112);
        button(L"按方案默认重新试填", ND_RESET_SIM, 20, 700, 206);
        button(L"采用这个示例", ND_ADOPT, 20, 526, 216);
        button(L"返回原方案草稿", ND_RESTORE, 250, 526, 216);
        button(L"定位并修正…", ND_FIX, 490, 650, 130);
        ShowWindow(GetDlgItem(window, ND_FIX), SW_HIDE);
        child(L"STATIC", L"", SS_NOPREFIX | SS_EDITCONTROL, ND_STRUCTURE, 20, 116, 600, 22);
        child(L"STATIC", L"", SS_NOPREFIX, ND_TEMPLATE_EFFECT, 20, 392, 600, 116);
        child(L"STATIC", L"", SS_NOPREFIX, ND_USAGE_SUMMARY, 20, 528, 600, 116);
        child(L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
              ND_SIM_EXPLANATION, 24, 500, 592, 172);
        for (int i = 0; i < 8; ++i) {
            child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_SIM_LABEL + i, 20, 362 + i * 42, 148, 24);
            combo(ND_SIM_VALUE + i, 180, 362 + i * 42, 440, {}, -3, true);
            child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_SIM_AUTO + i, 180, 362 + i * 42, 440, 24);
        }
        data->commonCreated = true;
    }
    if (!data->advanced) {
        for (int i = 0; i < static_cast<int>(data->draft.fields.size()); ++i) {
            int y = 116 + i * 48;
            const auto &field = data->draft.fields[i];
            if (!GetDlgItem(window, ND_SIMPLE_LABEL + i)) {
                child(L"STATIC", L"", SS_NOPREFIX | SS_EDITCONTROL, ND_SIMPLE_LABEL + i, 20, y + 5, 108, 22,
                      -2);
                child(L"STATIC", L"", SS_NOPREFIX | SS_CENTERIMAGE | SS_ENDELLIPSIS, ND_SIMPLE_AUTO + i, 140,
                      y, 224, 28, -2);
                child(L"STATIC", L"", SS_NOPREFIX | SS_EDITCONTROL, ND_SIMPLE_HINT + i, 140, y + 30, 440, 16,
                      -2);
                child(L"STATIC", L"", SS_NOPREFIX | SS_CENTERIMAGE, ND_ROW_SOURCE + i, 32, y + 30, 124, 28,
                      -2);
                child(L"BUTTON", L"选项", WS_TABSTOP | BS_OWNERDRAW, ND_SIMPLE_CHOICES + i, 376, y, 64, 28,
                      -2);
                child(L"BUTTON", L"来源与规则…", WS_TABSTOP | BS_OWNERDRAW, ND_SIMPLE_SETTINGS + i, 448, y,
                      76, 28, -2);
                child(L"BUTTON", L"日期格式…", WS_TABSTOP | BS_OWNERDRAW, ND_ROW_DATE + i, 320, y, 104, 28,
                      -2);
                child(L"BUTTON", L"×", WS_TABSTOP | BS_OWNERDRAW, ND_SIMPLE_REMOVE + i, 536, y, 44, 28, -2);
                child(L"BUTTON", L"↑", WS_TABSTOP | BS_OWNERDRAW, ND_ROW_UP + i, 524, y, 28, 28, -2);
                child(L"BUTTON", L"↓", WS_TABSTOP | BS_OWNERDRAW, ND_ROW_DOWN + i, 558, y, 28, 28, -2);
                SendMessageW(GetDlgItem(window, ND_SIMPLE_HINT + i), WM_SETFONT,
                             reinterpret_cast<WPARAM>(theme.compact->hintFont), TRUE);
            }
            bool automatic = field.defaultSource == 1 || field.defaultSource == 2;
            bool original = field.id == data->draft.primaryField;
            bool choices = !field.candidates.empty() && !original && field.defaultSource != 3;
            if (!automatic && choices && !GetDlgItem(window, ND_SIMPLE_VALUE + i))
                combo(ND_SIMPLE_VALUE + i, 140, y, 224, {}, -2, true);
            if (!automatic && !choices && !GetDlgItem(window, ND_SIMPLE_TEXT + i))
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_SIMPLE_TEXT + i, 144, y + 4, 216, 20, -2);
        }
    } else {
        if (!data->advancedCreated) {
            child(L"STATIC", L"文件名组成", SS_NOPREFIX, ND_ORDER_TITLE, 16, 124, 150, 18, -3);
            child(L"STATIC", L"按此顺序拼接", SS_NOPREFIX, ND_ORDER_HINT, 16, 146, 150, 18, -3);
            auto list = child(L"LISTBOX", L"",
                              WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS |
                                  LBS_NOINTEGRALHEIGHT,
                              ND_FIELDS, 16, 130, 150, 214, -3);
            SendMessageW(list, LB_SETITEMHEIGHT, 0, px(29));
            button(L"删除", ND_REMOVE, 94, 354, 72);
            button(L"上移", ND_UP, 16, 390, 72);
            button(L"下移", ND_DOWN, 94, 390, 72);
            child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_SCOPE, 188, 84, 296, 22, -3);
            button(L"基础与来源", ND_BASE, 188, 116, 132);
            button(L"文字处理", ND_OUTPUT, 330, 116, 132);
            button(L"可选内容", ND_CHOICES, 472, 116, 152);
            for (int id : {ND_ADD, ND_REMOVE, ND_UP, ND_DOWN, ND_BASE, ND_OUTPUT, ND_CHOICES, ND_SCHEME})
                data->pages.push_back({GetDlgItem(window, id), -3});
            // Compatibility selector stays hidden; the per-field checkbox edits the
            // same primaryField mapping, without introducing a second source model.
            combo(ND_PRIMARY, 188, 148, 436, {});
            data->advancedCreated = true;
        }
        if (!data->advancedPages[data->page] && data->page < 8) {
            if (data->page == 0 || data->page == 6) {
                label(L"这部分叫什么", 188, 166, 170, 0);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_LABEL, 192, 190, 188, 22, 0);
                label(L"从哪里获取", 400, 166, 220, 0);
                combo(ND_SOURCE, 400, 186, 224, {L"可编辑文字", L"当前项目名称", L"当天日期", L"固定文字"},
                      0);
                child(L"BUTTON", L"自动带入原文件名", WS_TABSTOP | BS_AUTOCHECKBOX, ND_ORIGINAL, 188, 226,
                      300, 24, 0);
                child(L"BUTTON", L"启用此项", WS_TABSTOP | BS_AUTOCHECKBOX, ND_ENABLED, 188, 166, 436, 24, 6);
                child(L"STATIC", L"", SS_NOPREFIX, ND_DEFAULT_LABEL, 188, 260, 436, 18, 0);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_DEFAULT, 192, 284, 428, 22, 0);
                child(L"STATIC", L"", SS_NOPREFIX | SS_ENDELLIPSIS, ND_SOURCE_VALUE, 188, 284, 436, 24, 0);
                child(L"STATIC", L"", SS_NOPREFIX, ND_SOURCE_HINT, 188, 316, 436, 30, 0);
                child(L"BUTTON", L"必填", WS_TABSTOP | BS_AUTOCHECKBOX, ND_REQUIRED, 188, 216, 436, 24, 6);
                child(L"BUTTON", L"在主面板显示", WS_TABSTOP | BS_AUTOCHECKBOX, ND_QUICK, 188, 266, 436, 24,
                      6);
                child(L"STATIC", L"关闭“启用此项”会从命名中移除，原设置保留；隐藏主面板只收起输入。",
                      SS_NOPREFIX, ND_BASIC_HINT, 188, 316, 436, 64, 6);
            } else if (data->page == 1) {
                label(L"大小写", 188, 166, 170, 1);
                combo(ND_CASING, 188, 186, 188, {L"保持原样", L"全部大写", L"全部小写"}, 1);
                label(L"允许输入什么", 400, 166, 210, 1);
                combo(ND_VALIDATION, 400, 186, 224,
                      {L"关闭限制（允许中文）", L"仅英文 / 数字 / 下划线", L"仅数字", L"不允许空格"}, 1);
                label(L"数字补齐位数（3 → 001，0 表示不补齐）", 188, 236, 370, 1);
                child(L"EDIT", L"0", WS_TABSTOP | ES_AUTOHSCROLL, ND_WIDTH, 568, 236, 52, 22, 1);
                child(L"BUTTON", L"单独设置前面的连接符", WS_TABSTOP | BS_AUTOCHECKBOX, ND_OWN_JOINER, 188,
                      280, 270, 24, 1);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_JOINER, 484, 282, 136, 22, 1);
                label(L"这部分前固定文字", 188, 328, 200, 1);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_PREFIX, 192, 352, 188, 22, 1);
                label(L"这部分后固定文字", 400, 328, 220, 1);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_SUFFIX, 404, 352, 216, 22, 1);
                child(L"BUTTON", L"恢复原样", WS_TABSTOP | BS_OWNERDRAW, ND_CLEAR_FORMAT, 188, 386, 132, 28,
                      1);
                label(L"会恢复原样、关闭补零、清空前后文字。", 332, 390, 292, 1);
            } else if (data->page == 2) {
                label(L"显示名称", 188, 166, 180, 2);
                label(L"写入文件名（留空则同左）", 400, 166, 234, 2);
                for (int i = 0; i < 3; ++i) {
                    int y = 194 + i * 36;
                    child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_CHOICE_NAME + i, 192, y, 184, 22, 2);
                    child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ND_CHOICE_VALUE + i, 394, y, 184, 22, 2);
                    child(L"BUTTON", L"×", WS_TABSTOP | BS_OWNERDRAW, ND_CHOICE_DELETE + i, 592, y - 2, 32,
                          26, 2);
                }
                child(L"BUTTON", L"＋ 常用选项", WS_TABSTOP | BS_OWNERDRAW, ND_CHOICE_ADD, 188, 306, 124, 28,
                      2);
                child(L"BUTTON", L"‹", WS_TABSTOP | BS_OWNERDRAW, ND_CHOICE_PREV, 470, 306, 36, 28, 2);
                child(L"STATIC", L"", SS_CENTER | SS_CENTERIMAGE, ND_CHOICE_PAGE, 510, 306, 72, 28, 2);
                child(L"BUTTON", L"›", WS_TABSTOP | BS_OWNERDRAW, ND_CHOICE_NEXT, 588, 306, 36, 28, 2);
            } else if (data->page == 7) {
                label(L"当以下部分写入指定文字时，才添加当前部分", 188, 166, 436, 7);
                combo(ND_CONDITION_FIELD, 188, 206, 204, {}, 7);
                combo(ND_CONDITION_VALUE, 408, 206, 216, {}, 7, true);
                label(L"选“始终添加”可关闭条件；不满足条件时不要求填写。", 188, 260, 436, 7);
            } else if (data->page == 3) {
                label(L"末尾自动编号", 20, 172, 280, 3);
                combo(ND_NUMBERING, 20, 196, 280, {L"关闭", L"自动版本", L"自动序号"}, 3);
                label(L"编号位数", 324, 172, 296, 3);
                combo(ND_DIGITS, 324, 196, 296, {L"2 位 · 01", L"3 位 · 001", L"4 位 · 0001"}, 3);
                child(L"BUTTON", L"版本使用大写 V", WS_TABSTOP | BS_AUTOCHECKBOX, ND_UPPER, 20, 250, 300, 24,
                      3);
            } else if (data->page == 4) {
                child(L"BUTTON", L"启用目录推荐", WS_TABSTOP | BS_AUTOCHECKBOX, ND_RECOMMEND, 20, 124, 280,
                      24, 4);
                label(L"关闭会保留规则；点击推荐才切换目录。", 304, 128, 316, 4);
                child(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 4700, 20,
                      164, 600, 114, 4);
                label(L"命名部分", 20, 298, 280, 4);
                label(L"写入文件名的文字等于", 324, 298, 296, 4);
                combo(4701, 20, 322, 280, {}, 4);
                combo(4702, 324, 322, 296, {}, 4, true);
                label(L"推荐文件夹 · 完整路径，或相对项目根目录的路径", 20, 366, 600, 4);
                child(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 4703, 24, 394, 476, 22, 4);
                SendDlgItemMessageW(window, 4703, EM_SETLIMITTEXT, 1024, 0);
                child(L"BUTTON", L"选择…", WS_TABSTOP | BS_OWNERDRAW, 4706, 520, 390, 100, 28, 4);
                child(L"BUTTON", L"新增一条", WS_TABSTOP | BS_OWNERDRAW, 4707, 20, 440, 116, 28, 4);
                child(L"BUTTON", L"清空 / 删除", WS_TABSTOP | BS_OWNERDRAW, 4705, 148, 440, 116, 28, 4);
                label(L"填写后直接保存方案，或新增下一条；最多 20 条。", 280, 446, 340, 4);
                child(L"BUTTON", L"保存前检查同名文件", WS_TABSTOP | BS_AUTOCHECKBOX, ND_WARN_EXISTING, 20,
                      494, 436, 24, 4);
                label(L"关闭后不检查、不显示提醒；原软件仍会确认覆盖。", 20, 528, 600, 4);
            } else if (data->page == 5) {
                child(L"BUTTON", L"作为全局默认方案", WS_TABSTOP | BS_AUTOCHECKBOX, ND_GLOBAL_DEFAULT, 20,
                      128, 600, 24, 5);
                label(L"没有单独关联的软件使用全局默认；修改在保存方案后生效。", 20, 154, 600, 5);
            }
            for (int id : {ND_ORDER_HINT, ND_SOURCE_HINT, ND_BASIC_HINT})
                SendMessageW(GetDlgItem(window, id), WM_SETFONT,
                             reinterpret_cast<WPARAM>(theme.compact->hintFont), FALSE);
            data->advancedPages[data->page] = true;
            if (data->page == 0 || data->page == 6)
                data->advancedPages[0] = data->advancedPages[6] = true;
        }
    }
    for (auto [id, text] : std::initializer_list<std::pair<int, const wchar_t *>>{
             {ND_ENABLED,
              L"控制整项是否参与命名。关闭会隐藏这项及依赖它的条件项，保留所有设置；重新勾选即可恢复。"},
             {ND_REQUIRED, L"勾选后，这项为空时会提示填写；取消勾选可留空，空白内容不会加入文件名。"},
             {ND_QUICK,
              L"只控制主面板是否直接显示输入项。取消后仍参与命名，可从“显示全部命名部分”访问；要完全关"
              L"闭请取消“启用此项”。"},
             {ND_VALIDATION,
              L"选择“关闭限制（允许中文）”可取消方案额外的字符限制。Windows "
              L"文件名不能包含的字符仍不可使用。数字补零也要求仅输入数字，允许文字时请同时将补零设为 0。"},
             {ND_WIDTH, L"填 0 关闭补零；填 1–8 时，这项只接受数字，并补齐对应位数，例如 1 → "
                        L"001。它与末尾自动编号是独立设置。"},
             {ND_CLEAR_FORMAT, L"一次关闭本页的输入限制、大小写转换、数字补零、独立连接符和前后固定文字。保留"
                               L"内容来源、必填、常用选项、出现条件等其他设置。"},
             {ND_RECOMMEND, L"控制这套方案的目录推荐。关闭后保留规则但不再显示推荐目录；重新开启可恢复。"},
             {ND_WARN_EXISTING, L"控制这套方案保存前的同名检查。关闭后不执行后台检查、不显示同名提醒；原软件"
                                L"的覆盖确认仍有效。"},
             {ND_COL_CONTENT, L"普通文字先预填默认内容；原名、项目和日期按来源自动填入。保存时手填或选择选项"
                              L"替换本次内容，不改方案默认。固定文字只读。"},
             {ND_SCHEME_MENU, L"方案操作：复制为新方案。副本不继承软件关联和全局默认。"},
             {ND_TOOLS_LABEL, L"点击配置目录推荐；没有规则时不会生成推荐。关闭后保留规则，可随时恢复。"},
             {ND_SCHEME,
              L"设置末尾自动编号的位数和版本字母大小写。真实保存时按目标目录续号，与手动编号部分独立。"}})
        designerTip(window, *data, id, text);
}
inline void designerApplicationSection(HWND window, NamingDesigner &data, bool visible) {
    data.applicationSettings = visible;
    designerLayout(window, data, false);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}
inline void designerSchemeMenu(HWND window, NamingDesigner &data) {
    auto menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ND_COPY, L"复制为新方案");
    RECT anchor{};
    GetWindowRect(GetDlgItem(window, ND_SCHEME_MENU), &anchor);
    std::vector<PopupMenuHint> hints{
        {menu, 0, L"复制当前草稿为新方案；不继承软件关联和全局默认。点击保存后才写入配置。"}};
    auto track = data.theme.compact ? data.theme.compact->popupMenu : nullptr;
    int command =
        track ? track(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, anchor.right, anchor.bottom, window, hints)
              : TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, anchor.right, anchor.bottom, 0, window,
                               nullptr);
    DestroyMenu(menu);
    if (command == ND_COPY)
        SendMessageW(window, WM_COMMAND, ND_COPY, 0);
}
inline void designerAddItem(HWND window, NamingDesigner &data) {
    StructuredNaming current;
    bool valid = readDesignerField(window, data, current);
    bool full = current.fields.size() >= 8;
    auto presets = designerContentPresets(*data.config);
    auto menu = CreatePopupMenu();
    auto contents = menu;
    std::vector<PopupMenuHint> hints;
    auto help = [&](HMENU target, std::wstring text) {
        hints.push_back({target, static_cast<UINT>(GetMenuItemCount(target) - 1), std::move(text)});
    };
    auto heading = L"命名内容 · " + std::to_wstring(current.fields.size()) + L" / 8";
    AppendMenuW(contents, MF_STRING | MF_GRAYED, 0, heading.c_str());
    help(contents, L"命名内容数量\n每个方案最多包含 8 "
                   L"项命名内容，按顺序组合成文件名。\n软件关联和文件名格式不占名额；已存在的内容可直接编辑。"
                   L"\n本次修改在点击“保存方案”后生效，取消则放弃修改。");
    auto fieldItem = [&](int id, const wchar_t *label) {
        bool present = designerExistingContent(current, id, presets) >= 0;
        auto text = std::wstring(label) + (present ? L" · 已有，点击编辑" : L"");
        AppendMenuW(contents, MF_STRING | (full && !present ? MF_GRAYED : 0), id, text.c_str());
        help(contents, designerContentHelp(id, current, presets));
    };
    fieldItem(NDA_PRIMARY, L"主要名称");
    fieldItem(NDA_PROJECT, L"项目名称");
    fieldItem(NDA_DATE, L"日期");
    AppendMenuW(contents, MF_SEPARATOR, 0, nullptr);
    fieldItem(NDA_TYPE, L"文件类别");
    fieldItem(NDA_DETAIL, L"补充说明");
    fieldItem(NDA_STATUS, L"当前状态");
    fieldItem(NDA_MANUAL_NUMBER, L"手动编号");
    AppendMenuW(contents, MF_SEPARATOR, 0, nullptr);
    fieldItem(NDA_FIXED, L"固定文字…");
    fieldItem(NDA_CUSTOM, L"自定义内容…");
    RECT anchor{};
    GetWindowRect(GetDlgItem(window, ND_ADD), &anchor);
    auto track = data.theme.compact ? data.theme.compact->popupMenu : nullptr;
    int command =
        track ? track(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, anchor.left, anchor.bottom, window, hints)
              : TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, anchor.left, anchor.bottom, 0, window,
                               nullptr);
    DestroyMenu(menu);
    if (!command)
        return;
    if (command == NDA_APPLICATION) {
        bool present = data.applicationSettings;
        designerApplicationSection(window, data, true);
        if (!present)
            designerAddApplication(window, data);
        designerFocus(window, data.applications.empty() ? ND_APPLICATION_ADD : ND_APPLICATIONS);
        return;
    }
    if (!valid) {
        designerSet(window, ND_ERROR, data.error);
        return;
    }
    if (command == NDA_LOCATIONS) {
        data.draft = std::move(current);
        if (editNamingLocations(window, data.draft, data.theme))
            designerLoad(window, data);
        return;
    }
    int page = -1, index = data.selected, targetFocus = ND_LABEL;
    if (command >= NDA_FIELD_BASE && command < NDA_FIELD_BASE + static_cast<int>(current.fields.size())) {
        page = 0;
        index = command - NDA_FIELD_BASE;
    }
    int existing = designerExistingContent(current, command, presets);
    if (existing >= 0) {
        page = 0;
        index = existing;
    }
    if (command == NDA_FORMAT) {
        page = 3;
        targetFocus = ND_NUMBERING;
    }
    if (command == NDA_ORDER) {
        page = 0;
        targetFocus = ND_FIELDS;
    }
    if (page >= 0) {
        data.draft = std::move(current);
        data.selected = index;
        if (page == 3) {
            if (data.page < 3)
                data.fieldPage = data.page;
        } else
            data.fieldPage = page;
        data.page = page;
        data.advanced = true;
        designerLoad(window, data);
        designerFocus(window, targetFocus);
        return;
    }
    if (command == NDA_COPY) {
        SendMessageW(window, WM_COMMAND, ND_COPY, 0);
        return;
    }
    if (full)
        return;
    NamingField field;
    field.defaultSource = 0;
    if (command == NDA_PRIMARY) {
        field.label = L"主要名称";
        field.required = true;
    } else if (command == NDA_PROJECT) {
        field.label = L"项目名称";
        field.source = field.defaultSource = 1;
        field.quick = false;
    } else if (command == NDA_DATE) {
        field.label = L"日期";
        field.source = field.defaultSource = 2;
        field.quick = false;
    } else if (auto preset = std::find_if(presets.begin(), presets.end(),
                                          [&](const auto &item) { return item.first == command; });
               preset != presets.end())
        field = preset->second;
    else if (command == NDA_FIXED || command == NDA_CUSTOM) {
        bool fixed = command == NDA_FIXED;
        field.label = fixed ? L"固定文字" : L"自定义内容";
        if (fixed) {
            field.source = field.defaultSource = 3;
            field.quick = false;
        }
    } else
        return;
    for (int i = 1;; ++i) {
        field.id = L"field" + std::to_wstring(i);
        if (std::none_of(current.fields.begin(), current.fields.end(),
                         [&](const auto &f) { return f.id == field.id; }))
            break;
    }
    if (command == NDA_PRIMARY)
        current.primaryField = field.id;
    current.fields.push_back(std::move(field));
    data.draft = std::move(current);
    data.selected = static_cast<int>(data.draft.fields.size() - 1);
    data.page = data.fieldPage = 0;
    data.advanced = command == NDA_CUSTOM;
    data.page = 0;
    designerLoad(window, data);
    const auto &added = data.draft.fields[data.selected];
    bool choices =
        !added.candidates.empty() && added.defaultSource != 3 && added.id != data.draft.primaryField;
    int focus = data.advanced ? ND_LABEL
                : command == NDA_PROJECT || command == NDA_DATE
                    ? ND_ADD
                    : (choices ? ND_SIMPLE_VALUE : ND_SIMPLE_TEXT) + data.selected;
    designerFocus(window, focus);
    if (focus == ND_SIMPLE_TEXT + data.selected)
        SendDlgItemMessageW(window, focus, EM_SETSEL, 0, -1);
}
inline INT_PTR CALLBACK namingDesignerProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto *data = reinterpret_cast<NamingDesigner *>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        data = reinterpret_cast<NamingDesigner *>(lp);
        SetWindowLongPtrW(window, DWLP_USER, lp);
        data->loading = true;
        auto &theme = data->theme;
        theme.dpi = static_cast<int>(GetDpiForWindow(window));
        BOOL dark = TRUE;
        DWORD corner = 2;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        DwmSetWindowAttribute(window, 33, &corner, sizeof(corner));
        DwmSetWindowAttribute(window, 35, &theme.background, sizeof(COLORREF));
        DwmSetWindowAttribute(window, 36, &theme.foreground, sizeof(COLORREF));
        data->tooltip =
            CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                            0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(data->tooltip, TTM_SETMAXTIPWIDTH, 0, MulDiv(440, theme.dpi, 96));
        SendMessageW(data->tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 450);
        data->loading = false;
        designerLoad(window, *data);
        SendMessageW(window, DM_SETDEFID, IDOK, 0);
        return TRUE;
    }
    if (!data)
        return FALSE;
    if (message == WM_SIZE || message == WM_SETTINGS_VIEWPORT_CHANGED) {
        designerFooter(window, *data);
        return TRUE;
    }
    if (message == WM_NAMING_DESIGNER_DPI) {
        const auto &fonts = *reinterpret_cast<const NamingDesignerDpi *>(lp);
        int before = data->theme.dpi;
        auto oldHint = data->style.hintFont;
        resetSettingsScroll(window);
        data->theme.dpi = fonts.dpi;
        data->theme.font = fonts.font;
        data->style.hintFont = fonts.hint;
        for (auto child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            RECT r{};
            GetWindowRect(child, &r);
            MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
            MoveWindow(child, MulDiv(r.left, fonts.dpi, before), MulDiv(r.top, fonts.dpi, before),
                       MulDiv(r.right - r.left, fonts.dpi, before),
                       MulDiv(r.bottom - r.top, fonts.dpi, before), FALSE);
            auto current = reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0));
            SendMessageW(child, WM_SETFONT,
                         reinterpret_cast<WPARAM>(current == oldHint ? fonts.hint : fonts.font), FALSE);
            wchar_t cls[32]{};
            GetClassNameW(child, cls, 32);
            if (!lstrcmpiW(cls, L"ComboBox")) {
                SendMessageW(child, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), MulDiv(26, fonts.dpi, 96));
                SendMessageW(child, CB_SETITEMHEIGHT, 0, MulDiv(28, fonts.dpi, 96));
            }
        }
        designerLayout(window, *data);
        RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
        return TRUE;
    }
    if ((message == WM_VSCROLL || message == WM_MOUSEWHEEL) && data->scrollMax) {
        int dest = data->scroll, step = MulDiv(28, data->theme.dpi, 96);
        if (message == WM_MOUSEWHEEL)
            dest -= GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * step * 3;
        else {
            SCROLLINFO scroll{sizeof(scroll), SIF_ALL};
            GetScrollInfo(window, SB_VERT, &scroll);
            switch (LOWORD(wp)) {
            case SB_LINEUP:
                dest -= step;
                break;
            case SB_LINEDOWN:
                dest += step;
                break;
            case SB_PAGEUP:
                dest -= scroll.nPage;
                break;
            case SB_PAGEDOWN:
                dest += scroll.nPage;
                break;
            case SB_THUMBTRACK:
                dest = scroll.nTrackPos;
                break;
            case SB_TOP:
                dest = 0;
                break;
            case SB_BOTTOM:
                dest = data->scrollMax;
                break;
            }
        }
        dest = std::clamp(dest, 0, data->scrollMax);
        int delta = data->scroll - dest;
        data->scroll = dest;
        ScrollWindowEx(window, 0, delta, nullptr, nullptr, nullptr, nullptr,
                       SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        SetScrollPos(window, SB_VERT, dest, TRUE);
        designerFooter(window, *data);
        return TRUE;
    }
    if (message == WM_DRAWITEM) {
        auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        auto &theme = data->theme;
        auto &style = *theme.compact;
        auto px = [&](int v) { return MulDiv(v, theme.dpi, 96); };
        if (item.CtlType == ODT_BUTTON) {
            bool active = (item.CtlID == ND_SIMULATE && data->advanced && data->page == 8) ||
                          (item.CtlID == ND_SECTION_APPS && data->advanced && data->page == 5) ||
                          (item.CtlID == ND_BEHAVIOR && data->page == 6) ||
                          (item.CtlID == ND_CONDITION && data->page == 7) ||
                          data->advanced && ((item.CtlID >= ND_BASE && item.CtlID <= ND_CHOICES &&
                                              item.CtlID == static_cast<UINT>(ND_BASE + data->page)) ||
                                             (item.CtlID == ND_SCHEME && data->page == 3));
            auto colors = style;
            auto paint = theme;
            HBRUSH brush = nullptr;
            if (active) {
                paint.background = style.surface;
                brush = CreateSolidBrush(paint.background);
                paint.brush = brush;
                colors.border = style.surface;
            }
            if (item.itemState & ODS_DISABLED) {
                colors.border = theme.background;
                paint.foreground = style.muted;
            }
            bool order = item.CtlID >= ND_ROW_UP && item.CtlID < ND_ROW_UP + 8 ||
                         item.CtlID >= ND_ROW_DOWN && item.CtlID < ND_ROW_DOWN + 8 ||
                         item.CtlID >= ND_SIMPLE_REMOVE && item.CtlID < ND_SIMPLE_REMOVE + 8;
            bool heading = item.CtlID == ND_SECTION_FIELDS && !data->advanced;
            if (heading) {
                FillRect(item.hDC, &item.rcItem, theme.brush);
                auto before = SelectObject(item.hDC, theme.font);
                SetTextColor(item.hDC, theme.foreground);
                SetBkMode(item.hDC, TRANSPARENT);
                auto r = item.rcItem;
                auto title = designerText(window, ND_SECTION_FIELDS);
                DrawTextW(item.hDC, title.c_str(), -1, &r,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                SelectObject(item.hDC, before);
                if (brush)
                    DeleteObject(brush);
                return TRUE;
            }
            if (order) {
                colors.border = theme.background;
                paint.foreground = style.muted;
            }
            if (item.CtlID == ND_SIMULATE) {
                paint.background = style.inputBackground;
                paint.brush = style.inputBrush;
            }
            if (item.CtlID == ND_ADD) {
                colors.border = style.accent;
                paint.foreground = style.accent;
            }
            paint.compact = &colors;
            drawCompactFormButton(item, paint, item.CtlID == IDOK);
            if (active) {
                auto pen = CreatePen(PS_SOLID, px(2), style.accent);
                auto old = SelectObject(item.hDC, pen);
                MoveToEx(item.hDC, item.rcItem.left + px(12), item.rcItem.bottom - px(2), nullptr);
                LineTo(item.hDC, item.rcItem.right - px(12), item.rcItem.bottom - px(2));
                SelectObject(item.hDC, old);
                DeleteObject(pen);
            }
            if (brush)
                DeleteObject(brush);
            return TRUE;
        }
        if (item.CtlType == ODT_STATIC && item.CtlID == ND_PREVIEW) {
            FillRect(item.hDC, &item.rcItem, style.inputBrush);
            auto old = SelectObject(item.hDC, theme.font);
            SetBkMode(item.hDC, TRANSPARENT);
            SetTextColor(item.hDC, theme.foreground);
            auto text = designerText(window, ND_PREVIEW);
            RECT rect = item.rcItem;
            DrawTextW(item.hDC, text.c_str(), -1, &rect,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            if (data->advanced && data->page < 3) {
                for (size_t i = 0; i < data->previewFields.size(); ++i)
                    if (data->previewFields[i] == static_cast<size_t>(data->selected)) {
                        auto [start, length] = data->previewMatches[i];
                        if (start + length > text.size())
                            break;
                        SIZE before{}, part{};
                        GetTextExtentPoint32W(item.hDC, text.c_str(), static_cast<int>(start), &before);
                        GetTextExtentPoint32W(item.hDC, text.c_str() + start, static_cast<int>(length),
                                              &part);
                        SIZE total{};
                        GetTextExtentPoint32W(item.hDC, text.c_str(), static_cast<int>(text.size()), &total);
                        if (before.cx + part.cx <=
                            rect.right - rect.left - (total.cx > rect.right - rect.left ? px(20) : 0)) {
                            RECT mark = rect;
                            mark.left += before.cx;
                            mark.right = mark.left + part.cx;
                            SetTextColor(item.hDC, style.accent);
                            auto value = text.substr(start, length);
                            DrawTextW(item.hDC, value.c_str(), -1, &mark,
                                      DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                        }
                        break;
                    }
            }
            SelectObject(item.hDC, old);
            return TRUE;
        }
        if (item.CtlType == ODT_LISTBOX) {
            bool selected = (item.itemState & ODS_SELECTED) && data->page < 3;
            auto brush = CreateSolidBrush(selected ? style.surface : style.inputBackground);
            FillRect(item.hDC, &item.rcItem, brush);
            DeleteObject(brush);
            if (item.itemID < data->draft.fields.size()) {
                auto old = SelectObject(item.hDC, theme.font);
                SetBkMode(item.hDC, TRANSPARENT);
                SetTextColor(item.hDC, selected ? theme.foreground : style.muted);
                const auto &schema = data->previewSchema.fields.size() == data->draft.fields.size()
                                         ? data->previewSchema
                                         : data->draft;
                const auto &field = schema.fields[item.itemID];
                RECT r = item.rcItem;
                r.left += px(28);
                r.right -= px(6);
                DrawTextW(item.hDC, field.label.c_str(), -1, &r,
                          DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
                SelectObject(item.hDC, style.hintFont);
                SetTextColor(item.hDC, style.muted);
                auto number = std::to_wstring(item.itemID + 1);
                RECT num = item.rcItem;
                num.left += px(8);
                num.right = num.left + px(16);
                DrawTextW(item.hDC, number.c_str(), -1, &num, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
                if (selected) {
                    RECT edge = item.rcItem;
                    edge.right = edge.left + px(2);
                    auto accent = CreateSolidBrush(style.accent);
                    FillRect(item.hDC, &edge, accent);
                    DeleteObject(accent);
                }
                if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT))
                    DrawFocusRect(item.hDC, &item.rcItem);
                SelectObject(item.hDC, old);
            }
            return TRUE;
        }
    }
    if (message == WM_PAINT || message == WM_PRINT || message == WM_PRINTCLIENT) {
        bool printing = message != WM_PAINT;
        PAINTSTRUCT paint{};
        auto dc = printing ? reinterpret_cast<HDC>(wp) : BeginPaint(window, &paint);
        if (printing)
            GetClientRect(window, &paint.rcPaint);
        auto &theme = data->theme;
        FillRect(dc, &paint.rcPaint, theme.brush);
        auto &style = *theme.compact;
        auto pen = CreatePen(PS_SOLID, 1, style.border);
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, style.inputBrush);
        RECT client{};
        GetClientRect(window, &client);
        int bodyBottom = std::max(0L, client.bottom - MulDiv(52, theme.dpi, 96));
        int clip = SaveDC(dc);
        IntersectClipRect(dc, 0, 0, client.right, bodyBottom);
        {
            auto r = data->previewFrame;
            int scroll = data->scroll;
            if (auto host = settingsDialog(window))
                scroll += host->scroll;
            OffsetRect(&r, 0, -scroll);
            int radius = MulDiv(10, theme.dpi, 96);
            style.roundedRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        }
        int scroll = data->scroll;
        if (auto host = settingsDialog(window))
            scroll += host->scroll;
        if (!data->advanced) {
            auto table = data->tableFrame;
            OffsetRect(&table, 0, -scroll);
            auto header = data->tableHeader;
            OffsetRect(&header, 0, -scroll);
            auto fill = CreateSolidBrush(style.surface);
            FillRect(dc, &header, fill);
            DeleteObject(fill);
            auto hollow = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, table.left, table.top, table.right, table.bottom);
            SelectObject(dc, hollow);
            auto format = data->formatFrame;
            OffsetRect(&format, 0, -scroll);
            hollow = SelectObject(dc, GetStockObject(NULL_BRUSH));
            style.roundedRect(dc, format.left, format.top, format.right, format.bottom,
                              MulDiv(6, theme.dpi, 96), MulDiv(6, theme.dpi, 96));
            SelectObject(dc, hollow);
        }
        for (auto r : data->rowFrames) {
            OffsetRect(&r, 0, -scroll);
            MoveToEx(dc, r.left, r.bottom, nullptr);
            LineTo(dc, r.right, r.bottom);
        }
        for (auto control : data->frames)
            if (IsWindowVisible(control)) {
                RECT r{};
                GetWindowRect(control, &r);
                MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
                InflateRect(&r, MulDiv(4, theme.dpi, 96), MulDiv(4, theme.dpi, 96));
                int radius = MulDiv(6, theme.dpi, 96);
                style.roundedRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
            }
        RestoreDC(dc, clip);
        MoveToEx(dc, MulDiv(20, theme.dpi, 96), bodyBottom, nullptr);
        LineTo(dc, MulDiv(620, theme.dpi, 96), bodyBottom);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
        if (printing && (lp & PRF_CHILDREN)) {
            auto printControl = [&](HWND child, int bottom) {
                if (!child || !IsWindowVisible(child))
                    return;
                RECT r{};
                GetWindowRect(child, &r);
                MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&r), 2);
                int saved = SaveDC(dc);
                IntersectClipRect(dc, 0, 0, client.right, bottom);
                OffsetViewportOrgEx(dc, r.left, r.top, nullptr);
                IntersectClipRect(dc, 0, 0, r.right - r.left, r.bottom - r.top);
                SendMessageW(child, WM_PRINT, reinterpret_cast<WPARAM>(dc),
                             PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
                RestoreDC(dc, saved);
            };
            auto first = GetWindow(window, GW_CHILD);
            for (auto child = first ? GetWindow(first, GW_HWNDLAST) : nullptr; child;
                 child = GetWindow(child, GW_HWNDPREV)) {
                if (!designerFooterControl(GetDlgCtrlID(child)))
                    printControl(child, bodyBottom);
            }
            RECT footer{0, bodyBottom, client.right, client.bottom};
            FillRect(dc, &footer, theme.brush);
            auto line = CreatePen(PS_SOLID, 1, style.border);
            auto previous = SelectObject(dc, line);
            MoveToEx(dc, MulDiv(20, theme.dpi, 96), bodyBottom, nullptr);
            LineTo(dc, MulDiv(620, theme.dpi, 96), bodyBottom);
            SelectObject(dc, previous);
            DeleteObject(line);
            for (int id : std::initializer_list<int>{ND_DIRTY, ND_FIX, IDCANCEL, IDOK})
                printControl(GetDlgItem(window, id), client.bottom);
        }
        if (!printing)
            EndPaint(window, &paint);
        return TRUE;
    }
    if (message == WM_SETCURSOR && reinterpret_cast<HWND>(wp) == GetDlgItem(window, ND_TOOLS_LABEL)) {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    }
    if (message == WM_CTLCOLORDLG || message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT ||
        message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLORBTN) {
        auto &theme = data->theme;
        auto &style = *theme.compact;
        int id = GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        bool input = message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX || id == ND_PREVIEW ||
                     id == ND_PREVIEW_LABEL || id == ND_RESULT_LABEL || id == ND_PREVIEW_NOTE ||
                     id == ND_TEST_ARROW;
        bool column = id == ND_COL_PART || id == ND_COL_CONTENT || id == ND_COL_ACTIONS;
        bool emphasized = id == ND_PREVIEW || (id >= ND_SIMPLE_LABEL && id < ND_SIMPLE_LABEL + 8) ||
                          (id >= ND_SIMPLE_AUTO && id < ND_SIMPLE_AUTO + 8);
        SetTextColor(reinterpret_cast<HDC>(wp),
                     message == WM_CTLCOLORSTATIC && !emphasized ? style.muted : theme.foreground);
        if (column) {
            auto dc = reinterpret_cast<HDC>(wp);
            SetBkMode(dc, OPAQUE);
            SetBkColor(dc, style.surface);
            SetDCBrushColor(dc, style.surface);
            return reinterpret_cast<INT_PTR>(GetStockObject(DC_BRUSH));
        }
        SetBkColor(reinterpret_cast<HDC>(wp), input ? style.inputBackground : theme.background);
        return reinterpret_cast<INT_PTR>(input ? style.inputBrush : theme.brush);
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        finishSettingsDialog(window, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND && !data->loading) {
        int id = LOWORD(wp), code = HIWORD(wp);
        StructuredNaming next;
        if (id == ND_ADOPT || id == ND_RESTORE) {
            if (id == ND_RESTORE && data->example != 0) {
                SendDlgItemMessageW(window, ND_TEMPLATE, CB_SETCURSEL, 0, 0);
                SendMessageW(window, WM_COMMAND, MAKEWPARAM(ND_TEMPLATE, CBN_SELCHANGE),
                             reinterpret_cast<LPARAM>(GetDlgItem(window, ND_TEMPLATE)));
                if (data->example != 0)
                    return TRUE;
            }
            data->adoptedExample = data->example;
            data->advanced = false;
            data->page = 0;
            designerLoad(window, *data);
            designerFocus(window, ND_NAME);
            return TRUE;
        }
        if (id == ND_RESET_SIM) {
            data->simulated.clear();
            designerLoadSimulation(window, *data);
            designerPreview(window, *data);
            return TRUE;
        }
        if (id >= ND_SIM_VALUE && id < ND_SIM_VALUE + 8 &&
            (code == CBN_EDITCHANGE || code == CBN_SELCHANGE)) {
            size_t index = id - ND_SIM_VALUE;
            if (index >= data->draft.fields.size())
                return TRUE;
            const auto &field = data->draft.fields[index];
            auto value = designerText(window, id);
            int choice = designerChoice(window, id);
            if (code == CBN_SELCHANGE && choice >= 0 && static_cast<size_t>(choice) < field.candidates.size())
                value = field.candidates[choice].value;
            data->simulated[field.id] = value;
            if (code == CBN_SELCHANGE) {
                data->loading = true;
                designerSet(window, id, value);
                data->loading = false;
            }
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_FIX) {
            if (data->errorControl == ND_QUICK_SEPARATOR) {
                if (!data->advanced) {
                    designerFocus(window, ND_QUICK_SEPARATOR);
                    return TRUE;
                }
                if (readDesignerField(window, *data, next, -1, true))
                    data->draft = std::move(next);
                data->advanced = false;
                data->page = 0;
                designerLoad(window, *data);
                designerFocus(window, ND_QUICK_SEPARATOR);
                return TRUE;
            }
            // A correction in the current task must keep the invalid text so the
            // user can repair it, rather than silently reverting to the draft.
            if (data->advanced && data->page == data->errorPage) {
                designerFocus(window, data->errorControl);
                return TRUE;
            }
            if (data->errorControl >= ND_SIM_VALUE && data->errorControl < ND_SIM_VALUE + 8 ||
                data->errorControl == ND_EXAMPLE) {
                if (!data->advanced || data->page != 8) {
                    if (readDesignerField(window, *data, next, -1, true)) {
                        data->draft = std::move(next);
                        data->page = 8;
                        data->advanced = true;
                        designerLoad(window, *data);
                    }
                }
                designerFocus(window, data->errorControl);
                return TRUE;
            }
            if (data->errorField >= 0)
                data->selected = data->errorField;
            if (readDesignerField(window, *data, next, -1, true))
                data->draft = std::move(next);
            data->advanced = true;
            data->page = data->errorPage;
            designerLoad(window, *data);
            designerFocus(window, data->errorControl ? data->errorControl : ND_LABEL);
            return TRUE;
        }
        bool rowUp = id >= ND_ROW_UP && id < ND_ROW_UP + 8,
             rowDown = id >= ND_ROW_DOWN && id < ND_ROW_DOWN + 8;
        if ((rowUp || rowDown) && !data->advanced) {
            int index = id - (rowUp ? ND_ROW_UP : ND_ROW_DOWN), dest = index + (rowUp ? -1 : 1);
            if (dest >= 0 && static_cast<size_t>(dest) < data->draft.fields.size() &&
                readDesignerField(window, *data, next, -1, true)) {
                std::swap(next.fields[index], next.fields[dest]);
                data->draft = std::move(next);
                data->selected = dest;
                designerLoad(window, *data);
                designerFocus(window, (rowUp ? ND_ROW_UP : ND_ROW_DOWN) + dest);
            }
            return TRUE;
        }
        if (id == ND_SCHEME_MENU) {
            designerSchemeMenu(window, *data);
            return TRUE;
        }
        if (id == ND_TRIAL) {
            if (!readDesignerField(window, *data, next, -1, true)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            if (!data->trialInitialized) {
                data->trialInput = namingTrialExamples(data->originalName);
                data->trialInitialized = true;
            }
            auto target = showNamingTrial(window, next, *data->config, data->theme, data->trialInput,
                                          data->originalName);
            if (target.first >= 0 && static_cast<size_t>(target.first) < next.fields.size()) {
                data->draft = std::move(next);
                data->selected = target.first;
                data->page = data->fieldPage = target.second;
                data->advanced = true;
                designerLoad(window, *data);
                designerFocus(window, target.second == 1 ? ND_VALIDATION : ND_DEFAULT);
            }
            return TRUE;
        }
        bool rowDate = id >= ND_ROW_DATE && id < ND_ROW_DATE + 8;
        if (rowDate || id >= ND_SIMPLE_SETTINGS && id < ND_SIMPLE_SETTINGS + 8) {
            int index = id - (rowDate ? ND_ROW_DATE : ND_SIMPLE_SETTINGS);
            if (data->advanced || static_cast<size_t>(index) >= data->draft.fields.size())
                return TRUE;
            if (!readDesignerField(window, *data, next, -1, true)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            data->draft = std::move(next);
            data->selected = index;
            data->page = 0;
            data->advanced = true;
            designerLoad(window, *data);
            designerFocus(window, rowDate ? ND_DATE_FORMAT : ND_LABEL);
            return TRUE;
        }
        if (id == ND_CLEAR_FORMAT) {
            data->loading = true;
            for (int control : {ND_CASING, ND_VALIDATION})
                SendDlgItemMessageW(window, control, CB_SETCURSEL, 0, 0);
            designerSet(window, ND_WIDTH, L"0");
            CheckDlgButton(window, ND_OWN_JOINER, BST_UNCHECKED);
            for (int control : {ND_JOINER, ND_PREFIX, ND_SUFFIX})
                designerSet(window, control, L"");
            EnableWindow(GetDlgItem(window, ND_JOINER), FALSE);
            data->loading = false;
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_SECTION_FIELDS || id == ND_SIMULATE || id == ND_TEMPLATES ||
            id == ND_SECTION_LOCATIONS || id == ND_TOOLS_LABEL || id == ND_SECTION_APPS ||
            id == ND_EDIT_LOCATIONS || id == ND_EDIT_APPLICATIONS) {
            if (!readDesignerField(window, *data, next, -1, true)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            data->draft = std::move(next);
            if (data->page < 3)
                data->fieldPage = data->page;
            data->page = id == ND_SECTION_FIELDS ? 0
                         : id == ND_SIMULATE     ? 8
                         : id == ND_TEMPLATES    ? 9
                         : (id == ND_SECTION_LOCATIONS || id == ND_TOOLS_LABEL || id == ND_EDIT_LOCATIONS)
                             ? 4
                             : 5;
            data->advanced = id != ND_SECTION_FIELDS;
            data->locationSelected = -1;
            designerLoad(window, *data);
            return TRUE;
        }
        if (id == ND_GLOBAL_DEFAULT) {
            data->globalDefault = designerChecked(window, id);
            designerUsageSummary(window, *data);
            designerPreview(window, *data);
            return TRUE;
        }
        if (designerChoicesCommand(window, *data, id, code)) {
            designerPreview(window, *data);
            return TRUE;
        }
        if (data->advanced && data->page == 4 && designerLocationsCommand(window, *data, id, code)) {
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_ADD && code == BN_CLICKED) {
            designerAddItem(window, *data);
            return TRUE;
        }
        if (id == ND_APPLICATION_DELETE && code == BN_CLICKED) {
            data->applications.clear();
            data->applicationNotice.clear();
            designerLoadApplications(window, *data);
            designerApplicationSection(window, *data, false);
            designerUsageSummary(window, *data);
            designerPreview(window, *data);
            designerFocus(window, ND_APPLICATION_ADD);
            return TRUE;
        }
        if (id == ND_APPLICATION_ADD && code == BN_CLICKED) {
            designerAddApplication(window, *data);
            designerUsageSummary(window, *data);
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_APPLICATION_REMOVE && code == BN_CLICKED) {
            int selected = designerChoice(window, ND_APPLICATIONS);
            if (selected >= 0 && static_cast<size_t>(selected) < data->applications.size()) {
                data->applications.erase(data->applications.begin() + selected);
                data->applicationNotice = L"移除后该软件跟随全局默认，保存方案后生效。";
                designerLoadApplications(window, *data, selected);
            }
            designerUsageSummary(window, *data);
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_APPLICATIONS && code == CBN_SELCHANGE) {
            data->applicationNotice.clear();
            designerApplicationHint(window, *data);
            return TRUE;
        }
        if (id == IDOK) {
            if (data->example != data->adoptedExample) {
                designerSet(window, ND_ERROR, L"正在试用示例，请先采用或返回原方案草稿。");
                return TRUE;
            }
            if (!readDesignerField(window, *data, next)) {
                designerPreview(window, *data);
                return TRUE;
            }
            data->draft = std::move(next);
            finishSettingsDialog(window, IDOK);
            return TRUE;
        }
        if (id == ND_ADVANCED) {
            if (!data->advanced)
                return TRUE;
            if (!readDesignerField(window, *data, next, -1, true)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            data->draft = std::move(next);
            data->advanced = false;
            data->page = 0;
            designerLoad(window, *data);
            designerFocus(window, ND_ADD);
            return TRUE;
        }
        if (id == ND_ORIGINAL && code == BN_CLICKED) {
            const auto &field = data->draft.fields[data->selected];
            auto found = std::find(data->primaryFields.begin(), data->primaryFields.end(), field.id);
            int current = designerChoice(window, ND_PRIMARY);
            if (designerChecked(window, ND_ORIGINAL)) {
                if (designerChoice(window, ND_SOURCE) != 0)
                    return TRUE;
                if (found == data->primaryFields.end()) {
                    data->primaryFields.push_back(field.id);
                    SendDlgItemMessageW(window, ND_PRIMARY, CB_ADDSTRING, 0,
                                        reinterpret_cast<LPARAM>(field.label.c_str()));
                    current = static_cast<int>(data->primaryFields.size() - 1);
                } else
                    current = static_cast<int>(found - data->primaryFields.begin());
            } else if (current >= 0 && static_cast<size_t>(current) < data->primaryFields.size() &&
                       data->primaryFields[current] == field.id)
                current = 0;
            SendDlgItemMessageW(window, ND_PRIMARY, CB_SETCURSEL, current, 0);
            designerSourceUI(window, *data);
            designerPreview(window, *data);
            return TRUE;
        }
        if (id == ND_CANDIDATES || (id >= ND_SIMPLE_CHOICES && id < ND_SIMPLE_CHOICES + 8)) {
            if (!readDesignerField(window, *data, next)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            data->draft = std::move(next);
            if (id != ND_CANDIDATES)
                data->selected = id - ND_SIMPLE_CHOICES;
            data->advanced = true;
            data->page = data->fieldPage = 2;
            designerLoad(window, *data);
            return TRUE;
        }
        bool simpleRemove = id >= ND_SIMPLE_REMOVE && id < ND_SIMPLE_REMOVE + 8;
        if ((simpleRemove || id == ND_REMOVE) && code == BN_CLICKED) {
            int index = simpleRemove ? id - ND_SIMPLE_REMOVE : data->selected;
            if ((simpleRemove && data->advanced) || data->draft.fields.size() <= 1 || index < 0 ||
                static_cast<size_t>(index) >= data->draft.fields.size())
                return TRUE;
            if (!readDesignerField(window, *data, next, index)) {
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            data->draft = std::move(next);
            if (data->selected > index)
                --data->selected;
            data->selected = std::min(data->selected, static_cast<int>(data->draft.fields.size() - 1));
            designerLoad(window, *data);
            int focus =
                data->advanced ? ND_FIELDS
                : data->draft.fields.size() > 1
                    ? ND_SIMPLE_REMOVE + std::min(index, static_cast<int>(data->draft.fields.size() - 1))
                    : ND_NAME;
            designerFocus(window, focus);
            return TRUE;
        }
        bool simpleCombo = id >= ND_SIMPLE_VALUE && id < ND_SIMPLE_VALUE + 8;
        bool simpleText = id >= ND_SIMPLE_TEXT && id < ND_SIMPLE_TEXT + 8;
        if ((simpleCombo || simpleText) &&
            (code == CBN_EDITCHANGE || code == CBN_SELCHANGE || code == EN_CHANGE)) {
            size_t index = id - (simpleText ? ND_SIMPLE_TEXT : ND_SIMPLE_VALUE);
            if (simpleCombo && code == CBN_SELCHANGE) {
                auto combo = GetDlgItem(window, id);
                int selected = designerChoice(window, id);
                if (selected >= 0) {
                    wchar_t value[256]{};
                    SendMessageW(combo, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(value));
                    data->loading = true;
                    designerSet(window, id, value);
                    data->loading = false;
                }
            }
            if (index < data->draft.fields.size() &&
                data->draft.fields[index].id == data->draft.primaryField) {
                data->loading = true;
                designerSet(window, ND_EXAMPLE, designerText(window, id));
                data->loading = false;
            }
        }
        bool fieldChange = id == ND_FIELDS && code == LBN_SELCHANGE;
        bool pageChange = ((id >= ND_BASE && id <= ND_SCHEME) || id == ND_BEHAVIOR || id == ND_CONDITION) &&
                          code == BN_CLICKED;
        bool operation = id >= ND_UP && id <= ND_COPY && code == BN_CLICKED;
        if (fieldChange || pageChange || operation) {
            if (!readDesignerField(window, *data, next, -1, true)) {
                designerSet(window, ND_ERROR, data->error);
                SendDlgItemMessageW(window, ND_FIELDS, LB_SETCURSEL, data->selected, 0);
                return TRUE;
            }
            data->draft = std::move(next);
            if (fieldChange) {
                data->selected = static_cast<int>(SendDlgItemMessageW(window, ND_FIELDS, LB_GETCURSEL, 0, 0));
                if (data->page >= 3)
                    data->page = data->fieldPage;
            }
            if (pageChange) {
                if (id == ND_SCHEME) {
                    if (data->page < 3)
                        data->fieldPage = data->page;
                    data->page = 3;
                } else
                    data->page = data->fieldPage = id == ND_BEHAVIOR    ? 6
                                                   : id == ND_CONDITION ? 7
                                                                        : id - ND_BASE;
                data->advanced = true;
            }
            if ((id == ND_UP && data->selected > 0) ||
                (id == ND_DOWN && static_cast<size_t>(data->selected + 1) < data->draft.fields.size())) {
                int dest = data->selected + (id == ND_UP ? -1 : 1);
                std::swap(data->draft.fields[data->selected], data->draft.fields[dest]);
                data->selected = dest;
            }
            if (id == ND_COPY) {
                data->draft.id = newNamingIdentity();
                data->draft.name += L" 副本";
                data->applications.clear();
                data->applicationNotice.clear();
                data->applicationSettings = false;
                data->globalDefault = false;
                data->advanced = false;
                data->page = 0;
                data->example = data->adoptedExample = 0;
                data->examples[0] = {};
            }
            designerLoad(window, *data);
            if (id == ND_COPY) {
                designerFocus(window, ND_NAME);
                SendDlgItemMessageW(window, ND_NAME, EM_SETSEL, 0, -1);
            }
            return TRUE;
        }
        if (id == ND_TEMPLATE && code == CBN_SELCHANGE) {
            int selected = designerChoice(window, id);
            if (selected < 0 || static_cast<size_t>(selected) >= data->examples.size() ||
                selected == data->example)
                return TRUE;
            if (!readDesignerField(window, *data, next, -1, true)) {
                SendDlgItemMessageW(window, id, CB_SETCURSEL, data->example, 0);
                designerSet(window, ND_ERROR, data->error);
                return TRUE;
            }
            // Keep each example's edits and identity for the lifetime of this dialog.
            auto original = designerText(window, ND_EXAMPLE);
            data->examples[data->example] = {
                std::move(next),    data->simulated, original,           data->selected,
                data->page,         data->fieldPage, data->applications, data->applicationSettings,
                data->globalDefault};
            auto &target = data->examples[selected];
            if (target.draft.fields.empty()) {
                target.draft = *data->templates[selected - 1];
                target.draft.id = newNamingIdentity();
                target.originalName = original;
                for (const auto &field : target.draft.fields)
                    if (field.id == target.draft.primaryField &&
                        !namingFieldError(field, target.originalName).empty())
                        target.originalName = field.validation == 2 || field.width ? L"1" : L"Asset";
            }
            data->draft = target.draft;
            data->simulated = target.simulated;
            data->selected = target.selected;
            data->page = target.page;
            data->fieldPage = target.fieldPage;
            data->example = selected;
            data->applications = target.applications;
            data->applicationSettings = target.applicationSettings;
            data->applicationNotice.clear();
            data->globalDefault = target.globalDefault;
            data->locationSelected = -1;
            data->advanced = true;
            data->page = 9;
            data->loading = true;
            designerSet(window, ND_EXAMPLE, target.originalName);
            data->loading = false;
            designerLoad(window, *data);
            return TRUE;
        }
        if (id == ND_SOURCE && code == CBN_SELCHANGE) {
            int primary = designerChoice(window, ND_PRIMARY);
            if (designerChoice(window, ND_SOURCE) != 0 && primary > 0 &&
                data->primaryFields[primary] == data->draft.fields[data->selected].id)
                SendDlgItemMessageW(window, ND_PRIMARY, CB_SETCURSEL, 0, 0);
            designerLayout(window, *data);
        }
        if (((id == ND_SOURCE || id == ND_PRIMARY || id == ND_DATE_FORMAT || id == ND_NUMBERING) &&
             code == CBN_SELCHANGE) ||
            (id == ND_EXAMPLE && code == EN_CHANGE))
            designerSourceUI(window, *data);
        if (id == ND_CONDITION_FIELD && code == CBN_SELCHANGE) {
            int selected = designerChoice(window, id);
            EnableWindow(GetDlgItem(window, ND_CONDITION_VALUE), selected > 0);
            data->loading = true;
            SendDlgItemMessageW(window, ND_CONDITION_VALUE, CB_RESETCONTENT, 0, 0);
            if (selected > 0 && static_cast<size_t>(selected) < data->conditions.size())
                for (const auto &f : data->draft.fields)
                    if (f.id == data->conditions[selected])
                        for (const auto &p : f.candidates) {
                            auto output = namingFieldOutput(f, p.value);
                            SendDlgItemMessageW(window, ND_CONDITION_VALUE, CB_ADDSTRING, 0,
                                                reinterpret_cast<LPARAM>(output.c_str()));
                        }
            data->loading = false;
        }
        if (id == ND_OWN_JOINER && code == BN_CLICKED)
            EnableWindow(GetDlgItem(window, ND_JOINER), designerChecked(window, ND_OWN_JOINER));
        if (id == ND_RECOMMEND || id == ND_WARN_EXISTING)
            designerGlobalHint(window, *data);
        if (code == EN_CHANGE || code == CBN_SELCHANGE || code == CBN_EDITCHANGE || code == BN_CLICKED)
            designerPreview(window, *data);
    }
    return FALSE;
}
inline bool showNamingDesigner(HWND owner, StructuredNaming &draft, const Config &config, FormData theme,
                               const std::wstring &subject, std::vector<std::wstring> &applications,
                               bool applyOnSave = true, int initialPage = 0, int initialField = -1,
                               bool *globalDefault = nullptr) {
    NamingDesigner data;
    data.applyOnSave = applyOnSave;
    data.theme = theme;
    data.style = *theme.compact;
    data.theme.compact = &data.style;
    data.theme.settings = true;
    data.draft = namingDefinition(draft);
    data.config = &config;
    data.originalName = subject;
    data.applications = applications;
    data.applicationSettings = !applications.empty();
    if (data.originalName.empty())
        data.originalName = L"Asset";
    for (const auto &field : data.draft.fields)
        if (field.id == data.draft.primaryField && !namingFieldError(field, data.originalName).empty())
            data.originalName = field.validation == 2 || field.width ? L"1" : L"Asset";
    for (size_t i = 0; i < data.draft.fields.size(); ++i)
        if (data.draft.fields[i].id == data.draft.primaryField) {
            data.selected = static_cast<int>(i);
            break;
        }
    if (initialField >= 0 && static_cast<size_t>(initialField) < data.draft.fields.size())
        data.selected = initialField;
    data.advanced = initialPage != 0 || initialField >= 0;
    data.page = std::clamp(initialPage, 0, 9);
    data.fieldPage = data.page < 3 ? data.page : 0;
    data.globalDefault = globalDefault ? *globalDefault : defaultNamingScheme(config).id == data.draft.id;
    data.draft.literal = false;
    data.draft.literalName.clear();
    const auto &schemes = namingSchemeCatalog(config);
    data.catalog = config.namingCatalog;
    data.initialSaved = std::any_of(schemes.begin(), schemes.end(),
                                    [&](const auto &scheme) { return scheme.id == data.draft.id; });
    StructuredNaming simple;
    simple.id = L"starter-simple";
    simple.name = L"简单命名";
    simple.enabled = true;
    simple.counter = 0;
    simple.fields = {{L"subject", L"原文件名", L"", true}};
    simple.fields[0].defaultSource = 0;
    auto game = gameNamingTemplates()[0];
    game.id = L"starter-game";
    game.name = L"游戏资源命名";
    game.fields.resize(2);
    game.counter = 1;
    auto blank = simple;
    blank.id = L"starter-blank";
    blank.name = L"从空白创建";
    blank.primaryField.clear();
    blank.fields = {{L"field1", L"自定义内容"}};
    blank.fields[0].defaultSource = 0;
    data.starters = {std::move(simple), std::move(game), std::move(blank)};
    for (const auto &s : data.starters)
        data.templates.push_back(&s);
    for (const auto &s : schemes)
        if (s.id != data.draft.id)
            data.templates.push_back(&s);
    data.examples.resize(data.templates.size() + 1);
    data.examples[0] = {data.draft,        {},
                        data.originalName, data.selected,
                        data.page,         data.fieldPage,
                        data.applications, data.applicationSettings,
                        data.globalDefault};
    struct Template {
        DLGTEMPLATE dialog;
        WORD menu, windowClass, title[5];
    } layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_CLIPCHILDREN;
    layout.dialog.dwExtendedStyle = WS_EX_DLGMODALFRAME;
    layout.dialog.cx = 400;
    layout.dialog.cy = 320;
    const wchar_t title[] = L"命名方案";
    std::copy(std::begin(title), std::end(title), layout.title);
    bool saved =
        (runSettingsDialog
             ? runSettingsDialog(owner, namingDesignerProcedure, reinterpret_cast<LPARAM>(&data), L"命名方案")
             : DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, owner,
                                       namingDesignerProcedure, reinterpret_cast<LPARAM>(&data))) == IDOK;
    if (saved) {
        draft = std::move(data.draft);
        applications = std::move(data.applications);
        if (globalDefault)
            *globalDefault = data.globalDefault;
    }
    return saved;
}
} // namespace jump
