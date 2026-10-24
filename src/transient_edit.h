// Included after structured_ui.h. Delay dismissal until the clicked control has
// completed its action, so rebuilding the panel cannot consume that click.
void resetEditorDismiss() {
    ++g->editorRevision;
    g->pendingEditorDismiss = nullptr;
    g->explicitEditorDismiss = false;
    KillTimer(g->panel, T_EDITOR_DISMISS);
}
HWND transientEditor() {
    if (g->structuredField >= 0)
        return GetDlgItem(g->panel,
                          quickNamingView() ? ID_QUICK_INPUT + g->structuredField : ID_STRUCT_INPUT);
    if (g->inlineTool == 2)
        return GetDlgItem(g->panel, ID_AFFIX_PREFIX);
    if (g->inlineTool == 3)
        return GetDlgItem(g->panel, ID_RULE_PATTERN);
    return nullptr;
}
bool insideTransientEditor(HWND target) {
    if (!target || GetParent(target) != g->panel)
        return false;
    int id = GetDlgCtrlID(target);
    // Opening or cancelling settings is not an instruction to finish editing.
    if (id == ID_STRUCT_SCHEMES || id == ID_STRUCT_NAME || id == ID_NUMBER_MENU || id == ID_CAPABILITIES ||
        id == ID_RULES_SUMMARY)
        return true;
    if (g->structuredField >= 0) {
        int index = g->structuredField;
        if (quickNamingView())
            return id == ID_QUICK_INPUT + index || id == ID_QUICK_EDIT + index || id == ID_QUICK_DONE ||
                   id == ID_QUICK_CHOICE + index ||
                   (id >= ID_QUICK_PICK + index * 6 && id < ID_QUICK_PICK + index * 6 + 6);
        return id == ID_STRUCT_INPUT || id == ID_STRUCT_CLOSE || id == ID_STRUCT_USE ||
               id == ID_STRUCT_CANCEL || id == ID_QUICK_CHOICE + index || id == ID_STRUCT_CLEAR ||
               id == ID_STRUCT_LIBRARY || id == ID_STRUCT_INHERIT || id == ID_STRUCT_TODAY ||
               id == ID_STRUCT_FIELD + index || id == ID_STRUCT_TITLE || id == ID_STRUCT_SOURCE ||
               id == ID_STRUCT_EMPTY || (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6);
    }
    if (g->inlineTool == 2) {
        auto action = g->actions.find(id);
        return id == ID_AFFIX_PREFIX || id == ID_AFFIX_SUFFIX || id == ID_ADD_PREFIX || id == ID_ADD_SUFFIX ||
               id == ID_NAMING_MENU || id == ID_AFFIX_SUMMARY ||
               (action != g->actions.end() &&
                (action->second.kind == Action::Prefix || action->second.kind == Action::Suffix));
    }
    return g->inlineTool == 3 &&
           (id == ID_RENAME || id == ID_REPLACE_SUMMARY || id == ID_RULE_MODE || id == ID_RULE_ENABLE ||
            id == ID_RULE_CLEAR || id == ID_RULE_PATTERN || id == ID_RULE_REPLACEMENT ||
            id == ID_RULE_SOURCE || id == ID_RULE_HINT || id == ID_RULE_RESULT);
}
void requestEditorDismiss(bool explicitClick) {
    if (g->building)
        return;
    if (auto editor = transientEditor())
        PostMessageW(g->panel, WM_EDITOR_DISMISS, reinterpret_cast<WPARAM>(editor),
                     static_cast<LPARAM>((g->editorRevision << 1) | (explicitClick ? 1 : 0)));
}
void finishEditorDismiss(HWND editor, bool explicitClick) {
    KillTimer(g->panel, T_EDITOR_DISMISS);
    auto clear = [&]() {
        g->pendingEditorDismiss = nullptr;
        g->explicitEditorDismiss = false;
    };
    // A click can switch field, open a different tool or rebuild the controls.
    // An old blur notification must never dismiss the new editor.
    if (!editor || !IsWindow(editor) || editor != transientEditor() || g->building) {
        clear();
        return;
    }
    GUITHREADINFO thread{sizeof(thread)};
    GetGUIThreadInfo(GetCurrentThreadId(), &thread);
    if (!IsWindowEnabled(g->panel) || (g->flyout && IsWindowVisible(g->flyout)) ||
        (thread.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE))) {
        clear();
        return;
    }
    explicitClick = explicitClick || (g->pendingEditorDismiss == editor && g->explicitEditorDismiss);
    if (!explicitClick && GetForegroundWindow() == g->panel && insideTransientEditor(GetFocus())) {
        clear();
        return;
    }
    g->pendingEditorDismiss = editor;
    g->explicitEditorDismiss = explicitClick;
    // IME end resumes this request; no polling runs while choosing characters.
    if (g->structuredIme || std::any_of(g->children.begin(), g->children.end(), [](HWND child) {
            return insideTransientEditor(child) && GetPropW(child, L"Jump.IME");
        }))
        return;
    // Native filling can briefly transfer focus, then restore the same editor.
    if (g->busy || GetCapture()) {
        SetTimer(g->panel, T_EDITOR_DISMISS, 30, nullptr);
        return;
    }
    clear();
    if (!structuredInputError().empty() || !g->ruleError.empty() ||
        (g->inlineTool == 3 && !g->rulePreview.error.empty())) {
        updatePanelContent();
        return;
    }
    if (GetForegroundWindow() == g->panel && insideTransientEditor(GetFocus()))
        SetFocus(g->panel);
    if (g->structuredField >= 0)
        closeStructuredInput();
    else if (g->inlineTool >= 2) {
        g->inlineTool = 0;
        rebuildPanel();
    }
}
LRESULT CALLBACK TransientEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_KILLFOCUS:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_NCDESTROY:
        break;
    default:
        return DefSubclassProc(h, msg, wp, lp);
    }
    // A new click back in the same group cancels an older deferred outside click.
    if ((msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) && insideTransientEditor(h))
        resetEditorDismiss();
    HWND editor = transientEditor();
    auto revision = g->editorRevision;
    bool outsideClick = (msg == WM_LBUTTONUP || msg == WM_RBUTTONUP) && editor && !insideTransientEditor(h);
    if (msg == WM_IME_STARTCOMPOSITION)
        SetPropW(h, L"Jump.IME", reinterpret_cast<HANDLE>(1));
    if (msg == WM_IME_ENDCOMPOSITION)
        RemovePropW(h, L"Jump.IME");
    if (msg == WM_NCDESTROY)
        RemovePropW(h, L"Jump.IME");
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (outsideClick && revision == g->editorRevision && editor == transientEditor())
        requestEditorDismiss(true);
    if (msg == WM_KILLFOCUS)
        requestEditorDismiss();
    if (msg == WM_IME_ENDCOMPOSITION && g->pendingEditorDismiss)
        requestEditorDismiss(g->explicitEditorDismiss);
    return result;
}
