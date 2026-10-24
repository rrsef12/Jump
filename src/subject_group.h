int subjectGroupIndex(HWND control) {
    if (GetParent(control) != g->panel)
        return -1;
    int id = GetDlgCtrlID(control), index = -1;
    if (id >= ID_QUICK_EDIT && id < ID_QUICK_EDIT + 8)
        index = id - ID_QUICK_EDIT;
    else if (id >= ID_QUICK_INPUT && id < ID_QUICK_INPUT + 8)
        index = id - ID_QUICK_INPUT;
    else if (id >= ID_QUICK_CHOICE && id < ID_QUICK_CHOICE + 8)
        index = id - ID_QUICK_CHOICE;
    else if (id == ID_QUICK_DONE)
        index = g->structuredField;
    return index >= 0 && static_cast<size_t>(index) < g->structure.fields.size() &&
                   !namingFieldFixed(g->structure.fields[index])
               ? index
               : -1;
}
void invalidateSubjectGroup(HWND control) {
    int index = subjectGroupIndex(control);
    if (index < 0)
        return;
    for (int id : {ID_QUICK_EDIT + index, ID_QUICK_CHOICE + index, ID_QUICK_DONE})
        if (auto part = GetDlgItem(g->panel, id))
            InvalidateRect(part, nullptr, FALSE);
}
bool drawSubjectGroup(const DRAWITEMSTRUCT &draw) {
    int index = subjectGroupIndex(draw.hwndItem);
    if (index < 0)
        return false;
    auto arrow = GetDlgItem(g->panel, ID_QUICK_CHOICE + index);
    auto name = GetDlgItem(g->panel, ID_QUICK_EDIT + index);
    auto input = GetDlgItem(g->panel, ID_QUICK_INPUT + index);
    if (!arrow || (!name && !input))
        return false;
    FillRect(draw.hDC, &draw.rcItem, g->brush);
    bool editing = input != nullptr, open = g->openPanelMenu == ID_QUICK_CHOICE + index;
    HWND focus = GetFocus();
    bool focused = focus == name || focus == input || focus == arrow ||
                   editing && focus == GetDlgItem(g->panel, ID_QUICK_DONE);
    bool enabled = IsWindowEnabled(draw.hwndItem) != FALSE;
    RECT frame{}, end{};
    GetWindowRect(name ? name : input, &frame);
    GetWindowRect(arrow, &end);
    if (editing)
        frame.left -= px(10);
    frame.top = end.top;
    frame.bottom = end.bottom;
    frame.right = end.right;
    MapWindowPoints(nullptr, draw.hwndItem, reinterpret_cast<POINT *>(&frame), 2);
    bool hover = false;
    for (int id : {ID_QUICK_EDIT + index, ID_QUICK_CHOICE + index, ID_QUICK_DONE})
        if (auto part = GetDlgItem(g->panel, id)) {
            DWORD_PTR feedback = 0;
            GetWindowSubclass(part, PillProc, 1, &feedback);
            hover = hover || (feedback & 1);
        }
    COLORREF fill = editing ? g->field : RGB(29, 35, 45);
    if (hover && !editing)
        fill = mixColor(fill, RGB(255, 255, 255), 5);
    auto brush = CreateSolidBrush(fill);
    auto pen = CreatePen(PS_SOLID, 1,
                         input && GetPropW(input, L"Jump.InputError") ? RGB(191, 106, 110)
                         : focused || open                            ? g->accent
                                                                      : RGB(59, 74, 94));
    auto ob = SelectObject(draw.hDC, brush), op = SelectObject(draw.hDC, pen);
    paintRoundedRect(draw.hDC, frame.left, frame.top, frame.right, frame.bottom, px(10), px(10));
    SelectObject(draw.hDC, op);
    SelectObject(draw.hDC, ob);
    DeleteObject(pen);
    DeleteObject(brush);
    RECT r = draw.rcItem;
    int id = GetDlgCtrlID(draw.hwndItem);
    COLORREF icon = enabled ? hover || focused ? g->fg : g->muted : RGB(96, 106, 122);
    if (id == ID_QUICK_CHOICE + index) {
        auto divider = CreatePen(PS_SOLID, 1, RGB(53, 65, 82));
        auto old = SelectObject(draw.hDC, divider);
        MoveToEx(draw.hDC, r.left, r.top + px(7), nullptr);
        LineTo(draw.hDC, r.left, r.bottom - px(7));
        SelectObject(draw.hDC, old);
        DeleteObject(divider);
        drawUiIcon(draw.hDC, open ? UiIcon::ChevronUp : UiIcon::Chevron, (r.left + r.right - px(12)) / 2.f,
                   (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)), icon);
    } else if (id == ID_QUICK_DONE) {
        drawUiIcon(draw.hDC, UiIcon::Check, (r.left + r.right - px(12)) / 2.f,
                   (r.top + r.bottom - px(12)) / 2.f, static_cast<float>(px(12)), icon);
    } else {
        RECT text = r;
        text.left += px(10);
        text.right -= px(10);
        auto old = SelectObject(draw.hDC, g->boldFont);
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, enabled ? g->fg : g->muted);
        auto value = windowText(draw.hwndItem);
        DrawTextW(draw.hDC, value.c_str(), -1, &text,
                  DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(draw.hDC, old);
    }
    return true;
}
