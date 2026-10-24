// Task layout for the reusable scheme editor. All coordinates are logical DIPs;
// the settings host provides scrolling and keyboard visibility at smaller sizes.
inline bool designerFooterControl(int id){return id==IDOK||id==IDCANCEL||id==ND_FIX||id==ND_DIRTY||id==ND_FOOTER_SURFACE;}
inline LRESULT CALLBACK designerFooterSurface(HWND control,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR value){
    if(message==WM_PAINT||message==WM_PRINTCLIENT||message==WM_PRINT){
        auto &data=*reinterpret_cast<NamingDesigner*>(value);PAINTSTRUCT paint{};
        auto dc=message==WM_PAINT?BeginPaint(control,&paint):reinterpret_cast<HDC>(wp);RECT client{};GetClientRect(control,&client);
        FillRect(dc,&client,data.theme.brush);auto pen=CreatePen(PS_SOLID,1,data.style.border);auto old=SelectObject(dc,pen);
        MoveToEx(dc,MulDiv(20,data.theme.dpi,96),0,nullptr);LineTo(dc,MulDiv(620,data.theme.dpi,96),0);
        SelectObject(dc,old);DeleteObject(pen);if(message==WM_PAINT)EndPaint(control,&paint);return 0;
    }
    return DefSubclassProc(control,message,wp,lp);
}
inline void designerFooter(HWND window,NamingDesigner &data){
    auto px=[&](int value){return MulDiv(value,data.theme.dpi,96);};
    RECT client{};GetClientRect(window,&client);if(!client.bottom)return;
    if(auto host=settingsDialog(window)){host->footerHeight=px(52);host->footerControl=designerFooterControl;host->initialFocus=ND_NAME;}
    int top=std::max(0L,client.bottom-px(52)),y=top+px(12);
    SetWindowSubclass(GetDlgItem(window,ND_FOOTER_SURFACE),designerFooterSurface,81,reinterpret_cast<DWORD_PTR>(&data));
    MoveWindow(GetDlgItem(window,ND_FOOTER_SURFACE),0,top,client.right,px(52),FALSE);
    MoveWindow(GetDlgItem(window,ND_DIRTY),px(20),y,px(244),px(28),FALSE);
    MoveWindow(GetDlgItem(window,ND_FIX),px(274),y,px(126),px(28),FALSE);
    MoveWindow(GetDlgItem(window,IDCANCEL),px(408),y,px(88),px(28),FALSE);
    MoveWindow(GetDlgItem(window,IDOK),px(508),y,px(112),px(28),FALSE);
    // An opaque sibling surface clips native controls reliably, including
    // editable combos that manage their own regions during DPI changes.
    // Tab order is cyclic: body -> correction -> Cancel -> Save -> name.
    for(int id:std::initializer_list<int>{ND_FOOTER_SURFACE,ND_DIRTY,IDOK,IDCANCEL,ND_FIX})
        SetWindowPos(GetDlgItem(window,id),HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOREDRAW);
    RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
inline void designerFocus(HWND window,int id){
    auto control=GetDlgItem(window,id);if(!control||!IsWindowVisible(control))return;SetFocus(control);
    if(designerFooterControl(id))return;
    if(auto host=settingsDialog(window)){
        RECT r{},client=settingsBodyRect(window);GetWindowRect(control,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
        int next=host->scroll;
        if(r.top<0)next+=r.top-8;else if(r.bottom>client.bottom)next+=r.bottom-client.bottom+8;
        next=std::clamp(next,0,std::max<int>(0,host->contentHeight-client.bottom-host->footerHeight));
        if(next!=host->scroll){ScrollWindowEx(window,0,host->scroll-next,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE|SW_ERASE);host->scroll=next;SetScrollPos(window,SB_VERT,next,TRUE);designerFooter(window,*reinterpret_cast<NamingDesigner*>(GetWindowLongPtrW(window,DWLP_USER)));}
    }
}
inline void designerLayout(HWND window,NamingDesigner &data,bool =true){
    resetSettingsScroll(window);
    auto px=[&](int v){return MulDiv(v,data.theme.dpi,96);};
    auto move=[&](int id,int x,int y,int w,int h){
        auto control=GetDlgItem(window,id);MoveWindow(control,px(x),px(y),px(w),px(h),FALSE);
        // Native editable combos can retain an edit height from the pre-DPI
        // font. Keep their child edit centered and tall enough for descenders.
        COMBOBOXINFO info{sizeof(info)};wchar_t cls[16]{};
        if(control&&GetComboBoxInfo(control,&info)&&info.hwndItem&&GetClassNameW(info.hwndItem,cls,16)&&!lstrcmpiW(cls,L"Edit")){
            SendMessageW(info.hwndItem,WM_SETFONT,reinterpret_cast<WPARAM>(data.theme.font),FALSE);
            auto dc=GetDC(control);auto old=SelectObject(dc,data.theme.font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(control,dc);
            RECT client{};GetClientRect(control,&client);int height=std::min<int>(client.bottom,metrics.tmHeight+px(2));
            MoveWindow(info.hwndItem,px(6),(client.bottom-height)/2,std::max(1L,client.right-px(30)),height,FALSE);
            // Programmatic combo selection leaves a persistent blue selection
            // even while another field is focused. Show the actual separator.
            if(GetFocus()!=control&&GetFocus()!=info.hwndItem)SendMessageW(info.hwndItem,EM_SETSEL,static_cast<WPARAM>(-1),0);
        }
    };
    auto show=[](HWND h,bool visible){if(h&&!!(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE)!=visible)ShowWindow(h,visible?SW_SHOW:SW_HIDE);};
    if(data.scroll){ScrollWindowEx(window,0,data.scroll,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN);data.scroll=0;}
    bool detail=data.advanced,simulation=detail&&data.page==8,templates=detail&&data.page==9,scope=detail&&data.page==5;
    bool field=detail&&(data.page<3||data.page==6||data.page==7);
    for(auto [h,page]:data.pages)show(h,detail?page>=0&&page==data.page:page==-2);
    for(int id:{ND_BASE,ND_OUTPUT,ND_CHOICES,ND_FIELDS,ND_ORDER_TITLE,ND_ORDER_HINT,ND_SCOPE,ND_REMOVE,ND_UP,ND_DOWN,ND_PRIMARY,ND_MORE})show(GetDlgItem(window,id),false);
    for(int id:{ND_SECTION_FIELDS,ND_SIMULATE,ND_SECTION_APPS,ND_TEMPLATES,ND_STRUCTURE,ND_PREVIEW,ND_RESULT_LABEL,ND_PREVIEW_NOTE})show(GetDlgItem(window,id),true);
    // Three anchored regions: identity, a single test bench, and an ordered table.
    move(ND_NAME_LABEL,20,18,68,22);move(ND_NAME,98,18,354,22);
    move(ND_TEMPLATES,464,14,112,30);move(ND_SCHEME_MENU,584,14,36,30);
    designerSet(window,ND_TEMPLATES,L"命名示例…");show(GetDlgItem(window,ND_COPY),false);show(GetDlgItem(window,ND_SCHEME_MENU),true);
    move(ND_USAGE_STATUS,20,54,426,22);move(ND_SECTION_APPS,450,50,170,28);
    move(ND_PREVIEW_LABEL,32,96,220,20);move(ND_RESULT_LABEL,276,96,144,20);
    move(ND_EXAMPLE,36,124,212,22);move(ND_TEST_ARROW,250,118,24,30);
    move(ND_PREVIEW,276,118,332,28);move(ND_SIMULATE,424,92,88,24);move(ND_TRIAL,520,92,88,24);
    show(GetDlgItem(window,ND_TRIAL),true);
    move(ND_PREVIEW_NOTE,32,152,576,20);data.previewFrame={px(20),px(88),px(620),px(180)};
    move(ND_SECTION_FIELDS,20,194,156,26);
    show(GetDlgItem(window,ND_USAGE_STATUS),true);show(GetDlgItem(window,ND_TEST_ARROW),true);
    show(GetDlgItem(window,ND_PREVIEW_PARTS),false);
    show(GetDlgItem(window,ND_GLOBAL_HINT),false);
    show(GetDlgItem(window,ND_ADVANCED),false);
    designerSet(window,ND_SECTION_FIELDS,detail?L"‹ 返回命名规则":L"文件名结构 · "+std::to_wstring(data.draft.fields.size())+L" 项");
    show(GetDlgItem(window,ND_DETAIL_TITLE),detail);
    show(GetDlgItem(window,ND_DETAIL_NOTE),detail&&!simulation);
    move(ND_ADVANCED,20,228,126,28);move(ND_DETAIL_TITLE,20,244,600,24);move(ND_DETAIL_NOTE,20,278,600,28);
    static const wchar_t *titles[]{L"基础内容与来源",L"文字处理与限制",L"可供选择的内容",L"末尾自动编号",L"保存位置 · 目录推荐与同名检查",L"使用范围",L"填写与显示行为",L"满足条件时添加",L"模拟一次保存",L"从示例开始"};
    std::wstring title=titles[data.page];if(field)title=data.draft.fields[data.selected].label+L" · "+title;
    designerSet(window,ND_DETAIL_TITLE,title);
    designerSet(window,ND_DETAIL_NOTE,simulation?L"下面只模拟一次保存，不会改“以后默认填写”，也不影响主面板本次输入。":
        templates?L"先选择示例试效果；采用后进入编辑，最后保存才会写入配置。各示例保留独立草稿。":
        scope?L"新保存窗口：软件专用方案优先，其次是全局默认。临时切换不改变默认。":
        data.page==2?L"保存时选择左侧名称，右侧文字写入文件名；右侧留空则使用左侧名称。":
        data.page==7?L"条件匹配另一部分写入文件名的文字；不满足时不添加，也不要求填写。":
        data.page==6?L"参与命名、必须填写和面板可见性互相独立；停用会保留原有规则。":
        data.page==0?L"这里修改的是可重复使用的规则。临时试填请进入“模拟一次保存”。":
        data.page==1?L"这些处理只作用于当前部分；“恢复原样”可关闭本页的文字处理。":
        data.page==4?L"例如文件类别写入 SM 时推荐 Models。用户点击推荐后才切换目录。":L"自动编号追加在文件名末尾，真实保存时按目标目录续号；与手动编号部分独立。");
    for(int id:{ND_TEMPLATE,ND_TEMPLATE_LABEL,ND_ADOPT,ND_RESTORE,ND_TEMPLATE_EFFECT})show(GetDlgItem(window,id),templates);
    move(ND_TEMPLATE_LABEL,20,316,600,20);move(ND_TEMPLATE,20,344,600,240);
    move(ND_TEMPLATE_EFFECT,20,392,600,116);move(ND_ADOPT,20,526,216,30);move(ND_RESTORE,250,526,216,30);
    show(GetDlgItem(window,ND_EXAMPLE),true);show(GetDlgItem(window,ND_PREVIEW_LABEL),true);
    show(GetDlgItem(window,ND_RESET_SIM),simulation);show(GetDlgItem(window,ND_SIM_EXPLANATION),simulation);
    int simEnd=274;
    for(int i=0;i<8;++i){bool exists=simulation&&i<static_cast<int>(data.draft.fields.size());
        show(GetDlgItem(window,ND_SIM_LABEL+i),exists);show(GetDlgItem(window,ND_SIM_VALUE+i),exists&&data.draft.fields[i].id!=data.draft.primaryField&&namingFieldDefaultSource(data.draft.fields[i])==0);
        show(GetDlgItem(window,ND_SIM_AUTO+i),exists&&(data.draft.fields[i].id==data.draft.primaryField||namingFieldDefaultSource(data.draft.fields[i])!=0));
        int y=274+i*42;move(ND_SIM_LABEL+i,20,y+4,148,24);move(ND_SIM_VALUE+i,180,y,440,240);move(ND_SIM_AUTO+i,180,y+4,440,24);
        if(exists)simEnd=y+42;
    }
    move(ND_RESET_SIM,20,simEnd+2,206,28);move(ND_SIM_EXPLANATION,24,simEnd+46,592,172);
    auto measure=[&](int id,int width,int maxLines){
        auto dc=GetDC(window);auto font=reinterpret_cast<HFONT>(SendDlgItemMessageW(window,id,WM_GETFONT,0,0));
        auto before=SelectObject(dc,font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
        RECT r{0,0,px(width),0};auto text=designerText(window,id);
        DrawTextW(dc,text.c_str(),-1,&r,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
        SelectObject(dc,before);ReleaseDC(window,dc);
        return std::max(20,MulDiv(std::min<int>(r.bottom,metrics.tmHeight*maxLines)+2,96,data.theme.dpi));
    };
    bool composition=!detail&&data.draft.fields.size()>1;
    int structureHeight=composition?measure(ND_STRUCTURE,600,2):0;
    move(ND_STRUCTURE,20,228,600,structureHeight);show(GetDlgItem(window,ND_STRUCTURE),composition);
    int tableTop=composition?228+structureHeight+8:226;
    for(int id:{ND_COL_PART,ND_COL_CONTENT,ND_COL_ACTIONS})show(GetDlgItem(window,id),!detail);
    move(ND_COL_PART,32,tableTop+3,156,24);move(ND_COL_CONTENT,204,tableTop+3,222,24);move(ND_COL_ACTIONS,440,tableTop+3,168,24);
    int afterRows=tableTop+32;
    data.tableHeader={px(20),px(tableTop),px(620),px(afterRows)};
    data.rowFrames.clear();
    for(int i=0;i<8;++i){bool exists=!detail&&i<static_cast<int>(data.draft.fields.size());
        for(int base:{ND_SIMPLE_LABEL,ND_SIMPLE_HINT,ND_ROW_SOURCE,ND_SIMPLE_SETTINGS,ND_SIMPLE_CHOICES,ND_SIMPLE_REMOVE,ND_ROW_UP,ND_ROW_DOWN})show(GetDlgItem(window,base+i),exists);
        show(GetDlgItem(window,ND_ROW_DATE+i),exists&&namingFieldDefaultSource(data.draft.fields[i])==2);
        if(!exists)for(int base:{ND_SIMPLE_VALUE,ND_SIMPLE_TEXT,ND_SIMPLE_AUTO})show(GetDlgItem(window,base+i),false);
        if(!exists)continue;
        const auto &f=data.draft.fields[i];bool automatic=f.defaultSource==1||f.defaultSource==2,original=f.id==data.draft.primaryField;
        bool choices=!f.candidates.empty()&&!original&&f.defaultSource!=3;
        show(GetDlgItem(window,ND_SIMPLE_VALUE+i),!automatic&&choices);
        show(GetDlgItem(window,ND_SIMPLE_TEXT+i),!automatic&&!original&&!choices);
        show(GetDlgItem(window,ND_SIMPLE_AUTO+i),automatic||original);
        int y=afterRows,titleHeight=measure(ND_SIMPLE_LABEL+i,156,3),inputY=y+6;
        int hintHeight=measure(ND_SIMPLE_HINT+i,576,2);
        int secondY=y+std::max(28,titleHeight)+10;
        bool complex=measure(ND_SIMPLE_HINT+i,304,2)>22;
        move(ND_SIMPLE_LABEL+i,32,y+6,156,titleHeight);
        move(ND_ROW_SOURCE+i,32,secondY,156,22);
        move(ND_SIMPLE_VALUE+i,204,inputY,222,240);move(ND_SIMPLE_TEXT+i,208,inputY+4,214,22);move(ND_SIMPLE_AUTO+i,204,inputY,222,28);
        if(namingFieldDefaultSource(f)==2){move(ND_SIMPLE_AUTO+i,204,inputY,108,28);move(ND_ROW_DATE+i,320,inputY,106,28);}
        move(ND_SIMPLE_CHOICES+i,440,inputY,80,28);move(ND_SIMPLE_SETTINGS+i,528,inputY,80,28);
        move(ND_ROW_UP+i,520,secondY,24,24);move(ND_ROW_DOWN+i,552,secondY,24,24);move(ND_SIMPLE_REMOVE+i,584,secondY,24,24);
        move(ND_SIMPLE_HINT+i,complex?32:204,complex?secondY+30:secondY,complex?576:304,complex?hintHeight:22);
        afterRows=complex?secondY+28+hintHeight+8:secondY+30;
        data.rowFrames.push_back({px(20),px(y),px(620),px(afterRows)});
    }
    data.tableFrame=detail?RECT{}:RECT{px(20),px(tableTop),px(620),px(afterRows)};
    show(GetDlgItem(window,ND_INTRO),false);
    for(int id:{ND_ADD,ND_QUICK_SEPARATOR,ND_QUICK_COUNTER,ND_QUICK_FORMAT_LABEL,ND_QUICK_COUNTER_LABEL,ND_SCHEME,ND_SECTION_LOCATIONS})show(GetDlgItem(window,id),!detail);
    move(ND_ADD,464,192,156,30);
    show(GetDlgItem(window,ND_FORMAT_TITLE),!detail);
    move(ND_FORMAT_TITLE,32,afterRows+26,180,20);
    move(ND_QUICK_FORMAT_LABEL,32,afterRows+62,60,20);move(ND_QUICK_SEPARATOR,100,afterRows+56,86,240);
    move(ND_QUICK_COUNTER_LABEL,204,afterRows+62,100,20);move(ND_QUICK_COUNTER,312,afterRows+56,164,240);
    move(ND_SCHEME,488,afterRows+56,120,28);
    data.formatFrame=detail?RECT{}:RECT{px(20),px(afterRows+16),px(620),px(afterRows+96)};
    show(GetDlgItem(window,ND_EXTRA_TITLE),!detail);show(GetDlgItem(window,ND_TOOLS_LABEL),!detail);
    move(ND_EXTRA_TITLE,20,afterRows+110,440,22);move(ND_TOOLS_LABEL,20,afterRows+136,456,20);
    designerSet(window,ND_TOOLS_LABEL,designerLocationStatus(data.draft.recommendLocations,data.draft.locations.size()));
    move(ND_SECTION_LOCATIONS,488,afterRows+118,132,28);
    for(int id=ND_APPLICATION_LABEL;id<=ND_APPLICATION_DELETE;++id)show(GetDlgItem(window,id),scope);
    move(ND_APPLICATION_LABEL,20,380,112,20);move(ND_APPLICATIONS,140,374,480,240);
    move(ND_APPLICATION_ADD,20,422,136,28);move(ND_APPLICATION_REMOVE,168,422,112,28);move(ND_APPLICATION_DELETE,292,422,124,28);
    move(ND_APPLICATION_HINT,20,462,600,48);
    show(GetDlgItem(window,ND_USAGE_SUMMARY),scope);move(ND_USAGE_SUMMARY,20,528,600,116);
    for(int i=0;i<3;++i)for(int base:{ND_CHOICE_NAME,ND_CHOICE_VALUE,ND_CHOICE_DELETE})show(GetDlgItem(window,base+i),detail&&data.page==2&&static_cast<size_t>(data.choicePage*3+i)<data.choiceRows.size());
    for(int id:{ND_BASE,ND_OUTPUT,ND_CHOICES,ND_BEHAVIOR,ND_CONDITION})show(GetDlgItem(window,id),field);
    move(ND_BASE,20,308,112,28);move(ND_CHOICES,142,308,112,28);move(ND_BEHAVIOR,264,308,112,28);move(ND_OUTPUT,386,308,112,28);move(ND_CONDITION,508,308,112,28);
    int detailFooters[]{624,690,610,548,814,720,650,590};
    int footer=templates?638:simulation?simEnd+316:detail?detailFooters[data.page]:afterRows+182;
    if(detail&&data.page==0&&designerChoice(window,ND_SOURCE)==2)footer=680;
    move(ND_ERROR,20,footer-(detail?54:22),detail?456:600,detail?44:20);move(ND_FIX,490,footer-54,130,30);
    move(IDCANCEL,408,footer,88,28);move(IDOK,508,footer,112,28);
    EnableWindow(GetDlgItem(window,IDOK),data.example==data.adoptedExample);
    int height=footer+52;
    if(auto host=settingsDialog(window))host->footerHeight=px(52);
    if(sizeSettingsDialog&&sizeSettingsDialog(window,640,height,data.theme.dpi)){designerFooter(window,data);return;}
    RECT rect{0,0,px(640),px(height)};auto style=static_cast<DWORD>(GetWindowLongPtrW(window,GWL_STYLE))&~WS_VSCROLL;
    AdjustWindowRectExForDpi(&rect,style,FALSE,WS_EX_DLGMODALFRAME,data.theme.dpi);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(GetParent(window),MONITOR_DEFAULTTONEAREST),&monitor);
    int w=rect.right-rect.left,h=rect.bottom-rect.top;data.scrollMax=std::max<int>(0,h-(monitor.rcWork.bottom-monitor.rcWork.top));
    SetWindowLongPtrW(window,GWL_STYLE,style|(data.scrollMax?WS_VSCROLL:0));
    if(data.scrollMax){h-=data.scrollMax;w+=GetSystemMetricsForDpi(SM_CXVSCROLL,data.theme.dpi);}
    SCROLLINFO scroll{sizeof(scroll),SIF_RANGE|SIF_PAGE|SIF_POS};scroll.nMax=px(height)-1;scroll.nPage=px(height)-data.scrollMax;SetScrollInfo(window,SB_VERT,&scroll,TRUE);
    RECT owner{};GetWindowRect(GetParent(window),&owner);int left=std::clamp<int>((owner.left+owner.right-w)/2,monitor.rcWork.left,std::max<int>(monitor.rcWork.left,monitor.rcWork.right-w));
    int top=std::clamp<int>((owner.top+owner.bottom-h)/2,monitor.rcWork.top,std::max<int>(monitor.rcWork.top,monitor.rcWork.bottom-h));
    SetWindowPos(window,nullptr,left,top,w,h,SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);InvalidateRect(window,nullptr,TRUE);
    designerFooter(window,data);
}
