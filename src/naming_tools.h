#pragma once
#include "form_dialog.h"
#include "structured_naming.h"
#include <shobjidl.h>

namespace jump {
inline std::wstring namingToolText(HWND window,int id){
    auto control=GetDlgItem(window,id);int length=GetWindowTextLengthW(control);
    std::wstring text(length+1,L'\0');GetWindowTextW(control,text.data(),length+1);text.resize(length);return text;
}
inline HWND namingToolChild(HWND window,const FormData &theme,const wchar_t *cls,const std::wstring &text,DWORD flags,int id,int x,int y,int width,int height){
    auto px=[&](int v){return MulDiv(v,theme.dpi,96);};
    auto control=CreateWindowExW(0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|flags,px(x),px(y),px(width),px(height),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(theme.font),FALSE);
    SetWindowTheme(control,L"DarkMode_Explorer",nullptr);
    if(std::wstring_view(cls)==L"BUTTON")SetWindowSubclass(control,compactFormControl,1,0);
    if(std::wstring_view(cls)==L"EDIT"){
        SendMessageW(control,EM_SETLIMITTEXT,8192,0);
        SendMessageW(control,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(px(4),px(4)));
    }
    if(std::wstring_view(cls)==L"COMBOBOX"&&theme.compact->dropdown)theme.compact->dropdown(control);
    return control;
}
inline void namingToolSize(HWND window,FormData &theme,int width,int height){
    theme.dpi=GetDpiForWindow(window);
    BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));
    if(sizeSettingsDialog&&sizeSettingsDialog(window,width,height,theme.dpi))return;
    RECT r{0,0,MulDiv(width,theme.dpi,96),MulDiv(height,theme.dpi,96)};
    AdjustWindowRectExForDpi(&r,WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME,FALSE,WS_EX_DLGMODALFRAME,theme.dpi);
    RECT owner{};GetWindowRect(GetParent(window),&owner);MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(GetParent(window),MONITOR_DEFAULTTONEAREST),&monitor);
    width=r.right-r.left;height=r.bottom-r.top;
    int x=std::clamp<int>((owner.left+owner.right-width)/2,monitor.rcWork.left,std::max<int>(monitor.rcWork.left,monitor.rcWork.right-width));
    int y=std::clamp<int>((owner.top+owner.bottom-height)/2,monitor.rcWork.top,std::max<int>(monitor.rcWork.top,monitor.rcWork.bottom-height));
    SetWindowPos(window,nullptr,x,y,width,height,SWP_NOACTIVATE|SWP_NOZORDER);
}
inline bool namingToolPaint(HWND window,UINT message,WPARAM wp,LPARAM lp,const FormData &theme,INT_PTR &result){
    if(message==WM_DRAWITEM){auto &item=*reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if(item.CtlType==ODT_BUTTON){drawCompactFormButton(item,theme,item.CtlID==IDOK);result=TRUE;return true;}}
    if(message==WM_CTLCOLORDLG||message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX){
        auto dc=reinterpret_cast<HDC>(wp);bool input=message==WM_CTLCOLOREDIT||message==WM_CTLCOLORLISTBOX||
            (message==WM_CTLCOLORSTATIC&&(GetWindowLongPtrW(reinterpret_cast<HWND>(lp),GWL_STYLE)&ES_READONLY));
        SetTextColor(dc,input?theme.foreground:theme.compact->muted);
        SetBkColor(dc,input?theme.compact->inputBackground:theme.background);
        result=reinterpret_cast<INT_PTR>(input?theme.compact->inputBrush:theme.brush);return true;
    }
    if(message==WM_PAINT){
        PAINTSTRUCT paint{};auto dc=BeginPaint(window,&paint);FillRect(dc,&paint.rcPaint,theme.brush);
        auto pen=CreatePen(PS_SOLID,1,theme.compact->border);auto old=SelectObject(dc,pen),brush=SelectObject(dc,theme.compact->inputBrush);
        for(auto control=GetWindow(window,GW_CHILD);control;control=GetWindow(control,GW_HWNDNEXT)){
            if(!(GetWindowLongPtrW(control,GWL_STYLE)&WS_VISIBLE))continue;
            wchar_t cls[32]{};GetClassNameW(control,cls,32);
            if(wcscmp(cls,L"Edit")&&wcscmp(cls,L"ListBox"))continue;
            RECT r{};GetWindowRect(control,&r);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&r),2);
            int gap=MulDiv(4,theme.dpi,96);InflateRect(&r,gap,gap);
            theme.compact->roundedRect(dc,r.left,r.top,r.right,r.bottom,gap*2,gap*2);
        }
        SelectObject(dc,brush);SelectObject(dc,old);DeleteObject(pen);EndPaint(window,&paint);result=TRUE;return true;
    }
    return false;
}
inline INT_PTR runNamingTool(HWND owner,DLGPROC procedure,LPARAM data,const wchar_t *title){
    if(runSettingsDialog)return runSettingsDialog(owner,procedure,data,title);
    struct Template{DLGTEMPLATE dialog;WORD menu,windowClass,title;} layout{};
    layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|WS_CLIPCHILDREN;
    layout.dialog.dwExtendedStyle=WS_EX_DLGMODALFRAME;layout.dialog.cx=400;layout.dialog.cy=300;
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&layout.dialog,owner,procedure,data);
}
struct NamingTrialItem {std::wstring original,name,error,explanation;int errorField=-1,settingsPage=0;};
struct NamingTrialPreview {std::vector<NamingTrialItem> items;size_t total=0;};
inline NamingTrialPreview namingTrialPreview(const StructuredNaming &draft,const Config &config,const std::wstring &input){
    std::wistringstream lines(input);std::wstring sample;NamingTrialPreview result;
    while(std::getline(lines,sample)){
        sample=trim(sample);if(sample.empty())continue;
        if(++result.total>20)continue;
        NamingTrialItem item;item.original=sample;
        if(!validName(sample)){
            if(sample.size()>240)item.error=L"名字太长，请缩短后再试。";
            else if(sample.find_first_of(L"\\/:")!=sample.npos)item.error=L"只填文件名，不要带文件夹路径。";
            else if(sample.back()==L'.')item.error=L"名字末尾不能是句点，请去掉。";
            else if(sample.find_first_of(L"*?\"<>|")!=sample.npos)item.error=L"名字里有 * ? \" < > | 等不能使用的字符。";
            else item.error=L"这个名字无法使用，请换一个名字。";
            item.explanation=L"原文件名："+sample+L"\r\n\r\n需要修改："+item.error+L"\r\n直接修改左侧对应的那一行即可。";
            result.items.push_back(std::move(item));continue;
        }
        std::filesystem::path path(sample);auto extension=path.extension().wstring();
        auto session=namingSession(draft,extension.empty()?sample:path.stem().wstring());
        auto preview=composeStructure(session,config);
        if(preview.error.empty()&&draft.counter){SmartNaming options{draft.counter};options.digits=draft.digits;options.upperVersion=draft.upper;
            preview.name=numberingExample(preview.name,options);if(preview.name.empty())preview.error=L"名字太长或编号已用完，请返回方案调整。";}
        item.error=preview.error;item.name=preview.name+extension;
        if(item.error.empty()&&!validName(item.name))item.error=L"新名字太长，请返回方案减少命名内容。";
        std::wstring used,hidden,empty;
        for(size_t i=0;i<session.fields.size();++i){const auto &field=session.fields[i];
            auto value=fieldValue(field,config,session.dateFormat);
            if(!field.enabled)hidden+=field.label+L"：已在方案中关闭，可从这项的“设置 → 基本设置”重新启用。\r\n";
            else if(!namingFieldVisible(session,i,config)){
                auto source=std::find_if(session.fields.begin(),session.fields.end(),[&](const auto &f){return f.id==field.conditionField;});
                hidden+=field.label+L"：只有“"+(source==session.fields.end()?field.conditionField:source->label)+L"”为“"+field.conditionValue+L"”时才会加入，目前不符合。\r\n";
            }else{
                auto error=namingFieldError(field,value);
                if(!error.empty()){
                    auto reason=value.empty()?L"请返回方案填写“"+field.label+L"”。":error+L"，请返回方案修改这一项。";
                    if(item.errorField<0){item.error=reason;item.errorField=static_cast<int>(i);item.settingsPage=value.empty()?0:1;}
                    hidden+=field.label+L"："+reason+L"\r\n";
                }else if(value.empty())empty+=(empty.empty()?L"":L"、")+field.label;
                else{
                    used+=field.label+L"："+namingFieldOutput(field,value);
                    if(field.id==session.primaryField&&field.source==0)used+=L"（来自左侧的原名）";
                    else if(field.source==1)used+=L"（来自当前项目）";
                    else if(field.source==2)used+=L"（今天的日期）";
                    else if(field.source==3)used+=L"（方案里的固定文字）";
                    used+=L"\r\n";
                }
            }
        }
        item.explanation=L"原文件名："+sample+L"\r\n"+(item.error.empty()?L"新文件名："+item.name:L"需要修改："+item.error)+L"\r\n\r\n名字由这些内容组成：\r\n"+used;
        if(draft.counter)item.explanation+=L"自动编号：这里使用示例编号，保存时会按目标文件夹重新计算。\r\n";
        if(!extension.empty())item.explanation+=L"文件后缀："+extension+L"（保留左侧原名的后缀）\r\n";
        if(!empty.empty())hidden+=empty+L"：没有填写，所以不加入名字。\r\n";
        if(!hidden.empty())item.explanation+=L"\r\n没有加入名字的内容：\r\n"+hidden;
        auto locations=namingRecommendedLocations(session,config);
        for(const auto &location:locations)item.explanation+=L"\r\n推荐文件夹："+location.path+L"（"+location.sources+L"）\r\n";
        if(!draft.recommendLocations)item.explanation+=L"\r\n目录推荐已关闭，可在方案设置的“目录推荐”中开启。";
        else if(locations.empty()&&!draft.locations.empty())item.explanation+=L"\r\n当前没有匹配的推荐文件夹；项目内的目录需要先选择项目。";
        result.items.push_back(std::move(item));
    }
    return result;
}
inline std::wstring namingTrialExamples(const std::wstring &original){
    return (trim(original).empty()?L"":original+L"\r\n")+std::wstring(L"守卫.fbx\r\n石块.png");
}
enum NamingTrialId {NT_INPUT=4600,NT_RESULTS,NT_SUMMARY,NT_DETAILS,NT_TOGGLE,NT_EXAMPLES,NT_CLEAR,NT_NOTE,NT_EMPTY,NT_TITLE,NT_INPUT_LABEL,NT_RESULT_LABEL,NT_SELECTION_HINT,NT_FIX};
struct NamingTrial {
    FormData theme;StructuredNaming draft;const Config *config{};std::wstring input,examples;
    NamingTrialPreview preview;bool loading=false,expanded=false;HFONT headingFont{};
    std::pair<int,int> editTarget{-1,0};
};
inline void selectNamingTrialResult(HWND window,NamingTrial &data){
    int index=static_cast<int>(SendDlgItemMessageW(window,NT_RESULTS,LB_GETCURSEL,0,0));
    auto explanation=index>=0&&static_cast<size_t>(index)<data.preview.items.size()?data.preview.items[index].explanation:L"先在左侧填写一个文件名，即可查看它的组成。";
    SetDlgItemTextW(window,NT_DETAILS,explanation.c_str());
    bool fix=index>=0&&static_cast<size_t>(index)<data.preview.items.size()&&data.preview.items[index].errorField>=0;
    ShowWindow(GetDlgItem(window,NT_FIX),fix?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(window,NT_SELECTION_HINT),!data.preview.items.empty()&&!fix?SW_SHOW:SW_HIDE);
}
inline void updateNamingTrial(HWND window,NamingTrial &data){
    data.input=namingToolText(window,NT_INPUT);data.preview=namingTrialPreview(data.draft,*data.config,data.input);
    auto list=GetDlgItem(window,NT_RESULTS);int selected=static_cast<int>(SendMessageW(list,LB_GETCURSEL,0,0));
    SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
    size_t errors=0;for(const auto &item:data.preview.items){
        auto text=L"原名："+item.original+L"\n"+(item.error.empty()?L"新名："+item.name:L"需要修改："+item.error);
        SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));if(!item.error.empty())++errors;
    }
    if(!data.preview.items.empty())SendMessageW(list,LB_SETCURSEL,std::clamp(selected,0,static_cast<int>(data.preview.items.size()-1)),0);
    SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,nullptr,TRUE);
    std::wstring summary=L"填好后，这里会显示新名字";
    if(data.preview.total>20)summary=L"共 "+std::to_wstring(data.preview.total)+L" 个，只显示前 20 个；请删去多余名字";
    else if(data.preview.total)summary=std::to_wstring(data.preview.total)+L" 个名字 · "+std::to_wstring(data.preview.total-errors)+L" 个可生成"+(errors?L" · "+std::to_wstring(errors)+L" 个需修改":L"");
    SetDlgItemTextW(window,NT_SUMMARY,summary.c_str());
    ShowWindow(GetDlgItem(window,NT_EMPTY),data.preview.items.empty()?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(window,NT_SELECTION_HINT),data.preview.items.empty()?SW_HIDE:SW_SHOW);
    EnableWindow(GetDlgItem(window,NT_CLEAR),!data.input.empty());EnableWindow(GetDlgItem(window,NT_TOGGLE),data.expanded||!data.preview.items.empty());
    selectNamingTrialResult(window,data);
}
inline void layoutNamingTrial(HWND window,NamingTrial &data){
    auto px=[&](int value){return MulDiv(value,data.theme.dpi,96);};int extra=data.expanded?122:0;
    ShowWindow(GetDlgItem(window,NT_DETAILS),data.expanded?SW_SHOW:SW_HIDE);
    SetDlgItemTextW(window,NT_TOGGLE,data.expanded?L"收起名字组成 ▴":L"查看名字组成 ▾");
    SetWindowPos(GetDlgItem(window,NT_NOTE),nullptr,px(20),px(392+extra),px(600),px(36),SWP_NOACTIVATE|SWP_NOZORDER);
    SetWindowPos(GetDlgItem(window,IDCANCEL),nullptr,px(492),px(448+extra),px(128),px(28),SWP_NOACTIVATE|SWP_NOZORDER);
    namingToolSize(window,data.theme,640,488+extra);InvalidateRect(window,nullptr,TRUE);
}
inline void drawNamingTrialResult(const DRAWITEMSTRUCT &item,const NamingTrial &data){
    auto &theme=data.theme;auto px=[&](int value){return MulDiv(value,theme.dpi,96);};
    auto brush=CreateSolidBrush(item.itemState&ODS_SELECTED?theme.compact->surface:theme.compact->inputBackground);
    FillRect(item.hDC,&item.rcItem,brush);DeleteObject(brush);
    if(item.itemID>=data.preview.items.size())return;const auto &row=data.preview.items[item.itemID];
    auto old=SelectObject(item.hDC,theme.compact->hintFont);SetBkMode(item.hDC,TRANSPARENT);SetTextColor(item.hDC,theme.compact->muted);
    RECT text=item.rcItem;text.left+=px(10);text.right-=px(10);text.top+=px(5);text.bottom=text.top+px(22);
    auto source=L"原名  "+row.original;DrawTextW(item.hDC,source.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER|DT_NOPREFIX);
    text.top+=px(24);text.bottom=text.top+px(24);SelectObject(item.hDC,theme.font);
    SetTextColor(item.hDC,row.error.empty()?theme.foreground:RGB(224,157,147));
    auto output=row.error.empty()?L"新名  "+row.name:L"需修改  "+row.error;
    DrawTextW(item.hDC,output.c_str(),-1,&text,DT_SINGLELINE|DT_END_ELLIPSIS|DT_VCENTER|DT_NOPREFIX);
    if(item.itemState&ODS_FOCUS){auto rect=item.rcItem;InflateRect(&rect,-px(1),-px(1));DrawFocusRect(item.hDC,&rect);}
    SelectObject(item.hDC,old);
}
inline INT_PTR CALLBACK namingTrialProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto data=reinterpret_cast<NamingTrial*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG){
        data=reinterpret_cast<NamingTrial*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);data->loading=true;data->theme.dpi=GetDpiForWindow(window);
        SetWindowTextW(window,L"文件名效果预览");auto &theme=data->theme;
        LOGFONTW font{};if(GetObjectW(theme.font,sizeof(font),&font)){font.lfWeight=FW_SEMIBOLD;data->headingFont=CreateFontIndirectW(&font);}
        auto title=namingToolChild(window,theme,L"STATIC",L"看看“"+data->draft.name+L"”会生成什么文件名",SS_NOPREFIX|SS_ENDELLIPSIS,NT_TITLE,20,14,600,26);
        if(data->headingFont)SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(data->headingFont),FALSE);
        namingToolChild(window,theme,L"STATIC",L"左边输入原来的名字，右边立即看到新名字。这里只预览，不会改动文件。",SS_NOPREFIX,-1,20,48,600,34);
        namingToolChild(window,theme,L"STATIC",L"1. 输入原文件名",SS_NOPREFIX,NT_INPUT_LABEL,20,94,196,22);
        namingToolChild(window,theme,L"STATIC",L"2. 查看新文件名",SS_NOPREFIX,NT_RESULT_LABEL,238,94,382,22);
        auto hint=namingToolChild(window,theme,L"STATIC",L"每行一个，最多 20 个；可粘贴",SS_NOPREFIX,-1,20,120,196,24);
        SendMessageW(hint,WM_SETFONT,reinterpret_cast<WPARAM>(theme.compact->hintFont),FALSE);
        auto summary=namingToolChild(window,theme,L"STATIC",L"",SS_NOPREFIX,NT_SUMMARY,238,120,382,24);
        SendMessageW(summary,WM_SETFONT,reinterpret_cast<WPARAM>(theme.compact->hintFont),FALSE);
        namingToolChild(window,theme,L"EDIT",data->input,WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN|WS_VSCROLL,NT_INPUT,24,148,188,180);
        auto list=namingToolChild(window,theme,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,NT_RESULTS,242,148,374,180);
        SendMessageW(list,LB_SETITEMHEIGHT,0,MulDiv(60,theme.dpi,96));
        namingToolChild(window,theme,L"STATIC",L"新名字会显示在这里\n先在左侧输入，或点击“填入示例”。",SS_CENTER|SS_NOPREFIX,NT_EMPTY,254,208,350,60);
        namingToolChild(window,theme,L"BUTTON",L"填入示例",WS_TABSTOP|BS_OWNERDRAW,NT_EXAMPLES,20,348,104,28);
        namingToolChild(window,theme,L"BUTTON",L"清空",WS_TABSTOP|BS_OWNERDRAW,NT_CLEAR,136,348,80,28);
        namingToolChild(window,theme,L"BUTTON",L"查看名字组成 ▾",WS_TABSTOP|BS_OWNERDRAW,NT_TOGGLE,238,348,176,28);
        auto selectionHint=namingToolChild(window,theme,L"STATIC",L"选中结果可查看完整名字",SS_NOPREFIX|SS_CENTERIMAGE,NT_SELECTION_HINT,426,348,194,28);
        SendMessageW(selectionHint,WM_SETFONT,reinterpret_cast<WPARAM>(theme.compact->hintFont),FALSE);
        namingToolChild(window,theme,L"BUTTON",L"修改这项设置…",WS_TABSTOP|BS_OWNERDRAW,NT_FIX,426,348,194,28);
        auto details=namingToolChild(window,theme,L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,NT_DETAILS,24,396,592,96);
        SendMessageW(details,EM_SETLIMITTEXT,100000,0);ShowWindow(details,SW_HIDE);
        auto note=namingToolChild(window,theme,L"STATIC",(data->draft.counter?L"自动编号仅供预览，保存时会按目标文件夹重新计算。\n":L"调整命名内容时，请返回方案修改，再来查看效果。\n")+std::wstring(L".fbx 等文件后缀会保留；实际保存格式仍由原软件决定。"),SS_NOPREFIX,NT_NOTE,20,392,600,36);
        SendMessageW(note,WM_SETFONT,reinterpret_cast<WPARAM>(theme.compact->hintFont),FALSE);
        namingToolChild(window,theme,L"BUTTON",L"返回修改方案",WS_TABSTOP|BS_OWNERDRAW,IDCANCEL,492,448,128,28);
        updateNamingTrial(window,*data);layoutNamingTrial(window,*data);data->loading=false;return TRUE;
    }
    if(!data)return FALSE;
    if(message==WM_DRAWITEM&&reinterpret_cast<DRAWITEMSTRUCT*>(lp)->CtlID==NT_RESULTS){drawNamingTrialResult(*reinterpret_cast<DRAWITEMSTRUCT*>(lp),*data);return TRUE;}
    if(message==WM_CTLCOLORSTATIC){auto id=GetDlgCtrlID(reinterpret_cast<HWND>(lp));
        if(id==NT_EMPTY){auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,data->theme.compact->muted);SetBkColor(dc,data->theme.compact->inputBackground);return reinterpret_cast<INT_PTR>(data->theme.compact->inputBrush);}
        if(id==NT_TITLE||id==NT_INPUT_LABEL||id==NT_RESULT_LABEL){auto dc=reinterpret_cast<HDC>(wp);SetTextColor(dc,data->theme.foreground);SetBkColor(dc,data->theme.background);return reinterpret_cast<INT_PTR>(data->theme.brush);}}
    INT_PTR result{};if(namingToolPaint(window,message,wp,lp,data->theme,result))return result;
    if(message==WM_COMMAND&&!data->loading){auto id=LOWORD(wp),code=HIWORD(wp);
        if(id==NT_INPUT&&code==EN_CHANGE){SetTimer(window,1,180,nullptr);return TRUE;}
        if(id==NT_RESULTS&&code==LBN_SELCHANGE){selectNamingTrialResult(window,*data);return TRUE;}
        if(id==NT_FIX){
            int index=static_cast<int>(SendDlgItemMessageW(window,NT_RESULTS,LB_GETCURSEL,0,0));
            if(index>=0&&static_cast<size_t>(index)<data->preview.items.size()){
                const auto &item=data->preview.items[index];if(item.errorField>=0){
                    data->editTarget={item.errorField,item.settingsPage};data->input=namingToolText(window,NT_INPUT);
                    KillTimer(window,1);finishSettingsDialog(window,IDOK);
                }
            }return TRUE;
        }
        if(id==NT_TOGGLE){data->expanded=!data->expanded;layoutNamingTrial(window,*data);return TRUE;}
        if(id==NT_EXAMPLES||id==NT_CLEAR){
            data->loading=true;SetDlgItemTextW(window,NT_INPUT,id==NT_EXAMPLES?data->examples.c_str():L"");data->loading=false;
            KillTimer(window,1);updateNamingTrial(window,*data);SetFocus(GetDlgItem(window,NT_INPUT));return TRUE;}
    }
    if(message==WM_TIMER&&wp==1){KillTimer(window,1);updateNamingTrial(window,*data);return TRUE;}
    if(message==WM_CLOSE||(message==WM_COMMAND&&(LOWORD(wp)==IDCANCEL||LOWORD(wp)==IDOK))){
        KillTimer(window,1);data->input=namingToolText(window,NT_INPUT);finishSettingsDialog(window,IDCANCEL);return TRUE;}
    if(message==WM_DESTROY&&data->headingFont){DeleteObject(data->headingFont);data->headingFont=nullptr;}
    return FALSE;
}
inline std::pair<int,int> showNamingTrial(HWND owner,const StructuredNaming &draft,const Config &config,const FormData &theme,std::wstring &input,const std::wstring &original){
    NamingTrial data{theme,draft,&config,input,namingTrialExamples(original)};
    runNamingTool(owner,namingTrialProcedure,reinterpret_cast<LPARAM>(&data),L"文件名效果预览");input=data.input;return data.editTarget;
}

struct NamingLocationsEditor {FormData theme;StructuredNaming draft;int selected=-1;bool loading=false;};
inline void loadNamingLocationValues(HWND window,NamingLocationsEditor &data){
    int index=static_cast<int>(SendDlgItemMessageW(window,4701,CB_GETCURSEL,0,0));auto combo=GetDlgItem(window,4702);
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    if(index>=0&&static_cast<size_t>(index)<data.draft.fields.size())for(const auto &candidate:data.draft.fields[index].candidates){
        auto value=namingFieldOutput(data.draft.fields[index],candidate.value);SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value.c_str()));}
}
inline void loadNamingLocations(HWND window,NamingLocationsEditor &data){
    data.loading=true;auto list=GetDlgItem(window,4700);SendMessageW(list,LB_RESETCONTENT,0,0);
    for(const auto &rule:data.draft.locations){
        auto field=std::find_if(data.draft.fields.begin(),data.draft.fields.end(),[&](const auto &f){return f.id==rule.field;});
        auto text=(field==data.draft.fields.end()?rule.field:field->label)+L" = "+rule.value+L" → "+rule.directory;
        SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
    }
    EnableWindow(GetDlgItem(window,4705),data.selected>=0);
    if(data.selected>=0&&static_cast<size_t>(data.selected)<data.draft.locations.size()){
        SendMessageW(list,LB_SETCURSEL,data.selected,0);const auto &rule=data.draft.locations[data.selected];
        auto field=std::find_if(data.draft.fields.begin(),data.draft.fields.end(),[&](const auto &f){return f.id==rule.field;});
        SendDlgItemMessageW(window,4701,CB_SETCURSEL,field-data.draft.fields.begin(),0);loadNamingLocationValues(window,data);
        SetDlgItemTextW(window,4702,rule.value.c_str());SetDlgItemTextW(window,4703,rule.directory.c_str());
    }
    SetDlgItemTextW(window,4704,data.selected<0?L"添加规则":L"更新这一条");data.loading=false;
}
inline INT_PTR CALLBACK namingLocationsProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto data=reinterpret_cast<NamingLocationsEditor*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG){
        data=reinterpret_cast<NamingLocationsEditor*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);data->theme.dpi=GetDpiForWindow(window);
        auto &theme=data->theme;SetWindowTextW(window,L"目录推荐");
        namingToolChild(window,theme,L"STATIC",L"当命名内容匹配时推荐目录；点击推荐后才切换，不会自动创建文件夹。",SS_NOPREFIX,-1,20,16,600,24);
        namingToolChild(window,theme,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,4700,24,52,592,130);
        namingToolChild(window,theme,L"STATIC",L"命名内容",SS_NOPREFIX,-1,20,202,260,20);
        namingToolChild(window,theme,L"STATIC",L"等于（实际输出文字）",SS_NOPREFIX,-1,320,202,300,20);
        auto field=namingToolChild(window,theme,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,4701,20,226,280,220);
        for(const auto &f:data->draft.fields)SendMessageW(field,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(f.label.c_str()));SendMessageW(field,CB_SETCURSEL,0,0);
        namingToolChild(window,theme,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWN|CBS_AUTOHSCROLL,4702,320,226,300,220);
        namingToolChild(window,theme,L"STATIC",L"推荐文件夹 · 完整路径，或相对当前项目根目录的路径",SS_NOPREFIX,-1,20,270,600,20);
        auto path=namingToolChild(window,theme,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,4703,24,298,488,24);SendMessageW(path,EM_SETLIMITTEXT,1024,0);
        namingToolChild(window,theme,L"BUTTON",L"选择…",WS_TABSTOP|BS_OWNERDRAW,4706,532,294,88,30);
        namingToolChild(window,theme,L"STATIC",L"例如：文件类别 = SM → Models；当前状态 = 交付 → delivery。",SS_NOPREFIX,-1,20,336,600,20);
        namingToolChild(window,theme,L"BUTTON",L"添加规则",WS_TABSTOP|BS_OWNERDRAW,4704,20,370,116,30);
        namingToolChild(window,theme,L"BUTTON",L"新增一条",WS_TABSTOP|BS_OWNERDRAW,4707,148,370,100,30);
        namingToolChild(window,theme,L"BUTTON",L"删除选中",WS_TABSTOP|BS_OWNERDRAW,4705,260,370,100,30);
        namingToolChild(window,theme,L"STATIC",L"最多 20 条；规则随方案复制与导出。相对目录需要先选择项目。",SS_NOPREFIX,4708,20,414,600,40);
        namingToolChild(window,theme,L"BUTTON",L"取消",WS_TABSTOP|BS_OWNERDRAW,IDCANCEL,412,466,88,30);
        namingToolChild(window,theme,L"BUTTON",L"完成",WS_TABSTOP|BS_OWNERDRAW,IDOK,512,466,108,30);
        namingToolSize(window,theme,640,516);loadNamingLocationValues(window,*data);loadNamingLocations(window,*data);return TRUE;
    }
    if(!data)return FALSE;INT_PTR result{};if(namingToolPaint(window,message,wp,lp,data->theme,result))return result;
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){finishSettingsDialog(window,IDCANCEL);return TRUE;}
    if(message==WM_COMMAND&&!data->loading){int id=LOWORD(wp),code=HIWORD(wp);
        if(id==4700&&code==LBN_SELCHANGE){data->selected=static_cast<int>(SendDlgItemMessageW(window,4700,LB_GETCURSEL,0,0));loadNamingLocations(window,*data);return TRUE;}
        if(id==4701&&code==CBN_SELCHANGE){loadNamingLocationValues(window,*data);return TRUE;}
        if(id==4706){
            IFileOpenDialog *picker=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&picker)))){
                DWORD options{};picker->GetOptions(&options);picker->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR);
                if(SUCCEEDED(picker->Show(GetAncestor(window,GA_ROOT)))){IShellItem *item=nullptr;if(SUCCEEDED(picker->GetResult(&item))){
                    PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){SetDlgItemTextW(window,4703,path);CoTaskMemFree(path);}item->Release();}}
                picker->Release();}return TRUE;
        }
        if(id==4707){data->selected=-1;SetDlgItemTextW(window,4702,L"");SetDlgItemTextW(window,4703,L"");loadNamingLocations(window,*data);SetFocus(GetDlgItem(window,4702));return TRUE;}
        if(id==4705&&data->selected>=0){data->draft.locations.erase(data->draft.locations.begin()+data->selected);data->selected=-1;SetDlgItemTextW(window,4702,L"");SetDlgItemTextW(window,4703,L"");loadNamingLocations(window,*data);return TRUE;}
        if(id==4704){
            int field=static_cast<int>(SendDlgItemMessageW(window,4701,CB_GETCURSEL,0,0));if(field<0)return TRUE;
            NamingLocationRule rule{data->draft.fields[field].id,trim(namingToolText(window,4702)),trim(namingToolText(window,4703))};
            auto next=data->draft;if(data->selected>=0)next.locations[data->selected]=rule;else next.locations.push_back(rule);
            auto error=namingSchemaError(next);if(!error.empty()){SetDlgItemTextW(window,4708,error.c_str());return TRUE;}
            data->draft=std::move(next);data->selected=-1;loadNamingLocations(window,*data);
            SetDlgItemTextW(window,4708,L"规则已加入草稿；完成后还需保存方案。可继续添加其他规则。");return TRUE;
        }
        if(id==IDOK){
            // A half-filled row must not disappear silently when closing this editor.
            auto value=trim(namingToolText(window,4702)),path=trim(namingToolText(window,4703));
            if(!value.empty()||!path.empty()){
                int field=static_cast<int>(SendDlgItemMessageW(window,4701,CB_GETCURSEL,0,0));
                bool saved=std::any_of(data->draft.locations.begin(),data->draft.locations.end(),[&](const auto &r){return field>=0&&r.field==data->draft.fields[field].id&&r.value==value&&r.directory==path;});
                if(!saved){SetDlgItemTextW(window,4708,data->selected<0?L"这一条尚未添加，请点击“添加规则”；或点击“新增一条”清空输入。":L"这一条有未保存的修改，请点击“更新这一条”。");return TRUE;}
            }
            finishSettingsDialog(window,IDOK);return TRUE;
        }
    }
    return FALSE;
}
inline bool editNamingLocations(HWND owner,StructuredNaming &draft,const FormData &theme){
    NamingLocationsEditor data{theme,draft};
    if(runNamingTool(owner,namingLocationsProcedure,reinterpret_cast<LPARAM>(&data),L"目录推荐")!=IDOK)return false;
    draft.locations=std::move(data.draft.locations);return true;
}
} // namespace jump
