#pragma once

struct LocationScrollState {
    bool hover = false, dragging = false;
    int grabOffset = 0, trackPosition = 0, repeatDirection = -1;
};
struct LocationScrollGeometry {
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    RECT client{}, thumb{};
    int maximum = 0, travel = 0;
};
LocationScrollGeometry locationScrollGeometry(HWND h) {
    LocationScrollGeometry layout;
    GetClientRect(h, &layout.client);
    GetScrollInfo(h, SB_CTL, &layout.info);
    auto &info = layout.info;
    int extent = std::max(1, info.nMax - info.nMin + 1);
    layout.maximum = std::max(info.nMin, info.nMax - static_cast<int>(info.nPage ? info.nPage - 1 : 0));
    int height = std::max(0L, layout.client.bottom - px(4));
    int thumbHeight = std::min(height, std::max(px(20), MulDiv(height, info.nPage, extent)));
    layout.travel = height - thumbHeight;
    int top = px(2) + (layout.maximum > info.nMin
                           ? MulDiv(layout.travel, info.nPos - info.nMin, layout.maximum - info.nMin)
                           : 0);
    layout.thumb = {0, top, layout.client.right, top + thumbHeight};
    return layout;
}
LRESULT CALLBACK LocationScrollProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    auto &state = *reinterpret_cast<LocationScrollState *>(data);
    auto notify = [&](int code) {
        SendMessageW(GetParent(h), WM_VSCROLL, MAKEWPARAM(code, state.trackPosition),
                     reinterpret_cast<LPARAM>(h));
    };
    auto stop = [&]() {
        state.dragging = false;
        state.repeatDirection = -1;
        KillTimer(h, 1);
        InvalidateRect(h, nullptr, FALSE);
    };
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
    case WM_PRINT:
    case WM_PRINTCLIENT: {
        ClientPaint paint(h, msg, wp);
        auto dc = paint.dc();
        auto layout = locationScrollGeometry(h);
        FillRect(dc, &layout.client, g->brush);
        int center = layout.client.right / 2;
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        auto trackBrush = CreateSolidBrush(g->line);
        auto oldBrush = SelectObject(dc, trackBrush);
        paintRoundedRect(dc, center - px(1), px(2), center + px(1), layout.client.bottom - px(2), px(2),
                         px(2));
        bool active = state.hover || state.dragging || GetFocus() == h;
        int width = px(active ? 8 : 6);
        auto color =
            state.dragging || GetFocus() == h ? g->accent : mixColor(g->bg, g->muted, active ? 80 : 55);
        auto thumbBrush = CreateSolidBrush(IsWindowEnabled(h) ? color : g->line);
        SelectObject(dc, thumbBrush);
        paintRoundedRect(dc, center - width / 2, layout.thumb.top, center - width / 2 + width,
                         layout.thumb.bottom, width, width);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(trackBrush);
        DeleteObject(thumbBrush);
        return 0;
    }
    case SBM_SETSCROLLINFO: {
        // Keep native range/accessibility storage, but never invoke its light-theme painting.
        SCROLLINFO before{sizeof(before), SIF_RANGE | SIF_PAGE | SIF_POS}, after = before;
        GetScrollInfo(h, SB_CTL, &before);
        auto result = DefSubclassProc(h, msg, FALSE, lp);
        GetScrollInfo(h, SB_CTL, &after);
        if (before.nMin != after.nMin || before.nMax != after.nMax || before.nPage != after.nPage ||
            before.nPos != after.nPos)
            InvalidateRect(h, nullptr, FALSE);
        return result;
    }
    case SBM_GETSCROLLINFO: {
        auto result = DefSubclassProc(h, msg, wp, lp);
        auto info = reinterpret_cast<SCROLLINFO *>(lp);
        if (result && state.dragging && (info->fMask & SIF_TRACKPOS))
            info->nTrackPos = state.trackPosition;
        return result;
    }
    case WM_MOUSEMOVE: {
        if (!state.hover) {
            state.hover = true;
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
            TrackMouseEvent(&tracking);
            InvalidateRect(h, nullptr, FALSE);
        }
        if (state.dragging && GetCapture() == h) {
            auto layout = locationScrollGeometry(h);
            int offset = std::clamp(GET_Y_LPARAM(lp) - state.grabOffset - px(2), 0, layout.travel);
            int position =
                layout.info.nMin +
                (layout.travel ? MulDiv(offset, layout.maximum - layout.info.nMin, layout.travel) : 0);
            if (position != state.trackPosition) {
                state.trackPosition = position;
                notify(SB_THUMBTRACK);
            }
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        state.hover = false;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        if (!IsWindowEnabled(h))
            return 0;
        auto layout = locationScrollGeometry(h);
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        state.trackPosition = layout.info.nPos;
        SetCapture(h);
        if (PtInRect(&layout.thumb, point)) {
            state.dragging = true;
            state.grabOffset = point.y - layout.thumb.top;
        } else {
            state.repeatDirection = point.y < layout.thumb.top ? SB_PAGEUP : SB_PAGEDOWN;
            SetTimer(h, 1, 400, nullptr);
            notify(state.repeatDirection);
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1 && state.repeatDirection != -1 && GetCapture() == h) {
            SetTimer(h, 1, 70, nullptr);
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(h, &point);
            auto layout = locationScrollGeometry(h);
            if (PtInRect(&layout.client, point) &&
                ((state.repeatDirection == SB_PAGEUP && point.y < layout.thumb.top) ||
                 (state.repeatDirection == SB_PAGEDOWN && point.y >= layout.thumb.bottom)))
                notify(state.repeatDirection);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state.dragging)
            notify(SB_THUMBPOSITION);
        stop();
        if (GetCapture() == h)
            ReleaseCapture();
        return 0;
    case WM_CANCELMODE:
        stop();
        if (GetCapture() == h)
            ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        stop();
        return 0;
    case WM_ENABLE:
        if (!wp) {
            stop();
            if (GetCapture() == h)
                ReleaseCapture();
        }
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        return SendMessageW(GetParent(h), msg, wp, lp);
    case WM_KEYDOWN:
        if (!IsWindowEnabled(h))
            return 0;
        switch (wp) {
        case VK_UP:
            notify(SB_LINEUP);
            return 0;
        case VK_DOWN:
            notify(SB_LINEDOWN);
            return 0;
        case VK_PRIOR:
            notify(SB_PAGEUP);
            return 0;
        case VK_NEXT:
            notify(SB_PAGEDOWN);
            return 0;
        case VK_HOME:
            notify(SB_TOP);
            return 0;
        case VK_END:
            notify(SB_BOTTOM);
            return 0;
        }
        break;
    case WM_NCDESTROY:
        RemoveWindowSubclass(h, LocationScrollProc, 1);
        delete &state;
        break;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
// Input frames belong to the parent, so invalidate only the frame when feedback
// changes. Do not repaint on every mouse move or change native text selection.
LRESULT CALLBACK InputFeedbackProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    auto before = feedback;
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        feedback |= 1;
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE || msg == WM_ENABLE)
        feedback &= ~DWORD_PTR(1);
    if (before != feedback)
        SetWindowSubclass(h, InputFeedbackProc, 1, feedback);
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (before != feedback || msg == WM_SETFOCUS || msg == WM_KILLFOCUS || msg == WM_ENABLE) {
        RECT frame{};
        GetWindowRect(h, &frame);
        MapWindowPoints(nullptr, GetParent(h), reinterpret_cast<POINT *>(&frame), 2);
        InflateRect(&frame, px(10), px(8));
        InvalidateRect(GetParent(h), &frame, FALSE);
        invalidateSubjectGroup(h);
    }
    return result;
}
COLORREF inputBorder(HWND parent, const RECT &frame) {
    auto h = ChildWindowFromPointEx(parent, {(frame.left + frame.right) / 2, (frame.top + frame.bottom) / 2},
                                    CWP_SKIPINVISIBLE);
    DWORD_PTR feedback = 0;
    if (h && GetPropW(h, L"Jump.InputError"))
        return RGB(191, 106, 110);
    if (h && h != parent && GetWindowSubclass(h, InputFeedbackProc, 1, &feedback) && IsWindowEnabled(h)) {
        if (GetFocus() == h)
            return g->accent;
        if (feedback & 1)
            return mixColor(g->line, g->muted, 35);
    }
    return g->line;
}
void paintInputFrame(HDC dc, HWND parent, const RECT &frame, int radius) {
    auto pen = CreatePen(PS_SOLID, 1, inputBorder(parent, frame));
    auto op = SelectObject(dc, pen), ob = SelectObject(dc, g->inputBrush);
    paintRoundedRect(dc, frame.left, frame.top, frame.right, frame.bottom, radius, radius);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
}
LRESULT CALLBACK FlyoutProc(HWND, UINT, WPARAM, LPARAM);
void closeFlyout(bool focusPanel = false);
void showPresetFlyout(HWND anchor = nullptr);
void showLocationFlyout(bool historyOnly = false);
void beginPathEdit();
void commitPathEdit();
void cancelPathEdit(bool restoreFocus = true);
void rebuildPanel();
void enterFreeNaming();
void resetStructuredInput();
HWND transientEditor();
bool insideTransientEditor(HWND target);
void resetEditorDismiss();
void requestEditorDismiss(bool explicitClick = false);
void finishEditorDismiss(HWND editor, bool explicitClick);
LRESULT CALLBACK TransientEditProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
void restoreNamingState(const NamingSnapshot &state);
void updateStructuredContent();
std::wstring structuredInputError();
void reposition(bool show);
void updateInlineRule(bool immediate = false);
void showManager(bool present = true);

void rememberManagerPosition();
void buildManagerPage(bool preserveDraft = false);
void initializeVariableDraft();
void openVariableEditor(bool present = true);
void updateVariableDraftPreview();
void buildInlineTools(int &y, int available);
void buildNamingHint(int id, int x, int y, int width);
void updateInlineControls();
HWND createChild(HWND parent, const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y,
                 int width, int height, HFONT font) {
    auto control = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, px(x), px(y),
                                   px(width), px(height), parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g->instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}
HWND child(const wchar_t *cls, const std::wstring &text, DWORD style, int id, int x, int y, int w, int h) {
    if (cls == std::wstring(L"BUTTON"))
        style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW | BS_NOTIFY;
    bool edit = cls == std::wstring(L"EDIT");
    bool staticText = cls == std::wstring(L"STATIC");
    DWORD textStyle = style;
    // Native STATIC also draws immediately on text/enable changes. Route every
    // path through our alpha-aware renderer, retaining the original text layout.
    if (staticText)
        style = (style & ~SS_TYPEMASK) | SS_OWNERDRAW;
    if (edit)
        g->fields.push_back({px(x), px(y), px(x + w), px(y + h)});
    HWND control = createChild(g->panel, cls, text, style, id, x + (edit ? 10 : 0), y + (edit ? 7 : 0),
                               w - (edit ? 20 : 0), h - (edit ? 12 : 0), g->bodyFont);
    g->children.push_back(control);
    SetWindowSubclass(control, TransientEditProc, 4, 0);
    if (cls == std::wstring(L"BUTTON"))
        SetWindowSubclass(control, PillProc, 1, 0);
    if (edit)
        SetWindowSubclass(control, InputFeedbackProc, 1, 0);
    if (staticText)
        SetWindowSubclass(control, MaterialControlProc, 2, textStyle);
    if (edit || cls == std::wstring(MSFTEDIT_CLASS))
        configureMaterialEditor(control);
    if (id == ID_FOLDER_SCROLL) {
        auto state = new LocationScrollState;
        if (!SetWindowSubclass(control, LocationScrollProc, 1, reinterpret_cast<DWORD_PTR>(state)))
            delete state;
    }
    return control;
}
void label(int x, int y, int w, const std::wstring &text, bool heading = false) {
    g->labels.push_back({{px(x), px(y), px(x + w), px(y + 22)}, text, heading});
}
void tip(HWND control, const std::wstring &text) {
    bool exists = g->tipStrings.contains(control);
    if (exists && g->tipStrings.at(control) == text)
        return;
    g->tipStrings[control] = text;
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = g->panel;
    info.uId = reinterpret_cast<UINT_PTR>(control);
    info.lpszText = g->tipStrings[control].data();
    SendMessageW(g->tooltip, exists ? TTM_UPDATETIPTEXTW : TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
}
LRESULT CALLBACK PillProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    int controlId = GetDlgCtrlID(h);
    // BS_NOTIFY turns the second fast press into BN_DOUBLECLICKED. Candidate
    // chips toggle per completed click, so keep native press/release handling
    // for that press too; location buttons retain their double-click Save.
    if (msg == WM_LBUTTONDBLCLK && GetParent(h) == g->panel &&
        ((controlId >= ID_QUICK_PICK && controlId < ID_QUICK_PICK + 48) ||
         (controlId >= ID_STRUCT_CHOICE && controlId < ID_STRUCT_CHOICE + 6)))
        msg = WM_LBUTTONDOWN;
    if (msg == WM_ERASEBKGND)
        return 1; // Owner drawing paints the complete button atomically.
    DWORD_PTR originalFeedback = feedback;
    if ((msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000)) ||
        (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000)))
        feedback |= 2;
    if (msg == WM_LBUTTONDOWN || msg == WM_KILLFOCUS)
        feedback &= ~DWORD_PTR(2);
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        feedback |= 1;
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
    }
    if (msg == WM_MOUSELEAVE)
        feedback &= ~DWORD_PTR(1);
    if (feedback != originalFeedback) {
        if (((feedback ^ originalFeedback) & 1) && GetParent(h) == g->panel && g->animations &&
            IsWindowVisible(h)) {
            int from = hoverAmount(h, (originalFeedback & 1) != 0);
            g->hoverMotions[h] = {from, feedback & 1 ? 100 : 0, GetTickCount64()};
            SetTimer(h, HOVER_TIMER, 15, nullptr);
        }
        SetWindowSubclass(h, PillProc, 1, feedback);
        InvalidateRect(h, nullptr, FALSE);
    }
    if (msg == WM_TIMER && wp == HOVER_TIMER) {
        InvalidateRect(h, nullptr, FALSE);
        auto motion = g->hoverMotions.find(h);
        if (motion == g->hoverMotions.end() || !g->animations ||
            GetTickCount64() - motion->second.started >= 140) {
            KillTimer(h, HOVER_TIMER);
            g->hoverMotions.erase(h);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        KillTimer(h, HOVER_TIMER);
        g->hoverMotions.erase(h);
    }
    if (msg == WM_MOUSEWHEEL && GetParent(h) != g->manager)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN && GetParent(h) == g->panel) {
        int id = GetDlgCtrlID(h);
        if (g->editingPath && id != ID_DIRECTORY && id != ID_DIRECTORY_EDIT)
            cancelPathEdit(false);
        // These commands open an editing window. Activate on mouse-down, before
        // the button's focus/capture handling and the later window hand-off.
        if (id == ID_MANAGER || id == ID_NAME_MANAGER || id == ID_RENAME || id == ID_DIRECTORY ||
            id == ID_DIRECTORY_EDIT)
            SetForegroundWindow(g->panel);
    }
    if (msg == WM_CONTEXTMENU) {
        int id = GetDlgCtrlID(h);
        if (GetParent(h) == g->panel && ((id >= ID_QUICK_PICK && id < ID_QUICK_PICK + 48) ||
                                         (id >= ID_STRUCT_CHOICE && id < ID_STRUCT_CHOICE + 6)))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
        auto action = g->actions.find(id);
        if (GetParent(h) == g->panel && action != g->actions.end() &&
            (action->second.kind == Action::Prefix || action->second.kind == Action::Suffix ||
             action->second.kind == Action::Location || action->second.kind == Action::Variables))
            return SendMessageW(g->panel, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), lp);
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    if (feedback != originalFeedback || msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        invalidateSubjectGroup(h);
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK VariableEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL)
        return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) {
        SetForegroundWindow(GetAncestor(h, GA_ROOT));
        SetFocus(h);
    }
    bool submit = GetDlgCtrlID(h) != ID_VARIABLE_VALUES;
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_ESCAPE || (submit && key->wParam == VK_RETURN)))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_ESCAPE || (submit && wp == VK_RETURN))) {
        SendMessageW(GetParent(h), WM_COMMAND, wp == VK_ESCAPE ? ID_VARIABLE_CANCEL : ID_VARIABLE_SAVE, 0);
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_ESCAPE || (submit && wp == VK_RETURN)))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
bool pasteTarget(HWND editor);
LRESULT CALLBACK PathEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_PASTE && pasteTarget(h))
        return 0;
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_KILLFOCUS)
        PostMessageW(g->panel, WM_PATH_BLUR, 0, 0);
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_RETURN)
            commitPathEdit();
        else
            cancelPathEdit();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK PreviewProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_IME_STARTCOMPOSITION)
        SetPropW(h, L"Jump.IME", reinterpret_cast<HANDLE>(1));
    if (msg == WM_IME_ENDCOMPOSITION)
        RemovePropW(h, L"Jump.IME");
    if (GetPropW(h, L"Jump.IME") && (msg == WM_KEYDOWN || msg == WM_CHAR))
        return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_PASTE && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT && pasteTarget(h))
        return 0;
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_GETDLGCODE && lp && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && key->wParam == VK_RETURN)
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (GetDlgCtrlID(h) == ID_PRESET_VALUE) {
        if (msg == WM_GETDLGCODE && lp) {
            auto key = reinterpret_cast<MSG *>(lp);
            if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
                return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
        }
        if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
            SendMessageW(g->panel, WM_COMMAND, wp == VK_RETURN ? ID_PRESET_SAVE : ID_PRESET_CANCEL, 0);
            return 0;
        }
    }
    if (msg == WM_LBUTTONDOWN) {
        if (g->editingPath)
            cancelPathEdit(false);
        closeFlyout();
        SetForegroundWindow(g->panel);
        SetFocus(h);
    }
    if (msg == WM_KEYDOWN && wp == VK_RETURN && GetDlgCtrlID(h) == ID_FREE_NAME_INPUT) {
        SendMessageW(g->panel, WM_COMMAND, ID_SUBMIT, 0);
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        SetForegroundWindow(g->dialog.hwnd);
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
LRESULT CALLBACK InlineEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL)
        return SendMessageW(g->panel, msg, wp, lp);
    if (msg == WM_LBUTTONDOWN) {
        cancelPathEdit(false);
        closeFlyout();
        SetForegroundWindow(g->panel);
        SetFocus(h);
    }
    if (msg == WM_GETDLGCODE && lp) {
        auto key = reinterpret_cast<MSG *>(lp);
        if (key->message == WM_KEYDOWN && (key->wParam == VK_RETURN || key->wParam == VK_ESCAPE))
            return DefSubclassProc(h, msg, wp, lp) | DLGC_WANTMESSAGE;
    }
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE)) {
        if (wp == VK_ESCAPE) {
            if (!structuredInputError().empty()) {
                g->building = true;
                setTextIfChanged(GetDlgItem(g->panel, ID_AFFIX_PREFIX), g->parts.prefix);
                setTextIfChanged(GetDlgItem(g->panel, ID_AFFIX_SUFFIX), g->parts.suffix);
                g->building = false;
                g->statusError = false;
            }
            g->inlineTool = 0;
            rebuildPanel();
        } else if (GetDlgCtrlID(h) == ID_AFFIX_PREFIX || GetDlgCtrlID(h) == ID_AFFIX_SUFFIX)
            SendMessageW(g->panel, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(h), EN_CHANGE),
                         reinterpret_cast<LPARAM>(h));
        else
            updateInlineRule();
        return 0;
    }
    if (msg == WM_CHAR && (wp == VK_RETURN || wp == VK_ESCAPE))
        return 0;
    return DefSubclassProc(h, msg, wp, lp);
}

LRESULT CALLBACK DropdownProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR feedback) {
    auto setFeedback = [&](DWORD_PTR value) {
        if (feedback == value)
            return;
        feedback = value;
        SetWindowSubclass(h, DropdownProc, 1, value);
        InvalidateRect(h, nullptr, FALSE);
    };
    if (msg == WM_MOUSEMOVE && !(feedback & 1)) {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
        TrackMouseEvent(&tracking);
        setFeedback(feedback | 1);
    }
    if (msg == WM_MOUSELEAVE)
        setFeedback(feedback & ~DWORD_PTR(1));
    if (msg == WM_KEYDOWN && (GetKeyState(static_cast<int>(wp)) & 0x8000))
        setFeedback(feedback | 2);
    if (msg == WM_SETFOCUS && (GetKeyState(VK_TAB) & 0x8000))
        setFeedback(feedback | 2);
    if (msg == WM_KILLFOCUS)
        setFeedback(feedback & ~DWORD_PTR(2));
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp);
        HDC dc = paint.dc();
        RECT r{};
        GetClientRect(h, &r);
        FillRect(dc, &r, g->brush);
        const bool expanded = (feedback & 4) != 0;
        const bool hover = (feedback & 1) != 0;
        const bool keyboardFocus = (feedback & 2) && GetFocus() == h;
        COLORREF fill =
            controlFill(expanded ? g->surface : g->field, false, hover, false, false, IsWindowEnabled(h));
        auto brush = CreateSolidBrush(fill);
        auto pen = CreatePen(PS_SOLID, 1, keyboardFocus ? g->accent : fill);
        auto ob = SelectObject(dc, brush), op = SelectObject(dc, pen);
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(10), px(10));
        SelectObject(dc, op);
        SelectObject(dc, ob);
        DeleteObject(pen);
        DeleteObject(brush);
        auto font = SelectObject(dc, g->bodyFont);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g->fg);
        r.left += px(6);
        r.right -= px(22);
        auto text = windowText(h);
        DrawTextW(dc, text.c_str(), -1, &r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        r.left = r.right;
        r.right += px(22);
        drawUiIcon(dc, expanded ? UiIcon::ChevronUp : UiIcon::Chevron, (r.left + r.right - px(14)) / 2.f,
                   (r.top + r.bottom - px(14)) / 2.f, static_cast<float>(px(14)),
                   expanded                 ? g->accent
                   : hover || keyboardFocus ? g->fg
                                            : g->muted);
        SelectObject(dc, font);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_LBUTTONDOWN) {
        setFeedback(feedback & ~DWORD_PTR(2));
        if (g->editingPath)
            cancelPathEdit(false);
        if (GetParent(h) == g->panel)
            closeFlyout();
        SetForegroundWindow(GetAncestor(h, GA_ROOT));
        SetFocus(h);
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
        InvalidateRect(h, nullptr, FALSE);
    return result;
}
LRESULT drawDropdownItem(DRAWITEMSTRUCT item) {
    PaintBuffer buffer(item.hDC, item.rcItem);
    item.hDC = buffer.dc;
    auto brush = CreateSolidBrush(g->surface);
    FillRect(item.hDC, &item.rcItem, brush);
    DeleteObject(brush);
    if (item.itemID == static_cast<UINT>(-1))
        return TRUE;
    auto length = SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0);
    if (length == CB_ERR)
        return TRUE;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(length));
    DWORD_PTR feedback = 0;
    GetWindowSubclass(item.hwndItem, DropdownProc, 1, &feedback);
    auto currentIndex = (feedback & 4) && (feedback >> 8) ? static_cast<LRESULT>((feedback >> 8) - 1)
                                                          : SendMessageW(item.hwndItem, CB_GETCURSEL, 0, 0);
    const bool current = currentIndex == item.itemID;
    bool hovered = false;
    COMBOBOXINFO combo{sizeof(combo)};
    if (GetComboBoxInfo(item.hwndItem, &combo) && IsWindowVisible(combo.hwndList)) {
        POINT mouse{};
        GetCursorPos(&mouse);
        ScreenToClient(combo.hwndList, &mouse);
        hovered = PtInRect(&item.rcItem, mouse) != FALSE;
    }
    if ((item.itemState & ODS_SELECTED) && (!current || hovered)) {
        RECT hover = item.rcItem;
        InflateRect(&hover, -px(4), -px(2));
        auto fill = CreateSolidBrush(RGB(45, 56, 72));
        auto pen = CreatePen(PS_SOLID, 1, RGB(45, 56, 72));
        auto ob = SelectObject(item.hDC, fill), op = SelectObject(item.hDC, pen);
        paintRoundedRect(item.hDC, hover.left, hover.top, hover.right, hover.bottom, px(8), px(8));
        SelectObject(item.hDC, op);
        SelectObject(item.hDC, ob);
        DeleteObject(pen);
        DeleteObject(fill);
    }

    auto old = SelectObject(item.hDC, g->font);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, item.itemState & ODS_DISABLED ? g->muted : g->fg);
    RECT textRect = item.rcItem;
    textRect.left += px(12);
    textRect.right -= px(30);
    DrawTextW(item.hDC, text.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (current) {
        int size = px(14);
        drawUiIcon(item.hDC, UiIcon::Check, static_cast<float>(item.rcItem.right - px(24)),
                   (item.rcItem.top + item.rcItem.bottom - size) / 2.f, static_cast<float>(size), g->accent);
    }
    SelectObject(item.hDC, old);
    return TRUE;
}
LRESULT CALLBACK DropdownListProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dimensions) {
    auto border = [&](HDC dc) {
        RECT r{};
        GetClientRect(h, &r);
        auto pen = CreatePen(PS_SOLID, 1, g->line);
        auto oldPen = SelectObject(dc, pen), oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        paintRoundedRect(dc, 0, 0, r.right, r.bottom, px(12), px(12));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(pen);
    };
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT) {
        ClientPaint paint(h, msg, wp);
        RECT r{};
        GetClientRect(h, &r);
        auto fill = CreateSolidBrush(g->surface);
        FillRect(paint.dc(), &r, fill);
        DeleteObject(fill);
        DefSubclassProc(h, WM_PRINT, reinterpret_cast<WPARAM>(paint.dc()), PRF_CLIENT | PRF_ERASEBKGND);
        border(paint.dc());
        return 0;
    }
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        int previous = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(h, L"Jump.PopupHotRow"))) - 1;
        auto hit = msg == WM_MOUSEMOVE ? SendMessageW(h, LB_ITEMFROMPOINT, 0, lp) : -1;
        int next = hit != -1 && !HIWORD(hit) ? LOWORD(hit) : -1;
        if (next != previous) {
            if (next < 0)
                RemovePropW(h, L"Jump.PopupHotRow");
            else
                SetPropW(h, L"Jump.PopupHotRow", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(next + 1)));
            for (int row : {previous, next}) {
                RECT r{};
                if (row >= 0 && SendMessageW(h, LB_GETITEMRECT, row, reinterpret_cast<LPARAM>(&r)) != LB_ERR)
                    InvalidateRect(h, &r, FALSE);
            }
        }
        if (msg == WM_MOUSEMOVE && previous < 0) {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
            TrackMouseEvent(&tracking);
        }
    }
    if (msg == WM_NCDESTROY)
        RemovePropW(h, L"Jump.PopupHotRow");
    if (msg == WM_WINDOWPOSCHANGED) {
        RECT r{};
        GetWindowRect(h, &r);
        int width = r.right - r.left, height = r.bottom - r.top;
        DWORD_PTR size = static_cast<DWORD_PTR>(MAKELONG(width, height));
        if (size != dimensions && width > 0 && height > 0) {
            SetWindowSubclass(h, DropdownListProc, 1, size);
            // Window owns the region after a successful SetWindowRgn call.
            auto region = CreateRoundRectRgn(0, 0, width + 1, height + 1, px(12), px(12));
            if (!SetWindowRgn(h, region, TRUE))
                DeleteObject(region);
        }
    }
    auto result = DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_PRINT || msg == WM_PRINTCLIENT)
        border(reinterpret_cast<HDC>(wp));
    return result;
}
void styleDropdownList(HWND combo) {
    COMBOBOXINFO info{sizeof(info)};
    if (!GetComboBoxInfo(combo, &info) || !info.hwndList)
        return;
    DWORD_PTR previous = 0;
    if (GetWindowSubclass(info.hwndList, DropdownListProc, 1, &previous))
        return;
    SetWindowSubclass(info.hwndList, DropdownListProc, 1, 0);
    SetWindowTheme(info.hwndList, L"", L"");
    SetWindowLongPtrW(info.hwndList, GWL_STYLE, GetWindowLongPtrW(info.hwndList, GWL_STYLE) & ~WS_BORDER);
    SetWindowLongPtrW(info.hwndList, GWL_EXSTYLE,
                      GetWindowLongPtrW(info.hwndList, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
    SetWindowPos(info.hwndList, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
LRESULT CALLBACK DropdownParentProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_DROPDOWN_COMMIT) {
        auto control = reinterpret_cast<HWND>(lp);
        DWORD_PTR feedback = 0;
        if (!IsWindow(control) || GetParent(control) != h ||
            !GetWindowSubclass(control, DropdownProc, 1, &feedback) || !(feedback & 32))
            return 0;
        auto original = static_cast<LRESULT>(feedback >> 8) - 1;
        SetWindowSubclass(control, DropdownProc, 1, feedback & 3);
        InvalidateRect(control, nullptr, FALSE);
        if (!(feedback & 8) || (feedback & 16)) {
            if (original >= 0)
                SendMessageW(control, CB_SETCURSEL, original, 0);
            return 0;
        }
        if (SendMessageW(control, CB_GETCURSEL, 0, 0) != original)
            return DefSubclassProc(h, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(control), CBN_SELCHANGE), lp);
        return 0;
    }
    if (msg == WM_COMMAND && lp) {
        auto control = reinterpret_cast<HWND>(lp);
        DWORD_PTR feedback = 0;
        if (GetWindowSubclass(control, DropdownProc, 1, &feedback)) {
            DWORD_PTR originalFeedback = feedback;
            int code = HIWORD(wp);
            if (code == CBN_DROPDOWN) {
                styleDropdownList(control);
                auto selected = SendMessageW(control, CB_GETCURSEL, 0, 0);
                feedback = (feedback & 3) | 4 | (static_cast<DWORD_PTR>(selected + 1) << 8);
                if (GetKeyState(VK_LBUTTON) & 0x8000)
                    feedback &= ~DWORD_PTR(2);
            } else if (code == CBN_SELCHANGE && (feedback & (4 | 32)))
                return 0;
            else if (code == CBN_SELENDOK && (feedback & (4 | 32)))
                feedback = (feedback | 8) & ~DWORD_PTR(16);
            else if (code == CBN_SELENDCANCEL && (feedback & (4 | 32)))
                feedback = (feedback | 16) & ~DWORD_PTR(8);
            else if (code == CBN_CLOSEUP && (feedback & 4)) {
                // Resolve after the complete native notification sequence; Windows
                // does not guarantee selection and close notification ordering.
                PostMessageW(h, WM_DROPDOWN_COMMIT, 0, lp);
                feedback = (feedback & ~DWORD_PTR(4)) | 32;
            }
            if (originalFeedback != feedback) {
                SetWindowSubclass(control, DropdownProc, 1, feedback);
                InvalidateRect(control, nullptr, FALSE);
            }
        }
    }
    if (msg == WM_DRAWITEM) {
        const auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (item.CtlType == ODT_COMBOBOX)
            return drawDropdownItem(item);
    }
    if (msg == WM_CTLCOLORLISTBOX && windowClass(reinterpret_cast<HWND>(lp)) == L"ComboLBox") {
        SetTextColor(reinterpret_cast<HDC>(wp), g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->surface);
        static HBRUSH popupBrush = CreateSolidBrush(RGB(37, 46, 59));
        return reinterpret_cast<LRESULT>(popupBrush);
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void styleDropdown(HWND combo) {
    SetWindowSubclass(combo, DropdownProc, 1, 0);
    SetWindowSubclass(GetParent(combo), DropdownParentProc, 1, 0);
    styleDropdownList(combo);
}
void pill(const std::wstring &text, Action action, size_t index, int &x, int &y, int available,
          const std::wstring &tooltip, int left = 16, int fixedWidth = 0) {
    HDC dc = GetDC(g->panel);
    auto old = SelectObject(dc, g->bodyFont);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(g->panel, dc);
    int width = fixedWidth ? fixedWidth : std::clamp(MulDiv(size.cx, 96, g->dpi) + 24, 64, available);
    if (x + width > available + left) {
        x = left;
        y += 40;
    }
    int id = g->nextAction++;
    HWND h = child(L"BUTTON", text, BS_OWNERDRAW | BS_NOTIFY | WS_TABSTOP, id, x, y, width, 32);
    g->actions[id] = {action, index};
    SetWindowSubclass(h, PillProc, 1, 0);
    int color = bubbleColorIndex({action, index});
    tip(h, color ? tooltip + L"\n颜色标记：" + bubbleColors[color].name : tooltip);
    x += width + 8;
}
std::vector<Location> locationChoices() {
    const auto &source = g->locationTab == 4   ? g->cfg.recentSaves
                         : g->locationTab == 1 ? g->cfg.favorites
                         : g->locationTab == 2 ? g->cfg.history
                                               : g->paths;
    std::vector<Location> locations;
    std::set<std::wstring> seen;
    if (g->locationTab == 0 && g->dialog.save)
        for (const auto &location : namingRecommendedLocations(g->structure, g->cfg))
            if (seen.insert(normalize(location.path)).second)
                locations.push_back(location);
    for (const auto &location : source)
        if (!location.path.empty() && seen.insert(normalize(location.path)).second)
            locations.push_back(location);
    return locations;
}
int locationColumnCount(size_t count) {
    return count == 4 ? 2 : static_cast<int>(std::clamp<size_t>(count, 1, 3));
}
int locationRowCount(size_t count) {
    if (g->locationTab == 2)
        return static_cast<int>(std::clamp<size_t>(count, 1, 3));
    int columns = locationColumnCount(count);
    return std::clamp((static_cast<int>(count) + columns - 1) / columns, 1, 3);
}
void resizeFolderRows(int rows) {
    if (rows == g->folderRows)
        return;
    int cutoff = px(g->folderTop + g->folderRows * FOLDER_ROW_STEP),
        delta = px((rows - g->folderRows) * FOLDER_ROW_STEP);
    for (auto h : g->children) {
        int id = GetDlgCtrlID(h);
        if ((id >= ID_LOCATION_FIRST && id < ID_LOCATION_FIRST + LOCATION_PAGE_SIZE) ||
            id == ID_FOLDER_SCROLL || id == ID_LOCATION_EMPTY || id == ID_LOCATION_PAGE)
            continue;
        RECT r{};
        GetWindowRect(h, &r);
        MapWindowPoints(nullptr, g->panel, reinterpret_cast<POINT *>(&r), 2);
        if (r.top >= cutoff)
            SetWindowPos(h, nullptr, r.left, r.top + delta, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    for (auto &r : g->fields)
        if (r.top >= cutoff)
            OffsetRect(&r, 0, delta);
    for (auto &label : g->labels)
        if (label.rect.top >= cutoff)
            OffsetRect(&label.rect, 0, delta);
    for (auto &y : g->dividers)
        if (y >= cutoff)
            y += delta;
    OffsetRect(&g->pathEditRect, 0, delta);
    g->height += (rows - g->folderRows) * FOLDER_ROW_STEP;
    g->folderRows = rows;
    SetWindowPos(g->panel, nullptr, 0, 0, px(g->width), px(g->height),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    reposition(false);
    InvalidateRect(g->panel, nullptr, TRUE);
}
void updateLocationShortcuts() {
    if (g->dialog.generic && !g->dialog.genericNavigate)
        return;
    auto locations = locationChoices();
    bool history = g->locationTab == 2;
    int columns = locationColumnCount(locations.size());
    int totalRows = (static_cast<int>(locations.size()) + columns - 1) / columns;
    int rows = locationRowCount(locations.size());
    resizeFolderRows(rows);
    g->locationPage = history ? 0 : std::clamp(g->locationPage, 0, std::max(0, totalRows - rows));
    size_t first = static_cast<size_t>(g->locationPage * columns);
    size_t count = history ? 2 : rows * columns;
    std::vector<Location> items(locations.begin() + first,
                                locations.begin() + std::min(first + count, locations.size()));
    bool changed =
        items.size() != g->displayPaths.size() ||
        !std::equal(items.begin(), items.end(), g->displayPaths.begin(), [](const auto &a, const auto &b) {
            return a.name == b.name && a.path == b.path && a.sources == b.sources;
        });
    if (changed) {
        for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
            g->actions.erase(ID_LOCATION_FIRST + i);
            showIfChanged(GetDlgItem(g->panel, ID_LOCATION_FIRST + i), SW_HIDE);
        }
        g->displayPaths = std::move(items);
    }
    int available = g->width - PANEL_MARGIN * 2 - (!history && totalRows > 3 ? 20 : 0);
    for (int i = 0; i < LOCATION_PAGE_SIZE; ++i) {
        int id = ID_LOCATION_FIRST + i;
        auto button = GetDlgItem(g->panel, id);
        if (!button)
            continue;
        bool present = static_cast<size_t>(i) < g->displayPaths.size();
        if (present) {
            const auto &location = g->displayPaths[i];
            auto name = location.name.empty() ? location.path : location.name;
            if (windowText(button) != name)
                setTextIfChanged(button, name.c_str());
            int left = history ? PANEL_MARGIN : PANEL_MARGIN + (available + 8) * (i % columns) / columns;
            int right = history ? PANEL_MARGIN + available
                                : PANEL_MARGIN + (available + 8) * (i % columns + 1) / columns - 8;
            positionChild(button, px(left), px(g->folderTop + (history ? i : i / columns) * FOLDER_ROW_STEP),
                          px(right - left), px(FOLDER_BUTTON_HEIGHT));
            g->actions[id] = {Action::Location, static_cast<size_t>(i)};
            tip(button, location.path +
                            (location.sources.starts_with(L"命名推荐") ? L"\n" + location.sources : L"") +
                            (g->locationTab == 0 && location.sources.find(L"已打开") != std::wstring::npos
                                 ? L"\n已打开 · 资源管理器窗口中的文件夹"
                                 : L"") +
                            L"\n单击切换目录；右键设置颜色；Ctrl + 单击直接保存");
        } else
            g->actions.erase(id);
        showIfChanged(button, present ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
    auto scroll = GetDlgItem(g->panel, ID_FOLDER_SCROLL);
    positionChild(scroll, px(g->width - PANEL_MARGIN - 16), px(g->folderTop), px(16),
                  px(rows * FOLDER_ROW_STEP - 8));
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
    info.nMin = 0;
    info.nMax = std::max(0, totalRows - 1);
    info.nPage = rows;
    info.nPos = g->locationPage;
    SetScrollInfo(scroll, SB_CTL, &info, TRUE);
    enableIfChanged(scroll, !g->busy);
    showIfChanged(scroll, !history && totalRows > 3 ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto more = GetDlgItem(g->panel, ID_LOCATION_PAGE);
    positionChild(more, px(PANEL_MARGIN), px(g->folderTop + 2 * FOLDER_ROW_STEP), px(available),
                  px(FOLDER_BUTTON_HEIGHT));
    setTextIfChanged(more, L"查看全部历史（" + std::to_wstring(locations.size()) + L"）…");
    showIfChanged(more, history && locations.size() > 2 ? SW_SHOWNOACTIVATE : SW_HIDE);
    const wchar_t *empty = g->locationTab == 1   ? L"暂无收藏 · 点击路径旁的 ☆ 添加"
                           : g->locationTab == 4 ? L"暂无最近保存记录 · 保存后自动记录（仅浏览见历史）"
                           : g->locationTab == 2 ? L"暂无浏览历史"
                                                 : L"暂无位置，可点击下方路径输入目录";
    setTextIfChanged(GetDlgItem(g->panel, ID_LOCATION_EMPTY), empty);
    showIfChanged(GetDlgItem(g->panel, ID_LOCATION_EMPTY), locations.empty() ? SW_SHOWNOACTIVATE : SW_HIDE);
    auto clearRecent = GetDlgItem(g->panel, ID_CLEAR_RECENT);
    showIfChanged(clearRecent, g->locationTab == 4 ? SW_SHOWNOACTIVATE : SW_HIDE);
    enableIfChanged(clearRecent, !g->busy && !g->cfg.recentSaves.empty());
    auto clearHistory = GetDlgItem(g->panel, ID_CLEAR_HISTORY);
    showIfChanged(clearHistory, history ? SW_SHOWNOACTIVATE : SW_HIDE);
    enableIfChanged(clearHistory, !g->busy && !g->cfg.history.empty());
}
