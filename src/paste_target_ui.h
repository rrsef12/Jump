#pragma once
// Included with the foreground naming helpers; filesystem work is in paste_target.h.
bool pasteTarget(HWND editor) {
    if (!OpenClipboard(editor)) return false;
    std::wstring text;
    if (auto memory = GetClipboardData(CF_UNICODETEXT)) {
        auto size = GlobalSize(memory) / sizeof(wchar_t);
        if (size && size <= 32768) {
            if (auto data = static_cast<const wchar_t *>(GlobalLock(memory))) {
                auto end = std::find(data, data + size, L'\0');
                if (end != data + size) text.assign(data, end);
                GlobalUnlock(memory);
            }
        }
    }
    CloseClipboard();
    auto path = pathEditValue(text);
    if (path.empty()) return false; // Ordinary text keeps the edit control's undo behavior.
    if (!g->attached || g->busy) return true;
    if (g->dialog.generic && !g->dialog.genericNavigate) {
        g->status = L"当前窗口未确认目录切换能力，请在原软件选择目录。";
        g->statusError = true; updatePanelContent(); return true;
    }
    invalidateNaming(); KillTimer(g->control, T_ACTION);
    g->pastePending = true; g->statusError = false;
    g->status = L"正在检查粘贴的路径…";
    Job job{JobType::ResolvePaste, g->dialog.hwnd, g->dialog.pid, g->generation.load(), path};
    job.pasteId = g->pasteId;
    job.expectedDirectory = g->dialog.directory; job.expectedExtension = g->dialog.extension;
    job.expectedRawName = g->dialog.name;
    enqueue(std::move(job)); updatePanelContent();
    return true;
}
void finishPastedTarget(const Result &result) {
    if (!g->attached || result.job.generation != g->generation || result.job.pasteId != g->pasteId || !g->pastePending) return;
    g->pastePending = false;
    auto fail = [&](const std::wstring &message) {
        g->status = message; g->statusError = true; updatePanelContent();
    };
    if (!result.error.empty()) { fail(result.error); return; }
    if (g->busy || !sameTarget(result.job) || !focusAllowsAction(result.job) ||
        result.job.expectedDirectory != g->dialog.directory || result.job.expectedExtension != g->dialog.extension ||
        result.job.expectedRawName != g->dialog.name) {
        fail(L"原窗口或目标已改变，请重新粘贴完整路径。"); return;
    }
    const auto &name = result.data.name;
    if (!name.empty()) {
        if(g->structuredIme||!structuredInputError().empty()){fail(L"请先完成或撤回当前输入，再粘贴路径。");return;}
        auto extension = lower(std::filesystem::path(name).extension().wstring());
        if (!extension.empty() && g->dialog.extension.empty()) {
            fail(L"当前格式尚未确认，请先在原软件选择格式，再粘贴完整路径。"); return;
        }
        if (!extension.empty() && !lower(name).ends_with(lower(g->dialog.extension))) {
            fail(L"粘贴的是 " + extension + L"，当前格式是 " + g->dialog.extension + L"；请先在原软件切换格式，再粘贴。"); return;
        }
        auto stem = stripExtension(name, g->dialog.extension);
        if (!g->dialog.save || !validName(stem)) { fail(L"当前窗口无法填入这个文件名。"); return; }
        if(g->structure.enabled&&!g->structure.free)enterFreeNaming();
        resetSmartNaming(); if(!g->structure.enabled)g->namingHistory.clear();
        g->ruleEnabled = false; g->ruleError.clear();
        g->stemName = stem; g->parts = splitName(stem, g->cfg.modifiers);
        g->dirtyName = g->localNamePending = true;
    }
    g->editingPath = false; g->pathDraft.clear(); g->statusError = false;
    if (name.empty() && (g->numberMode || g->folderName || g->variablesEnabled))
        g->stemName = composedName(&result.data.directory);
    updatePanelContent();
    apply(result.data.directory, g->stemName, false);
}
