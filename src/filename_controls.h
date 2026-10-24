#pragma once
#include "model.h"
#include <ole2.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <algorithm>
#include <vector>

namespace filename_controls {
using Microsoft::WRL::ComPtr;
inline std::wstring take(BSTR value) {
    std::wstring text = value ? std::wstring(value, SysStringLen(value)) : L"";
    SysFreeString(value); return text;
}
inline std::wstring name(IUIAutomationElement *element) {
    BSTR text = nullptr; element->get_CurrentName(&text); return take(text);
}
inline std::wstring id(IUIAutomationElement *element) {
    BSTR text = nullptr; element->get_CurrentAutomationId(&text); return take(text);
}
inline std::wstring cls(IUIAutomationElement *element) {
    BSTR text = nullptr; element->get_CurrentClassName(&text); return take(text);
}
inline std::wstring value(IUIAutomationElement *element) {
    ComPtr<IUIAutomationValuePattern> pattern;
    element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
    BSTR text = nullptr; if (pattern) pattern->get_CurrentValue(&text); return take(text);
}
inline std::wstring compact(std::wstring text) {
    text = jump::lower(text);
    // Chinese labels commonly append an access key, e.g. 文件名(&N) or 保存(S).
    for (size_t at = 0; at < text.size();) {
        if (text[at] != L'(' && text[at] != L'（') { ++at; continue; }
        auto end = text.find(text[at] == L'(' ? L')' : L'）', at + 1);
        auto key = end == text.npos ? L"" : text.substr(at + 1, end - at - 1);
        if (key.size() == 2 && key.front() == L'&') key.erase(0, 1);
        if (key.size() == 1 && key[0] >= L'a' && key[0] <= L'z') text.erase(at, end - at + 1);
        else ++at;
    }
    std::erase_if(text, [](wchar_t c) { return iswspace(c) || wcschr(L"&:：_-.()（）…", c); });
    return text;
}
inline bool filenameLabel(const std::wstring &text) {
    auto key = compact(text);
    return key == L"filename" || key == L"outputfilename" || key == L"exportfilename" ||
        key == L"文件名" || key == L"文件名称" || key == L"输出文件名" || key == L"导出文件名" ||
        key == L"outputfile" || key == L"exportfile" || key == L"输出文件" || key == L"导出文件";
}
// Only explicit output destinations are directly writable. A generic Folder or
// address editor may require navigation; its SetValue echo is not proof of it.
inline bool outputDirectoryLabel(const std::wstring &text) {
    auto key = compact(text);
    return key == L"outputdirectory" || key == L"exportdirectory" || key == L"outputfolder" ||
        key == L"exportfolder" || key == L"destinationfolder" || key == L"输出目录" ||
        key == L"导出目录" || key == L"输出文件夹" || key == L"导出文件夹" || key == L"目标文件夹";
}
inline bool saveTitle(std::wstring title) {
    title = jump::lower(jump::trim(title));
    return title == L"save" || title.starts_with(L"save ") || title == L"export" || title.starts_with(L"export ") ||
        title.starts_with(L"另存为") || title.starts_with(L"保存") || title.starts_with(L"导出");
}
inline bool saveButtonLabel(const std::wstring &text) {
    const auto key = compact(text);
    return key == L"save" || key == L"saveas" || key == L"export" ||
        key == L"保存" || key == L"另存为" || key == L"导出";
}
inline bool directoryLabel(const std::wstring &text) {
    const auto key = compact(text);
    return outputDirectoryLabel(text) || key == L"folder" || key == L"directory" || key == L"outputdirectory" ||
        key == L"exportdirectory" || key == L"currentfolder" || key == L"savein" || key == L"lookin" ||
        key == L"目录" || key == L"文件夹" || key == L"输出目录" || key == L"保存位置";
}
inline bool qtFileField(IUIAutomationElement *element, const wchar_t *field) {
    // Qt exposes object paths, not bare AutomationIds. Restrict structural aliases
    // to its standard file dialog, rather than matching arbitrary suffixes.
    auto key = id(element);
    return key == std::wstring(L"QFileDialog.") + field;
}
inline bool visible(IUIAutomationElement *element) {
    BOOL hidden = TRUE; element->get_CurrentIsOffscreen(&hidden); return !hidden;
}
inline bool visibleEnabled(IUIAutomationElement *element) {
    BOOL enabled = FALSE, hidden = TRUE;
    element->get_CurrentIsEnabled(&enabled); element->get_CurrentIsOffscreen(&hidden);
    return enabled && !hidden;
}
inline bool invokable(IUIAutomationElement *element) {
    ComPtr<IUIAutomationInvokePattern> pattern;
    element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pattern));
    return pattern && visibleEnabled(element);
}
inline bool addressAction(IUIAutomationElement *element, bool execute = false) {
    if (!visibleEnabled(element)) return false;
    ComPtr<IUIAutomationInvokePattern> invoke;
    element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    if (invoke) return !execute || SUCCEEDED(invoke->Invoke());
    ComPtr<IUIAutomationLegacyIAccessiblePattern> legacy;
    element->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, IID_PPV_ARGS(&legacy));
    BSTR action = nullptr; if (legacy) legacy->get_CurrentDefaultAction(&action);
    auto text = take(action);
    return legacy && !text.empty() && (!execute || SUCCEEDED(legacy->DoDefaultAction()));
}
inline bool scope(const jump::FilenameBinding &binding, const std::wstring &exe,
                  const std::wstring &windowClass, const std::wstring &title) {
    return jump::lower(binding.executable) == jump::lower(exe) && binding.windowClass == windowClass &&
        binding.windowTitle == jump::lower(jump::trim(title));
}
inline bool writable(IUIAutomationElement *element) {
    CONTROLTYPEID type = 0; BOOL enabled = FALSE, hidden = TRUE, password = TRUE;
    element->get_CurrentControlType(&type); element->get_CurrentIsEnabled(&enabled);
    element->get_CurrentIsOffscreen(&hidden); element->get_CurrentIsPassword(&password);
    if ((type != UIA_EditControlTypeId && type != UIA_ComboBoxControlTypeId) || !enabled || hidden || password) return false;
    ComPtr<IUIAutomationValuePattern> pattern;
    element->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&pattern));
    BOOL readOnly = TRUE; if (pattern) pattern->get_CurrentIsReadOnly(&readOnly);
    return pattern && !readOnly;
}
inline bool equal(IUIAutomation *uia, IUIAutomationElement *a, IUIAutomationElement *b) {
    BOOL result = FALSE; return a && b && SUCCEEDED(uia->CompareElements(a, b, &result)) && result;
}
inline std::wstring label(IUIAutomationElement *element) {
    ComPtr<IUIAutomationElement> labelled; element->get_CurrentLabeledBy(&labelled);
    return labelled ? name(labelled.Get()) : name(element);
}
inline std::wstring component(IUIAutomationElement *element) {
    CONTROLTYPEID type = 0; element->get_CurrentControlType(&type);
    auto automationId = id(element);
    // IDs are strongest. Labels supplement unnamed controls; editable values never become selectors.
    auto text = automationId.empty() ? label(element) : L"";
    if ((type == UIA_EditControlTypeId || type == UIA_ComboBoxControlTypeId) && text == value(element)) text.clear();
    auto part = [](const std::wstring &s) { return std::to_wstring(s.size()) + L":" + s; };
    return std::to_wstring(type) + L"/" + part(cls(element)) + part(automationId) + part(text);
}
inline std::wstring selector(IUIAutomation *uia, IUIAutomationTreeWalker *walker,
                             IUIAutomationElement *root, IUIAutomationElement *element) {
    std::wstring result; ComPtr<IUIAutomationElement> cursor = element;
    for (int depth = 0; cursor && depth < 16; ++depth) {
        if (equal(uia, cursor.Get(), root)) return result;
        auto part = component(cursor.Get()); result = std::to_wstring(part.size()) + L":" + part + result;
        ComPtr<IUIAutomationElement> parent; walker->GetParentElement(cursor.Get(), &parent); cursor = parent;
    }
    return {};
}
struct Field { ComPtr<IUIAutomationElement> element; std::wstring selector; bool semantic = false; };
struct Breadcrumb {
    ComPtr<IUIAutomationElement> container, opener;
    std::wstring signature;
};
struct Scan {
    std::vector<Field> fields, directories, saves, formats, navigationButtons;
    std::vector<Breadcrumb> breadcrumbs;
    bool saveButton = false, complete = true;
};
inline Scan scan(IUIAutomation *uia, IUIAutomationElement *root) {
    Scan result; ComPtr<IUIAutomationTreeWalker> walker; uia->get_ControlViewWalker(&walker);
    if (!walker) { result.complete = false; return result; }
    struct Node { ComPtr<IUIAutomationElement> element; int depth; };
    std::vector<Node> stack{{root, 0}}; int visited = 0; auto deadline = GetTickCount64() + 900;
    while (!stack.empty()) {
        if (++visited > 256 || GetTickCount64() > deadline) { result.complete = false; break; }
        auto node = std::move(stack.back()); stack.pop_back();
        CONTROLTYPEID type = 0; node.element->get_CurrentControlType(&type);
        // Some custom file pickers expose a breadcrumb row as checkboxes and a
        // separate button that opens its address editor (including WPS). Match
        // the structure and filesystem root, never an executable or pixel offset.
        if (type == UIA_GroupControlTypeId && visible(node.element.Get())) {
            ComPtr<IUIAutomationElement> child;
            walker->GetFirstChildElement(node.element.Get(), &child);
            bool rootLabel = false, onlyCrumbs = true; int count = 0;
            std::wstring signature;
            while (child && count < 64) {
                CONTROLTYPEID childType = 0; child->get_CurrentControlType(&childType);
                if (childType != UIA_CheckBoxControlTypeId) { onlyCrumbs = false; break; }
                auto text = name(child.Get()); auto key = compact(text);
                rootLabel = rootLabel || key == L"此电脑" || key == L"thispc" || key == L"computer" || key == L"计算机";
                signature += std::to_wstring(text.size()) + L":" + text; ++count;
                ComPtr<IUIAutomationElement> next; walker->GetNextSiblingElement(child.Get(), &next); child = next;
            }
            if (onlyCrumbs && !child && rootLabel && count) {
                ComPtr<IUIAutomationElement> parent, sibling, opener;
                walker->GetParentElement(node.element.Get(), &parent);
                if (parent) walker->GetFirstChildElement(parent.Get(), &sibling);
                int siblings = 0;
                while (sibling && siblings < 4) {
                    if (!equal(uia, sibling.Get(), node.element.Get())) {
                        CONTROLTYPEID siblingType = 0; sibling->get_CurrentControlType(&siblingType);
                        if (siblingType == UIA_ButtonControlTypeId && name(sibling.Get()).empty() && addressAction(sibling.Get())) opener = sibling;
                    }
                    ++siblings;
                    ComPtr<IUIAutomationElement> next; walker->GetNextSiblingElement(sibling.Get(), &next); sibling = next;
                }
                if (siblings == 2 && !sibling && opener) result.breadcrumbs.push_back({parent, opener, signature});
            }
        }
        // File listings can contain thousands of UIA nodes. Filename editors live outside
        // those lists; skip their rows rather than scanning every file on each click.
        if (node.depth && (type == UIA_ListControlTypeId || type == UIA_TreeControlTypeId ||
            type == UIA_DataGridControlTypeId || type == UIA_TableControlTypeId ||
            type == UIA_MenuBarControlTypeId || type == UIA_MenuControlTypeId || type == UIA_TitleBarControlTypeId)) continue;
        if (type == UIA_ButtonControlTypeId && saveButtonLabel(name(node.element.Get())) && visible(node.element.Get())) {
            result.saveButton = true;
            // An empty filename commonly disables Save. It still identifies a save
            // dialog; enabled/Invoke checks belong to submission, not attachment.
            // Count disabled alternatives too: they must never conceal ambiguity.
            result.saves.push_back({node.element, selector(uia, walker.Get(), root, node.element.Get())});
        }
        if (type == UIA_ButtonControlTypeId && visible(node.element.Get()) &&
            (saveButtonLabel(name(node.element.Get())) || compact(name(node.element.Get())) == L"open" || compact(name(node.element.Get())) == L"打开"))
            result.navigationButtons.push_back({node.element, selector(uia, walker.Get(), root, node.element.Get())});
        if ((type == UIA_EditControlTypeId || type == UIA_ComboBoxControlTypeId) &&
            visibleEnabled(node.element.Get()) &&
            (directoryLabel(label(node.element.Get())) || directoryLabel(id(node.element.Get())) ||
             qtFileField(node.element.Get(), L"lookInCombo"))) {
            BOOL password = TRUE; node.element->get_CurrentIsPassword(&password);
            if (!password) result.directories.push_back({node.element, selector(uia, walker.Get(), root, node.element.Get())});
        }
        if (type == UIA_ComboBoxControlTypeId && visibleEnabled(node.element.Get()) &&
            qtFileField(node.element.Get(), L"fileTypeCombo"))
            result.formats.push_back({node.element, selector(uia, walker.Get(), root, node.element.Get())});
        if (writable(node.element.Get())) {
            bool semantic = filenameLabel(label(node.element.Get())) || filenameLabel(id(node.element.Get())) ||
                qtFileField(node.element.Get(), L"fileNameEdit");
            // An editable child of a labelled combo is the same filename field.
            ComPtr<IUIAutomationElement> parent; walker->GetParentElement(node.element.Get(), &parent);
            CONTROLTYPEID parentType = 0; if (parent) parent->get_CurrentControlType(&parentType);
            if (parentType == UIA_ComboBoxControlTypeId)
                semantic = semantic || filenameLabel(label(parent.Get())) || filenameLabel(id(parent.Get()));
            auto key = selector(uia, walker.Get(), root, node.element.Get());
            if (!key.empty() && key.size() <= 8000) result.fields.push_back({node.element, key, semantic});
        }
        if (node.depth >= 15) { result.complete = false; break; }
        ComPtr<IUIAutomationElement> child; walker->GetFirstChildElement(node.element.Get(), &child);
        for (int count = 0; child; ++count) {
            if (count >= 256) { result.complete = false; break; }
            stack.push_back({child, node.depth + 1});
            ComPtr<IUIAutomationElement> next; walker->GetNextSiblingElement(child.Get(), &next); child = next;
        }
    }
    // A combo and its edit child may both expose ValuePattern. Prefer the edit, never two aliases.
    const auto allFields = result.fields;
    std::erase_if(result.fields, [&](const Field &field) {
        CONTROLTYPEID type = 0; field.element->get_CurrentControlType(&type);
        if (type != UIA_ComboBoxControlTypeId) return false;
        return std::any_of(allFields.begin(), allFields.end(), [&](const Field &other) {
            ComPtr<IUIAutomationElement> parent; walker->GetParentElement(other.element.Get(), &parent);
            return equal(uia, parent.Get(), field.element.Get());
        });
    });
    const auto directories = result.directories;
    std::erase_if(result.directories, [&](const Field &field) {
        return std::any_of(directories.begin(), directories.end(), [&](const Field &other) {
            ComPtr<IUIAutomationElement> parent; walker->GetParentElement(other.element.Get(), &parent);
            return equal(uia, parent.Get(), field.element.Get());
        });
    });
    return result;
}
inline std::wstring directory(const Scan &scan) {
    if (!scan.complete || scan.directories.size() != 1) return {};
    auto text = jump::trim(value(scan.directories.front().element.Get()));
    if (text.empty() || text.find_first_of(L"\r\n\t\"<>|*?") != text.npos) return {};
    std::filesystem::path path(text);
    if (!path.is_absolute()) return {};
    return path.lexically_normal().wstring();
}
inline ComPtr<IUIAutomationElement> saveControl(const Scan &scan) {
    return scan.complete && scan.saves.size() == 1 && !scan.saves.front().selector.empty() &&
        invokable(scan.saves.front().element.Get())
        ? scan.saves.front().element : ComPtr<IUIAutomationElement>{};
}
inline ComPtr<IUIAutomationElement> outputDirectory(const Scan &scan) {
    if (!scan.complete || scan.directories.size() != 1) return {};
    auto element = scan.directories[0].element;
    return writable(element.Get()) &&
        (outputDirectoryLabel(label(element.Get())) || outputDirectoryLabel(id(element.Get()))) ? element : ComPtr<IUIAutomationElement>{};
}
inline bool qtNavigation(const Scan &scan, IUIAutomationElement *filename) {
    if (!scan.complete || !filename || !qtFileField(filename, L"fileNameEdit") ||
        scan.directories.size() != 1 || !qtFileField(scan.directories[0].element.Get(), L"lookInCombo") ||
        scan.formats.size() != 1 || scan.saves.size() != 1) return false;
    ComPtr<IUIAutomationInvokePattern> invoke;
    scan.saves[0].element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    return invoke && !directory(scan).empty();
}
inline bool breadcrumbNavigation(const Scan &scan, IUIAutomationElement *filename) {
    if (!scan.complete || !filename || scan.breadcrumbs.size() != 1 || scan.saves.size() != 1) return false;
    RECT address{}, file{};
    scan.breadcrumbs[0].container->get_CurrentBoundingRectangle(&address);
    filename->get_CurrentBoundingRectangle(&file);
    return address.right > address.left && address.bottom > address.top && address.bottom <= file.top;
}
inline ComPtr<IUIAutomationElement> choose(const Scan &scan, const std::wstring &key = L"") {
    if (!scan.complete) return {};
    ComPtr<IUIAutomationElement> result;
    for (const auto &field : scan.fields) if (key.empty() ? field.semantic && scan.saveButton : field.selector == key) {
        if (result) return {}; // Ambiguity is not resolved by choosing the first textbox.
        result = field.element;
    }
    return result;
}
inline std::wstring extension(const std::wstring &filename) {
    if (jump::lower(filename).ends_with(L".hip.gz")) return filename.substr(filename.size() - 7);
    auto dot = filename.rfind(L'.');
    if (dot == filename.npos || dot == 0 || filename.size() - dot > 20) return {};
    auto suffix = filename.substr(dot);
    return suffix.size() > 1 && std::all_of(suffix.begin() + 1, suffix.end(), [](wchar_t c) { return iswalnum(c); }) ? suffix : L"";
}
inline std::wstring filterExtension(const std::wstring &filter) {
    // Only an unambiguous literal extension is a usable counter suffix. All-files,
    // multiple alternatives and compound patterns require the host's decision.
    auto start = filter.find(L"*.");
    if (start == filter.npos || filter.find(L"*.", start + 2) != filter.npos) return {};
    auto end = filter.find_first_of(L" ;)\t\r\n", start + 2);
    auto ext = filter.substr(start + 1, end == filter.npos ? end : end - start - 1);
    return extension(L"file" + ext) == ext ? ext : L"";
}
inline bool selfTest() {
    jump::FilenameBinding binding{L"C:\\Apps\\Tool.exe", L"CustomSave", L"asset output", L"selector"};
    return filenameLabel(L"文件名：") && filenameLabel(L"File &name:") && filenameLabel(L"file_name") &&
        filenameLabel(L"文件名(&N):") && saveButtonLabel(L"保存(S)") && saveButtonLabel(L"Save As…") &&
        !filenameLabel(L"Folder") && !filenameLabel(L"Search") && !filenameLabel(L"Directory path") &&
        directoryLabel(L"Output directory:") && !directoryLabel(L"Search path") &&
        saveButtonLabel(L"&Save") && !saveButtonLabel(L"Save settings") &&
        saveTitle(L"Export FBX") && saveTitle(L"另存为") && !saveTitle(L"Unsaved document - Editor") &&
        scope(binding, L"c:\\apps\\tool.exe", L"CustomSave", L"Asset output") &&
        !scope(binding, L"C:\\Other\\Tool.exe", L"CustomSave", L"Asset output") &&
        !scope(binding, L"C:\\Apps\\Tool.exe", L"CustomSave", L"Open asset") &&
        extension(L"name.asset") == L".asset" && extension(L"name.hip.gz") == L".hip.gz" &&
        extension(L"untitled").empty() && extension(L".hidden").empty() &&
        filterExtension(L"PNG (*.png)") == L".png" && filterExtension(L"All files (*)").empty() &&
        filterExtension(L"JPEG (*.jpg *.jpeg)").empty();
}
} // namespace filename_controls
