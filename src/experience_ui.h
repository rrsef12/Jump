// The compact panel exposes everyday actions without adding a second naming pipeline.
bool composingNaming(){
    return g->structuredIme||std::any_of(g->children.begin(),g->children.end(),[](HWND child){return GetPropW(child,L"Jump.IME")!=nullptr;});
}
std::wstring capabilitySummary() {
    if(!g->attached)return L"尚未关联文件窗口 · 查看接入帮助";
    if(!g->dialog.save)return L"位置跳转可用 · 当前为打开窗口";
    if(g->dialog.generic&&!g->dialog.genericNavigate)return L"名称可修改 · 目录请在原窗口选择";
    if(g->dialog.directory.empty())return L"位置跳转可用 · 选择目录后可编号";
    return L"位置与命名可用 · 在原窗口保存";
}
std::wstring numberingCaption() {
    if(g->structure.free)return L"编号：整名编辑 ▾";
    if(!g->numberMode)return L"编号：关闭 ▾";
    auto text=g->numberMode==1?std::wstring(L"自动版本"):std::wstring(L"自动序号");
    if(g->dialog.directory.empty())return text+L" · 待选目录 ▾";
    if(g->namingPending)return text+L" · 计算中 ▾";
    if(!g->ruleError.empty())return text+L" · 待修正 ▾";
    const auto &name=g->stemName;
    size_t start=name.find_last_of(L'_');
    if(start!=name.npos&&start+1<name.size()) {
        auto tail=name.substr(start+1);size_t digit=g->numberMode==1?1:0;
        if((!digit||tail[0]==L'v'||tail[0]==L'V')&&tail.size()>digit&&
            std::all_of(tail.begin()+digit,tail.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))text+=L" · "+tail;
    }
    return text+L" ▾";
}
void buildNamingActions(int &y,int available) {
    const int x=PANEL_MARGIN;
    auto number=child(L"BUTTON",L"",WS_TABSTOP,ID_NUMBER_MENU,x,y,available-120,28);
    tip(number,L"直接选择自动版本、序号或关闭；菜单可查看计算范围。");
    auto undo=child(L"BUTTON",L"撤销",WS_TABSTOP,ID_COMBO_UNDO,x+available-114,y,54,28);
    tip(undo,L"撤销上一次命名修改；不会回退目录。输入框内 Ctrl+Z 仍撤销文字。");
    auto redo=child(L"BUTTON",L"重做",WS_TABSTOP,ID_COMBO_REDO,x+available-54,y,54,28);
    tip(redo,L"重做已撤销的命名修改；不会改变保存位置。");y+=34;
    child(L"BUTTON",L"",WS_TABSTOP,ID_AFFIX_SUMMARY,x,y,(available-6)/2,24);
    child(L"BUTTON",L"",WS_TABSTOP,ID_REPLACE_SUMMARY,x+(available+6)/2,y,(available-6)/2,24);
    child(L"BUTTON",L"修饰规则：关闭 ▾",WS_TABSTOP,ID_RULES_SUMMARY,x,y,available,24);y+=30;
}
void updatePanelStatusLayout(){
    auto status=GetDlgItem(g->panel,ID_STATUS),capabilities=GetDlgItem(g->panel,ID_CAPABILITIES);
    if(!status||!capabilities)return;
    bool visible=(GetWindowLongPtrW(status,GWL_STYLE)&WS_VISIBLE)!=0;
    bool detail=visible&&(!g->namingPending&&!g->pastePending||g->statusError||g->settingsDirty||!structuredInputError().empty());
    RECT r{},s{};GetWindowRect(capabilities,&r);GetWindowRect(status,&s);
    MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&r),2);
    MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&s),2);
    // Anchor the entire footer to the result, including wrapped names and
    // contextual warnings. Independent cumulative offsets can overlap rows.
    if(auto preview=GetDlgItem(g->panel,ID_PREVIEW)){
        RECT result{};GetWindowRect(preview,&result);MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&result),2);
        int top=result.bottom+px(6);
        if(auto target=GetDlgItem(g->panel,ID_TARGET_INFO)){
            RECT info{};GetWindowRect(target,&info);MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&info),2);
            positionChild(target,info.left,top,info.right-info.left,info.bottom-info.top);
            top+=info.bottom-info.top;
        }
        positionChild(status,s.left,top+px(3),s.right-s.left,s.bottom-s.top);s.top=top+px(3);
    }
    int top=s.top-px(3)+(detail?px(24):0),delta=top-r.top;
    showIfChanged(capabilities,visible&&!detail?SW_HIDE:SW_SHOWNOACTIVATE);
    if(!delta)return;
    positionChild(capabilities,r.left,top,r.right-r.left,r.bottom-r.top);
    g->height+=MulDiv(delta,96,g->dpi);
    SetWindowPos(g->panel,nullptr,0,0,px(g->width),px(g->height),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    reposition(false);InvalidateRect(g->panel,nullptr,FALSE);
}
void updateExperienceContent() {
    updateNamingFeedback();
    auto resultLabel=GetDlgItem(g->panel,ID_FINAL_NAME_LABEL);
    if(!g->ruleError.empty()||!structuredInputError().empty())setTextIfChanged(resultLabel,L"最终文件名 · 待修正（保留上次结果）");
    else if(windowText(resultLabel).find(L"待修正")!=std::wstring::npos)setTextIfChanged(resultLabel,L"最终文件名");
    if((!g->settingsDirty||g->configPending)&&localNamingErrorVisible())showIfChanged(GetDlgItem(g->panel,ID_STATUS),SW_HIDE);
    setTextIfChanged(GetDlgItem(g->panel,ID_NUMBER_MENU),numberingCaption());
    bool editable=!g->busySubmitting&&!composingNaming()&&structuredInputError().empty();
    enableIfChanged(GetDlgItem(g->panel,ID_COMBO_UNDO),!g->busy&&!composingNaming()&&g->namingHistory.canUndo());
    enableIfChanged(GetDlgItem(g->panel,ID_COMBO_REDO),!g->busy&&!composingNaming()&&g->namingHistory.canRedo());
    enableIfChanged(GetDlgItem(g->panel,ID_NUMBER_MENU),editable);
    enableIfChanged(GetDlgItem(g->panel,ID_STRUCT_NAME),editable);
    for(int id:{ID_AFFIX_SUMMARY,ID_REPLACE_SUMMARY,ID_RULES_SUMMARY})enableIfChanged(GetDlgItem(g->panel,id),editable&&!g->busy);
    bool expanded=!g->parts.prefix.empty()||!g->parts.suffix.empty()||g->ruleEnabled||g->inlineTool>=2;
    showIfChanged(GetDlgItem(g->panel,ID_RULES_SUMMARY),expanded?SW_HIDE:SW_SHOWNOACTIVATE);
    for(int id:{ID_AFFIX_SUMMARY,ID_REPLACE_SUMMARY})showIfChanged(GetDlgItem(g->panel,id),expanded?SW_SHOWNOACTIVATE:SW_HIDE);
    std::wstring affix;
    if(!g->parts.prefix.empty())affix=L"前："+g->parts.prefix;
    if(!g->parts.suffix.empty())affix+=(affix.empty()?L"":L" · ")+std::wstring(L"后：")+g->parts.suffix;
    auto affixButton=GetDlgItem(g->panel,ID_AFFIX_SUMMARY);
    setTextIfChanged(affixButton,affix.empty()?L"前后缀：关闭":affix);
    if(affixButton)tip(affixButton,(affix.empty()?L"本次未添加前后缀":affix)+L"\n点击编辑；收起后继续生效。");
    auto rule=GetDlgItem(g->panel,ID_REPLACE_SUMMARY);
    auto ruleName=g->ruleIndex>0&&static_cast<size_t>(g->ruleIndex)<renameRules().size()?renameRules()[g->ruleIndex].name:L"自定义";
    setTextIfChanged(rule,!g->ruleEnabled?L"替换：关闭":L"替换："+ruleName);
    if(rule)tip(rule,g->ruleEnabled?L"正在应用："+ruleName+L"\n查找："+g->rulePattern+L"\n替换为："+g->ruleReplacement+L"\n点击修改或停用。":L"本次未启用替换；点击设置。");
    auto capabilities=GetDlgItem(g->panel,ID_CAPABILITIES);
    setTextIfChanged(capabilities,capabilitySummary());
}
int experienceMenu(HMENU menu,int id) {
    auto control=GetDlgItem(g->panel,id);
    if(!control)control=GetDlgItem(g->panel,ID_STRUCT_SCHEMES);
    if(!control)return 0;
    RECT anchor{};GetWindowRect(control,&anchor);SetForegroundWindow(g->panel);
    g->openPanelMenu=id;InvalidateRect(control,nullptr,FALSE);UpdateWindow(control);
    int choice=trackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,anchor.left,anchor.bottom+px(4),g->panel);
    g->openPanelMenu=0;if(IsWindow(control))InvalidateRect(control,nullptr,FALSE);
    return choice;
}
