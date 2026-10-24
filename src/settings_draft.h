// Inline settings use the same draft protection as the hosted editors. Compare
// values with the last loaded/saved form so undoing an edit also clears dirty.
struct ManagerDraft {
    int page = -1;
    std::vector<std::wstring> values;
} managerDraft;

std::vector<std::wstring> managerDraftValues() {
    auto text = [](int id) { return windowText(GetDlgItem(g->manager, id)); };
    if (g->managerTab == 1 || g->managerTab == 2)
        return {text(M_NAME), text(M_VALUE)};
    if (g->managerTab == MANAGER_AFFIXES)
        return {text(M_VALUE)};
    if (g->managerTab == MANAGER_TEMPLATES)
        return {g->variableTemplateDraft, g->variableValuesDraft};
    if (g->managerTab == 3)
        return {std::to_wstring(IsDlgButtonChecked(g->manager, M_AUTO)),
                std::to_wstring(IsDlgButtonChecked(g->manager, M_QUICK)),
                std::to_wstring(IsDlgButtonChecked(g->manager, M_STARTUP)),
                std::to_wstring(SendDlgItemMessageW(g->manager, M_LAYOUT, CB_GETCURSEL, 0, 0))};
    return {};
}
bool managerDraftDirty() {
    return g->manager && managerDraft.page == g->managerTab && managerDraft.values != managerDraftValues();
}
void invalidateManagerHeading() {
    RECT heading{px(MANAGER_CONTENT_LEFT + 20), px(16), px(MANAGER_SHELL_WIDTH - 20), px(40)};
    InvalidateRect(g->manager, &heading, FALSE);
}
void rememberManagerDraft() {
    managerDraft = {g->managerTab, managerDraftValues()};
    invalidateManagerHeading();
}
bool discardManagerDraft() {
    if (!managerDraftDirty())
        return true;
    return MessageBoxW(g->manager, L"编辑内容尚未保存。放弃本次编辑并离开？\n选择“否”可继续编辑。",
                       L"未保存的编辑", MB_YESNO | MB_DEFBUTTON2) == IDYES;
}
void managerFeedback(const std::wstring &message) {
    SetDlgItemTextW(g->manager, managingAffixes() ? M_AFFIX_STATUS : M_FEEDBACK, message.c_str());
}
bool commitManagerConfig(Config previous) {
    if (persist())
        return true;
    g->cfg = std::move(previous);
    managerFeedback(L"保存失败，原设置已保留。请检查配置目录写入权限后重试。");
    return false;
}
