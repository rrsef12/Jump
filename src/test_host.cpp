#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <commdlg.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static std::wstring output, source;
static bool classic = false, openDialog = false, markdown = false;
static IFileDialog *activeDialog = nullptr;
static std::wstring knownFolder;
static DWORD writeDelay = 0;
static std::wstring customMode;
static HWND customName{}, customDirectory{};
static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND && LOWORD(wp) == 601 && HIWORD(wp) == EN_CHANGE && customMode == L"output-ignore") {
        wchar_t value[2048]{};
        GetWindowTextW(customDirectory, value, 2048);
        if (value != output)
            SetWindowTextW(customDirectory, output.c_str());
    }
    if (msg == WM_COMMAND && LOWORD(wp) == 602 && HIWORD(wp) == EN_CHANGE && customMode == L"empty")
        EnableWindow(GetDlgItem(hwnd, IDOK), GetWindowTextLengthW(customName) != 0);
    if (msg == WM_COMMAND && LOWORD(wp) == 602 && HIWORD(wp) == EN_CHANGE && customMode == L"redirect") {
        wchar_t value[2048]{};
        GetWindowTextW(customName, value, 2048);
        if (std::wstring(value).starts_with(L"角色")) {
            auto redirected = std::filesystem::path(output) / L"redirected";
            std::filesystem::create_directories(redirected);
            SetWindowTextW(customDirectory, redirected.c_str());
        }
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK && customName) {
        wchar_t name[2048]{};
        GetWindowTextW(customName, name, 2048);
        auto file = std::filesystem::path(output) / L"custom-result.txt";
        std::ofstream result(file, std::ios::binary);
        auto text = std::filesystem::path(name).u8string();
        result.write(reinterpret_cast<const char *>(text.data()), static_cast<std::streamsize>(text.size()));
        result.close();
        wchar_t directory[2048]{};
        GetWindowTextW(customDirectory, directory, 2048);
        auto target = std::filesystem::path(directory) / name;
        if (std::filesystem::exists(target) &&
            MessageBoxW(hwnd, L"Replace existing file?", L"Fixture overwrite", MB_YESNO | MB_DEFBUTTON2) !=
                IDYES)
            return 0;
        std::ofstream saved(target, std::ios::binary);
        saved << "Jump custom fixture saved";
        saved.close();
        if (customMode != L"stay-open")
            DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_APP + 20 && activeDialog) {
        activeDialog->SetFileTypeIndex(2);
        activeDialog->SetDefaultExtension(L"md");
        return 0;
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    std::wstring initialName;
    for (int i = 1; i < count; ++i) {
        if (std::wstring(args[i]) == L"--classic")
            classic = true;
        else if (std::wstring(args[i]) == L"--markdown")
            markdown = true;
        else if (std::wstring(args[i]) == L"--open")
            openDialog = true;
        else if (std::wstring(args[i]) == L"--known-folder" && i + 1 < count)
            knownFolder = args[++i];
        else if (std::wstring(args[i]) == L"--write-delay-ms" && i + 1 < count)
            writeDelay = std::min<DWORD>(wcstoul(args[++i], nullptr, 10), 5000);
        else if (std::wstring(args[i]) == L"--output" && i + 1 < count)
            output = args[++i];
        else if (std::wstring(args[i]) == L"--source" && i + 1 < count)
            source = args[++i];
        else if (std::wstring(args[i]) == L"--name" && i + 1 < count)
            initialName = args[++i];
        else if (std::wstring(args[i]) == L"--custom" && i + 1 < count)
            customMode = args[++i];
    }
    LocalFree(args);
    if (output.empty())
        return 2;
    std::filesystem::create_directories(output);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpszClassName = L"Jump.TestHost";
    wc.lpfnWndProc = HostProc;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    auto host = CreateWindowW(wc.lpszClassName, source.empty() ? L"Jump 测试文档" : source.c_str(),
                              WS_OVERLAPPEDWINDOW, 100, 100, 700, 500, nullptr, nullptr, instance, nullptr);
    if (!customMode.empty()) {
        SetWindowTextW(host, customMode == L"manual" || customMode == L"unknown-title" ? L"Asset output"
                                                                                       : L"Export asset");
        auto make = [&](const wchar_t *cls, const wchar_t *text, DWORD style, int id, int x, int y,
                        int width) {
            auto h = CreateWindowW(cls, text, WS_VISIBLE | WS_CHILD | style, x, y, width, 30, host,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
            SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            return h;
        };
        make(L"STATIC",
             customMode == L"no-directory"        ? L"Search:"
             : customMode.starts_with(L"output-") ? L"Output directory:"
                                                  : L"Folder:",
             0, 501, 20, 30, 110);
        customDirectory = make(L"EDIT", output.c_str(),
                               WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP |
                                   (customMode == L"output-readonly" ? ES_READONLY : 0),
                               601, 140, 30, 480);
        make(L"STATIC", customMode == L"manual" ? L"Asset key:" : L"File name:", 0, 502, 20, 100, 110);
        customName = make(L"EDIT", L"original.asset",
                          WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP |
                              (customMode == L"readonly"   ? ES_READONLY
                               : customMode == L"password" ? ES_PASSWORD
                                                           : 0),
                          602, 140, 100, 480);
        if (customMode == L"fullpath")
            SetWindowTextW(customName, (std::filesystem::path(output) / L"original.asset").c_str());
        if (customMode == L"output-empty")
            SetWindowTextW(customDirectory, L"");
        if (customMode == L"ambiguous") {
            make(L"STATIC", L"File name:", 0, 503, 20, 155, 110);
            make(L"EDIT", L"another.asset", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 603, 140, 155, 480);
        }
        if (customMode == L"ambiguous-directory" || customMode == L"output-ambiguous") {
            make(L"STATIC", L"Directory:", 0, 503, 20, 155, 110);
            make(L"EDIT", output.c_str(), WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 603, 140, 155, 480);
        }
        make(L"BUTTON", L"Save", BS_DEFPUSHBUTTON | WS_TABSTOP, IDOK, 500, 215, 120);
        if (customMode == L"empty") {
            SetWindowTextW(customName, L"");
            EnableWindow(GetDlgItem(host, IDOK), FALSE);
        }
        if (customMode == L"ambiguous-save")
            make(L"BUTTON", L"Export", WS_TABSTOP, 604, 350, 215, 120);
        ShowWindow(host, SW_SHOW);
        SetForegroundWindow(host);
        SetFocus(customName);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        CoUninitialize();
        return 0;
    }
    ShowWindow(host, SW_SHOW);
    SetForegroundWindow(host);
    std::wstring selected;
    if (classic) {
        wchar_t filename[32768] = L"原文件.txt";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = host;
        ofn.lpstrFilter = L"Text file (*.txt)\0*.txt\0Markdown (*.md)\0*.md\0\0";
        if (markdown)
            wcscpy_s(filename, L"原文件.md");
        if (!initialName.empty())
            wcsncpy_s(filename, initialName.c_str(), _TRUNCATE);
        ofn.nFilterIndex = markdown ? 2 : 1;
        ofn.lpstrFile = filename;
        ofn.nMaxFile = 32768;
        ofn.lpstrInitialDir = output.c_str();
        ofn.lpstrDefExt = markdown ? L"md" : L"txt";
        ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        bool ok = openDialog ? GetOpenFileNameW(&ofn) : GetSaveFileNameW(&ofn);
        if (ok)
            selected = filename;
    } else {
        IFileDialog *dialog = nullptr;
        HRESULT hr = CoCreateInstance(openDialog ? CLSID_FileOpenDialog : CLSID_FileSaveDialog, nullptr,
                                      CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (SUCCEEDED(hr)) {
            COMDLG_FILTERSPEC filters[] = {{L"Text file (*.txt)", L"*.txt"}, {L"Markdown (*.md)", L"*.md"}};
            dialog->SetFileTypes(2, filters);
            dialog->SetFileTypeIndex(markdown ? 2 : 1);
            dialog->SetDefaultExtension(markdown ? L"md" : L"txt");
            dialog->SetFileName(markdown ? L"原文件.md" : L"原文件.txt");
            if (!initialName.empty())
                dialog->SetFileName(initialName.c_str());
            IShellItem *folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(output.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
                dialog->SetFolder(folder);
                folder->Release();
            }
            if (!knownFolder.empty()) {
                const KNOWNFOLDERID *id = knownFolder == L"downloads" ? &FOLDERID_Downloads
                                          : knownFolder == L"desktop" ? &FOLDERID_Desktop
                                                                      : &FOLDERID_Documents;
                PIDLIST_ABSOLUTE pidl = nullptr;
                if (SUCCEEDED(SHGetKnownFolderIDList(*id, 0, nullptr, &pidl))) {
                    if (SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&folder)))) {
                        dialog->SetFolder(folder);
                        folder->Release();
                    }
                    CoTaskMemFree(pidl);
                }
            }
            activeDialog = dialog;
            if (SUCCEEDED(dialog->Show(host))) {
                IShellItem *item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item))) {
                    PWSTR value = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &value))) {
                        selected = value;
                        CoTaskMemFree(value);
                    }
                    item->Release();
                }
            }
            activeDialog = nullptr;
            dialog->Release();
        }
    }
    if (!selected.empty()) {
        if (writeDelay)
            Sleep(writeDelay);
        std::ofstream result(std::filesystem::path(output) / "host-result.txt", std::ios::binary);
        auto utf8 = std::filesystem::path(selected).u8string();
        result.write(reinterpret_cast<const char *>(utf8.data()), static_cast<std::streamsize>(utf8.size()));
        result.close();
        if (!openDialog) {
            std::ofstream saved(std::filesystem::path(selected), std::ios::binary);
            saved << "Jump native integration test\n";
        }
    }
    DestroyWindow(host);
    CoUninitialize();
    return selected.empty() ? 1 : 0;
}
