// Native popup navigation and accessibility stay with Windows; only Jump's
// own menu surfaces are painted here. The session restores every item on exit.
struct PopupMenuItem {
    HMENU menu{};
    UINT position{}, type{};
    ULONG_PTR data{};
    std::wstring text;
    bool submenu = false;
    bool clipped = false;
    std::wstring help;
};
struct PopupMenuSession {
    HWND owner{};
    HWND tooltip{};
    int maxWidth = 0;
    std::wstring tooltipText;
    std::deque<PopupMenuItem> items;
    std::vector<std::pair<HMENU, MENUINFO>> menus;
    HBRUSH background{};
    bool native = false;
    const PopupMenuItem *pendingTip{};
    UINT_PTR timerId() const {
        return reinterpret_cast<UINT_PTR>(this);
    }
    void hideTip() {
        KillTimer(owner, timerId());
        pendingTip = nullptr;
        if (tooltip) {
            TOOLINFOW info{sizeof(info)};
            info.hwnd = owner;
            info.uId = 1;
            SendMessageW(tooltip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&info));
        }
    }
    void showTip(const PopupMenuItem &item) {
        hideTip();
        if (!item.clipped && item.help.empty())
            return;
        if (!tooltip) {
            tooltip = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT, TOOLTIPS_CLASSW,
                                      nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0, owner,
                                      nullptr, g->instance, nullptr);
            if (!tooltip)
                return;
            if (!native) {
                SetWindowTheme(tooltip, L"", L"");
                SendMessageW(tooltip, TTM_SETTIPBKCOLOR, g->surface, 0);
                SendMessageW(tooltip, TTM_SETTIPTEXTCOLOR, g->fg, 0);
            }
            SendMessageW(tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(g->font), FALSE);
            SendMessageW(tooltip, TTM_SETMAXTIPWIDTH, 0, maxWidth - px(20));
            RECT margin{px(10), px(8), px(10), px(8)};
            SendMessageW(tooltip, TTM_SETMARGIN, 0, reinterpret_cast<LPARAM>(&margin));
            TOOLINFOW info{sizeof(info)};
            info.hwnd = owner;
            info.uId = 1;
            info.uFlags = TTF_TRACK | TTF_ABSOLUTE;
            info.lpszText = const_cast<wchar_t *>(L"");
            SendMessageW(tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
        }
        tooltipText = item.help;
        if (tooltipText.empty())
            for (size_t i = 0; i < item.text.size(); ++i) {
                tooltipText += item.text[i];
                if (item.text[i] == L'&' && i + 1 < item.text.size() && item.text[i + 1] == L'&')
                    ++i;
            }
        RECT row{};
        if (!GetMenuItemRect(nullptr, item.menu, item.position, &row))
            return;
        TOOLINFOW info{sizeof(info)};
        info.hwnd = owner;
        info.uId = 1;
        info.lpszText = tooltipText.data();
        SendMessageW(tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&info));
        auto size = SendMessageW(tooltip, TTM_GETBUBBLESIZE, 0, reinterpret_cast<LPARAM>(&info));
        int width = LOWORD(size), height = HIWORD(size);
        RECT open{};
        // Place help outside both the current menu and its open submenus.
        EnumThreadWindows(
            GetCurrentThreadId(),
            [](HWND window, LPARAM value) -> BOOL {
                wchar_t name[32]{};
                GetClassNameW(window, name, 32);
                if (IsWindowVisible(window) && wcscmp(name, L"#32768") == 0) {
                    RECT r{};
                    GetWindowRect(window, &r);
                    auto &bounds = *reinterpret_cast<RECT *>(value);
                    if (IsRectEmpty(&bounds))
                        bounds = r;
                    else
                        UnionRect(&bounds, &bounds, &r);
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&open));
        if (IsRectEmpty(&open))
            open = row;
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromRect(&row, MONITOR_DEFAULTTONEAREST), &monitor);
        const auto &work = monitor.rcWork;
        int gap = px(8), x = open.right + gap, y = row.top;
        if (x + width > work.right) {
            if (open.left - gap - width >= work.left)
                x = open.left - gap - width;
            else {
                x = row.left;
                y = open.bottom + gap;
                if (y + height > work.bottom)
                    y = open.top - gap - height;
            }
        }
        x = std::clamp(x, static_cast<int>(work.left),
                       std::max(static_cast<int>(work.left), static_cast<int>(work.right) - width));
        y = std::clamp(y, static_cast<int>(work.top),
                       std::max(static_cast<int>(work.top), static_cast<int>(work.bottom) - height));
        SendMessageW(tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(x, y));
        SendMessageW(tooltip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&info));
    }
    void selectTip(const PopupMenuItem &item) {
        hideTip();
        if (item.type & MFT_SEPARATOR)
            return;
        if (!item.help.empty()) {
            pendingTip = &item;
            SetTimer(owner, timerId(), 450, nullptr);
        } else if (item.clipped)
            showTip(item);
    }
    PopupMenuItem *find(ULONG_PTR data) {
        for (auto &item : items)
            if (reinterpret_cast<ULONG_PTR>(&item) == data)
                return &item;
        return nullptr;
    }
    void prepare(HMENU menu, const std::vector<PopupMenuHint> &hints) {
        if (!native) {
            MENUINFO previous{sizeof(previous)};
            previous.fMask = MIM_BACKGROUND | MIM_STYLE;
            GetMenuInfo(menu, &previous);
            menus.push_back({menu, previous});
            MENUINFO info{sizeof(info)};
            info.fMask = MIM_BACKGROUND | MIM_STYLE;
            info.hbrBack = background;
            info.dwStyle = previous.dwStyle | MNS_NOCHECK;
            SetMenuInfo(menu, &info);
        }
        for (int i = 0; i < GetMenuItemCount(menu); ++i) {
            wchar_t text[2048]{};
            MENUITEMINFOW entry{sizeof(entry)};
            entry.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING | MIIM_SUBMENU;
            entry.dwTypeData = text;
            entry.cch = static_cast<UINT>(std::size(text));
            if (!GetMenuItemInfoW(menu, i, TRUE, &entry))
                continue;
            if (entry.hSubMenu)
                prepare(entry.hSubMenu, hints);
            if (entry.fType & MFT_OWNERDRAW)
                continue;
            items.push_back(
                {menu, static_cast<UINT>(i), entry.fType, entry.dwItemData, text, entry.hSubMenu != nullptr});
            for (const auto &hint : hints)
                if (hint.menu == menu && hint.position == static_cast<UINT>(i)) {
                    items.back().help = hint.text;
                    break;
                }
            if (native)
                continue;
            entry.fMask = MIIM_FTYPE | MIIM_DATA;
            entry.fType |= MFT_OWNERDRAW;
            entry.dwItemData = reinterpret_cast<ULONG_PTR>(&items.back());
            SetMenuItemInfoW(menu, i, TRUE, &entry);
        }
    }
    ~PopupMenuSession() {
        hideTip();
        if (tooltip)
            DestroyWindow(tooltip);
        for (const auto &item : items)
            if (!native && IsMenu(item.menu)) {
                MENUITEMINFOW entry{sizeof(entry)};
                entry.fMask = MIIM_FTYPE | MIIM_DATA;
                entry.fType = item.type;
                entry.dwItemData = item.data;
                SetMenuItemInfoW(item.menu, item.position, TRUE, &entry);
            }
        for (auto &[menu, info] : menus)
            if (IsMenu(menu))
                SetMenuInfo(menu, &info);
        if (background)
            DeleteObject(background);
    }
};
LRESULT CALLBACK PopupMenuOwnerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    auto &session = *reinterpret_cast<PopupMenuSession *>(data);
    if (msg == WM_MEASUREITEM) {
        auto &measure = *reinterpret_cast<MEASUREITEMSTRUCT *>(lp);
        if (measure.CtlType == ODT_MENU)
            if (auto item = session.find(measure.itemData)) {
                HDC dc = GetDC(hwnd);
                auto old = SelectObject(dc, g->font);
                RECT text{};
                DrawTextW(dc, item->text.c_str(), -1, &text, DT_CALCRECT | DT_SINGLELINE);
                SelectObject(dc, old);
                ReleaseDC(hwnd, dc);
                // Windows adds its own border/gutter after owner measurement.
                int available = std::max(px(60), session.maxWidth - px(24));
                int width = static_cast<int>(text.right) + px(58);
                item->clipped = width > available;
                measure.itemWidth = std::clamp(width, std::min(px(160), available), available);
                measure.itemHeight = px(item->type & MFT_SEPARATOR ? 9 : 30);
                return TRUE;
            }
    }
    if (msg == WM_DRAWITEM) {
        const auto &draw = *reinterpret_cast<DRAWITEMSTRUCT *>(lp);
        if (draw.CtlType == ODT_MENU)
            if (auto item = session.find(draw.itemData)) {
                RECT r = draw.rcItem;
                FillRect(draw.hDC, &r, session.background);
                if (item->type & MFT_SEPARATOR) {
                    auto pen = CreatePen(PS_SOLID, 1, g->line);
                    auto old = SelectObject(draw.hDC, pen);
                    MoveToEx(draw.hDC, r.left + px(12), (r.top + r.bottom) / 2, nullptr);
                    LineTo(draw.hDC, r.right - px(12), (r.top + r.bottom) / 2);
                    SelectObject(draw.hDC, old);
                    DeleteObject(pen);
                    return TRUE;
                }
                bool disabled = (draw.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
                bool selected = !disabled && (draw.itemState & ODS_SELECTED);
                if (selected) {
                    auto brush = CreateSolidBrush(RGB(44, 60, 81));
                    RECT highlight = r;
                    InflateRect(&highlight, -px(4), -px(2));
                    auto ob = SelectObject(draw.hDC, brush),
                         op = SelectObject(draw.hDC, GetStockObject(NULL_PEN));
                    paintRoundedRect(draw.hDC, highlight.left, highlight.top, highlight.right,
                                     highlight.bottom, px(7), px(7));
                    SelectObject(draw.hDC, op);
                    SelectObject(draw.hDC, ob);
                    DeleteObject(brush);
                }
                COLORREF ink = disabled ? g->muted : g->fg;
                auto old = SelectObject(draw.hDC, g->font);
                SetBkMode(draw.hDC, TRANSPARENT);
                SetTextColor(draw.hDC, ink);
                RECT text = r;
                text.left += px(30);
                text.right -= px(26);
                DrawTextW(draw.hDC, item->text.c_str(), -1, &text,
                          DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                              (draw.itemState & ODS_NOACCEL ? DT_HIDEPREFIX : 0));
                SelectObject(draw.hDC, old);
                if (draw.itemState & ODS_CHECKED)
                    drawUiIcon(draw.hDC, UiIcon::Check, static_cast<float>(r.left + px(10)),
                               (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)),
                               disabled ? g->muted : RGB(167, 203, 252));
                if (item->submenu) {
                    auto pen = CreatePen(PS_SOLID, px(1), ink);
                    auto op = SelectObject(draw.hDC, pen);
                    int x = r.right - px(14), y = (r.top + r.bottom) / 2;
                    MoveToEx(draw.hDC, x - px(2), y - px(4), nullptr);
                    LineTo(draw.hDC, x + px(2), y);
                    LineTo(draw.hDC, x - px(2), y + px(4));
                    SelectObject(draw.hDC, op);
                    DeleteObject(pen);
                }
                return TRUE;
            }
    }
    if (msg == WM_MENUCHAR) {
        auto menu = reinterpret_cast<HMENU>(lp);
        wchar_t key = static_cast<wchar_t>(towupper(LOWORD(wp)));
        for (const auto &item : session.items)
            if (item.menu == menu &&
                !(GetMenuState(menu, item.position, MF_BYPOSITION) & (MF_DISABLED | MF_GRAYED))) {
                for (size_t i = 0; i + 1 < item.text.size(); ++i)
                    if (item.text[i] == L'&') {
                        if (item.text[i + 1] == L'&') {
                            ++i;
                            continue;
                        }
                        if (towupper(item.text[i + 1]) == key)
                            return MAKELRESULT(item.position, MNC_EXECUTE);
                    }
            }
    }
    if (msg == WM_MENUSELECT) {
        session.hideTip();
        auto menu = reinterpret_cast<HMENU>(lp);
        UINT flags = HIWORD(wp), selected = LOWORD(wp);
        if (menu && flags != 0xffff && !(flags & MF_SEPARATOR))
            for (const auto &item : session.items)
                if (item.menu == menu &&
                    (flags & MF_POPUP ? item.position == selected
                                      : GetMenuItemID(menu, item.position) == selected)) {
                    session.selectTip(item);
                    break;
                }
    }
    if (msg == WM_TIMER && wp == session.timerId()) {
        if (auto item = session.pendingTip)
            session.showTip(*item);
        else
            KillTimer(hwnd, session.timerId());
        return 0;
    }
    if (msg == WM_EXITMENULOOP)
        session.hideTip();
    return DefSubclassProc(hwnd, msg, wp, lp);
}
int trackPopupMenuWithHelp(HMENU menu, UINT flags, int x, int y, HWND owner,
                           const std::vector<PopupMenuHint> &hints) {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    bool native = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    if (native && hints.empty())
        return TrackPopupMenu(menu, flags, x, y, 0, owner, nullptr);
    PopupMenuSession session;
    session.owner = owner;
    session.native = native;
    session.background = CreateSolidBrush(g->surface);
    MONITORINFO monitor{sizeof(monitor)};
    POINT point{x, y};
    GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor);
    session.maxWidth = std::max(
        px(100), std::min(px(440), static_cast<int>(monitor.rcWork.right - monitor.rcWork.left) - px(32)));
    session.prepare(menu, hints);
    auto id = reinterpret_cast<UINT_PTR>(&session);
    SetWindowSubclass(owner, PopupMenuOwnerProc, id, reinterpret_cast<DWORD_PTR>(&session));
    int selected = TrackPopupMenu(menu, flags, x, y, 0, owner, nullptr);
    RemoveWindowSubclass(owner, PopupMenuOwnerProc, id);
    return selected;
}
int trackPopupMenu(HMENU menu, UINT flags, int x, int y, HWND owner) {
    return trackPopupMenuWithHelp(menu, flags, x, y, owner, {});
}
