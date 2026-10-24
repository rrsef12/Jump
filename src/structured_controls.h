// Included in the panel namespace after the standard child-control helpers.
void updateStructuredChoices();
bool structuredCommand(int id, int code);
void restoreNamingState(const NamingSnapshot &s);
void rememberNamingDraft();
void resumeNamingDraft();
void enterFreeNaming();
void updateStructuredContent();
void useNamingScheme(StructuredNaming scheme,bool remember=true);
void showNamingSettings(bool direct=false);
bool storeNamingScheme(const StructuredNaming &scheme,bool activate=true,const std::vector<std::wstring> *applications=nullptr,const bool *globalDefault=nullptr);
bool quickNamingView(){return !g->structuredAdvanced&&!g->structure.free;}
bool primarySubject(int index){return index>=0&&static_cast<size_t>(index)<g->structure.fields.size()&&
    g->structure.fields[index].id==g->structure.primaryField&&!namingFieldFixed(g->structure.fields[index]);}
unsigned namingVisibility(){unsigned mask=0;for(size_t i=0;i<g->structure.fields.size();++i)
    if(namingFieldVisible(g->structure,i,g->cfg))mask|=1u<<i;return mask;}
bool liveStructuredControl(int id){
    return (id>=ID_QUICK_INPUT&&id<=ID_QUICK_DONE)||id==ID_STRUCT_INPUT||id==ID_STRUCT_CLOSE||id==ID_STRUCT_USE||id==ID_STRUCT_CANCEL||id==ID_STRUCT_CLEAR||
        id==ID_STRUCT_SCHEMES||id==ID_STRUCT_NAME||id==ID_NUMBER_MENU||id==ID_STRUCT_QUICK_VIEW||id==ID_STRUCT_FIELDS_VIEW||
        id==ID_STRUCT_INHERIT||id==ID_STRUCT_TODAY||(id>=ID_STRUCT_FIELD&&id<ID_STRUCT_FIELD+8)||
        (id>=ID_STRUCT_CHOICE&&id<ID_STRUCT_CHOICE+6);
}
LRESULT CALLBACK QuickEditProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_IME_STARTCOMPOSITION){g->structuredIme=true;KillTimer(g->control,T_ACTION);updatePanelContent();}
    if(msg==WM_IME_ENDCOMPOSITION){g->structuredIme=false;PostMessageW(g->panel,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),EN_CHANGE),reinterpret_cast<LPARAM>(h));}
    if(msg==WM_NCDESTROY)g->structuredIme=false;
    if(msg==WM_LBUTTONDOWN){SetForegroundWindow(g->panel);SetFocus(h);}
    if(msg==WM_GETDLGCODE&&lp){auto k=reinterpret_cast<MSG*>(lp);if(k->message==WM_KEYDOWN&&(k->wParam==VK_RETURN||k->wParam==VK_ESCAPE||k->wParam==VK_DOWN))return DefSubclassProc(h,msg,wp,lp)|DLGC_WANTMESSAGE;}
    if(msg==WM_KEYDOWN&&!g->structuredIme){
        if(wp==VK_ESCAPE){SendMessageW(g->panel,WM_COMMAND,ID_QUICK_CANCEL,0);return 0;}
        if(wp==VK_DOWN){PostMessageW(g->panel,WM_COMMAND,ID_QUICK_CHOICE+GetDlgCtrlID(h)-ID_QUICK_INPUT,0);return 0;}
        if(wp==VK_RETURN){PostMessageW(g->panel,WM_COMMAND,ID_QUICK_DONE,0);return 0;}
    }
    if(msg==WM_CHAR&&!g->structuredIme&&(wp==VK_RETURN||wp==VK_ESCAPE))return 0;
    return DefSubclassProc(h,msg,wp,lp);
}
std::vector<Preset> quickCandidates(int index,bool includeSaved=true){
    std::vector<Preset> choices;
    if(index<0||static_cast<size_t>(index)>=g->structure.fields.size())return choices;
    const auto &field=g->structure.fields[index];
    auto add=[&](Preset p){
        for(const auto &rename:g->cfg.candidateRenames)
            if(rename.schema==g->structure.id&&rename.field==field.id&&rename.from==p.value){p.name=p.value=rename.to;break;}
        if(!p.value.empty()&&namingFieldError(field,p.value).empty()&&std::none_of(choices.begin(),choices.end(),[&](const auto &v){return v.value==p.value;}))choices.push_back(std::move(p));
    };
    for(const auto &p:field.candidates)add(p);
    if(includeSaved)for(const auto &record:g->cfg.savedFiles){if(record.projectId!=g->cfg.activeProject)continue;
        std::vector<std::wstring> v;if(!unpackNaming(record.structure,v)||v.size()!=8||v[0]!=L"JNSAVE1")continue;
        auto saved=deserializeStructure(v[1]);if(saved.id!=g->structure.id)continue;
        for(const auto &f:saved.fields)if(f.id==field.id&&f.source==0)add({f.value,f.value});
    }
    return choices;
}
std::vector<Preset> visibleQuickChoices(int index){
    // Recent subjects stay in the dropdown rather than adding rows to the panel.
    auto choices=quickCandidates(index,!primarySubject(index));
    const auto &field=g->structure.fields[index];
    if(field.candidates.empty()||namingFieldFixed(field))return {};
    size_t limit=3;
    if(choices.size()>limit)choices.resize(limit);
    auto value=fieldValue(field,g->cfg,g->structure.dateFormat);
    if(!primarySubject(index)&&!value.empty()&&namingFieldError(field,value).empty()&&std::none_of(choices.begin(),choices.end(),[&](const auto &p){return p.value==value;})){
        if(choices.size()==limit)choices.back()={value,value};else choices.push_back({value,value});
    }
    return choices;
}
void buildQuickNaming(int &y,int available){
    const int x=PANEL_MARGIN;
    g->quickPicks.clear();
    if(g->inlineTool>=2){
        child(L"BUTTON",g->inlineTool==2?L"收起前后缀":L"收起替换规则",WS_TABSTOP,g->inlineTool==2?ID_NAMING_MENU:ID_RENAME,x,y,available,26);y+=32;
        buildInlineTools(y,available);y+=6;return;
    }
    auto add=[&](size_t i){
            const auto &f=g->structure.fields[i];
            bool fixed=namingFieldFixed(f),subject=primarySubject(static_cast<int>(i))||f.candidates.empty()||fixed;
            bool combined=!fixed;
            const int labelWidth=f.label.size()>3?62:30,contentX=x+labelWidth+6,contentWidth=available-labelWidth-38;
            label(x,y+3,labelWidth,f.label);
            if(!fixed){auto more=child(L"BUTTON",primarySubject(static_cast<int>(i))?L"最近"+f.label:L"⋯",WS_TABSTOP,ID_QUICK_CHOICE+static_cast<int>(i),x+available-28,y,28,g->structuredField==static_cast<int>(i)?32:28);
                tip(more,primarySubject(static_cast<int>(i))?L"最近使用的"+f.label+L"：点击选择；输入时按 ↓ 也可打开。":f.label+L"的全部选项、自定义输入和常用选项管理");}
            auto value=g->structuredField==static_cast<int>(i)?g->structuredDraft:fieldValue(f,g->cfg,g->structure.dateFormat);
            if(g->structuredField==static_cast<int>(i)){
                auto edit=child(L"EDIT",value,ES_AUTOHSCROLL|WS_TABSTOP,ID_QUICK_INPUT+static_cast<int>(i),contentX,y,contentWidth-(combined?24:48),32);
                if(combined)g->fields.back().right=px(x+available);
                SendMessageW(edit,EM_SETLIMITTEXT,120,0);SetWindowSubclass(edit,QuickEditProc,3,0);
                tip(edit,L"输入即生效，点击别处或 Enter 收起；Esc 撤回本次修改。");
                child(L"BUTTON",L"收起",WS_TABSTOP,ID_QUICK_DONE,contentX+contentWidth-(combined?24:44),y,combined?28:44,32);y+=36;
            }else if(subject){
                auto name=child(fixed?L"STATIC":L"BUTTON",value.empty()?L"点击填写":value,fixed?SS_LEFT:WS_TABSTOP,ID_QUICK_EDIT+static_cast<int>(i),contentX,y,contentWidth+(combined?4:0),28);
                tip(name,f.label+L"："+value+(fixed?L"\n固定文字，可在方案编辑中修改。":primarySubject(static_cast<int>(i))?L"\n点击填写；右侧下拉可复用最近名称。":L"\n点击填写；更多选项中可编辑常用选项。"));y+=34;
            }
            auto choices=visibleQuickChoices(static_cast<int>(i));
            g->quickPicks[static_cast<int>(i)]=choices;
            int width=(contentWidth-8)/3;
            size_t slots=fixed||f.candidates.empty()?0:3;
            for(size_t j=0;j<slots;++j){auto button=child(L"BUTTON",j<choices.size()?(choices[j].name.empty()?choices[j].value:choices[j].name):L"",WS_TABSTOP,
                ID_QUICK_PICK+static_cast<int>(i)*6+static_cast<int>(j),contentX+static_cast<int>(j%3)*(width+4),y+static_cast<int>(j/3)*34,width,28);
                if(j<choices.size())tip(button,choices[j].value+(namingFieldCanOmit(g->structure,i)?L"\n点击替换，再点取消":L"\n点击替换此项")+L"；右键改名");else showIfChanged(button,SW_HIDE);}
            if(slots)y+=34;
            buildNamingHint(ID_FIELD_ERROR+static_cast<int>(i),contentX,y,available-labelWidth-6);
    };
    for(auto i:namingDisplayOrder(g->structure,g->cfg,true))add(i);
    y+=4;
}
int structuredTextHeight(const std::wstring &text,int width){
    HDC dc=GetDC(g->panel);auto old=SelectObject(dc,g->bodyFont);RECT r{0,0,px(width),0};
    auto wrapped=wrappedFilename(dc,text,px(width));
    DrawTextW(dc,wrapped.c_str(),-1,&r,DT_CALCRECT|DT_NOPREFIX);
    SelectObject(dc,old);ReleaseDC(g->panel,dc);return std::max(20,MulDiv(r.bottom-r.top,96,g->dpi)+4);
}
int structuredPreviewHeight(const std::wstring &text,int width){
    return std::max(32,structuredTextHeight(text,width));
}
LRESULT CALLBACK StructuredEditProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR) {
    if(msg==WM_IME_STARTCOMPOSITION){g->structuredIme=true;KillTimer(g->control,T_ACTION);updatePanelContent();}
    if(msg==WM_IME_ENDCOMPOSITION){g->structuredIme=false;PostMessageW(g->panel,WM_COMMAND,ID_STRUCT_LIVE,0);}
    if(msg==WM_NCDESTROY)g->structuredIme=false;
    if(msg==WM_KEYDOWN&&!g->structuredIme&&wp==VK_DOWN){
        auto choice=GetDlgItem(g->panel,ID_STRUCT_CHOICE);
        if(choice&&IsWindowVisible(choice)){SetFocus(choice);return 0;}
    }
    if(msg==WM_GETDLGCODE&&lp){auto k=reinterpret_cast<MSG*>(lp);if(k->message==WM_KEYDOWN&&(k->wParam==VK_RETURN||k->wParam==VK_ESCAPE))return DefSubclassProc(h,msg,wp,lp)|DLGC_WANTMESSAGE;}
    if(msg==WM_KEYDOWN&&!g->structuredIme&&(wp==VK_RETURN||wp==VK_ESCAPE)){
        PostMessageW(g->panel,WM_COMMAND,wp==VK_RETURN?ID_STRUCT_USE:ID_STRUCT_CANCEL,0);return 0;
    }
    if(msg==WM_CHAR&&!g->structuredIme&&(wp==VK_RETURN||wp==VK_ESCAPE))return 0;
    if(msg==WM_LBUTTONDOWN){SetForegroundWindow(g->panel);SetFocus(h);}
    return DefSubclassProc(h,msg,wp,lp);
}
LRESULT CALLBACK StructuredChoiceProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(msg==WM_GETDLGCODE&&lp){auto key=reinterpret_cast<MSG*>(lp);
        if(key->message==WM_KEYDOWN&&(key->wParam==VK_RETURN||key->wParam==VK_ESCAPE||key->wParam==VK_UP||key->wParam==VK_DOWN))
            return DefSubclassProc(h,msg,wp,lp)|DLGC_WANTMESSAGE;
    }
    if(msg==WM_KEYDOWN){
        if(wp==VK_RETURN||wp==VK_ESCAPE){PostMessageW(g->panel,WM_COMMAND,wp==VK_RETURN?GetDlgCtrlID(h):ID_STRUCT_CLOSE,0);return 0;}
        if(wp==VK_UP||wp==VK_DOWN){int index=GetDlgCtrlID(h)-ID_STRUCT_CHOICE+(wp==VK_UP?-1:1);
            if(index<0)SetFocus(GetDlgItem(g->panel,ID_STRUCT_INPUT));
            else if(index<6){auto next=GetDlgItem(g->panel,ID_STRUCT_CHOICE+index);if(IsWindowVisible(next))SetFocus(next);}return 0;
        }
    }
    if(msg==WM_CHAR&&(wp==VK_RETURN||wp==VK_ESCAPE))return 0;
    return DefSubclassProc(h,msg,wp,lp);
}
void buildStructuredControls(int &y,int available){
    const int x=PANEL_MARGIN;
    g->structuredVisibility=namingVisibility();
    auto title=child(L"BUTTON",(g->structure.free?L"整名编辑 · ":g->structuredAdvanced?L"全部字段 · ":L"方案 · ")+g->structure.name,
        WS_TABSTOP,ID_STRUCT_NAME,x,y,available-100,28);
    tip(title,g->structure.name+L"\n点击直接切换命名方案；字段和规则可从右侧“命名设置”编辑。");
    auto settings=child(L"BUTTON",L"命名设置 ▾",WS_TABSTOP,ID_STRUCT_SCHEMES,x+available-96,y,96,28);
    tip(settings,L"统一管理方案、字段、前后缀、替换和末尾编号；也可撤销或还原本次命名。");y+=34;
    if(g->structure.free){
        child(L"STATIC",L"完整名称 · 输入即生效",SS_LEFT,ID_STRUCT_TITLE,x,y,available,22);y+=26;
        auto edit=child(L"EDIT",g->stemName,ES_AUTOHSCROLL|WS_TABSTOP,ID_FREE_NAME_INPUT,x,y,available,32);
        SetWindowSubclass(edit,PreviewProc,1,0);
        tip(edit,L"修改完整名称；下方最终文件名只展示结果。恢复分项填写请使用“命名设置”。");y+=38;
        buildNamingHint(ID_FREE_NAME_ERROR,x,y,available);
        if(g->inlineTool<2)return;
    }
    if(quickNamingView()){buildQuickNaming(y,available);return;}
    int cell=(available-12)/3;
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(g->attached?g->dialog.hwnd:g->panel,MONITOR_DEFAULTTONEAREST),&monitor);
    int room=MulDiv(monitor.rcWork.bottom-monitor.rcWork.top,96,g->dpi);
    auto displayOrder=namingDisplayOrder(g->structure,g->cfg,false);
    int columns=3;
    int gridRows=static_cast<int>((displayOrder.size()+columns-1)/columns);
    // On short screens the active editor replaces the overview; closing it restores every field.
    bool focusedEditor=g->structuredField>=0&&y+gridRows*48+188+190+structuredPreviewHeight(g->stemName+g->dialog.extension,available)>room-16;
    if(!g->structure.free&&!focusedEditor&&!g->inlineTool){
        if(!g->structure.compact){
            int width=(available-(columns-1)*6)/columns;
            for(size_t slot=0;slot<displayOrder.size();++slot){size_t i=displayOrder[slot];const auto &f=g->structure.fields[i];
                auto value=fieldValue(f,g->cfg,g->structure.dateFormat);
                auto button=child(L"BUTTON",f.label+L"\n"+(value.empty()?L"未添加":value),WS_TABSTOP,ID_STRUCT_FIELD+static_cast<int>(i),x+static_cast<int>(slot%columns)*(width+6),y+static_cast<int>(slot/columns)*48,width,42);
                tip(button,f.label+(f.required?L"（必填）":L"")+L"："+(value.empty()?L"未添加":value));
            }
            y+=gridRows*48;
        }else{
            int left=x;HDC dc=GetDC(g->panel);auto old=SelectObject(dc,g->font);
            for(size_t i:displayOrder){const auto &f=g->structure.fields[i];auto value=fieldValue(f,g->cfg,g->structure.dateFormat);auto text=value.empty()?L"+"+f.label:value;
                SIZE size{};GetTextExtentPoint32W(dc,text.c_str(),static_cast<int>(text.size()),&size);int width=std::clamp(MulDiv(size.cx,96,g->dpi)+20,56,available);
                if(left+width>x+available){left=x;y+=32;}
                auto button=child(L"BUTTON",text,WS_TABSTOP,ID_STRUCT_FIELD+static_cast<int>(i),left,y,width,28);tip(button,f.label+L"："+value);left+=width+5;
            }SelectObject(dc,old);ReleaseDC(g->panel,dc);y+=36;
        }
    }
    if(g->structuredField>=0&&static_cast<size_t>(g->structuredField)<g->structure.fields.size()&&!g->structure.free){
        const auto &f=g->structure.fields[g->structuredField];
        child(L"STATIC",f.label+L" · 输入即生效",SS_LEFT,ID_STRUCT_TITLE,x,y+3,available-(primarySubject(g->structuredField)?128:60),22);
        if(primarySubject(g->structuredField))child(L"BUTTON",L"最近名称",WS_TABSTOP,ID_QUICK_CHOICE+g->structuredField,x+available-122,y,66,24);
        child(L"BUTTON",L"收起",WS_TABSTOP,ID_STRUCT_CLOSE,x+available-52,y,52,24);y+=26;
        auto edit=child(L"EDIT",g->structuredDraft,ES_AUTOHSCROLL|WS_TABSTOP,ID_STRUCT_INPUT,x,y,available,32);
        SendMessageW(edit,EM_SETLIMITTEXT,120,0);SetWindowSubclass(edit,StructuredEditProc,1,0);
        tip(edit,L"直接修改，停顿后自动填入原窗口；点击别处或 Enter 收起，Esc 撤回本次输入。输入也会筛选常用选项。");y+=38;
        buildNamingHint(ID_FIELD_ERROR+g->structuredField,x,y,available);
        for(int i=0;i<6;++i){auto button=child(L"BUTTON",L"",WS_TABSTOP,ID_STRUCT_CHOICE+i,x+(i%3)*(cell+6),y+(i/3)*32,cell,28);SetWindowSubclass(button,StructuredChoiceProc,2,0);}
        child(L"STATIC",L"",SS_LEFT,ID_STRUCT_EMPTY,x,y+4,available,56);
        y+=64;
        child(L"BUTTON",L"清除此项",WS_TABSTOP,ID_STRUCT_CLEAR,x,y,80,24);
        child(L"BUTTON",L"常用选项…",WS_TABSTOP,ID_STRUCT_LIBRARY,x+86,y,88,24);
        child(L"STATIC",f.source==1?L"继承项目":f.source==2?L"动态日期":L"本次值",SS_RIGHT,ID_STRUCT_SOURCE,x+available-112,y+3,112,20);y+=32;
        child(L"BUTTON",L"使用项目名称",WS_TABSTOP,ID_STRUCT_INHERIT,x,y,(available-6)/2,24);
        child(L"BUTTON",L"使用当天日期",WS_TABSTOP,ID_STRUCT_TODAY,x+(available+6)/2,y,(available-6)/2,24);y+=28;
    }
    if(g->inlineTool>=2){
        child(L"BUTTON",g->inlineTool==2?L"收起前后缀":L"收起替换规则",WS_TABSTOP,g->inlineTool==2?ID_NAMING_MENU:ID_RENAME,x,y,available,26);y+=32;
        buildInlineTools(y,available);
    }
}
