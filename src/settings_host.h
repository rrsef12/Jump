#pragma once
#include <windows.h>

namespace jump {
// The settings shell owns modeless child dialogs. Their callers retain their
// stack-local drafts until the host returns, just as with a native dialog.
struct SettingsDialog {
    DLGPROC procedure{};
    LPARAM data{};
    HWND window{};
    int result=IDCANCEL, scroll=0, contentHeight=0, contentWidth=0, footerHeight=0;
    bool done=false;int initialFocus=0;
    bool (*footerControl)(int)=nullptr;
    // Editors with a baseline can report their actual draft state. Other forms
    // retain the conservative leave confirmation until they opt in.
    bool unsavedChanges=true;
};
inline constexpr auto SETTINGS_DIALOG_PROPERTY=L"Jump.SettingsDialog";
inline constexpr UINT WM_SETTINGS_VIEWPORT_CHANGED=WM_APP+42;
inline INT_PTR (*runSettingsDialog)(HWND,DLGPROC,LPARAM,const wchar_t*)=nullptr;
inline bool (*sizeSettingsDialog)(HWND,int,int,int)=nullptr;
inline SettingsDialog *settingsDialog(HWND window){
    return static_cast<SettingsDialog*>(GetPropW(window,SETTINGS_DIALOG_PROPERTY));
}
inline RECT settingsBodyRect(HWND window){
    RECT client{};GetClientRect(window,&client);
    if(auto host=settingsDialog(window))client.bottom=std::max<LONG>(0,client.bottom-host->footerHeight);
    return client;
}
inline void resetSettingsScroll(HWND window){
    if(auto host=settingsDialog(window);host&&host->scroll){
        ScrollWindowEx(window,0,host->scroll,nullptr,nullptr,nullptr,nullptr,SW_SCROLLCHILDREN|SW_INVALIDATE);
        host->scroll=0;
    }
}
inline void finishSettingsDialog(HWND window,int result){
    if(auto host=settingsDialog(window)){host->result=result;host->done=true;}
    else EndDialog(window,result);
}
}
