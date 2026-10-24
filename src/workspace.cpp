#include "model.h"
#include <algorithm>
#include <sstream>

namespace jump {
const Project *activeProject(const Config &cfg) {
    for (const auto &project : cfg.projects) if (project.id == cfg.activeProject) return &project;
    return nullptr;
}
std::wstring projectPath(const Project &project, const std::wstring &relative) {
    std::filesystem::path root(project.root), part(relative);
    if (!root.is_absolute() || part.is_absolute() || part.has_root_name() || part.has_root_directory()) return {};
    for (const auto &element : part) if (element == L"..") return {};
    if (relative.find_first_of(L":*?\"<>|\r\n") != relative.npos) return {};
    return (root / part).lexically_normal().wstring();
}
std::wstring mergedVariables(const std::wstring &defaults, const std::wstring &project) {
    // Keep malformed input visible to the naming validator instead of discarding it.
    std::map<std::wstring, std::wstring> values;
    std::wstring invalid;
    for (const auto &input : {defaults, project}) {
        std::wistringstream stream(input); std::wstring line;
        std::map<std::wstring, bool> seen;
        while (std::getline(stream, line)) {
            line = trim(line); if (line.empty()) continue;
            auto at = line.find(L'=');
            if (at == line.npos) { invalid += line + L"\n"; continue; }
            auto key = trim(line.substr(0, at));
            if (seen.contains(key)) invalid += line + L"\n";
            seen[key] = true; values[key] = trim(line.substr(at + 1));
        }
    }
    for (const auto &[key, value] : values) invalid += key + L"=" + value + L"\n";
    return invalid;
}
void rememberSavedFile(Config &cfg, const SavedFile &file) {
    if (file.path.empty()) return;
    std::erase_if(cfg.savedFiles, [&](const auto &old) { return normalize(old.path) == normalize(file.path); });
    cfg.savedFiles.insert(cfg.savedFiles.begin(), file);
    if (cfg.savedFiles.size() > 50) cfg.savedFiles.resize(50);
}
std::vector<std::filesystem::path> configBackups(const std::filesystem::path &file) {
    std::vector<std::filesystem::path> result;
    for (int i = 1; i <= 5; ++i) {
        auto path = file; path += L".bak" + std::to_wstring(i);
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error)) result.push_back(path);
    }
    return result;
}
bool backupConfig(const std::filesystem::path &file) {
    std::error_code error;
    if (!std::filesystem::exists(file, error)) return !error;
    auto at = [&](int i) { auto p = file; p += L".bak" + std::to_wstring(i); return p; };
    // Copy before rotation so a failure never removes the only recoverable file.
    auto pending = file; pending += L".backup-pending";
    if (!CopyFileW(file.c_str(), pending.c_str(), FALSE)) return false;
    for (int i = 4; i >= 1; --i) {
        if (GetFileAttributesW(at(i).c_str()) != INVALID_FILE_ATTRIBUTES &&
            !CopyFileW(at(i).c_str(), at(i + 1).c_str(), FALSE)) { DeleteFileW(pending.c_str()); return false; }
    }
    bool ok = MoveFileExW(pending.c_str(), at(1).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) DeleteFileW(pending.c_str());
    return ok;
}
} // namespace jump
