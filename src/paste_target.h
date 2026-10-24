#pragma once
#include "save_session_types.h"

namespace jump {
// Filesystem checks run on the existing worker, including for network paths.
// This operation only resolves the target; it never creates or saves a file.
inline void resolvePastedTarget(Result &result) {
    const std::filesystem::path path(result.job.directory);
    auto attributes = GetFileAttributesW(path.c_str());
    auto error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_SUCCESS;
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        result.data.directory = path.wstring();
        return;
    }
    if (error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
        result.error = L"无法访问粘贴的路径，请检查磁盘连接和访问权限。";
        return;
    }
    auto name = path.filename().wstring();
    if (!validName(name)) {
        result.error = L"粘贴的文件名无效，或文件夹不存在。";
        return;
    }
    attributes = GetFileAttributesW(path.parent_path().c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        result.error = L"粘贴路径的上级文件夹不存在或不可访问，请先选择有效目录。";
        return;
    }
    result.data.directory = path.parent_path().wstring();
    result.data.name = name;
}
} // namespace jump
