// The panel uses the compositor's transient acrylic backdrop on Windows 11.
// GDI content gets explicit alpha below; text and native editors stay opaque.
// No global window opacity is applied to the floating panel.
bool panelTransparencyAllowed() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    if (g->solidMaterial || (contrast.dwFlags & HCF_HIGHCONTRASTON) || GetSystemMetrics(SM_REMOTESESSION)) return false;
    DWORD enabled = 1, size = sizeof(enabled);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &enabled, &size);
    return enabled != 0;
}
void configureMaterialEditor(HWND editor) {
    // Native EDIT draws caret and selection directly through GDI as well as in
    // WM_PAINT. An opaque redirected child surface preserves all those paths.
    // Alpha 255 applies to this editor only, never to the floating panel.
    LONG_PTR style = GetWindowLongPtrW(editor, GWL_EXSTYLE);
    if (g->panelMaterial) {
        SetWindowLongPtrW(editor, GWL_EXSTYLE, style | WS_EX_LAYERED);
        SetLayeredWindowAttributes(editor, 0, 255, LWA_ALPHA);
    } else if (style & WS_EX_LAYERED) SetWindowLongPtrW(editor, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
}
void updatePanelMaterial() {
    BOOL composition = FALSE, animation = TRUE;
    DwmIsCompositionEnabled(&composition);
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animation, 0);
    g->animations = animation && !GetSystemMetrics(SM_REMOTESESSION);
    // Numeric attributes keep the binary compatible with older Windows SDKs.
    // SYSTEMBACKDROP_TYPE (38): NONE=1, TRANSIENTWINDOW=3 (Desktop Acrylic).
    DWORD backdrop = composition && panelTransparencyAllowed() ? 3 : 1;
    g->panelMaterial = SUCCEEDED(DwmSetWindowAttribute(g->panel, 38, &backdrop, sizeof(backdrop))) && backdrop == 3;
    MARGINS glass = g->panelMaterial ? MARGINS{-1, -1, -1, -1} : MARGINS{};
    if (FAILED(DwmExtendFrameIntoClientArea(g->panel, &glass))) {
        g->panelMaterial = false;
        backdrop = 1;
        DwmSetWindowAttribute(g->panel, 38, &backdrop, sizeof(backdrop));
        MARGINS reset{}; DwmExtendFrameIntoClientArea(g->panel, &reset);
    }
    SetPropW(g->panel, L"Jump.Material", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(g->panelMaterial ? 1 : 0)));
    for (auto child : g->children) {
        auto cls = windowClass(child);
        if (cls == L"Edit" || cls == MSFTEDIT_CLASS) configureMaterialEditor(child);
    }
}

bool panelComposited(HWND window) {
    return g->panelMaterial && (window == g->panel || GetParent(window) == g->panel);
}

// Give ordinary GDI pixels a defined alpha channel before presenting to DWM.
// Only the panel background reveals the acrylic; every foreground pixel remains
// opaque, including antialiased text, focus rings, selections and input fields.
void prepareMaterialPixels(DWORD *pixels, size_t count, bool backdrop) {
    DWORD background = (GetRValue(g->bg) << 16) | (GetGValue(g->bg) << 8) | GetBValue(g->bg);
    for (size_t i = 0; i < count; ++i) {
        DWORD rgb = pixels[i] & 0xffffff;
        if (backdrop && rgb == background) {
            constexpr DWORD alpha = 224;
            pixels[i] = (alpha << 24) | ((GetRValue(g->bg) * alpha / 255) << 16) |
                ((GetGValue(g->bg) * alpha / 255) << 8) | (GetBValue(g->bg) * alpha / 255);
        } else pixels[i] = rgb | 0xff000000;
    }
}
