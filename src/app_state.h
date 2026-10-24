#pragma once

// Window/session state is kept in one plain aggregate so the message handlers
// can share it without hiding ownership in UI helper classes. The aggregate is
// intentionally separate from main.cpp; this file contains no behavior.

#include "config_writer.h"
#include "model.h"
#include "naming_session.h"
#include "naming_worker.h"
#include "save_session_types.h"
#include "save_target.h"
#include "ui_types.h"
#include <atomic>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct State {
    // Window handles & process identity
    HINSTANCE instance{};
    HWND control{}, panel{}, manager{}, flyout{}, savedNotice{}, tooltip{};
    UINT taskbarCreated{};
    DWORD pid = GetCurrentProcessId();
    DWORD testTargetPid = 0;
    std::wstring testTargetImage;
    DWORD testFillDelayMs = 0;
    HANDLE singleton{}, workEvent{}, worker{};

    // Configuration & persistence
    std::filesystem::path configFile;
    jump::Config cfg;
    std::unique_ptr<jump::ConfigWriter> configWriter;
    bool settingsDirty = false, configPending = false;

    // Attached save dialog, worker queue & hooks
    jump::DialogData dialog;
    jump::SaveProbe saveProbe;
    bool attached = false, busy = false, busySubmitting = false, paused = false, building = false;
    std::optional<jump::Job> saveAfterFill;
    HWND suppressed{}, inspectCandidate{};
    int inspectAttempts = 0;
    std::atomic<ULONGLONG> generation{0};
    std::mutex jobsMutex;
    std::deque<jump::Job> jobs;
    std::vector<jump::Job> saveChecks;
    std::vector<HWINEVENTHOOK> globalHooks, targetHooks;
    std::atomic<bool> stopWorker{false};
    std::wstring savedNoticePath, savedNoticeKey, lastHistoryDirectory;
    bool historyOnly = false;
    ULONGLONG pasteId = 0;
    bool pastePending = false;
    HWND bindingHint{};
    HHOOK bindingHook{};
    POINT bindingPoint{};
    ULONGLONG bindingDeadline = 0;

    // Naming engine, background numbering & target verification
    std::wstring originalName, originalStem, originalDirectory, stemName, status = L"后台待命", search;
    bool dirtyName = false, localNamePending = false, statusError = false;
    jump::NameParts parts;
    int numberMode = 0, namingMode = 1;
    bool folderName = false, variablesEnabled = false, variableEditor = false, variableAdvanced = false;
    std::wstring variableTemplateDraft, variableValuesDraft, smartCacheKey;
    std::vector<std::wstring> variableTokens, variableInputKeys;
    jump::RenamePreview smartPreview;
    std::unique_ptr<jump::NamingWorker> namingWorker;
    jump::NamingRequest namingRequest;
    std::optional<jump::NamingResult> readyNaming;
    unsigned long long namingId = 0;
    bool namingPending = false, namingSubmit = false, numberingApproved = false;
    std::unique_ptr<jump::SaveTargetWorker> targetWorker;
    std::wstring targetPath;
    unsigned long long targetGeneration = 0;
    std::optional<jump::SaveTargetInfo> targetInfo;
    jump::NamingHistory namingHistory;
    std::wstring sessionExpression, sessionVariables;
    int sessionDigits = 0, sessionDateFormat = -1;
    bool sessionUpper = false;

    // Structured naming & inline editor state
    jump::StructuredNaming structure;
    std::optional<jump::NamingSnapshot> freeRecovery;
    int structuredField = -1;
    std::wstring structuredDraft, structuredProject;
    std::optional<jump::NamingField> structuredOriginal;
    std::optional<jump::StructuredNaming> structuredBaseline;
    bool structuredEdited = false, structuredCheckpoint = false, structuredIme = false;
    bool structuredAdvanced = false, structuredResumeNumber = false;
    unsigned long long structuredHistoryStart = 0;
    std::vector<jump::Preset> structuredChoices;
    std::map<int, std::vector<jump::Preset>> quickPicks;
    unsigned structuredVisibility = 0;
    int structuredFullHeight = 0;
    HWND pendingEditorDismiss{};
    bool explicitEditorDismiss = false;
    ULONG_PTR editorRevision = 0;
    std::optional<jump::RecentSubject> nativeSubjectPending;
    ULONG_PTR draftToken = 0;
    struct Draft {
        HWND hwnd{};
        DWORD pid{};
        std::wstring observed, directory, original, originalDirectory;
        jump::NamingSnapshot naming;
        jump::NamingHistory history;
        std::optional<jump::NamingSnapshot> recovery;
        std::wstring project;
        ULONG_PTR token = 0;
    };
    std::vector<Draft> namingDrafts;

    // Location shortcuts, flyouts & inline modifiers
    enum class Flyout { None, Locations, Presets } flyoutKind = Flyout::None;
    std::vector<jump::Location> flyoutLocations, filteredLocations;
    std::map<int, ButtonAction> flyoutActions;
    std::vector<jump::Location> paths, openedLocations, displayPaths;
    int locationTab = 1, locationPage = 0, folderRows = 1, folderTop = 92, folderWheel = 0;
    bool editingPath = false, pathPending = false;
    std::wstring pathDraft;
    RECT pathEditRect{}, locationTabsRect{};
    int inlineTool = 0, ruleIndex = 1;
    bool ruleEnabled = false, ruleRegex = false;
    std::wstring rulePattern, ruleReplacement, ruleError;
    jump::RenamePreview rulePreview;
    bool presetEditor = false;
    size_t presetIndex = static_cast<size_t>(-1);
    std::wstring presetDraft;

    // Panel layout, theme & GDI resources
    bool dark = false, bottom = false, pendingQuick = false, rebuildPending = false;
    int openPanelMenu = 0;
    HWND applyFocus{};
    ButtonAction lastAction{Action::Location, 0};
    HFONT font{}, smallFont{}, boldFont{}, bodyFont{};
    bool panelMaterial = false, solidMaterial = false, animations = true;
    std::map<HWND, HoverMotion> hoverMotions;
    HBRUSH brush{}, inputBrush{};
    COLORREF field{}, surface{}, primary{}, bg = RGB(255, 255, 255), fg = RGB(35, 40, 50),
                                            muted = RGB(100, 108, 120), line = RGB(220, 225, 234),
                                            accent = RGB(46, 91, 190), selection = RGB(237, 243, 255);
    int dpi = 96, width = 320, height = 500;
    std::vector<HWND> children;
    std::map<int, ButtonAction> actions;
    std::vector<PanelLabel> labels;
    std::vector<RECT> fields, managerFields;
    std::map<HWND, TextHighlights> highlights;
    std::vector<int> dividers;
    std::map<HWND, std::wstring> tipStrings;
    int nextAction = 1000;

    // Settings center state
    size_t editing = 0;
    int managerTab = 0;
    int affixCategory = 0, builtAffixCategory = -1;
    bool managerLoading = false, addingAffix = false;
};
