#pragma once
// A short-lived, non-activating native window. It outlives the save dialog, so
// verified completion stays visible even after Jump's attached panel closes.
LRESULT CALLBACK SavedNoticeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{}; GetClientRect(hwnd, &rect); FillRect(dc, &rect, g->brush);
        EndPaint(hwnd, &paint); return 0;
    }
    case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(wp), GetDlgCtrlID(reinterpret_cast<HWND>(lp)) == 2 ? g->muted : g->fg);
        SetBkColor(reinterpret_cast<HDC>(wp), g->bg); return reinterpret_cast<LRESULT>(g->brush);
    case WM_DRAWITEM: return SendMessageW(g->panel, msg, wp, lp);
    case WM_COMMAND:
        if (LOWORD(wp) == 3) {
            auto directory = std::filesystem::path(g->savedNoticePath).parent_path().wstring();
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
                SetDlgItemTextW(hwnd, 1, L"无法打开所在目录，可从保存记录复制路径。");
                SetTimer(hwnd, 1, 8000, nullptr); return 0;
            }
            DestroyWindow(hwnd);
        } else if (LOWORD(wp) == 4) DestroyWindow(hwnd);
        return 0;
    case WM_TIMER: {
        POINT point{}; GetCursorPos(&point); RECT rect{}; GetWindowRect(hwnd, &rect);
        if (PtInRect(&rect, point)) SetTimer(hwnd, 1, 1000, nullptr);
        else DestroyWindow(hwnd);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_NCDESTROY:
        if (g->savedNotice == hwnd) g->savedNotice = nullptr;
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
void showVerifiedSaveNotice(const Result &result) {
    if (!result.saved || result.verifiedFile.empty()) return;
    const auto &before = result.job.probe.before;
    auto key = normalize(result.verifiedFile) + L"|" + std::to_wstring(before.ftLastWriteTime.dwHighDateTime) + L":" +
        std::to_wstring(before.ftLastWriteTime.dwLowDateTime) + L":" + std::to_wstring(before.nFileSizeHigh) + L":" +
        std::to_wstring(before.nFileSizeLow);
    if (key == g->savedNoticeKey) return;
    g->savedNoticeKey = std::move(key);
    if (g->savedNotice) DestroyWindow(g->savedNotice);
    g->savedNoticePath = result.verifiedFile;
    WNDCLASSEXW cls{sizeof(cls)}; cls.hInstance = g->instance;
    cls.lpszClassName = L"Jump.SavedNotice"; cls.lpfnWndProc = SavedNoticeProc;
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW); RegisterClassExW(&cls);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromWindow(g->panel, MONITOR_DEFAULTTONEAREST), &monitor);
    auto title = L"已保存：" + std::filesystem::path(result.verifiedFile).filename().wstring();
    int width = px(344), height = px(104);
    auto window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, cls.lpszClassName,
        title.c_str(), WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
        monitor.rcWork.right - width - px(16), monitor.rcWork.bottom - height - px(16), width, height,
        nullptr, nullptr, g->instance, nullptr);
    if (!window) return;
    g->savedNotice = window;
    createChild(window, L"STATIC", title, SS_ENDELLIPSIS, 1, 12, 10, 316, 24, g->font);
    createChild(window, L"STATIC", result.verifiedFile, SS_PATHELLIPSIS, 2, 12, 36, 316, 20, g->smallFont);
    createChild(window, L"BUTTON", L"打开所在目录", BS_OWNERDRAW | WS_TABSTOP, 3, 10, 66, 144, 28, g->font);
    createChild(window, L"BUTTON", L"关闭", BS_OWNERDRAW | WS_TABSTOP, 4, 266, 66, 64, 28, g->font);
    SetTimer(window, 1, 8000, nullptr);
    ShowWindow(window, SW_SHOWNOACTIVATE);
}
