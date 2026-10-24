#pragma once
#include "form_dialog.h"
#include "structured_naming.h"

namespace jump {
struct NamingChoiceRow {Preset preset;std::wstring original;};
struct NamingChoiceEditor {
    FormData theme;
    NamingField field;
    std::vector<NamingChoiceRow> rows;
    int page=0,rowsPerPage=6;
    bool loading=false;
};
inline std::wstring choiceEditorText(HWND window,int id){
    auto control=GetDlgItem(window,id);int length=GetWindowTextLengthW(control);
    std::wstring text(length+1,L'\0');GetWindowTextW(control,text.data(),length+1);text.resize(length);return trim(text);
}
inline void choiceEditorRead(HWND window,NamingChoiceEditor &data){
    for(int i=0;i<data.rowsPerPage;++i){size_t index=data.page*data.rowsPerPage+i;if(index>=data.rows.size())break;
        data.rows[index].preset.name=choiceEditorText(window,100+i);
        data.rows[index].preset.value=choiceEditorText(window,200+i);
    }
}
inline void choiceEditorLoad(HWND window,NamingChoiceEditor &data){
    resetSettingsScroll(window);
    data.loading=true;
    for(int i=0;i<6;++i){size_t index=data.page*data.rowsPerPage+i;bool exists=i<data.rowsPerPage&&index<data.rows.size();
        for(int base:{100,200,20})ShowWindow(GetDlgItem(window,base+i),exists?SW_SHOW:SW_HIDE);
        if(exists){SetDlgItemTextW(window,100+i,data.rows[index].preset.name.c_str());SetDlgItemTextW(window,200+i,data.rows[index].preset.value.c_str());}
    }
    EnableWindow(GetDlgItem(window,10),data.page>0);
    EnableWindow(GetDlgItem(window,11),static_cast<size_t>((data.page+1)*data.rowsPerPage)<data.rows.size());
    EnableWindow(GetDlgItem(window,12),data.rows.size()<50);
    auto page=std::to_wstring(data.page+1)+L" / "+std::to_wstring((data.rows.size()+data.rowsPerPage-1)/data.rowsPerPage);
    SetDlgItemTextW(window,13,page.c_str());SetDlgItemTextW(window,300,L"实际文字可留空，将直接使用左侧名称。");
    auto px=[&](int v){return MulDiv(v,data.theme.dpi,96);};
    int visible=std::clamp<int>(static_cast<int>(data.rows.size())-data.page*data.rowsPerPage,1,data.rowsPerPage),footer=90+visible*38;
    auto move=[&](int id,int x,int y,int width,int height){MoveWindow(GetDlgItem(window,id),px(x),px(y),px(width),px(height),TRUE);};
    move(12,20,footer,116,28);move(10,344,footer,34,28);move(13,382,footer,60,28);move(11,446,footer,34,28);
    move(300,20,footer+42,460,32);move(IDCANCEL,284,footer+88,88,28);move(IDOK,384,footer+88,96,28);
    if(sizeSettingsDialog&&sizeSettingsDialog(window,500,footer+136,data.theme.dpi)){
        data.loading=false;InvalidateRect(window,nullptr,TRUE);return;
    }
    RECT rect{0,0,px(500),px(footer+136)};AdjustWindowRectExForDpi(&rect,
        WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME,FALSE,WS_EX_DLGMODALFRAME,data.theme.dpi);
    RECT owner{};GetWindowRect(GetParent(window),&owner);MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(GetParent(window),MONITOR_DEFAULTTONEAREST),&monitor);
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    int x=std::clamp<int>((owner.left+owner.right-width)/2,monitor.rcWork.left,std::max<int>(monitor.rcWork.left,monitor.rcWork.right-width));
    int y=std::clamp<int>((owner.top+owner.bottom-height)/2,monitor.rcWork.top,std::max<int>(monitor.rcWork.top,monitor.rcWork.bottom-height));
    SetWindowPos(window,nullptr,x,y,width,height,SWP_NOACTIVATE|SWP_NOZORDER);
    data.loading=false;InvalidateRect(window,nullptr,TRUE);
}
inline INT_PTR CALLBACK namingChoiceProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto *data=reinterpret_cast<NamingChoiceEditor*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG){
        data=reinterpret_cast<NamingChoiceEditor*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);
        auto &theme=data->theme;theme.dpi=GetDpiForWindow(window);auto px=[&](int v){return MulDiv(v,theme.dpi,96);};
        MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(GetParent(window),MONITOR_DEFAULTTONEAREST),&monitor);
        data->rowsPerPage=std::clamp<int>((MulDiv(monitor.rcWork.bottom-monitor.rcWork.top,96,theme.dpi)-260)/38,1,6);
        SetWindowTextW(window,(data->field.label+L" · 常用选项").c_str());
        auto child=[&](const wchar_t *cls,const std::wstring &text,DWORD style,int id,int x,int y,int width,int height){
            auto h=CreateWindowExW(0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(theme.font),TRUE);SetWindowTheme(h,L"DarkMode_Explorer",nullptr);
            if(std::wstring(cls)==L"EDIT"){SendMessageW(h,EM_SETLIMITTEXT,120,0);SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(px(4),px(4)));}
            if(std::wstring(cls)==L"BUTTON")SetWindowSubclass(h,compactFormControl,1,0);return h;
        };
        child(L"STATIC",L"把常用内容填在这里，使用时就能直接点选。",SS_NOPREFIX,-1,20,16,460,24);
        child(L"STATIC",L"显示名称",SS_NOPREFIX,-1,24,54,190,20);child(L"STATIC",L"实际文字",SS_NOPREFIX,-1,242,54,190,20);
        for(int i=0;i<6;++i){int y=84+i*38;
            child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,100+i,24,y,194,22);
            child(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,200+i,242,y,194,22);
            child(L"BUTTON",L"×",WS_TABSTOP|BS_OWNERDRAW,20+i,452,y-2,28,26);
        }
        child(L"BUTTON",L"＋ 添加一项",WS_TABSTOP|BS_OWNERDRAW,12,20,318,116,28);
        child(L"BUTTON",L"‹",WS_TABSTOP|BS_OWNERDRAW,10,344,318,34,28);
        child(L"STATIC",L"",SS_CENTER|SS_CENTERIMAGE,13,382,318,60,28);
        child(L"BUTTON",L"›",WS_TABSTOP|BS_OWNERDRAW,11,446,318,34,28);
        child(L"STATIC",L"",SS_NOPREFIX,300,20,360,460,32);
        child(L"BUTTON",L"取消",WS_TABSTOP|BS_OWNERDRAW,IDCANCEL,284,406,88,28);
        child(L"BUTTON",L"完成",WS_TABSTOP|BS_OWNERDRAW,IDOK,384,406,96,28);
        BOOL dark=TRUE;DWORD corner=2;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));DwmSetWindowAttribute(window,33,&corner,sizeof(corner));
        DwmSetWindowAttribute(window,35,&theme.background,sizeof(COLORREF));DwmSetWindowAttribute(window,36,&theme.foreground,sizeof(COLORREF));
        SendMessageW(window,DM_SETDEFID,IDOK,0);
        choiceEditorLoad(window,*data);return TRUE;
    }
    if(!data)return FALSE;
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){finishSettingsDialog(window,IDCANCEL);return TRUE;}
    if(message==WM_DRAWITEM){auto &item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if(item.CtlType==ODT_BUTTON){drawCompactFormButton(item,data->theme,item.CtlID==IDOK);return TRUE;}}
    if(message==WM_PAINT){
        PAINTSTRUCT paint{};auto dc=BeginPaint(window,&paint);auto &theme=data->theme;auto &style=*theme.compact;
        FillRect(dc,&paint.rcPaint,theme.brush);auto pen=CreatePen(PS_SOLID,1,style.border);
        auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,style.inputBrush);
        for(int i=0;i<6;++i)for(int base:{100,200}){auto control=GetDlgItem(window,base+i);if(!IsWindowVisible(control))continue;
            RECT r{};GetWindowRect(control,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
            InflateRect(&r,MulDiv(4,theme.dpi,96),MulDiv(4,theme.dpi,96));int radius=MulDiv(6,theme.dpi,96);
            style.roundedRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
        }
        SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);EndPaint(window,&paint);return TRUE;
    }
    if(message==WM_CTLCOLORDLG||message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORBTN){
        auto &theme=data->theme;auto &style=*theme.compact;bool input=message==WM_CTLCOLOREDIT;
        SetTextColor(reinterpret_cast<HDC>(wp),input?theme.foreground:style.muted);
        SetBkColor(reinterpret_cast<HDC>(wp),input?style.inputBackground:theme.background);
        return reinterpret_cast<INT_PTR>(input?style.inputBrush:theme.brush);
    }
    if(message==WM_COMMAND&&!data->loading&&HIWORD(wp)==BN_CLICKED){
        int id=LOWORD(wp);choiceEditorRead(window,*data);
        if(id==IDOK){
            std::set<std::wstring> outputs;auto check=data->field;check.required=false;
            for(auto &row:data->rows){auto candidate=row.preset;
                if(candidate.value.empty())candidate.value=candidate.name;
                if(candidate.name.empty())candidate.name=candidate.value;
                if(candidate.value.empty())continue;
                auto error=namingFieldError(check,candidate.value);
                if(candidate.name.size()>80)error=L"显示名称最多 80 个字符";
                if(!outputs.insert(namingFieldOutput(check,candidate.value)).second)error=L"实际文字有重复，请合并或修改";
                if(!error.empty()){SetDlgItemTextW(window,300,error.c_str());return TRUE;}
            }
            finishSettingsDialog(window,IDOK);return TRUE;
        }
        if(id==10&&data->page>0)--data->page;
        if(id==11&&static_cast<size_t>((data->page+1)*data->rowsPerPage)<data->rows.size())++data->page;
        if(id==12&&data->rows.size()<50){data->rows.push_back({});data->page=static_cast<int>((data->rows.size()-1)/data->rowsPerPage);}
        if(id>=20&&id<26){size_t index=data->page*data->rowsPerPage+id-20;
            if(index<data->rows.size())data->rows.erase(data->rows.begin()+index);
            if(data->rows.empty())data->rows.push_back({});
            data->page=std::min(data->page,static_cast<int>((data->rows.size()-1)/data->rowsPerPage));
        }
        choiceEditorLoad(window,*data);return TRUE;
    }
    return FALSE;
}
inline std::wstring validateNamingChoices(const NamingField &field,const std::vector<NamingChoiceRow> &rows){
    std::set<std::wstring> outputs;auto check=field;check.required=false;
    for(const auto &row:rows){auto candidate=row.preset;
        if(candidate.value.empty())candidate.value=candidate.name;
        if(candidate.name.empty())candidate.name=candidate.value;
        if(candidate.value.empty())continue;
        auto error=namingFieldError(check,candidate.value);
        if(candidate.name.size()>80)error=L"显示名称最多 80 个字符";
        if(!outputs.insert(namingFieldOutput(check,candidate.value)).second)error=L"实际文字有重复，请合并或修改";
        if(!error.empty())return error;
    }
    return L"";
}
inline void applyNamingChoices(StructuredNaming &scheme,size_t index,std::vector<NamingChoiceRow> rows){
    auto &field=scheme.fields[index];auto originalDefault=field.defaultValue;auto originalFields=scheme.fields;
    auto originalLocations=scheme.locations;field.candidates.clear();
    for(auto &row:rows){auto &candidate=row.preset;
        if(candidate.value.empty())candidate.value=candidate.name;
        if(candidate.name.empty())candidate.name=candidate.value;
        if(candidate.value.empty())continue;
        if(!row.original.empty()&&row.original!=candidate.value){
            if(originalDefault==row.original)field.value=field.defaultValue=candidate.value;
            auto from=namingFieldOutput(field,row.original),to=namingFieldOutput(field,candidate.value);
            for(size_t i=0;i<scheme.fields.size();++i)if(originalFields[i].conditionField==field.id&&originalFields[i].conditionValue==from)
                scheme.fields[i].conditionValue=to;
            for(size_t i=0;i<scheme.locations.size();++i)if(originalLocations[i].field==field.id&&originalLocations[i].value==from)scheme.locations[i].value=to;
        }
        field.candidates.push_back(std::move(candidate));
    }
}
inline bool editNamingChoices(HWND owner,StructuredNaming &scheme,size_t index,const FormData &theme){
    NamingChoiceEditor data;data.theme=theme;data.field=scheme.fields[index];
    for(const auto &candidate:data.field.candidates)data.rows.push_back({candidate,candidate.value});
    if(data.rows.empty())data.rows.push_back({});
    struct Template{DLGTEMPLATE dialog;WORD menu,windowClass,title;} layout{};
    layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|WS_CLIPCHILDREN;
    layout.dialog.dwExtendedStyle=WS_EX_DLGMODALFRAME;layout.dialog.cx=300;layout.dialog.cy=280;
    if((runSettingsDialog?runSettingsDialog(owner,namingChoiceProcedure,reinterpret_cast<LPARAM>(&data),(data.field.label+L" · 常用选项").c_str()):
        DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&layout.dialog,owner,namingChoiceProcedure,reinterpret_cast<LPARAM>(&data)))!=IDOK)return false;
    applyNamingChoices(scheme,index,std::move(data.rows));
    return true;
}
} // namespace jump
