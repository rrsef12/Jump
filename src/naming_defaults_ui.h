// Included after workflow_ui.h. Both entry points use the same defaults model.
struct NamingDefaultsMenu {
    HMENU menu{};
    std::wstring scheme, application;
    std::vector<std::wstring> applications;
};
constexpr int DEFAULT_GLOBAL=52000,DEFAULT_APPLICATION=52001,DEFAULT_FOLLOW_GLOBAL=52002,
    DEFAULT_CHOOSE_APPLICATION=52003,DEFAULT_EXISTING_FIRST=52100;
std::wstring namingMenuText(const std::wstring &text){
    std::wstring result;for(auto ch:text){result+=ch;if(ch==L'&')result+=ch;}return result;
}
std::wstring namingDefaultsSummary(const std::wstring &id){
    bool global=defaultNamingScheme(g->cfg).id==id;size_t count=0;std::wstring applications;
    for(const auto &[application,scheme]:g->cfg.applicationNamingSchemes)if(scheme==id){
        if(count<2){if(!applications.empty())applications+=L"、";applications+=application;}++count;
    }
    if(count>2)applications+=L" 等 "+std::to_wstring(count)+L" 个软件";
    return L"默认："+(global?std::wstring(L"全局")+(count?L"；":L""):std::wstring())+
        (count?applications:global?L"":L"未设为默认");
}
NamingDefaultsMenu namingDefaultsMenu(const StructuredNaming &scheme){
    NamingDefaultsMenu data;data.menu=CreatePopupMenu();data.scheme=scheme.id;
    if(g->attached&&g->dialog.save)data.application=namingApplicationKey(g->dialog.process);
    const auto &schemes=namingSchemeCatalog(g->cfg);
    bool available=std::any_of(schemes.begin(),schemes.end(),[&](const auto &s){return s.id==scheme.id;});
    UINT enabled=MF_STRING|(available?0:MF_GRAYED);
    const auto &global=defaultNamingScheme(g->cfg);
    AppendMenuW(data.menu,MF_STRING|MF_GRAYED,52010,namingMenuText(L"全局默认："+global.name).c_str());
    AppendMenuW(data.menu,enabled|(global.id==scheme.id?MF_CHECKED:0),DEFAULT_GLOBAL,L"设为全局默认");
    auto application=g->cfg.applicationNamingSchemes.find(data.application);
    bool assigned=application!=g->cfg.applicationNamingSchemes.end();
    AppendMenuW(data.menu,enabled|(data.application.empty()?MF_GRAYED:0)|(assigned&&application->second==scheme.id?MF_CHECKED:0),
        DEFAULT_APPLICATION,data.application.empty()?L"设为当前软件默认（请先打开保存窗口）":namingMenuText(L"设为 "+data.application+L" 的默认").c_str());
    AppendMenuW(data.menu,MF_STRING|(data.application.empty()?MF_GRAYED:0)|(!assigned?MF_CHECKED:0),
        DEFAULT_FOLLOW_GLOBAL,data.application.empty()?L"当前软件跟随全局默认":namingMenuText(data.application+L" 跟随全局默认").c_str());
    AppendMenuW(data.menu,enabled,DEFAULT_CHOOSE_APPLICATION,L"为其他软件设置…");
    if(!g->cfg.applicationNamingSchemes.empty()){
        AppendMenuW(data.menu,MF_SEPARATOR,0,nullptr);auto applications=CreatePopupMenu();
        for(const auto &[executable,id]:g->cfg.applicationNamingSchemes){
            std::wstring name=L"方案已不可用";for(const auto &s:schemes)if(s.id==id){name=s.name;break;}
            auto submenu=CreatePopupMenu();int command=DEFAULT_EXISTING_FIRST+static_cast<int>(data.applications.size())*2;
            AppendMenuW(submenu,enabled|(id==scheme.id?MF_CHECKED:0),command,L"改用选中的方案");
            AppendMenuW(submenu,MF_STRING,command+1,L"恢复跟随全局默认");
            AppendMenuW(applications,MF_POPUP,reinterpret_cast<UINT_PTR>(submenu),namingMenuText(executable+L" · "+name).c_str());
            data.applications.push_back(executable);
        }
        AppendMenuW(data.menu,MF_POPUP,reinterpret_cast<UINT_PTR>(applications),L"已设置的软件");
    }
    return data;
}
bool handleNamingDefaultsChoice(int choice,const NamingDefaultsMenu &menu){
    if(choice<DEFAULT_GLOBAL||choice>=DEFAULT_EXISTING_FIRST+200)return false;
    std::wstring application;bool clear=false;
    if(choice==DEFAULT_APPLICATION||choice==DEFAULT_FOLLOW_GLOBAL){
        application=menu.application;if(application.empty())return true;clear=choice==DEFAULT_FOLLOW_GLOBAL;
    }else if(choice==DEFAULT_CHOOSE_APPLICATION){
        wchar_t path[32768]{};OPENFILENAMEW picker{sizeof(picker)};picker.hwndOwner=workflowOwner();
        picker.lpstrTitle=L"选择要指定默认方案的软件（.exe）";picker.lpstrFilter=L"应用程序 (*.exe)\0*.exe\0\0";
        picker.lpstrFile=path;picker.nMaxFile=32768;picker.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
        if(!GetOpenFileNameW(&picker))return true;
        application=namingApplicationKey(path);if(application.empty()){workflowNotice(L"请选择软件的 .exe 文件。");return true;}
    }else if(choice>=DEFAULT_EXISTING_FIRST){
        size_t index=(choice-DEFAULT_EXISTING_FIRST)/2;if(index>=menu.applications.size())return true;
        application=menu.applications[index];clear=(choice-DEFAULT_EXISTING_FIRST)%2!=0;
    }else if(choice!=DEFAULT_GLOBAL)return true;
    auto previous=g->cfg;
    if(clear)g->cfg.applicationNamingSchemes.erase(application);
    else if(!setDefaultNamingScheme(g->cfg,menu.scheme,application)){
        workflowNotice(L"无法设置默认方案。请先保存有效方案；最多为 100 个软件指定默认方案。");return true;
    }
    if(!persist()){g->cfg=std::move(previous);workflowNotice(L"默认方案未保存，请检查配置目录权限。");return true;}
    g->status=clear?application+L" 已跟随全局默认":application.empty()?L"已设为全局默认，新保存窗口生效":application+L" 的默认方案已更新，新保存窗口生效";
    refreshSettingsPage();if(g->attached)updatePanelContent();return true;
}
void showNamingDefaults(const StructuredNaming &scheme,HWND control){
    auto menu=namingDefaultsMenu(scheme);RECT anchor{};GetWindowRect(control,&anchor);
    auto owner=GetParent(control);SetForegroundWindow(owner);
    int choice=trackPopupMenu(menu.menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,anchor.left,anchor.bottom+px(4),owner);
    DestroyMenu(menu.menu);handleNamingDefaultsChoice(choice,menu);
}
