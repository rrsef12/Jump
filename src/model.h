#pragma once
#include <windows.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace jump {
constexpr int PRESET_COLOR_COUNT = 6;
struct Preset {
    std::wstring name, value, keywords;
    bool pinned = true;
    int color = 0;
    bool operator==(const Preset &) const = default;
};
struct Location {
    std::wstring name, path, sources;
};
struct NamingField {
    std::wstring id, label, value;
    bool required = false;
    int source = 0; // 0: session value, 1: project name, 2: today's date, 3: fixed text
    std::vector<Preset> candidates;
    std::wstring defaultValue, joiner, prefix, suffix, conditionField, conditionValue;
    bool quick = true, ownJoiner = false;
    int casing = 0, validation = 0, width = 0, defaultSource = -1;
    // Explicitly excluded from this filename; reusable required/default rules stay intact.
    bool omitted = false;
    bool enabled = true; // Reusable scheme switch, independent of per-file omission.
    bool operator==(const NamingField &) const = default;
};
struct NamingLocationRule {
    std::wstring field, value, directory;
    bool operator==(const NamingLocationRule &) const = default;
};
struct StructuredNaming {
    std::wstring id, name, separator = L"_";
    std::vector<NamingField> fields;
    bool enabled = false, free = false, compact = false;
    int counter = 1, digits = 3, dateFormat = 0;
    bool upper = false;
    std::wstring primaryField = L"subject";
    // A manual native edit is a literal baseline until the next field action.
    bool literal = false;
    std::wstring literalName;
    std::vector<NamingLocationRule> locations;
    bool recommendLocations = true, warnExisting = true;
    bool operator==(const StructuredNaming &) const = default;
};
struct Recipe {
    std::wstring name, directory, value;
    bool stage = false, date = false;
    bool complete = false, relativeDirectory = false, folder = false, variables = false;
    int counter = 0, digits = 3, dateFormat = 0;
    bool upperVersion = false, ruleEnabled = false, ruleRegex = false;
    std::wstring prefix, suffix, stageValue, expression, variableValues, pattern, replacement;
    std::wstring structure;
    bool namingOnly = false;
};
struct Project {
    std::wstring id, name, root, variables, work = L"work", delivery = L"delivery";
};
struct SavedFile {
    std::wstring path, process, verifiedAt;
    std::wstring structure, projectId;
};
// A semantic UIA selector, scoped to one executable and one kind of window.
// Never persist HWNDs, runtime IDs, screen coordinates or the filename itself.
struct FilenameBinding {
    std::wstring executable, windowClass, windowTitle, selector;
};
struct CandidateRename {
    std::wstring schema, field, from, to;
    bool operator==(const CandidateRename &) const = default;
};
struct RecentSubject {
    std::wstring projectId, value;
    bool operator==(const RecentSubject &) const = default;
};
struct NamingCatalog;
struct Config {
    bool automatic = true, quickSave = true, presetPackage = false;
    int layout = 0;
    std::vector<Location> favorites, history, recentSaves;
    std::vector<std::wstring> modifiers, prefixes, suffixes;
    std::map<std::wstring, int> prefixColors, suffixColors;
    std::map<std::wstring, int> locationColors;
    std::vector<Recipe> recipes;
    std::vector<Project> projects;
    std::wstring activeProject;
    std::vector<SavedFile> savedFiles;
    std::vector<FilenameBinding> filenameBindings;
    std::vector<CandidateRename> candidateRenames;
    std::vector<RecentSubject> recentSubjects;
    std::vector<RecentSubject> removedSubjects;
    std::wstring variableTemplate = L"{项目}_{name}_{作者}", variableValues;
    bool managerPositionSaved = false;
    int managerX = 0, managerY = 0;
    int dateFormat = 0, numberDigits = 3;
    bool upperVersion = false;
    std::map<std::wstring, int> modifierColors;
    std::wstring activeNamingScheme;
    std::wstring defaultNamingScheme = L"asset-v2";
    // Lowercase executable basename -> scheme identity; independent of session picks.
    std::map<std::wstring, std::wstring> applicationNamingSchemes;
    // Immutable, shared by rollback snapshots; never serialized.
    mutable std::shared_ptr<const NamingCatalog> namingCatalog;
};
struct NameParts {
    std::wstring base, stage, date, prefix, suffix;
};
std::wstring expandAffix(std::wstring value, int dateFormat = 0);
bool validAffix(const std::wstring &value, bool prefix);
std::wstring composeBody(const NameParts &value);
struct RenameRule {
    std::wstring name, pattern, replacement, hint;
    bool regex = true;
};
struct RenamePreview {
    std::wstring name, error;
    std::vector<std::pair<size_t, size_t>> matches;
};
struct SmartNaming {
    int counter = 0; // 0: none, 1: _v001, 2: _001; mutually exclusive.
    bool folder = false;
    std::wstring expression, variables;
    int dateFormat = 0, digits = 3;
    bool upperVersion = false;
};
RenamePreview smartNamingPreview(const std::wstring &source, const std::wstring &directory,
                                 const std::wstring &extension, const SmartNaming &options,
                                 const std::function<bool()> &cancelled = {});
const Project *activeProject(const Config &cfg);
std::wstring projectPath(const Project &project, const std::wstring &relative);
std::wstring mergedVariables(const std::wstring &defaults, const std::wstring &project);
void rememberSavedFile(Config &cfg, const SavedFile &file);
bool backupConfig(const std::filesystem::path &file);
std::vector<std::filesystem::path> configBackups(const std::filesystem::path &file);
const std::vector<RenameRule> &renameRules();
RenamePreview renamePreview(const std::wstring &source, const std::wstring &prefix,
                            const std::wstring &suffix, const std::wstring &pattern,
                            const std::wstring &replacement, bool regex);
std::wstring trim(std::wstring value);
std::wstring lower(std::wstring value);
std::wstring normalize(std::wstring value);
std::wstring namingApplicationKey(const std::wstring &executable);
bool validName(const std::wstring &value);
std::wstring stripExtension(const std::wstring &value, const std::wstring &extension);
NameParts splitName(std::wstring value, const std::vector<std::wstring> &modifiers);
std::wstring compose(const NameParts &value);
std::wstring today(int format = 0);
// Counter format preview without reading a directory.
std::wstring numberingExample(const std::wstring &source, const SmartNaming &options);
Config defaults();
Config load(const std::filesystem::path &file, bool allowPresetPackage = false);
bool save(const Config &cfg, const std::filesystem::path &file);
void rememberLocation(Config &cfg, const Location &location);
void rememberSavedLocation(Config &cfg, const Location &location);
bool rememberSubject(Config &cfg, const std::wstring &projectId, std::wstring value);
bool removeRecentSubject(Config &cfg, const std::wstring &projectId, const std::wstring &value);
std::vector<Location> rankLocations(const Config &cfg, const std::wstring &source,
                                    const std::vector<Location> &opened);
bool selfTest(const std::filesystem::path &directory);
} // namespace jump
