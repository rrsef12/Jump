// Panel GDI/GDI+ rendering primitives, icons, theme colors, and fonts.
#include "panel_material.h"
class PaintBuffer {
    HDC target{}, memory{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    RECT bounds{};
    bool presented = false;
    DWORD *pixels = nullptr;
    bool material = false, revealBackdrop = false;

  public:
    HDC dc{};
    PaintBuffer(HDC output, const RECT &rect, bool composited = false, bool backdrop = true)
        : target(output), bounds(rect), material(composited), revealBackdrop(backdrop), dc(output) {
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0)
            return;
        memory = CreateCompatibleDC(output);
        if (!memory)
            return;
        if (material) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            bitmap = CreateDIBSection(output, &info, DIB_RGB_COLORS, reinterpret_cast<void **>(&pixels),
                                      nullptr, 0);
        } else
            bitmap = CreateCompatibleBitmap(output, width, height);
        if (!bitmap) {
            DeleteDC(memory);
            memory = nullptr;
            return;
        }
        previous = SelectObject(memory, bitmap);
        SetViewportOrgEx(memory, -rect.left, -rect.top, nullptr);
        dc = memory;
    }
    void present() {
        if (memory && !presented) {
            if (pixels) {
                GdiFlush();
                prepareMaterialPixels(
                    pixels, static_cast<size_t>(bounds.right - bounds.left) * (bounds.bottom - bounds.top),
                    revealBackdrop);
            }
            BitBlt(target, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                   memory, bounds.left, bounds.top, SRCCOPY);
            presented = true;
        }
    }
    ~PaintBuffer() {
        present();
        if (memory) {
            SelectObject(memory, previous);
            DeleteObject(bitmap);
            DeleteDC(memory);
        }
    }
    PaintBuffer(const PaintBuffer &) = delete;
    PaintBuffer &operator=(const PaintBuffer &) = delete;
};
class ClientPaint {
    HWND window{};
    bool painting{};
    PAINTSTRUCT ps{};
    HDC target{};
    static RECT client(HWND h) {
        RECT r{};
        GetClientRect(h, &r);
        return r;
    }
    PaintBuffer buffer;

  public:
    ClientPaint(HWND h, UINT msg, WPARAM output)
        : window(h), painting(msg == WM_PAINT),
          target(painting ? BeginPaint(h, &ps) : reinterpret_cast<HDC>(output)),
          buffer(target, client(h), panelComposited(h) && msg == WM_PAINT) {}
    HDC dc() const {
        return buffer.dc;
    }
    ~ClientPaint() {
        buffer.present();
        if (painting)
            EndPaint(window, &ps);
    }
};
// Filename fragments can be a single long word. Use the same explicit line
// breaks for measuring and painting instead of relying on word boundaries.
std::wstring wrappedFilename(HDC dc, const std::wstring &text, int width) {
    std::wstring result;
    for (size_t at = 0; at < text.size();) {
        int fit = 0;
        SIZE extent{};
        GetTextExtentExPointW(dc, text.c_str() + at, static_cast<int>(text.size() - at), std::max(1, width),
                              &fit, nullptr, &extent);
        size_t count = std::max(1, fit);
        if (at + count < text.size() && IS_HIGH_SURROGATE(text[at + count - 1]) &&
            IS_LOW_SURROGATE(text[at + count])) {
            if (count > 1)
                --count;
            else
                ++count;
        }
        if (!result.empty())
            result += L'\n';
        result += text.substr(at, count);
        at += count;
    }
    return result;
}
std::wstring trailingPath(HDC dc, const std::wstring &path, int width) {
    auto fits = [&](const std::wstring &text) {
        SIZE size{};
        GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
        return size.cx <= width;
    };
    if (fits(path))
        return path;
    for (size_t at = 0; (at = path.find_first_of(L"\\/", at)) != path.npos; ++at) {
        auto tail = L"…" + path.substr(at);
        if (fits(tail))
            return tail;
    }
    // An unusually long final directory still keeps its end visible.
    for (size_t at = 0; at < path.size(); ++at) {
        if (IS_LOW_SURROGATE(path[at]))
            continue;
        auto tail = L"…" + path.substr(at);
        if (fits(tail))
            return tail;
    }
    return L"…";
}
void paintMaterialText(HWND h, HDC dc, RECT r, DWORD style) {
    FillRect(dc, &r, g->brush);
    SetBkMode(dc, TRANSPARENT);
    auto id = GetDlgCtrlID(h);
    SetTextColor(dc,
                 (id == ID_STATUS && g->statusError) || (id == ID_RULE_HINT && !g->rulePreview.error.empty())
                     ? RGB(226, 122, 122)
                     : g->muted);
    if (id == ID_PREVIEW)
        SetTextColor(dc, g->fg);
    if (id == ID_STATUS && !g->statusError)
        SetTextColor(dc, RGB(124, 130, 140));
    if (id >= ID_FIELD_ERROR && id <= ID_RULE_ERROR)
        SetTextColor(dc, RGB(226, 122, 122));
    auto font = SelectObject(dc, reinterpret_cast<HFONT>(SendMessageW(h, WM_GETFONT, 0, 0)));
    UINT flags = DT_NOPREFIX | DT_WORDBREAK;
    if ((style & SS_TYPEMASK) == SS_CENTER)
        flags |= DT_CENTER;
    if (style & SS_ELLIPSISMASK)
        flags = (flags & ~DT_WORDBREAK) | DT_SINGLELINE | DT_END_ELLIPSIS;
    if ((style & SS_ELLIPSISMASK) == SS_PATHELLIPSIS)
        flags = DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX;
    if (id == ID_LOCATION_EMPTY)
        flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
    auto text = windowText(h);
    if (id == ID_GENERIC_DIRECTORY) {
        text = trailingPath(dc, text, r.right - r.left);
        flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
    }
    if (id == ID_PREVIEW) {
        text = wrappedFilename(dc, text, r.right - r.left);
        flags = DT_NOPREFIX;
        if (text.find(L'\n') == text.npos)
            flags |= DT_SINGLELINE | DT_VCENTER;
    }
    DrawTextW(dc, text.c_str(), -1, &r, flags);
    SelectObject(dc, font);
}
LRESULT CALLBACK MaterialControlProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR textStyle) {
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_PAINT || msg == WM_PRINTCLIENT) {
        ClientPaint paint(h, msg, wp);
        RECT r{};
        GetClientRect(h, &r);
        paintMaterialText(h, paint.dc(), r, static_cast<DWORD>(textStyle));
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}
void positionChild(HWND h, int x, int y, int width, int height) {
    RECT r{};
    GetWindowRect(h, &r);
    MapWindowPoints(nullptr, GetParent(h), reinterpret_cast<POINT *>(&r), 2);
    if (r.left == x && r.top == y && r.right - r.left == width && r.bottom - r.top == height)
        return;
    SetWindowPos(h, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}
void setTextIfChanged(HWND h, const std::wstring &text) {
    if (h && windowText(h) != text)
        SetWindowTextW(h, text.c_str());
}
void showIfChanged(HWND h, int command) {
    if (h && ((GetWindowLongPtrW(h, GWL_STYLE) & WS_VISIBLE) != 0) != (command != SW_HIDE))
        ShowWindow(h, command);
}
void enableIfChanged(HWND h, BOOL enabled) {
    if (h && IsWindowEnabled(h) != !!enabled)
        EnableWindow(h, enabled);
}
void setHighlightedText(HWND h, const std::wstring &text,
                        const std::vector<std::pair<size_t, size_t>> &matches) {
    if (!h)
        return;
    auto previous = g->highlights.find(h);
    if (previous != g->highlights.end() && previous->second.text == text &&
        previous->second.matches == matches)
        return;
    DWORD start = 0, end = 0;
    POINT scroll{};
    SendMessageW(h, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    SendMessageW(h, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    bool visible = IsWindowVisible(h) != FALSE;
    if (visible)
        SendMessageW(h, WM_SETREDRAW, FALSE, 0);
    setTextIfChanged(h, text);
    SendMessageW(h, EM_SETBKGNDCOLOR, 0, g->field);
    SendMessageW(h, EM_SETSEL, 0, -1);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = g->fg;
    format.crBackColor = g->field;
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    format.crTextColor = g->accent;
    format.crBackColor = g->selection;
    for (auto [at, length] : matches) {
        if (!length)
            continue;
        SendMessageW(h, EM_SETSEL, at, at + length);
        SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    SendMessageW(h, EM_SETSEL, start, end);
    SendMessageW(h, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    if (visible)
        SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, nullptr, FALSE);
    g->highlights[h] = {text, matches};
}
enum class UiIcon {
    Folder,
    Edit,
    Search,
    Star,
    Settings,
    Close,
    Plus,
    Check,
    Chevron,
    ChevronUp,
    Layers,
    Number,
    Undo,
    Redo,
    Affix,
    Replace,
    Info,
    CheckCircle
};
void ensureGraphics() {
    struct Runtime {
        ULONG_PTR token{};
        Runtime() {
            Gdiplus::GdiplusStartupInput input;
            Gdiplus::GdiplusStartup(&token, &input, nullptr);
        }
        ~Runtime() {
            if (token)
                Gdiplus::GdiplusShutdown(token);
        }
    };
    static Runtime runtime;
}
void paintRoundedRect(HDC dc, int left, int top, int right, int bottom, int ellipseWidth, int ellipseHeight) {
    ensureGraphics();
    LOGBRUSH brush{};
    LOGPEN pen{};
    GetObjectW(GetCurrentObject(dc, OBJ_BRUSH), sizeof(brush), &brush);
    GetObjectW(GetCurrentObject(dc, OBJ_PEN), sizeof(pen), &pen);
    const bool fill = brush.lbStyle != BS_NULL;
    const bool stroke = pen.lopnStyle != PS_NULL && (!fill || pen.lopnColor != brush.lbColor);
    float inset = stroke ? .5f : 0.f;
    float x = left + inset, y = top + inset, width = right - left - inset * 2,
          height = bottom - top - inset * 2;
    if (width <= 0 || height <= 0)
        return;
    float rx = std::min(width / 2, ellipseWidth / 2.f), ry = std::min(height / 2, ellipseHeight / 2.f);
    if (rx <= 0 || ry <= 0)
        return;
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, rx * 2, ry * 2, 180.f, 90.f);
    path.AddArc(x + width - rx * 2, y, rx * 2, ry * 2, 270.f, 90.f);
    path.AddArc(x + width - rx * 2, y + height - ry * 2, rx * 2, ry * 2, 0.f, 90.f);
    path.AddArc(x, y + height - ry * 2, rx * 2, ry * 2, 90.f, 90.f);
    path.CloseFigure();
    auto color = [](COLORREF value) {
        return Gdiplus::Color(255, GetRValue(value), GetGValue(value), GetBValue(value));
    };
    if (fill) {
        Gdiplus::SolidBrush background(color(brush.lbColor));
        graphics.FillPath(&background, &path);
    }
    if (stroke) {
        Gdiplus::Pen border(color(pen.lopnColor), 1.f);
        graphics.DrawPath(&border, &path);
    }
}
void drawUiIcon(HDC dc, UiIcon icon, float x, float y, float size, COLORREF color, bool filled = false) {
    ensureGraphics();
    Gdiplus::Graphics graphics(dc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.TranslateTransform(x, y);
    graphics.ScaleTransform(size / 16.f, size / 16.f);
    Gdiplus::Color ink(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(ink, 1.45f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    auto line = [&](std::initializer_list<Gdiplus::PointF> points) {
        graphics.DrawLines(&pen, points.begin(), static_cast<INT>(points.size()));
    };
    switch (icon) {
    case UiIcon::Folder: {
        Gdiplus::GraphicsPath path;
        path.StartFigure();
        path.AddLine(2.f, 5.f, 2.f, 3.5f);
        path.AddBezier(2.f, 3.5f, 2.f, 2.8f, 2.4f, 2.5f, 3.f, 2.5f);
        path.AddLine(3.f, 2.5f, 6.f, 2.5f);
        path.AddLine(6.f, 2.5f, 8.f, 4.5f);
        path.AddLine(8.f, 4.5f, 13.f, 4.5f);
        path.AddBezier(13.f, 4.5f, 13.7f, 4.5f, 14.f, 4.8f, 14.f, 5.5f);
        path.AddLine(14.f, 5.5f, 14.f, 12.5f);
        path.AddBezier(14.f, 12.5f, 14.f, 13.2f, 13.7f, 13.5f, 13.f, 13.5f);
        path.AddLine(13.f, 13.5f, 3.f, 13.5f);
        path.AddBezier(3.f, 13.5f, 2.3f, 13.5f, 2.f, 13.2f, 2.f, 12.5f);
        path.CloseFigure();
        graphics.DrawPath(&pen, &path);
        line({{2, 6}, {14, 6}});
        break;
    }
    case UiIcon::Edit:
        line({{3, 10.5f}, {10.5f, 3}, {13, 5.5f}, {5.5f, 13}, {2.5f, 13.5f}, {3, 10.5f}});
        line({{9, 4.5f}, {11.5f, 7}});
        break;
    case UiIcon::Search:
        graphics.DrawEllipse(&pen, 2.5f, 2.5f, 8.f, 8.f);
        line({{9.5f, 9.5f}, {13.5f, 13.5f}});
        break;
    case UiIcon::Star: {
        Gdiplus::PointF points[]{{8, 1.7f},  {9.9f, 5.6f},  {14.2f, 6.2f}, {11.1f, 9.2f}, {11.8f, 13.5f},
                                 {8, 11.5f}, {4.2f, 13.5f}, {4.9f, 9.2f},  {1.8f, 6.2f},  {6.1f, 5.6f}};
        Gdiplus::GraphicsPath path;
        path.AddPolygon(points, 10);
        if (filled) {
            Gdiplus::SolidBrush brush(ink);
            graphics.FillPath(&brush, &path);
        }
        graphics.DrawPath(&pen, &path);
        break;
    }
    case UiIcon::Settings:
        line({{2, 4}, {5, 4}});
        line({{9, 4}, {14, 4}});
        line({{2, 12}, {8, 12}});
        line({{12, 12}, {14, 12}});
        graphics.DrawEllipse(&pen, 5, 2, 4, 4);
        graphics.DrawEllipse(&pen, 8, 10, 4, 4);
        break;
    case UiIcon::Close:
        line({{4, 4}, {12, 12}});
        line({{12, 4}, {4, 12}});
        break;
    case UiIcon::Plus:
        line({{3, 8}, {13, 8}});
        line({{8, 3}, {8, 13}});
        break;
    case UiIcon::Check:
        line({{3, 8}, {6.5f, 11.5f}, {13, 4.5f}});
        break;
    case UiIcon::Chevron:
        line({{4, 6}, {8, 10}, {12, 6}});
        break;
    case UiIcon::ChevronUp:
        line({{4, 10}, {8, 6}, {12, 10}});
        break;
    case UiIcon::Layers:
        line({{2, 5}, {8, 2}, {14, 5}, {8, 8}, {2, 5}});
        line({{2, 8.5f}, {8, 11.5f}, {14, 8.5f}});
        line({{2, 12}, {8, 15}, {14, 12}});
        break;
    case UiIcon::Number:
        line({{6, 2}, {4, 14}});
        line({{12, 2}, {10, 14}});
        line({{3, 6}, {14, 6}});
        line({{2, 10}, {13, 10}});
        break;
    case UiIcon::Undo:
    case UiIcon::Redo: {
        if (icon == UiIcon::Redo) {
            graphics.TranslateTransform(16.f, 0.f);
            graphics.ScaleTransform(-1.f, 1.f);
        }
        Gdiplus::GraphicsPath path;
        path.AddBezier(14.f, 12.f, 14.f, 7.f, 11.f, 5.f, 7.f, 5.f);
        path.AddLine(7.f, 5.f, 2.f, 5.f);
        graphics.DrawPath(&pen, &path);
        line({{5, 2}, {2, 5}, {5, 8}});
        break;
    }
    case UiIcon::Affix:
        line({{5, 3}, {2, 3}, {2, 13}, {5, 13}});
        line({{11, 3}, {14, 3}, {14, 13}, {11, 13}});
        line({{7, 6}, {9, 6}});
        line({{7, 10}, {9, 10}});
        break;
    case UiIcon::Replace:
        line({{2, 5}, {14, 5}});
        line({{11, 2}, {14, 5}, {11, 8}});
        line({{14, 11}, {2, 11}});
        line({{5, 8}, {2, 11}, {5, 14}});
        break;
    case UiIcon::Info:
        graphics.DrawEllipse(&pen, 2.f, 2.f, 12.f, 12.f);
        line({{8, 7}, {8, 11}});
        graphics.DrawEllipse(&pen, 7.6f, 4.2f, .8f, .8f);
        break;
    case UiIcon::CheckCircle:
        graphics.DrawEllipse(&pen, 1.5f, 1.5f, 13.f, 13.f);
        line({{4.5f, 8}, {7, 10.5f}, {11.5f, 5.5f}});
        break;
    }
}
void paintTheme() {
    g->highlights.clear();
    g->dark = true;
    g->bg = RGB(22, 26, 33);
    g->field = RGB(16, 20, 27);
    g->surface = RGB(31, 37, 47);
    g->primary = RGB(68, 116, 193);
    g->fg = RGB(234, 238, 245);
    g->muted = RGB(148, 158, 175);
    g->line = RGB(43, 50, 62);
    g->accent = RGB(137, 172, 225);
    g->selection = RGB(41, 59, 83);
    if (g->brush)
        DeleteObject(g->brush);
    if (g->inputBrush)
        DeleteObject(g->inputBrush);
    g->brush = CreateSolidBrush(g->bg);
    g->inputBrush = CreateSolidBrush(g->field);
    BOOL dark = g->dark;
    DwmSetWindowAttribute(g->panel, 20, &dark, sizeof(dark));
    DWORD corners = 2;
    DwmSetWindowAttribute(g->panel, 33, &corners, sizeof(corners));
    COLORREF border = RGB(65, 73, 87);
    DwmSetWindowAttribute(g->panel, 34, &border, sizeof(border));
    updatePanelMaterial();
}
void fonts() {
    if (g->font)
        DeleteObject(g->font);
    if (g->smallFont)
        DeleteObject(g->smallFont);
    if (g->boldFont)
        DeleteObject(g->boldFont);
    if (g->bodyFont)
        DeleteObject(g->bodyFont);
    // Use the Windows Chinese UI sans-serif family consistently on every device.
    auto make = [](int size, int weight) {
        return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    };
    g->font = make(12, FW_NORMAL);
    g->smallFont = make(11, FW_NORMAL);
    g->boldFont = make(12, FW_MEDIUM);
    g->bodyFont = make(12, FW_NORMAL);
    if (g->savedNotice) {
        for (int id = 1; id <= 4; ++id)
            SendDlgItemMessageW(g->savedNotice, id, WM_SETFONT,
                                reinterpret_cast<WPARAM>(id == 2 ? g->smallFont : g->font), TRUE);
    }
}
