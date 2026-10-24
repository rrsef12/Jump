#pragma once
#include "model.h"
#include <algorithm>

namespace jump {
struct SaveStamp {
    std::wstring file;
    WIN32_FILE_ATTRIBUTE_DATA before{};
    bool existed = false, baselineKnown = false;
};
struct SaveProbe : SaveStamp {
    std::wstring process;
    std::wstring directory;
    std::vector<SaveStamp> alternatives;
    std::wstring namingStructure, namingProject;
    bool namingKnown = false;
};

inline SaveStamp readSaveStamp(const std::wstring &file) {
    SaveStamp stamp;
    stamp.file = file;
    stamp.existed = GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &stamp.before) != FALSE;
    auto error = stamp.existed ? ERROR_SUCCESS : GetLastError();
    stamp.baselineKnown = stamp.existed || error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    return stamp;
}
inline bool savedStampChanged(const SaveStamp &probe) {
    WIN32_FILE_ATTRIBUTE_DATA after{};
    if (!probe.baselineKnown || probe.file.empty() ||
        !GetFileAttributesExW(probe.file.c_str(), GetFileExInfoStandard, &after) ||
        (after.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;
    return !probe.existed || CompareFileTime(&after.ftLastWriteTime, &probe.before.ftLastWriteTime) != 0 ||
           CompareFileTime(&after.ftCreationTime, &probe.before.ftCreationTime) != 0 ||
           after.nFileSizeHigh != probe.before.nFileSizeHigh ||
           after.nFileSizeLow != probe.before.nFileSizeLow;
}
inline bool savedFileChanged(const SaveProbe &probe) {
    return savedStampChanged(probe) ||
           std::any_of(probe.alternatives.begin(), probe.alternatives.end(),
                       [](const SaveStamp &stamp) { return savedStampChanged(stamp); });
}
inline SaveProbe preserveSaveBaseline(const SaveProbe &previous, SaveProbe current) {
    // A transient empty address/button label during closing is not a new target.
    if (current.file.empty())
        return previous;
    auto preserve = [&](SaveStamp &stamp) {
        const SaveStamp *original = nullptr;
        if (normalize(previous.file) == normalize(stamp.file))
            original = &previous;
        for (const auto &alternative : previous.alternatives)
            if (normalize(alternative.file) == normalize(stamp.file))
                original = &alternative;
        if (original && original->baselineKnown) {
            stamp.before = original->before;
            stamp.existed = original->existed;
            stamp.baselineKnown = true;
        }
    };
    if (!current.file.empty())
        preserve(current);
    if (normalize(previous.file) == normalize(current.file) && !current.namingKnown) {
        current.namingStructure = previous.namingStructure;
        current.namingProject = previous.namingProject;
        current.namingKnown = previous.namingKnown;
    }
    for (auto &alternative : current.alternatives)
        preserve(alternative);
    return current;
}
} // namespace jump
