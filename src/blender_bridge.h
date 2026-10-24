#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Local, bounded RPC to Blender's file-browser adapter. The server process is
// checked before sending paths or names; no window coordinates or key injection.
namespace blender_bridge {
struct Reply {
    bool ok = false, save = false, submitted = false, navigated = false, canSubmit = false;
    std::wstring name, directory, extension, format, source, error, revision;
};
inline std::vector<std::wstring> lines(const std::wstring &text) {
    std::vector<std::wstring> out;
    size_t start = 0, end;
    while ((end = text.find(L'\n', start)) != text.npos) {
        out.push_back(text.substr(start, end - start)); start = end + 1;
    }
    out.push_back(text.substr(start));
    return out;
}
inline Reply request(HWND target, DWORD pid, HWND panel, const wchar_t *verb,
                     const std::wstring &directory = L"", const std::wstring &name = L"",
                     const std::wstring &extension = L"", bool submit = false,
                     const wchar_t *service = L"Blender", const wchar_t *protocol = L"JUMP_BLENDER_1",
                     const std::vector<std::wstring> &expected = {}) {
    Reply out;
    auto pipeName = std::wstring(L"\\\\.\\pipe\\Jump.") + service + L"." + std::to_wstring(pid);
    HANDLE pipe = INVALID_HANDLE_VALUE;
    auto deadline = GetTickCount64() + 1800;
    do {
        pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        auto error = GetLastError();
        if (error != ERROR_PIPE_BUSY && error != ERROR_FILE_NOT_FOUND) break;
        Sleep(25);
    } while (GetTickCount64() < deadline);
    if (pipe == INVALID_HANDLE_VALUE) {
        out.error = std::wstring(service) + L" 接入未就绪，请启用对应接入脚本。"; return out;
    }
    ULONG serverPid = 0;
    DWORD mode = PIPE_READMODE_MESSAGE | PIPE_NOWAIT;
    if (!GetNamedPipeServerProcessId(pipe, &serverPid) || serverPid != pid ||
        !SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
        CloseHandle(pipe); out.error = std::wstring(service) + L" 接入身份核验失败。"; return out;
    }
    auto message = std::wstring(protocol) + L"\n" + verb + L"\n" +
        std::to_wstring(reinterpret_cast<UINT_PTR>(target)) + L"\n" + directory + L"\n" + name + L"\n" +
        extension + L"\n" + (submit ? L"1" : L"0") + L"\n" + std::to_wstring(reinterpret_cast<UINT_PTR>(panel));
    for (const auto &part : expected) message += L"\n" + part;
    if (message.size() > 16000 || message.find(L'\0') != message.npos) {
        CloseHandle(pipe); out.error = L"接入请求过长或包含无效字符。"; return out;
    }
    DWORD bytes = 0;
    if (!WriteFile(pipe, message.data(), static_cast<DWORD>(message.size() * sizeof(wchar_t)), &bytes, nullptr) ||
        bytes != message.size() * sizeof(wchar_t)) {
        CloseHandle(pipe); out.error = std::wstring(L"无法发送 ") + service + L" 命名请求。"; return out;
    }
    wchar_t buffer[16384]{};
    deadline = GetTickCount64() + 3000;
    bool received = false;
    do {
        if (ReadFile(pipe, buffer, sizeof(buffer), &bytes, nullptr)) { received = bytes > 0; break; }
        auto error = GetLastError();
        if (error != ERROR_NO_DATA && error != ERROR_PIPE_LISTENING) break;
        Sleep(15);
    } while (GetTickCount64() < deadline);
    CloseHandle(pipe);
    if (!received || bytes % sizeof(wchar_t)) { out.error = std::wstring(service) + L" 未响应，未确认填入或保存。"; return out; }
    auto fields = lines(std::wstring(buffer, bytes / sizeof(wchar_t)));
    if (fields.size() >= 3 && fields[0] == protocol && fields[1] == L"ERROR") {
        out.error = fields[2]; return out;
    }
    const bool universal = std::wstring(protocol) == L"JUMP_SAVE_1";
    if ((universal ? (fields.size() != 11u && fields.size() != 12u) : fields.size() != 10u) ||
        fields[0] != protocol || fields[1] != L"OK" ||
        (fields[2] != L"0" && fields[2] != L"1") || (fields[8] != L"0" && fields[8] != L"1") ||
        (fields[9] != L"0" && fields[9] != L"1")) {
        out.error = std::wstring(service) + L" 接入响应无效。"; return out;
    }
    out.ok = true; out.save = fields[2] == L"1";
    out.name = fields[3]; out.directory = fields[4]; out.extension = fields[5];
    out.format = fields[6]; out.source = fields[7];
    out.submitted = fields[8] == L"1"; out.navigated = fields[9] == L"1";
    if (universal) {
        out.revision = fields[10];
        if (fields.size() == 12) {
            if (fields[11] != L"0" && fields[11] != L"1") { out.ok = false; out.error = std::wstring(service) + L" 接入能力响应无效。"; }
            out.canSubmit = fields[11] == L"1";
        }
    }
    return out;
}
} // namespace blender_bridge
