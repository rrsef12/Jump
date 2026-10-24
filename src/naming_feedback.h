// Expand validation beside its editor without rebuilding the active input.
void buildNamingHint(int id,int x,int y,int width){
    auto hint=child(L"STATIC",L"",SS_LEFT,id,x,y,width,0);
    SendMessageW(hint,WM_SETFONT,reinterpret_cast<WPARAM>(g->smallFont),TRUE);
    showIfChanged(hint,SW_HIDE);
}
void updateNamingHint(int id,HWND input,const std::wstring &error){
    auto hint=GetDlgItem(g->panel,id);if(!hint)return;
    if(input){
        bool marked=GetPropW(input,L"Jump.InputError")!=nullptr;
        if(marked!=!error.empty()){
            if(error.empty())RemovePropW(input,L"Jump.InputError");
            else SetPropW(input,L"Jump.InputError",reinterpret_cast<HANDLE>(1));
            invalidateSubjectGroup(input);InvalidateRect(g->panel,nullptr,FALSE);
        }
    }
    setTextIfChanged(hint,error);tip(hint,error);
    RECT r{};GetWindowRect(hint,&r);MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&r),2);
    int height=0;
    if(!error.empty()){
        HDC dc=GetDC(hint);auto old=SelectObject(dc,g->smallFont);RECT text{0,0,r.right-r.left,0};
        DrawTextW(dc,error.c_str(),-1,&text,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,old);ReleaseDC(hint,dc);height=text.bottom+px(6);
    }
    int delta=height-(r.bottom-r.top);
    if(delta){
        int cutoff=r.bottom;
        for(auto control:g->children){
            if(control==hint)continue;
            RECT below{};GetWindowRect(control,&below);MapWindowPoints(nullptr,g->panel,reinterpret_cast<POINT*>(&below),2);
            if(below.top>=cutoff)positionChild(control,below.left,below.top+delta,below.right-below.left,below.bottom-below.top);
        }
        for(auto &field:g->fields)if(field.top>=cutoff)OffsetRect(&field,0,delta);
        for(auto &label:g->labels)if(label.rect.top>=cutoff)OffsetRect(&label.rect,0,delta);
        for(auto &line:g->dividers)if(line>=cutoff)line+=delta;
        positionChild(hint,r.left,r.top,r.right-r.left,height);
        g->height+=MulDiv(delta,96,g->dpi);
        SetWindowPos(g->panel,nullptr,0,0,px(g->width),px(g->height),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        reposition(false);InvalidateRect(g->panel,nullptr,FALSE);
    }
    showIfChanged(hint,error.empty()?SW_HIDE:SW_SHOWNOACTIVATE);
}
void updateNamingFeedback(){
    for(int i=0;i<8;++i){
        bool editing=g->structuredField==i;
        auto input=editing?GetDlgItem(g->panel,!g->structuredAdvanced&&!g->structure.free?ID_QUICK_INPUT+i:ID_STRUCT_INPUT):nullptr;
        std::wstring error;
        if(g->structure.enabled&&!g->structure.free&&!g->structure.literal&&!g->structuredIme&&
           static_cast<size_t>(i)<g->structure.fields.size()&&namingFieldVisible(g->structure,i,g->cfg)&&
           (!g->structure.fields[i].omitted||(editing&&g->structuredEdited))){
            auto value=editing&&g->structuredEdited?trim(g->structuredDraft):fieldValue(g->structure.fields[i],g->cfg,g->structure.dateFormat);
            error=namingFieldError(g->structure.fields[i],value);
            if(editing&&g->structuredEdited&&value.size()>120)error=L"字段内容过长";
            if(!error.empty()&&editing&&g->structuredEdited)error+=L"；Esc 撤回本次输入。";
        }
        updateNamingHint(ID_FIELD_ERROR+i,input,error);
    }
    for(bool prefix:{true,false}){
        auto input=GetDlgItem(g->panel,prefix?ID_AFFIX_PREFIX:ID_AFFIX_SUFFIX);std::wstring error;
        if(input){auto value=windowText(input);if(!value.empty()&&!validAffix(value,prefix))
            error=std::wstring(prefix?L"前缀":L"后缀")+L"包含无效字符，请修改或清空。";}
        updateNamingHint(prefix?ID_PREFIX_ERROR:ID_SUFFIX_ERROR,input,error);
    }
    auto input=GetDlgItem(g->panel,ID_FREE_NAME_INPUT);
    updateNamingHint(ID_FREE_NAME_ERROR,input,input&&!validName(windowText(input))?L"文件名无效，不能包含路径字符或保留名称。":L"");
    updateNamingHint(ID_RULE_ERROR,GetDlgItem(g->panel,ID_RULE_PATTERN),g->rulePreview.error);
}
bool localNamingErrorVisible(){
    for(int id=ID_FIELD_ERROR;id<=ID_RULE_ERROR;++id){
        auto hint=GetDlgItem(g->panel,id);
        if(hint&&(GetWindowLongPtrW(hint,GWL_STYLE)&WS_VISIBLE))return true;
    }
    auto rule=GetDlgItem(g->panel,ID_RULE_HINT);
    return rule&&(GetWindowLongPtrW(rule,GWL_STYLE)&WS_VISIBLE)&&!g->rulePreview.error.empty();
}
