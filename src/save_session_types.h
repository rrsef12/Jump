#pragma once

// Data shared by the foreground UI and the background save-dialog worker.
// Keeping these transport types separate makes the threading boundary explicit
// and prevents the main window implementation from becoming the type registry.

#include "model.h"
#include "save_verification.h"
#include <windows.h>
#include <string>
#include <vector>

namespace jump {

struct DialogData {
    HWND hwnd = nullptr;
    HWND nameControl = nullptr, formatControl = nullptr;
    DWORD pid = 0;
    std::wstring name, directory, extension, format, source, process;
    bool save = false;
    bool blender = false;
    bool houdini = false;
    bool generic = false;
    bool bridge = false;
    std::wstring bridgeRevision;
    std::wstring selector, rawName, submitSelector;
    bool genericSubmit = false, genericNavigate = false, genericAddress = false;
    // 1: complete output filename, 2: explicit output-directory field, 3: app API.
    int genericDestination = 0;
    FilenameBinding controlScope;
};

enum class JobType { Inspect, Refresh, Apply, VerifySave, FocusFormat, BindControl, ResolvePaste };

struct Job {
    JobType type = JobType::Inspect;
    HWND hwnd = nullptr;
    DWORD pid = 0;
    ULONGLONG generation = 0;
    std::wstring directory, name;
    bool submit = false;
    SaveProbe probe;
    int verifyAttempt = 0;
    ULONGLONG verifyAt = 0;
    bool saveDialog = false;
    bool blender = false;
    bool houdini = false;
    bool generic = false, focusedBinding = false, requestedInspect = false;
    bool bridge = false;
    std::wstring bridgeRevision;
    POINT point{};
    std::wstring selector, expectedRawName, expectedDirectory, expectedExtension, submitSelector;
    std::vector<FilenameBinding> bindings;
    FilenameBinding controlScope;
    ULONGLONG pasteId = 0;
    std::wstring namingStructure, namingProject;
    HWND nameControl = nullptr, formatControl = nullptr;
};

struct Result {
    Job job;
    DialogData data;
    std::vector<Location> opened;
    std::wstring error;
    bool navigated = false, submitted = false;
    SaveProbe probe;
    bool saved = false;
    std::wstring verifiedFile;
    FilenameBinding binding;
};

} // namespace jump
