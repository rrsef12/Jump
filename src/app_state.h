#pragma once

// Window/session state is kept in one plain aggregate so the message handlers
// can share it without hiding ownership in UI helper classes.  The aggregate is
// intentionally separate from main.cpp; this file contains no behavior.

#include "model.h"
#include "naming_session.h"
#include "naming_worker.h"
#include "save_target.h"
#include "config_writer.h"
#include "save_session_types.h"
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
    HINSTANCE instance{};
    HWND control{}, panel{}, manager{}, flyout{};
    HWND savedNotice{};
    std::wstring savedNoticePath, savedNoticeKey, lastHistoryDirectory;
    bool historyOnly = false;
    ULONGLONG pasteId = 0;
    bool pastePending = false;
    enum class Flyout { None, Locations, Presets } flyoutKind = Flyout::None;
    std::vector<jump::Location> flyoutLocations, filteredLocations;
    std::map<int, ButtonAction> flyoutActions;
    DWORD pid = GetCurrentProcessId();
    DWORD testTargetPid = 0;
    std::wstring testTargetImage;
    DWORD testFillDelayMs = 0;
    HANDLE singleton{}, workEvent{}, worker{};
    std::filesystem::path configFile;
    jump::Config cfg;
    std::unique_ptr<jump::ConfigWriter> configWriter;
    jump::DialogData dialog;
    jump::SaveProbe saveProbe;
    bool busySubmitting = false;
    std::optional<jump::Job> saveAfterFill;
    std::wstring originalName, originalStem, originalDirectory, stemName, status = L"后台待命", search;
    jump::NameParts parts;
    int numberMode = 0;
    bool folderName = false, variablesEnabled = false, variableEditor = false;
    std::wstring variableTemplateDraft, variableValuesDraft, smartCacheKey;
    bool variableAdvanced = false;
    std::vector<std::wstring> variableTokens, variableInputKeys;
    jump::RenamePreview smartPreview;
    std::unique_ptr<jump::NamingWorker> namingWorker;
    jump::NamingRequest namingRequest;
    std::optional<jump::NamingResult> readyNaming;
    unsigned long long namingId = 0;
    bool namingPending = false, namingSubmit = false, numberingApproved = false;
    std::unique_ptr<jump::SaveTargetWorker> targetWorker;
    std::wstring targetPath;
    unsigned long long targetGeneration=0;
    std::optional<jump::SaveTargetInfo> targetInfo;
    jump::NamingHistory namingHistory;
    jump::StructuredNaming structure;
    std::optional<jump::NamingSnapshot> freeRecovery;
    int structuredField = -1;
    std::wstring structuredDraft;
    std::optional<jump::NamingField> structuredOriginal;
    std::optional<jump::StructuredNaming> structuredBaseline;
    bool structuredEdited = false, structuredCheckpoint = false;
    unsigned long long structuredHistoryStart = 0;
    std::vector<jump::Preset> structuredChoices;
    bool structuredIme = false;
    HWND pendingEditorDismiss{};
    bool explicitEditorDismiss = false;
    ULONG_PTR editorRevision = 0;
    bool structuredAdvanced = false;
    bool structuredResumeNumber = false;
    std::map<int, std::vector<jump::Preset>> quickPicks;
    unsigned structuredVisibility = 0;
    int structuredFullHeight = 0;
    std::wstring structuredProject;
    std::optional<jump::RecentSubject> nativeSubjectPending;
    ULONG_PTR draftToken = 0;
    struct Draft { HWND hwnd{}; DWORD pid{}; std::wstring observed, directory, original, originalDirectory;
        jump::NamingSnapshot naming; jump::NamingHistory history; std::optional<jump::NamingSnapshot> recovery;
        std::wstring project; ULONG_PTR token=0; };
    std::vector<Draft> namingDrafts;
    std::wstring sessionExpression, sessionVariables;
    int sessionDigits = 0, sessionDateFormat = -1;
    bool sessionUpper = false;
    std::vector<jump::Location> paths, openedLocations, displayPaths;
    int locationTab = 1, locationPage = 0, folderRows = 1, folderTop = 92, folderWheel = 0;
    int inlineTool = 0, ruleIndex = 1;
    bool ruleEnabled = false, ruleRegex = false;
    std::wstring rulePattern, ruleReplacement, ruleError;
    jump::RenamePreview rulePreview;
    int namingMode = 1;
    bool settingsDirty = false, configPending = false, localNamePending = false;
    bool statusError = false;
    bool editingPath = false, pathPending = false;
    std::wstring pathDraft;
    RECT pathEditRect{}, locationTabsRect{};
    bool presetEditor = false;
    size_t presetIndex = static_cast<size_t>(-1);
    std::wstring presetDraft;
    bool attached = false, busy = false, paused = false,
         building = false, dirtyName = false, dark = false;
    HWND suppressed{};
    std::atomic<ULONGLONG> generation{0};
    std::mutex jobsMutex;
    std::deque<jump::Job> jobs;
    std::vector<jump::Job> saveChecks;
    std::vector<HWINEVENTHOOK> globalHooks, targetHooks;
    std::atomic<bool> stopWorker{false};
    std::map<HWND, std::wstring> tipStrings;
    bool bottom = false, pendingQuick = false;
    HWND bindingHint{};
    HHOOK bindingHook{};
    POINT bindingPoint{};
    ULONGLONG bindingDeadline = 0;
    HWND inspectCandidate{};
    int inspectAttempts = 0;
    bool rebuildPending = false;
    int openPanelMenu = 0;
    HWND applyFocus{};
    ButtonAction lastAction{Action::Location, 0};
    HFONT font{}, smallFont{}, boldFont{}, bodyFont{};
    bool panelMaterial = false, solidMaterial = false, animations = true;
    std::map<HWND, HoverMotion> hoverMotions;
    HBRUSH brush{}, inputBrush{};
    COLORREF field{}, surface{}, primary{}, bg = RGB(255, 255, 255), fg = RGB(35, 40, 50), muted = RGB(100, 108, 120),
             line = RGB(220, 225, 234), accent = RGB(46, 91, 190), selection = RGB(237, 243, 255);
    int dpi = 96, width = 320, height = 500;
    std::vector<HWND> children;
    std::map<int, ButtonAction> actions;
    std::vector<PanelLabel> labels;
    std::vector<RECT> fields;
    std::vector<RECT> managerFields;
    std::map<HWND, TextHighlights> highlights;
    std::vector<int> dividers;
    int nextAction = 1000;
    RECT lastTargetRect{};
    HWND tooltip{};
    UINT taskbarCreated{};
    size_t editing = 0;
    int managerTab = 0;
    int affixCategory = 0, builtAffixCategory = -1;
    bool managerLoading = false, editingFavorite = false, addingAffix = false;
};
