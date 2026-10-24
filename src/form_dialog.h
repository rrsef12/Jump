#pragma once
#include <commctrl.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include "settings_host.h"

namespace jump {
struct FormField {
    std::wstring label, value;
    bool multiline = false, readOnly = false;
};
struct PopupMenuHint {
    HMENU menu{};
    UINT position{};
    std::wstring text;
};
struct CompactFormStyle {
    HFONT hintFont{};
    HBRUSH inputBrush{};
    COLORREF inputBackground{}, muted{}, border{}, accent{}, primary{}, surface{};
    void (*roundedRect)(HDC, int, int, int, int, int, int){};
    void (*dropdown)(HWND){};
    int (*popupMenu)(HMENU,UINT,int,int,HWND,const std::vector<PopupMenuHint>&){};
};
struct FormData {
    std::wstring title, note;
    std::vector<FormField> fields;
    HFONT font{};
    HBRUSH brush{};
    COLORREF background{}, foreground{};
    int dpi = 96;
    const CompactFormStyle *compact{};
    RECT inputFrame{};
    bool settings=false;
    std::wstring confirmText=L"确定";
};
inline LRESULT CALLBACK compactFormControl(HWND control, UINT message, WPARAM wp, LPARAM lp,
                                         UINT_PTR id, DWORD_PTR hovered) {
    bool button = GetDlgCtrlID(control) != 100;
    if (button && message == WM_GETDLGCODE)
        return DefSubclassProc(control, message, wp, lp) | DLGC_BUTTON |
            (GetDlgCtrlID(control) == IDOK ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
    // The dialog manager changes default button styles during Tab navigation.
    // Keep native keyboard/click handling while retaining our dark painting.
    if (button && message == BM_SETSTYLE) wp = (wp & ~BS_TYPEMASK) | BS_OWNERDRAW;
    if (button && message == WM_MOUSEMOVE && !hovered) {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, control, 0}; TrackMouseEvent(&track);
        SetWindowSubclass(control, compactFormControl, id, 1); InvalidateRect(control, nullptr, FALSE);
    }
    if (button && message == WM_MOUSELEAVE) {
        SetWindowSubclass(control, compactFormControl, id, 0); InvalidateRect(control, nullptr, FALSE);
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
        auto *data = reinterpret_cast<FormData *>(GetWindowLongPtrW(GetParent(control), DWLP_USER));
        if (button) InvalidateRect(control, nullptr, FALSE);
        else if (data) InvalidateRect(GetParent(control), &data->inputFrame, FALSE);
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(control, compactFormControl, id);
    return DefSubclassProc(control, message, wp, lp);
}
inline void drawCompactFormButton(const DRAWITEMSTRUCT &item,const FormData &data,bool primary){
    auto &style = *data.compact; DWORD_PTR hover = 0;
    GetWindowSubclass(item.hwndItem, compactFormControl, 1, &hover);
    bool pressed = item.itemState & ODS_SELECTED, disabled=item.itemState&ODS_DISABLED;
    bool focused = (item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT);
    FillRect(item.hDC, &item.rcItem, data.brush);
    COLORREF fill = primary ? (pressed ? style.surface : hover ? style.accent : style.primary) :
        pressed ? style.border : hover ? style.surface : data.background;
    auto brush = CreateSolidBrush(fill);
    auto pen = CreatePen(PS_SOLID, 1, focused ? style.accent : primary ? fill : style.border);
    auto oldPen = SelectObject(item.hDC, pen), oldBrush = SelectObject(item.hDC, brush);
    auto r = item.rcItem; int radius = MulDiv(8, data.dpi, 96);
    style.roundedRect(item.hDC, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(item.hDC, oldBrush); SelectObject(item.hDC, oldPen); DeleteObject(brush); DeleteObject(pen);
    auto oldFont = SelectObject(item.hDC, data.font); SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled?style.muted:primary && hover && !pressed ? style.inputBackground : data.foreground);
    wchar_t text[128]{};GetWindowTextW(item.hwndItem,text,128);
    DrawTextW(item.hDC, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(item.hDC, oldFont);
}
inline INT_PTR CALLBACK formProcedure(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto *data = reinterpret_cast<FormData *>(GetWindowLongPtrW(window, DWLP_USER));
    if (message == WM_INITDIALOG) {
        data = reinterpret_cast<FormData *>(lp); SetWindowLongPtrW(window, DWLP_USER, lp);
        SetWindowTextW(window, data->title.c_str());
        data->dpi = static_cast<int>(GetDpiForWindow(window));
        auto px = [&](int v) { return MulDiv(v, data->dpi, 96); };
        auto child = [&](const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y, int w, int h) {
            auto control = CreateWindowExW(!data->compact && cls == std::wstring(L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text.c_str(),
                WS_CHILD | WS_VISIBLE | style, px(x), px(y), px(w), px(h), window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(data->font), TRUE);
            return control;
        };
        int y = 14;
        if (data->compact) {
            BOOL dark = TRUE; DWORD corners = 2;
            DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
            DwmSetWindowAttribute(window, 33, &corners, sizeof(corners));
            DwmSetWindowAttribute(window, 34, &data->compact->border, sizeof(COLORREF));
            DwmSetWindowAttribute(window, 35, &data->background, sizeof(COLORREF));
            DwmSetWindowAttribute(window, 36, &data->foreground, sizeof(COLORREF));
            child(L"STATIC", data->fields[0].label, SS_NOPREFIX | SS_CENTERIMAGE, -1, 16, 14, 40, 18);
            auto hint = child(L"STATIC", data->note, SS_RIGHT | SS_NOPREFIX | SS_CENTERIMAGE, 99, 60, 14, 252, 18);
            SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(data->compact->hintFont), TRUE);
            data->inputFrame = {px(16), px(38), px(312), px(70)};
            auto edit = child(L"EDIT", data->fields[0].value, WS_TABSTOP | ES_AUTOHSCROLL, 100, 24, 44, 280, 20);
            SendMessageW(edit, EM_SETLIMITTEXT, 1024, 0);
            SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
            SetWindowSubclass(edit, compactFormControl, 1, 0);
            auto cancel = child(L"BUTTON", L"取消", WS_TABSTOP | BS_OWNERDRAW, IDCANCEL, 164, 82, 70, 28);
            auto confirm = child(L"BUTTON", L"确定", WS_TABSTOP | BS_OWNERDRAW, IDOK, 242, 82, 70, 28);
            SetWindowSubclass(cancel, compactFormControl, 1, 0);
            SetWindowSubclass(confirm, compactFormControl, 1, 0);
            SendMessageW(window, DM_SETDEFID, IDOK, 0);
            y = 80;
        } else {
          if (!data->note.empty()) { child(L"STATIC", data->note, 0, -1, 16, y, 488, 48); y += 54; }
          for (size_t i = 0; i < data->fields.size(); ++i) {
            const auto &field = data->fields[i];
            child(L"STATIC", field.label, 0, -1, 16, y, 488, 20); y += 22;
            int height = field.multiline ? (data->fields.size() == 1 ? 260 : data->fields.size() <= 4 ? 150 : 80) : 26;
            auto edit = child(L"EDIT", field.value, WS_TABSTOP | (field.readOnly ? ES_READONLY : 0) |
                (field.multiline ? ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN : ES_AUTOHSCROLL),
                100 + static_cast<int>(i), 16, y, 488, height);
            SendMessageW(edit, EM_SETLIMITTEXT, field.multiline ? 8000 : 1024, 0); y += height + 10;
          }
          child(L"BUTTON", data->confirmText, WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK, 326, y, 84, 30);
          child(L"BUTTON", L"取消", WS_TABSTOP, IDCANCEL, 420, y, 84, 30);
        }
        if(sizeSettingsDialog&&sizeSettingsDialog(window,data->compact?328:520,y+44,data->dpi))return TRUE;
        RECT rect{0, 0, px(data->compact ? 328 : 520), px(y + 44)};
        if (data->compact)
            AdjustWindowRectExForDpi(&rect, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)), data->dpi);
        else
            AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), FALSE,
                static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)));
        RECT owner{}; GetWindowRect(GetParent(window), &owner);
        MONITORINFO monitor{sizeof(monitor)}; GetMonitorInfoW(MonitorFromWindow(GetParent(window), MONITOR_DEFAULTTONEAREST), &monitor);
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        int left = (owner.left + owner.right - width) / 2, top = (owner.top + owner.bottom - height) / 2;
        left = std::max<LONG>(monitor.rcWork.left, std::min<LONG>(left, monitor.rcWork.right - width));
        top = std::max<LONG>(monitor.rcWork.top, std::min<LONG>(top, monitor.rcWork.bottom - height));
        SetWindowPos(window, HWND_TOP, left, top, width, height, SWP_NOACTIVATE);
        if (data->compact) {
            auto edit = GetDlgItem(window, 100); SetFocus(edit); SendMessageW(edit, EM_SETSEL, 0, -1);
            return FALSE;
        }
        return TRUE;
    }
    if (!data) return FALSE;
    if (data->compact && message == WM_PAINT) {
        PAINTSTRUCT paint{}; auto dc = BeginPaint(window, &paint);
        FillRect(dc, &paint.rcPaint, data->brush);
        auto &style = *data->compact;
        auto pen = CreatePen(PS_SOLID, 1, GetFocus() == GetDlgItem(window, 100) ? style.accent : style.border);
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, style.inputBrush);
        auto r = data->inputFrame; int radius = MulDiv(8, data->dpi, 96);
        style.roundedRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
        EndPaint(window, &paint); return TRUE;
    }
    if (data->compact && message == WM_DRAWITEM) {
        auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (item.CtlType != ODT_BUTTON) return FALSE;
        drawCompactFormButton(item,*data,item.CtlID==IDOK);return TRUE;
    }
    if (message == WM_CTLCOLORDLG || message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
        bool input = data->compact && message == WM_CTLCOLOREDIT;
        SetTextColor(reinterpret_cast<HDC>(wp), data->compact && GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == 99 ?
            data->compact->muted : data->foreground);
        SetBkColor(reinterpret_cast<HDC>(wp), input ? data->compact->inputBackground : data->background);
        return reinterpret_cast<INT_PTR>(input ? data->compact->inputBrush : data->brush);
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK) {
        for (size_t i = 0; i < data->fields.size(); ++i) {
            auto edit = GetDlgItem(window, 100 + static_cast<int>(i));
            auto length = GetWindowTextLengthW(edit);
            std::wstring text(static_cast<size_t>(length) + 1, L'\0');
            GetWindowTextW(edit, text.data(), length + 1); text.resize(length); data->fields[i].value = text;
        }
        finishSettingsDialog(window, IDOK); return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wp) == IDCANCEL)) {
        finishSettingsDialog(window, IDCANCEL); return TRUE;
    }
    return FALSE;
}
inline bool showForm(HWND owner, FormData &data) {
    if(data.settings&&runSettingsDialog)
        return runSettingsDialog(owner,formProcedure,reinterpret_cast<LPARAM>(&data),data.title.c_str())==IDOK;
    struct Template { DLGTEMPLATE dialog; WORD menu, windowClass, title; } layout{};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | (data.compact ? WS_CLIPCHILDREN : 0);
    layout.dialog.dwExtendedStyle = WS_EX_DLGMODALFRAME;
    layout.dialog.cx = 320; layout.dialog.cy = 240;
    return DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &layout.dialog, owner, formProcedure,
        reinterpret_cast<LPARAM>(&data)) == IDOK;
}
} // namespace jump
