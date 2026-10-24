#include "model.h"
#include "structured_naming.h"
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <set>
#include <sstream>
#include <mutex>
#include <shlobj.h>
// Keep live previews bounded even when a custom expression backtracks heavily.
#ifdef _MSC_VER
#define _REGEX_MAX_COMPLEXITY_COUNT 200000L
#define _REGEX_MAX_STACK_COUNT 300L
#endif
#include <regex>

namespace jump {
// A date is a naming field, never an existing sequence counter.
static bool isCompactDate(const std::wstring &value) {
    if (value.size() != 8 || !std::all_of(value.begin(), value.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }))
        return false;
    int year = std::stoi(value.substr(0, 4)), month = std::stoi(value.substr(4, 2)), day = std::stoi(value.substr(6, 2));
    if (year < 1900 || year > 2199 || month < 1 || month > 12) return false;
    constexpr int days[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int limit = days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    return day >= 1 && day <= limit;
}
static bool isNamingDate(const std::wstring &value) {
    if (isCompactDate(value)) return true;
    return value.size() == 10 && (value[4] == L'-' || value[4] == L'_') && value[7] == value[4] &&
           isCompactDate(value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2));
}
static bool splitNamingCounter(const std::wstring &value,int counter,std::wstring &root,unsigned long &number,int &width) {
    if (counter == 2) for (size_t length : {size_t(10), size_t(8)})
        if (value.size() >= length && (value.size() == length || value[value.size() - length - 1] == L'_') &&
            isNamingDate(value.substr(value.size() - length))) return false;
    auto at = value.find_last_of(L'_');
    size_t digits = at == value.npos ? value.npos : at + (counter == 1 ? 2 : 1);
    if (at == value.npos || digits >= value.size() ||
        (counter == 1 && value[at + 1] != L'v' && value[at + 1] != L'V') ||
        value.size() - digits > 9 ||
        !std::all_of(value.begin() + digits, value.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return false;
    if (counter == 2 && isCompactDate(value.substr(digits))) return false;
    root = value.substr(0, at); number = wcstoul(value.c_str() + digits, nullptr, 10);
    width = static_cast<int>(value.size() - digits); return true;
}
std::wstring numberingExample(const std::wstring &source,const SmartNaming &options) {
    if(!options.counter)return source;
    std::wstring root=source;unsigned long number=0;int parsedWidth=0,width=std::clamp(options.digits,2,4);
    if(splitNamingCounter(source,options.counter,root,number,parsedWidth))width=std::max(width,parsedWidth);
    if(number>=999999999)return {};
    auto digits=std::to_wstring(number+1);if(digits.size()<static_cast<size_t>(width))digits.insert(0,width-digits.size(),L'0');
    auto result=root+(options.counter==1?(options.upperVersion?L"_V":L"_v"):L"_")+digits;
    return validName(result)?result:L"";
}
std::wstring trim(std::wstring v) {
    auto a = v.find_first_not_of(L" \r\n\t");
    if (a == v.npos)
        return {};
    return v.substr(a, v.find_last_not_of(L" \r\n\t") - a + 1);
}
std::wstring lower(std::wstring v) {
    std::transform(v.begin(), v.end(), v.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return v;
}
std::wstring normalize(std::wstring v) {
    v = lower(trim(v));
    std::replace(v.begin(), v.end(), L'/', L'\\');
    while (v.size() > 3 && v.back() == L'\\')
        v.pop_back();
    return v;
}
bool validName(const std::wstring &v) {
    if (v.empty() || v.size() > 240 || v.back() == L'.' || v.back() == L' ')
        return false;
    if (v.find_first_of(L"\\/:*?\"<>|") != v.npos)
        return false;
    if (std::any_of(v.begin(), v.end(), [](wchar_t c) { return c < 32; }))
        return false;
    auto base = lower(v.substr(0, v.find(L'.')));
    while (!base.empty() && base.back() == L' ') base.pop_back();
    if (base == L"con" || base == L"prn" || base == L"aux" || base == L"nul" ||
        base == L"conin$" || base == L"conout$")
        return false;
    if (base.size() == 4 && (base.starts_with(L"com") || base.starts_with(L"lpt")) &&
        ((base.back() >= L'1' && base.back() <= L'9') ||
         base.back() == L'¹' || base.back() == L'²' || base.back() == L'³'))
        return false;
    return true;
}
std::wstring namingApplicationKey(const std::wstring &executable) {
    auto name=lower(std::filesystem::path(trim(executable)).filename().wstring());
    return name.ends_with(L".exe")&&validName(name)?name:L"";
}
std::wstring stripExtension(const std::wstring &v, const std::wstring &ext) {
    if (!ext.empty() && v.size() > ext.size() && lower(v).ends_with(lower(ext)))
        return v.substr(0, v.size() - ext.size());
    return v;
}
NameParts splitName(std::wstring v, const std::vector<std::wstring> &mods) {
    NameParts p;
    for (size_t length : {size_t(10), size_t(8)}) {
        if (v.size() > length + 1 && v[v.size() - length - 1] == L'_' && isNamingDate(v.substr(v.size() - length))) {
            p.date = v.substr(v.size() - length); v.resize(v.size() - length - 1); break;
        }
    }
    auto sorted = mods;
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.size() > b.size(); });
    bool again = true;
    while (again) {
        again = false;
        for (const auto &m : sorted) {
            if (!m.empty() && v.size() > m.size() + 1 && v.ends_with(L"_" + m)) {
                if (p.stage.empty())
                    p.stage = m;
                v.resize(v.size() - m.size() - 1);
                again = true;
                break;
            }
        }
    }
    p.base = v;
    return p;
}
std::wstring compose(const NameParts &p) {
    return p.prefix + composeBody(p) + p.suffix;
}
std::wstring composeBody(const NameParts &p) {
    return p.base + (p.stage.empty() ? L"" : L"_" + p.stage) + (p.date.empty() ? L"" : L"_" + p.date);
}
std::wstring expandAffix(std::wstring value, int dateFormat) {
    size_t at = 0;
    auto date = today(dateFormat);
    while ((at = value.find(L"{date}", at)) != value.npos) {
        value.replace(at, 6, date);
        at += date.size();
    }
    return value;
}
bool validAffix(const std::wstring &value, bool prefix) {
    if (value.empty() || value.size() > 80) return false;
    auto expanded = expandAffix(value);
    return validName(prefix ? expanded + L"文件名" : L"文件名" + expanded);
}
std::wstring today(int format) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t b[16];
    swprintf_s(b, format == 1 ? L"%04u-%02u-%02u" : format == 2 ? L"%04u_%02u_%02u" : L"%04u%02u%02u", t.wYear, t.wMonth, t.wDay);
    return b;
}
RenamePreview smartNamingPreview(const std::wstring &source, const std::wstring &directory,
                                const std::wstring &extension, const SmartNaming &options,
                                const std::function<bool()> &cancelled) {
    RenamePreview out;
    auto fail = [&](const wchar_t *message) { out.error = message; return out; };
    std::wstring folder = directory;
    while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/')) folder.pop_back();
    folder = std::filesystem::path(folder).filename().wstring();
    if (!validName(folder)) folder.clear();
    std::wstring name = source;
    if (!options.expression.empty()) {
        if (options.expression.size() > 160 || options.variables.size() > 1200)
            return fail(L"表达式或变量内容过长，请缩短。");
        std::map<std::wstring, std::wstring> variables;
        std::wistringstream lines(options.variables); std::wstring line;
        while (std::getline(lines, line)) {
            line = trim(line); if (line.empty()) continue;
            auto at = line.find(L'=');
            if (at == line.npos) return fail(L"变量请按“项目=项目名”填写，每行一个。");
            auto key = trim(line.substr(0, at)), value = trim(line.substr(at + 1));
            if (key.empty() || key.size() > 32 || key.find_first_of(L"{}= \t") != key.npos ||
                (!value.empty() && !validName(value)) || variables.contains(key) || variables.size() >= 20)
                return fail(L"变量名不能重复，变量值不能包含路径字符。");
            if (key == L"name" || key == L"名称" || key == L"folder" || key == L"文件夹" || key == L"date" || key == L"日期")
                return fail(L"name、folder、date 和对应中文名称是内置变量，请换一个变量名。");
            variables.emplace(key, value);
        }
        variables[L"name"] = variables[L"名称"] = source;
        variables[L"folder"] = variables[L"文件夹"] = folder;
        variables[L"date"] = variables[L"日期"] = today(options.dateFormat);
        name.clear();
        for (size_t i = 0; i < options.expression.size();) {
            auto ch = options.expression[i];
            if (ch == L'}') return fail(L"表达式中的花括号不成对。");
            if (ch != L'{') { name += ch; ++i; continue; }
            auto end = options.expression.find(L'}', i + 1);
            if (end == std::wstring::npos) return fail(L"表达式中的花括号不成对。");
            auto key = options.expression.substr(i + 1, end - i - 1);
            auto found = variables.find(key);
            if (found == variables.end() || found->second.empty()) {
                out.error = L"请先填写变量：" + key; return out;
            }
            name += found->second; i = end + 1;
        }
    }
    if (options.folder) {
        if (folder.empty()) return fail(L"当前窗口没有可用的文件夹名，请先选择保存位置。");
        if (!name.starts_with(folder + L"_")) name = folder + L"_" + name;
    }
    if (!validName(name)) return fail(L"组合结果不是有效文件名，请调整表达式或变量。");
    if (options.counter) {
        if (cancelled && cancelled()) return fail(L"编号计算已取消。");
        if (directory.empty()) return fail(L"当前窗口无法读取保存目录，请手动编号。");
        auto marker = options.counter == 1 ? (options.upperVersion ? L"_V" : L"_v") : L"_";
        auto splitCounter = [&](const std::wstring &value, std::wstring &root, unsigned long &number, int &width) {
            return splitNamingCounter(value,options.counter,root,number,width);
        };
        std::wstring root, parsedRoot; unsigned long highest = 0, number = 0; int width = std::clamp(options.digits, 2, 4), parsedWidth = 0;
        if (!splitCounter(name, root, highest, parsedWidth)) root = name;
        else width = std::max(width, parsedWidth);
        if (!directory.empty()) {
            if (extension.find_first_of(L"\\/:*?\"<>|") != extension.npos)
                return fail(L"扩展名无效，无法计算编号。");
            auto mask = std::filesystem::path(directory) / (root + marker + L"*" + extension);
            WIN32_FIND_DATAW data{};
            HANDLE handle = FindFirstFileExW(mask.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
            DWORD error = handle == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
            if (handle != INVALID_HANDLE_VALUE) {
                int count = 0;
                do {
                    if (cancelled && cancelled()) { error = ERROR_CANCELLED; break; }
                    if (++count > 10000) { error = ERROR_MORE_DATA; break; }
                    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    std::wstring file = data.cFileName;
                    if (!extension.empty() && !lower(file).ends_with(lower(extension))) continue;
                    file = stripExtension(file, extension);
                    if (splitCounter(file, parsedRoot, number, parsedWidth) && lower(parsedRoot) == lower(root)) {
                        highest = std::max(highest, number); width = std::max(width, parsedWidth);
                    }
                } while (FindNextFileW(handle, &data));
                if (error == ERROR_SUCCESS) error = GetLastError();
                FindClose(handle);
            }
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_NO_MORE_FILES && error != ERROR_SUCCESS)
                return fail(error == ERROR_MORE_DATA ? L"同名文件过多，请手动指定编号。" : L"无法读取保存目录，请检查位置后重试。");
        }
        if (highest >= 999999999) return fail(L"编号已到上限，请更换文件名。");
        auto digits = std::to_wstring(highest + 1);
        if (digits.size() < static_cast<size_t>(width)) digits.insert(0, width - digits.size(), L'0');
        name = root + marker + digits;
    }
    if (!validName(name)) return fail(L"组合后的文件名过长，请缩短名称。");
    out.name = name;
    return out;
}
const std::vector<RenameRule> &renameRules() {
    static const std::vector<RenameRule> rules = {
        {L"仅添加前缀 / 后缀", L"", L"", L"输入前缀或后缀，下面立即预览结果。", false},
        {L"文字替换", L"", L"", L"输入要查找的文字；替换为留空即删除。", false},
        {L"空格 → 下划线", LR"(\s+)", L"_", L"将连续空格合并为一个下划线。"},
        {L"删除数字", LR"(\d+)", L"", L"删除文件名中的数字，保留其他文字。"},
        {L"去掉括号内容", LR"(\([^()]*\)|（[^（）]*）|\[[^\[\]]*\]|【[^【】]*】)", L"",
         L"去掉 ()、（）、[]、【】及里面的内容。"},
        {L"统一分隔符", LR"([\s_-]+)", L"_", L"将空格、横线、下划线统一为一个下划线。"},
        {L"去掉末尾版本号", LR"([_ -]?[vV]\d+(\.\d+)*$)", L"", L"例如：角色_v02 → 角色，报告 V1.2 → 报告。"},
        {L"自定义正则", LR"((.+)_([0-9]+))", L"$1-$2",
         L"括号分组，$1 / $2 引用分组；\\d 数字，\\s 空格，^ 开头，$ 结尾。"}
    };
    return rules;
}
RenamePreview renamePreview(const std::wstring &source, const std::wstring &prefix,
                            const std::wstring &suffix, const std::wstring &pattern,
                            const std::wstring &replacement, bool regex) {
    RenamePreview out;
    if (source.size() > 240 || prefix.size() > 120 || suffix.size() > 120 || pattern.size() > 160 ||
        replacement.size() > 240) {
        out.error = L"内容过长，请缩短名称或规则。";
        return out;
    }
    std::wstring renamed = source;
    if (!pattern.empty()) {
        try {
            if (regex) {
                std::wregex expression(pattern, std::regex_constants::ECMAScript);
                for (std::wsregex_iterator it(source.begin(), source.end(), expression), end; it != end; ++it)
                    out.matches.push_back({static_cast<size_t>(it->position()), static_cast<size_t>(it->length())});
                renamed = std::regex_replace(source, expression, replacement);
            } else {
                renamed.clear();
                size_t start = 0, at = 0;
                while ((at = source.find(pattern, start)) != source.npos) {
                    out.matches.push_back({at, pattern.size()});
                    renamed += source.substr(start, at - start) + replacement;
                    start = at + pattern.size();
                }
                renamed += source.substr(start);
            }
        } catch (const std::regex_error &e) {
            out.error = e.code() == std::regex_constants::error_complexity ||
                        e.code() == std::regex_constants::error_stack ?
                        L"表达式过于复杂，请简化规则。" : L"表达式有误，请检查括号、方括号或转义符。";
            return out;
        }
    }
    out.name = prefix + renamed + suffix;
    if (!validName(out.name)) out.error = L"结果不是有效文件名，请调整规则或前后缀。";
    return out;
}
Config defaults() {
    Config c;
    c.modifiers = {L"初稿", L"定稿", L"交付", L"预览"};
    c.prefixes = {L"项目_", L"角色_", L"场景_", L"道具_", L"参考_", L"{date}_"};
    c.suffixes = {L"_初稿", L"_定稿", L"_交付", L"_预览", L"_参考", L"_{date}"};
    // Favorites are explicit user choices. System folders belong in location
    // search and must not be silently marked as collected on first launch.
    return c;
}
static std::wstring encode(const std::wstring &s) {
    static constexpr wchar_t hex[]=L"0123456789ABCDEF";
    std::wstring out(s.size()*4,L'0');
    for(size_t i=0;i<s.size();++i){auto ch=static_cast<unsigned short>(s[i]);
        for(int digit=0;digit<4;++digit)out[i*4+digit]=hex[(ch>>(12-digit*4))&15];
    }
    return out;
}
static std::wstring decode(const std::wstring &s) {
    if (s.size() % 4)
        return {};
    std::wstring out(s.size()/4,L'\0');
    for(size_t i=0;i<out.size();++i){unsigned value=0;
        for(size_t digit=0;digit<4;++digit){auto ch=s[i*4+digit];
            unsigned nibble=ch>=L'0'&&ch<=L'9'?ch-L'0':ch>=L'A'&&ch<=L'F'?ch-L'A'+10:ch>=L'a'&&ch<=L'f'?ch-L'a'+10:16;
            if(nibble==16)return {};value=(value<<4)|nibble;
        }
        out[i]=static_cast<wchar_t>(value);
    }
    return out;
}
// Read each section once. Keep Windows' INI parsing/encoding semantics instead
// of rescanning the whole profile and allocating 64 KiB for every single key.
class ProfileReader {
    std::filesystem::path path;
    mutable std::map<std::wstring,std::map<std::wstring,std::wstring>> sections;
public:
    explicit ProfileReader(const std::filesystem::path &file):path(file){}
    std::wstring value(const std::wstring &section,const std::wstring &key) const {
        auto identity=lower(section);auto found=sections.find(identity);
        if(found==sections.end()){
            std::vector<wchar_t> buffer(4096);DWORD length;
            for(;;){
                length=GetPrivateProfileSectionW(section.c_str(),buffer.data(),static_cast<DWORD>(buffer.size()),path.c_str());
                if(length<buffer.size()-2)break;
                if(buffer.size()>=1048576)return {};
                buffer.resize(buffer.size()*2);
            }
            std::map<std::wstring,std::wstring> values;
            for(size_t at=0;at<length;){std::wstring entry(buffer.data()+at);at+=entry.size()+1;
                auto equal=entry.find(L'=');if(equal==entry.npos)continue;
                auto name=lower(trim(entry.substr(0,equal))),text=trim(entry.substr(equal+1));
                if(text.size()>=2&&(text.front()==L'\''||text.front()==L'"')&&text.back()==text.front())text=text.substr(1,text.size()-2);
                values.try_emplace(std::move(name),std::move(text));
            }
            found=sections.emplace(std::move(identity),std::move(values)).first;
        }
        auto item=found->second.find(lower(key));return item==found->second.end()?L"":item->second;
    }
    int integer(const std::wstring &section,const std::wstring &key,int fallback) const {
        auto text=value(section,key);if(text.empty())return fallback;
        // Normal settings are signed decimal values. Let Windows handle unusual
        // legacy spellings (hex, overflow, malformed numbers) exactly as before.
        size_t at=text[0]==L'-'||text[0]==L'+'?1:0;
        if(text.size()-at>9||at==text.size()||text.find_first_not_of(L"0123456789",at)!=text.npos)
            return static_cast<int>(GetPrivateProfileIntW(section.c_str(),key.c_str(),fallback,path.c_str()));
        return static_cast<int>(wcstol(text.c_str(),nullptr,10));
    }
};
static std::wstring readText(const ProfileReader &file,const std::wstring &section,const std::wstring &key){
    return decode(file.value(section,key));
}
static int number(const ProfileReader &file, const std::wstring &section, const std::wstring &key,
                  int fallback) {
    return file.integer(section,key,fallback);
}
static std::wstring readStructureText(const ProfileReader &file,const std::wstring &section){
    int parts=number(file,section,L"structureParts",0);
    if(!parts)return readText(file,section,L"structure");
    if(parts<0||parts>40)return {};
    std::wstring result;
    for(int i=0;i<parts;++i){auto part=readText(file,section+L".Structure"+std::to_wstring(i),L"value");
        if(part.empty()||part.size()>3000)return {};
        result+=part;
    }
    return result;
}
Config load(const std::filesystem::path &configFile, bool allowPresetPackage) {
    if (!std::filesystem::exists(configFile))
        return defaults();
    ProfileReader file(configFile);
    Config c;
    int schema = number(file, L"Jump", L"schema", 0);
    if (schema != 1)
        return defaults();
    c.presetPackage = allowPresetPackage && number(file, L"Jump", L"presetPackage", 0) == 1;
    c.automatic = number(file, L"Jump", L"automatic", 1) != 0;
    c.quickSave = number(file, L"Jump", L"quickSave", 1) != 0;
    c.layout = std::clamp(number(file, L"Jump", L"layout", 0), 0, 2);
    auto expression = readText(file, L"NamingVariables", L"expression");
    if (!expression.empty() && expression.size() <= 160) c.variableTemplate = expression;
    auto variables = readText(file, L"NamingVariables", L"values");
    if (variables.size() <= 1200) c.variableValues = variables;
    c.managerPositionSaved = number(file, L"ManagerWindow", L"rememberPosition", 0) == 1;
    c.managerX = std::clamp(number(file, L"ManagerWindow", L"x", 0), -100000, 100000);
    c.managerY = std::clamp(number(file, L"ManagerWindow", L"y", 0), -100000, 100000);
    c.dateFormat = std::clamp(number(file, L"NamingRules", L"dateFormat", 0), 0, 2);
    c.numberDigits = std::clamp(number(file, L"NamingRules", L"numberDigits", 3), 2, 4);
    c.upperVersion = number(file, L"NamingRules", L"upperVersion", 0) == 1;
    std::vector<Recipe> migratedGroups;
    int count = std::clamp(number(file, L"Jump", L"groups", 0), 0, 100);
    for (int i = 0; i < count; ++i) {
        auto section = L"Group" + std::to_wstring(i);
        struct LegacyGroup {std::wstring id,name;std::vector<Preset> presets;} g;
        g.id = readText(file, section, L"id");
        g.name = readText(file, section, L"name");
        int total = std::clamp(number(file, section, L"presets", 0), 0, 500);
        for (int j = 0; j < total; ++j) {
            auto s = section + L".Name" + std::to_wstring(j);
            Preset p{readText(file, s, L"name"), readText(file, s, L"value"),
                     readText(file, s, L"keywords"), number(file, s, L"pinned", 0) != 0};
            p.color = std::clamp(number(file, s, L"color", 0), 0, PRESET_COLOR_COUNT);
            if (validName(p.value)) {
                if(p.name.empty())p.name=p.value;
                if(p.name.size()>80){p.keywords+=L" "+p.name;p.name.resize(80);}
                if(std::none_of(g.presets.begin(),g.presets.end(),[&](const auto &v){return v.value==p.value;}))
                    g.presets.push_back(p);
            }
        }
        if (!g.id.empty() && !g.name.empty()) {
            for(size_t offset=0;offset<g.presets.size();offset+=50){
                StructuredNaming scheme;scheme.enabled=true;scheme.counter=0;
                scheme.id=L"migrated-"+std::to_wstring(i)+L"-"+std::to_wstring(offset/50);
                scheme.name=g.name.substr(0,70)+(offset?L" "+std::to_wstring(offset/50+1):L"");
                scheme.digits=c.numberDigits;scheme.dateFormat=c.dateFormat;scheme.upper=c.upperVersion;
                scheme.fields={{L"subject",L"主要名称",L"",true}};
                scheme.fields[0].defaultSource=0;
                scheme.fields[0].candidates.assign(g.presets.begin()+offset,g.presets.begin()+std::min(offset+50,g.presets.size()));
                Recipe migrated;migrated.name=scheme.name;migrated.value=L"Asset";migrated.complete=migrated.namingOnly=true;
                migrated.structure=serializeStructure(scheme);migrated.counter=scheme.counter;migrated.digits=scheme.digits;
                migrated.dateFormat=scheme.dateFormat;migrated.upperVersion=scheme.upper;migratedGroups.push_back(std::move(migrated));
            }
        }
    }
    auto locations = [&](const std::wstring &kind, std::vector<Location> &dst) {
        int n = std::clamp(number(file, L"Jump", kind, 0), 0, 100);
        for (int i = 0; i < n; ++i) {
            auto s = kind + std::to_wstring(i);
            Location l{readText(file, s, L"name"), readText(file, s, L"path"),
                       kind == L"Favorite" ? L"收藏" : kind == L"RecentSave" ? L"最近保存" : L"历史"};
            if (!l.path.empty())
                dst.push_back(l);
        }
    };
    locations(L"Favorite", c.favorites);
    locations(L"History", c.history);
    locations(L"RecentSave", c.recentSaves);
    int locationMarks = std::clamp(number(file, L"Jump", L"LocationColors", 0), 0, 1000);
    for (int i = 0; i < locationMarks; ++i) {
        auto s = L"LocationColor" + std::to_wstring(i);
        auto path = normalize(readText(file, s, L"path"));
        int color = number(file, s, L"color", 0);
        if (!path.empty() && color > 0 && color <= PRESET_COLOR_COUNT) c.locationColors[path] = color;
    }
    auto presetValues = [&](const std::wstring &section, int count, auto valid,
                            std::vector<std::wstring> &values, std::map<std::wstring, int> &colors) {
        for (int i = 0; i < count; ++i) {
            auto key = std::to_wstring(i);
            auto value = readText(file, section, key);
            if (!valid(value) || std::find(values.begin(), values.end(), value) != values.end()) continue;
            values.push_back(value);
            int color = std::clamp(number(file, section + L"Color", key, 0), 0, PRESET_COLOR_COUNT);
            if (color) colors[value] = color;
        }
    };
    int m = std::clamp(number(file, L"Jump", L"modifiers", -1), -1, 50);
    c.activeNamingScheme = readText(file,L"Jump",L"activeNamingScheme");
    c.defaultNamingScheme=readText(file,L"Jump",L"defaultNamingScheme");
    // Preserve the former startup choice once when upgrading an old configuration.
    if(c.defaultNamingScheme.empty())c.defaultNamingScheme=c.activeNamingScheme.empty()?L"asset-v2":c.activeNamingScheme;
    for(int i=0,n=std::clamp(number(file,L"Jump",L"applicationNamingSchemes",0),0,100);i<n;++i){
        auto section=L"ApplicationNamingScheme"+std::to_wstring(i);
        auto application=namingApplicationKey(readText(file,section,L"executable"));
        auto scheme=readText(file,section,L"scheme");
        if(!application.empty()&&!scheme.empty()&&scheme.size()<=80)c.applicationNamingSchemes[application]=std::move(scheme);
    }
    int candidateRenames = std::clamp(number(file,L"Jump",L"candidateRenames",0),0,512);
    for(int i=0;i<candidateRenames;++i){
        auto section=L"CandidateRename"+std::to_wstring(i);
        CandidateRename rename{readText(file,section,L"schema"),readText(file,section,L"field"),
            readText(file,section,L"from"),readText(file,section,L"to")};
        if(!rename.schema.empty()&&rename.schema.size()<=80&&!rename.field.empty()&&rename.field.size()<=40&&
            validName(rename.from)&&validName(rename.to)&&rename.to.size()<=80)c.candidateRenames.push_back(std::move(rename));
    }
    presetValues(L"Modifier", m, validName, c.modifiers, c.modifierColors);
    auto affixes = [&](const std::wstring &key, const std::wstring &section, bool prefix,
                       std::vector<std::wstring> &values, const std::vector<std::wstring> &fallback,
                       std::map<std::wstring, int> &colors) {
        int count = number(file, L"Jump", key, -1);
        if (count < 0) { values = fallback; return; }
        presetValues(section, std::min(count, 100),
                     [prefix](const auto &value) { return validAffix(value, prefix); }, values, colors);
    };
    auto fallback = defaults();
    affixes(L"prefixes", L"Prefix", true, c.prefixes, fallback.prefixes, c.prefixColors);
    affixes(L"suffixes", L"Suffix", false, c.suffixes, fallback.suffixes, c.suffixColors);
    // Up to 1,000 migrated chunks plus 100 existing recipes must survive reload.
    int r = std::clamp(number(file, L"Jump", L"recipes", 0), 0, 1100);
    for (int i = 0; i < r; ++i) {
        auto s = L"Recipe" + std::to_wstring(i);
        Recipe p{readText(file, s, L"name"), readText(file, s, L"directory"),
                 readText(file, s, L"value"), number(file, s, L"stage", 0) != 0,
                 number(file, s, L"date", 0) != 0};
        p.complete = number(file, s, L"complete", 0) != 0;
        p.relativeDirectory = number(file, s, L"relativeDirectory", 0) != 0;
        p.folder = number(file, s, L"folder", 0) != 0;
        p.variables = number(file, s, L"variables", 0) != 0;
        p.counter = std::clamp(number(file, s, L"counter", 0), 0, 2);
        p.digits = std::clamp(number(file, s, L"digits", 3), 2, 4);
        p.dateFormat = std::clamp(number(file, s, L"dateFormat", 0), 0, 2);
        p.upperVersion = number(file, s, L"upperVersion", 0) != 0;
        p.ruleEnabled = number(file, s, L"ruleEnabled", 0) != 0;
        p.ruleRegex = number(file, s, L"ruleRegex", 0) != 0;
        p.prefix = readText(file, s, L"prefix"); p.suffix = readText(file, s, L"suffix");
        p.stageValue = readText(file, s, L"stageValue"); p.expression = readText(file, s, L"expression");
        p.variableValues = readText(file, s, L"variableValues");
        p.pattern = readText(file, s, L"pattern"); p.replacement = readText(file, s, L"replacement");
        p.structure = readStructureText(file, s);
        int namingOnly = number(file,s,L"namingOnly",-1);
        if(namingOnly<0){
            // Older writers put this flag after the final structure chunk.
            int parts=number(file,s,L"structureParts",0);
            namingOnly=parts>0&&parts<=40?
                number(file,s+L".Structure"+std::to_wstring(parts-1),L"namingOnly",0):0;
        }
        p.namingOnly = namingOnly!=0;
        if (!p.name.empty() && validName(p.value)) c.recipes.push_back(p);
    }
    // Identity collision checks are only needed when there is legacy data to
    // migrate. Normal startup must not deserialize every scheme a second time.
    if(!migratedGroups.empty()){
    std::set<std::wstring> recipeNames,schemeIds;
    for(const auto &recipe:c.recipes){recipeNames.insert(lower(recipe.name));
        auto scheme=deserializeStructure(recipe.structure);if(scheme.enabled)schemeIds.insert(scheme.id);}
    for(auto &recipe:migratedGroups){
        auto scheme=deserializeStructure(recipe.structure);auto base=recipe.name;int suffix=2;
        while(recipeNames.contains(lower(recipe.name)))
            recipe.name=base+L" "+std::to_wstring(suffix++);
        auto identity=scheme.id;suffix=2;
        while(schemeIds.contains(scheme.id))
            scheme.id=identity+L"-"+std::to_wstring(suffix++);
        recipeNames.insert(lower(recipe.name));schemeIds.insert(scheme.id);
        scheme.name=recipe.name;recipe.structure=serializeStructure(scheme);c.recipes.push_back(std::move(recipe));
    }
    }
    c.activeProject = readText(file, L"Jump", L"activeProject");
    for (int i = 0, n = std::clamp(number(file, L"Jump", L"projects", 0), 0, 100); i < n; ++i) {
        auto s = L"Project" + std::to_wstring(i);
        Project p{readText(file, s, L"id"), readText(file, s, L"name"), readText(file, s, L"root"),
                  readText(file, s, L"variables"), readText(file, s, L"work"), readText(file, s, L"delivery")};
        if (!p.id.empty() && !p.name.empty() && !projectPath(p, p.work).empty() && !projectPath(p, p.delivery).empty() &&
            std::none_of(c.projects.begin(), c.projects.end(), [&](const auto &v) { return v.id == p.id; })) c.projects.push_back(p);
    }
    if (!activeProject(c)) c.activeProject.clear();
    // Keep the stored newest-first order while applying the same limits as live input.
    int subjects=std::clamp(number(file,L"Jump",L"recentSubjects",0),0,100);
    for(int i=subjects-1;i>=0;--i){
        auto section=L"RecentSubject"+std::to_wstring(i);
        rememberSubject(c,readText(file,section,L"projectId"),readText(file,section,L"value"));
    }
    for(int i=std::clamp(number(file,L"Jump",L"removedSubjects",0),0,512)-1;i>=0;--i){
        auto section=L"RemovedSubject"+std::to_wstring(i);
        removeRecentSubject(c,readText(file,section,L"projectId"),readText(file,section,L"value"));
    }
    for (int i = 0, n = std::clamp(number(file, L"Jump", L"savedFiles", 0), 0, 50); i < n; ++i) {
        auto s = L"SavedFile" + std::to_wstring(i);
        SavedFile f{readText(file, s, L"path"), readText(file, s, L"process"), readText(file, s, L"verifiedAt")};
        f.structure = readStructureText(file, s); f.projectId = readText(file, s, L"projectId");
        if (std::filesystem::path(f.path).is_absolute()) c.savedFiles.push_back(f);
    }
    int bindings = std::clamp(number(file, L"Jump", L"filenameBindings", 0), 0, 100);
    for (int i = 0; i < bindings; ++i) {
        auto s = L"FilenameBinding" + std::to_wstring(i);
        FilenameBinding binding{readText(file, s, L"executable"), readText(file, s, L"windowClass"),
            readText(file, s, L"windowTitle"), readText(file, s, L"selector")};
        if (!binding.executable.empty() && !binding.windowClass.empty() &&
            !binding.windowTitle.empty() && !binding.selector.empty() && binding.selector.size() <= 8000)
            c.filenameBindings.push_back(std::move(binding));
    }
    if (m < 0)
        c.modifiers = fallback.modifiers;
    return c;
}
bool save(const Config &c, const std::filesystem::path &file) {
    // Export can run on the UI thread while the automatic writer is active.
    static std::mutex saveMutex;
    std::lock_guard saveLock(saveMutex);
    std::wostringstream out;
    auto section = [&](const std::wstring &s) { out << L"\r\n[" << s << L"]\r\n"; };
    auto txt = [&](const std::wstring &k, const std::wstring &v) {
        out << k << L"=" << encode(v) << L"\r\n";
    };
    auto num = [&](const std::wstring &k, size_t v) { out << k << L"=" << v << L"\r\n"; };
    // The Windows profile API also limits entire UTF-16 sections. Store large
    // structures in separate bounded sections, not just a larger read buffer.
    auto structureText=[&](const std::wstring &owner,const std::wstring &value){
        if(value.size()<=1000){txt(L"structure",value);return;}
        txt(L"structure",L"");size_t count=(value.size()+2999)/3000;num(L"structureParts",count);
        for(size_t i=0;i<count;++i){section(owner+L".Structure"+std::to_wstring(i));txt(L"value",value.substr(i*3000,3000));}
    };
    section(L"Jump");
    num(L"schema", 1);
    num(L"candidateRenames",c.candidateRenames.size());
    num(L"presetPackage", c.presetPackage);
    num(L"automatic", c.automatic);
    num(L"quickSave", c.quickSave);
    num(L"layout", c.layout);
    num(L"Favorite", c.favorites.size());
    num(L"History", c.history.size());
    num(L"RecentSave", c.recentSaves.size());
    num(L"LocationColors", c.locationColors.size());
    num(L"modifiers", c.modifiers.size());
    num(L"prefixes", c.prefixes.size());
    num(L"suffixes", c.suffixes.size());
    num(L"recipes", c.recipes.size());
    txt(L"activeNamingScheme", c.activeNamingScheme);
    txt(L"defaultNamingScheme",c.defaultNamingScheme);
    num(L"applicationNamingSchemes",c.applicationNamingSchemes.size());
    num(L"filenameBindings", c.filenameBindings.size());
    num(L"projects", c.projects.size()); txt(L"activeProject", c.activeProject);
    num(L"savedFiles", c.savedFiles.size()); num(L"features", 2);
    num(L"recentSubjects",c.recentSubjects.size());
    num(L"removedSubjects",c.removedSubjects.size());
    for(size_t i=0;i<c.recentSubjects.size();++i){
        section(L"RecentSubject"+std::to_wstring(i));
        txt(L"projectId",c.recentSubjects[i].projectId);txt(L"value",c.recentSubjects[i].value);
    }
    for(size_t i=0;i<c.removedSubjects.size();++i){
        section(L"RemovedSubject"+std::to_wstring(i));
        txt(L"projectId",c.removedSubjects[i].projectId);txt(L"value",c.removedSubjects[i].value);
    }
    for(size_t i=0;i<c.candidateRenames.size();++i){
        section(L"CandidateRename"+std::to_wstring(i));const auto &rename=c.candidateRenames[i];
        txt(L"schema",rename.schema);txt(L"field",rename.field);txt(L"from",rename.from);txt(L"to",rename.to);
    }
    size_t applicationIndex=0;
    for(const auto &[application,scheme]:c.applicationNamingSchemes){
        section(L"ApplicationNamingScheme"+std::to_wstring(applicationIndex++));
        txt(L"executable",application);txt(L"scheme",scheme);
    }
    for (size_t i = 0; i < c.projects.size(); ++i) {
        section(L"Project" + std::to_wstring(i)); const auto &p = c.projects[i];
        txt(L"id", p.id); txt(L"name", p.name); txt(L"root", p.root); txt(L"variables", p.variables);
        txt(L"work", p.work); txt(L"delivery", p.delivery);
    }
    for (size_t i = 0; i < c.savedFiles.size(); ++i) {
        section(L"SavedFile" + std::to_wstring(i)); const auto &f = c.savedFiles[i];
        txt(L"path", f.path); txt(L"process", f.process); txt(L"verifiedAt", f.verifiedAt);
        txt(L"projectId", f.projectId); structureText(L"SavedFile"+std::to_wstring(i),f.structure);
    }
    section(L"NamingVariables");
    txt(L"expression", c.variableTemplate);
    txt(L"values", c.variableValues);
    section(L"ManagerWindow");
    num(L"rememberPosition", c.managerPositionSaved);
    out << L"x=" << c.managerX << L"\r\ny=" << c.managerY << L"\r\n";
    section(L"NamingRules");
    num(L"dateFormat", c.dateFormat); num(L"numberDigits", c.numberDigits); num(L"upperVersion", c.upperVersion);
    auto locations = [&](const std::wstring &s, const std::vector<Location> &v) {
        for (size_t i = 0; i < v.size(); ++i) {
            section(s + std::to_wstring(i));
            txt(L"name", v[i].name);
            txt(L"path", v[i].path);
        }
    };
    locations(L"Favorite", c.favorites);
    locations(L"History", c.history);
    locations(L"RecentSave", c.recentSaves);
    size_t markIndex = 0;
    for (const auto &[path, color] : c.locationColors) {
        section(L"LocationColor" + std::to_wstring(markIndex++));
        txt(L"path", normalize(path));
        num(L"color", std::clamp(color, 0, PRESET_COLOR_COUNT));
    }
    auto values = [&](const std::wstring &s, const std::vector<std::wstring> &items) {
        section(s);
        for (size_t i = 0; i < items.size(); ++i) txt(std::to_wstring(i), items[i]);
    };
    auto colors = [&](const std::wstring &s, const std::vector<std::wstring> &values,
                      const std::map<std::wstring, int> &marks) {
        section(s);
        for (size_t i = 0; i < values.size(); ++i) {
            auto found = marks.find(values[i]);
            if (found != marks.end()) num(std::to_wstring(i), std::clamp(found->second, 0, PRESET_COLOR_COUNT));
        }
    };
    values(L"Modifier", c.modifiers);
    colors(L"ModifierColor", c.modifiers, c.modifierColors);
    values(L"Prefix", c.prefixes);
    values(L"Suffix", c.suffixes);
    colors(L"PrefixColor", c.prefixes, c.prefixColors);
    colors(L"SuffixColor", c.suffixes, c.suffixColors);
    for (size_t i = 0; i < c.recipes.size(); ++i) {
        section(L"Recipe" + std::to_wstring(i));
        const auto &p = c.recipes[i];
        txt(L"name", p.name);
        txt(L"directory", p.directory);
        txt(L"value", p.value);
        num(L"stage", p.stage);
        num(L"date", p.date); num(L"complete", p.complete); num(L"relativeDirectory", p.relativeDirectory);
        num(L"folder", p.folder); num(L"variables", p.variables); num(L"counter", p.counter);
        num(L"digits", p.digits); num(L"dateFormat", p.dateFormat); num(L"upperVersion", p.upperVersion);
        num(L"ruleEnabled", p.ruleEnabled); num(L"ruleRegex", p.ruleRegex);
        txt(L"prefix", p.prefix); txt(L"suffix", p.suffix); txt(L"stageValue", p.stageValue);
        txt(L"expression", p.expression); txt(L"variableValues", p.variableValues);
        txt(L"pattern", p.pattern); txt(L"replacement", p.replacement);
        num(L"namingOnly", p.namingOnly);
        structureText(L"Recipe"+std::to_wstring(i),p.structure);
    }
    for (size_t index = 0; index < c.filenameBindings.size(); ++index) {
        section(L"FilenameBinding" + std::to_wstring(index));
        const auto &binding = c.filenameBindings[index];
        txt(L"executable", binding.executable); txt(L"windowClass", binding.windowClass);
        txt(L"windowTitle", binding.windowTitle); txt(L"selector", binding.selector);
    }
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error)
        return false;
    auto temp = file;
    temp += L".tmp";
    HANDLE f =
        CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    const auto text = std::wstring(1, static_cast<wchar_t>(0xFEFF)) + out.str();
    DWORD size = static_cast<DWORD>(text.size() * sizeof(wchar_t)), written = 0;
    bool ok = WriteFile(f, text.data(), size, &written, nullptr) && written == size;
    ok = FlushFileBuffers(f) && ok;
    CloseHandle(f);
    if (!ok) {
        DeleteFileW(temp.c_str());
        return false;
    }
    // One recovery point per configuration per process, before the first replacement.
    // Explicit import/restore also creates a recovery point.
    static std::set<std::wstring> backedUp;
    auto identity = lower(std::filesystem::absolute(file).wstring());
    if (!backedUp.contains(identity)) {
        if (!backupConfig(file)) { DeleteFileW(temp.c_str()); return false; }
        backedUp.insert(identity);
    }
    bool replaced=false;
    for(int attempt=0;attempt<5;++attempt){
        if(MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){replaced=true;break;}
        auto code=GetLastError();
        if(code!=ERROR_SHARING_VIOLATION&&code!=ERROR_ACCESS_DENIED)break;
        if(attempt<4)Sleep(15*(attempt+1));
    }
    if (!replaced) {
        DeleteFileW(temp.c_str());
        return false;
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, file.c_str());
    return true;
}
static void rememberRecentLocation(std::vector<Location> &locations, const Location &l) {
    if (l.path.empty())
        return;
    const auto path = normalize(l.path);
    std::erase_if(locations, [&](const Location &x) { return normalize(x.path) == path; });
    locations.insert(locations.begin(), l);
    if (locations.size() > 20)
        locations.resize(20);
}
void rememberLocation(Config &c, const Location &l) {
    rememberRecentLocation(c.history, l);
}
void rememberSavedLocation(Config &c, const Location &l) {
    rememberRecentLocation(c.recentSaves, l);
}
bool rememberSubject(Config &c,const std::wstring &projectId,std::wstring value){
    value=trim(std::move(value));
    if(value.size()>120||!validName(value))return false;
    const auto same=[&](const RecentSubject &s){return s.projectId==projectId&&s.value==value;};
    std::erase_if(c.removedSubjects,same);
    if(!c.recentSubjects.empty()&&same(c.recentSubjects.front()))return false;
    std::erase_if(c.recentSubjects,same);
    c.recentSubjects.insert(c.recentSubjects.begin(),{projectId,std::move(value)});
    size_t count=0;
    std::erase_if(c.recentSubjects,[&](const auto &s){return s.projectId==projectId&&++count>8;});
    if(c.recentSubjects.size()>100)c.recentSubjects.resize(100);
    return true;
}
bool removeRecentSubject(Config &c,const std::wstring &projectId,const std::wstring &value){
    if(value.size()>120||!validName(value))return false;
    const auto same=[&](const RecentSubject &s){return s.projectId==projectId&&s.value==value;};
    bool changed=std::erase_if(c.recentSubjects,same)!=0;
    if(std::none_of(c.removedSubjects.begin(),c.removedSubjects.end(),same)){
        c.removedSubjects.insert(c.removedSubjects.begin(),{projectId,value});changed=true;
        if(c.removedSubjects.size()>512)c.removedSubjects.resize(512);
    }
    return changed;
}
std::vector<Location> rankLocations(const Config &c, const std::wstring &source,
                                    const std::vector<Location> &opened) {
    std::vector<Location> all;
    if (!source.empty())
        all.push_back({std::filesystem::path(source).filename().wstring(), source, L"当前关联"});
    auto append = [&](const std::vector<Location> &locations, const wchar_t *sourceLabel) {
        for (auto location : locations) {
            location.sources = sourceLabel;
            all.push_back(std::move(location));
        }
    };
    append(c.favorites, L"收藏");
    std::vector<Location> projectLocations;
    if (const auto *project = activeProject(c)) {
        projectLocations = {{project->name + L" · 工作", projectPath(*project, project->work), L"项目"},
                            {project->name + L" · 交付", projectPath(*project, project->delivery), L"项目"}};
        append(projectLocations, L"项目");
    }
    append(c.recentSaves, L"最近保存");
    append(c.history, L"历史");
    append(opened, L"已打开");
    std::map<std::wstring, Location> merged;
    for (const auto &x : all) {
        auto key = normalize(x.path);
        if (merged.contains(key)) {
            if (merged[key].sources.find(x.sources) == std::wstring::npos)
                merged[key].sources += L" / " + x.sources;
        } else
            merged[key] = x;
    }
    std::vector<Location> result;
    std::set<std::wstring> seen;
    auto take = [&](const std::vector<Location> &v, size_t limit, bool includeAll = false) {
        size_t n = 0;
        for (auto x : v) {
            auto key = normalize(x.path);
            if (seen.contains(key))
                continue;
            if (n >= limit || (!includeAll && result.size() >= 6))
                break;
            seen.insert(key);
            result.push_back(merged[key]);
            ++n;
        }
    };
    if (!source.empty())
        take({all.front()}, 1);
    take(projectLocations, 2);
    take(c.recentSaves, 1);
    take(c.favorites, 2);
    take(c.history, 2);
    take(opened, 1);
    take(all, 6);
    // Keep every Explorer folder reachable now that Recommended is its only tab.
    take(opened, opened.size(), true);
    std::map<std::wstring, int> counts;
    for (auto &x : result)
        ++counts[x.name];
    for (auto &x : result)
        if (counts[x.name] > 1) {
            auto p = std::filesystem::path(x.path);
            x.name = p.parent_path().filename().wstring() + L" / " + p.filename().wstring();
        }
    return result;
}


bool selfTest(const std::filesystem::path &directory) {
    auto require = [](bool v) {
        if (!v)
            throw std::runtime_error("model check failed");
    };
    try {
        require(validName(L"角色设定"));
        require(!validName(L"bad/name"));
        require(!validName(L"CON.txt"));
        require(!validName(L"LPT1"));
        require(!validName(L"COM¹.txt") && !validName(L"lpt²") && !validName(L"COM³.log"));
        require(!validName(L"CON .txt") && !validName(L"CONIN$") && !validName(L"CONOUT$"));
        require(validName(L"COM10") && validName(L"COM0") && validName(L"COM¹_asset"));
        require(validName(L"report.v1"));
        auto c = defaults();
        require(c.favorites.empty());
        require(c.prefixes.size() == 6 && c.suffixes.size() == 6);
        require(expandAffix(L"{date}_") == today() + L"_");
        auto smartDirectory = std::filesystem::absolute(directory / L"SmartNaming");
        std::filesystem::create_directories(smartDirectory);
        for (const auto *file : {L"角色_v001.blend", L"角色_v003.blend", L"角色_v099.txt", L"角色_001.blend", L"角色_009.blend"})
            std::ofstream(smartDirectory / file).put('x');
        auto smart = [&](const std::wstring &source, SmartNaming options) {
            return smartNamingPreview(source, smartDirectory.wstring(), L".blend", options);
        };
        require(smart(L"角色", {1}).name == L"角色_v004");
        std::filesystem::create_directories(smartDirectory / L"角色_v900.blend");
        require(smart(L"角色", {1}).name == L"角色_v004");
        require(smart(L"角色_v009", {1}).name == L"角色_v010");
        require(smart(L"角色", {2}).name == L"角色_010");
        require(smart(L"角色_009", {2}).name == L"角色_010");
        require(smart(L"角色_20261016", {2}).name == L"角色_20261016_001");
        require(smart(L"角色_20261016_009", {2}).name == L"角色_20261016_010");
        require(smart(L"角色_20240229", {2}).name == L"角色_20240229_001");
        require(smart(L"角色_20261016", {1}).name == L"角色_20261016_v001");
        require(smart(L"角色_2026_10_16", {2}).name == L"角色_2026_10_16_001");
        require(smart(L"角色_2026-10-16", {2}).name == L"角色_2026-10-16_001");
        auto formatted = splitName(L"角色_初稿_2026_10_16", c.modifiers);
        require(formatted.base == L"角色" && formatted.stage == L"初稿" && formatted.date == L"2026_10_16");
        SmartNaming numbering{1}; numbering.digits = 4; numbering.upperVersion = true;
        require(smart(L"新角色", numbering).name == L"新角色_V0001");
        SmartNaming dated{2, false, L"{name}_{date}", L""}; dated.dateFormat = 2; dated.digits = 4;
        require(smart(L"角色", dated).name == L"角色_" + today(2) + L"_0001");
        require(smart(L"角色", {2, false, L"{name}_{date}", L""}).name == L"角色_" + today() + L"_001");
        std::ofstream(smartDirectory / L"日期查重_20261016_003.blend").put('x');
        require(smart(L"日期查重_20261016", {2}).name == L"日期查重_20261016_004");
        require(smart(L"角色", {0, true}).name == L"SmartNaming_角色");
        require(smartNamingPreview(L"角色", smartDirectory.wstring() + L"\\", L".blend", {0, true}).name == L"SmartNaming_角色");
        require(smart(L"角色", {1, true}).name == L"SmartNaming_角色_v001");
        c.variableValues = L"项目=海港\r\n作者=李";
        require(smart(L"角色", {0, false, c.variableTemplate, c.variableValues}).name == L"海港_角色_李");
        require(smart(L"角色", {0, false, L"{folder}_{name}_{date}", L""}).name == L"SmartNaming_角色_" + today());
        require(!smart(L"角色", {0, false, L"{未知}_{name}", c.variableValues}).error.empty());
        require(!smart(L"角色", {0, false, L"{name", L""}).error.empty());
        require(!smart(L"角色", {0, false, L"{项目}_{name}", L"项目=bad/path"}).error.empty());
        require(!smart(L"角色", {0, false, L"{项目}_{name}", L"项目=A\n项目=B"}).error.empty());
        require(validAffix(L"项目_", true) && validAffix(L"_定稿", false));
        require(!validAffix(L"bad/", true) && !validAffix(L".", false));
        auto n = splitName(L"角色_初稿_初稿_20261014", c.modifiers);
        require(n.base == L"角色" && n.stage == L"初稿" && n.date == L"20261014");
        n.stage = L"定稿";
        require(compose(n) == L"角色_定稿_20261014");
        n.prefix = L"项目_"; n.suffix = L"_交付";
        require(composeBody(n) == L"角色_定稿_20261014" && compose(n) == L"项目_角色_定稿_20261014_交付");
        require(stripExtension(L"report.v1.png", L".png") == L"report.v1");
        require(stripExtension(L"report.v1", L".png") == L"report.v1");
        auto renamed = renamePreview(L"角色 001", L"项目_", L"_定稿", renameRules()[2].pattern, L"_", true);
        require(renamed.error.empty() && renamed.name == L"项目_角色_001_定稿" && renamed.matches.size() == 1);
        require(renamePreview(L"角色(副本)（旧）[2]【备份】", L"", L"", renameRules()[4].pattern, L"", true).name == L"角色");
        require(renamePreview(L"角色_v02.1", L"", L"", renameRules()[6].pattern, L"", true).name == L"角色");
        require(renamePreview(L"角色_012", L"", L"", LR"((.+)_([0-9]+))", L"$2_$1", true).name == L"012_角色");
        require(renamePreview(L"角色.角色", L"", L"", L".", L"$1", false).name == L"角色$1角色");
        require(!renamePreview(L"001", L"", L"", LR"(\d+)", L"", true).error.empty());
        require(!renamePreview(L"角色", L"", L"", L"[", L"", true).error.empty());
        require(!renamePreview(L"角色", L"bad/", L"", L"", L"", false).error.empty());
        require(!renamePreview(std::wstring(80, L'a') + L"!", L"", L"", L"^(a+)+$", L"", true).error.empty());
        c.favorites = {{L"角色", L"D:\\Project", L"收藏"}};
        rememberLocation(c, {L"角色", L"D:\\Project", L"历史"});
        auto ranked = rankLocations(c, L"D:\\Project", {{L"打开", L"D:\\Open", L"资源管理器"}});
        require(ranked.size() == 2 && ranked[0].sources.find(L"收藏") != std::wstring::npos);
        {
            Config crowded;
            for (int i = 0; i < 8; ++i) {
                auto folder = L"D:\\Favorites\\" + std::to_wstring(i);
                crowded.favorites.push_back({L"收藏 " + std::to_wstring(i), folder});
                crowded.history.push_back({L"历史 " + std::to_wstring(i), L"D:\\History\\" + std::to_wstring(i)});
            }
            crowded.recentSaves = {{L"最近保存", L"D:\\Recent"}};
            std::vector<Location> opened;
            for (int i = 0; i < 12; ++i)
                opened.push_back({L"目录", L"D:\\Open\\" + std::to_wstring(i)});
            opened.push_back({L"重复目录", L"d:\\favorites\\0\\"});
            opened.push_back(opened.front());
            const auto recommendations = rankLocations(crowded, L"D:\\Source", opened);
            std::set<std::wstring> unique;
            for (const auto &location : recommendations) require(unique.insert(normalize(location.path)).second);
            for (const auto &location : opened) {
                auto match = std::find_if(recommendations.begin(), recommendations.end(), [&](const auto &entry) {
                    return normalize(entry.path) == normalize(location.path);
                });
                require(match != recommendations.end() && match->sources.find(L"已打开") != std::wstring::npos);
            }
            auto favorite = std::find_if(recommendations.begin(), recommendations.end(), [](const auto &entry) {
                return normalize(entry.path) == L"d:\\favorites\\0";
            });
            require(favorite != recommendations.end() && favorite->name == L"收藏 0" &&
                favorite->sources.find(L"收藏") != std::wstring::npos);
            const auto closed = rankLocations(crowded, L"D:\\Source", {});
            require(closed.size() == 6 && std::none_of(closed.begin(), closed.end(), [](const auto &entry) {
                return entry.sources.find(L"已打开") != std::wstring::npos;
            }));
        }
        auto path = directory / L"roundtrip.ini";
        c.filenameBindings.push_back({L"C:\\Apps\\Tool.exe", L"CustomSave", L"Asset output", L"50004:filename"});
        c.candidateRenames.push_back({L"asset-v2",L"stage",L"工作",L"制作中"});
        for(int i=0;i<10;++i)rememberSubject(c,L"",L"主体"+std::to_wstring(i));
        require(c.recentSubjects.size()==8&&c.recentSubjects.back().value==L"主体2");
        rememberSubject(c,L"",L"主体5");
        require(c.recentSubjects.size()==8&&c.recentSubjects.front().value==L"主体5");
        require(!rememberSubject(c,L"",L"bad/name")&&!rememberSubject(c,L"",L"CON"));
        rememberSubject(c,L"another-project",L"主体5");
        rememberSubject(c,L"",L"守卫 & 钟楼=正面");
        require(c.recentSubjects.size()==9);
        require(removeRecentSubject(c,L"",L"主体5"));
        require(std::none_of(c.recentSubjects.begin(),c.recentSubjects.end(),[](const auto &s){return s.projectId.empty()&&s.value==L"主体5";}));
        require(std::any_of(c.recentSubjects.begin(),c.recentSubjects.end(),[](const auto &s){return s.projectId==L"another-project"&&s.value==L"主体5";}));
        require(!removeRecentSubject(c,L"",L"主体5"));
        require(rememberSubject(c,L"",L"主体5")&&c.removedSubjects.empty());
        require(removeRecentSubject(c,L"",L"旧保存主体"));
        c.managerPositionSaved = true; c.managerX = -1300; c.managerY = 240;
        c.dateFormat = 2; c.numberDigits = 4; c.upperVersion = true;
        c.modifierColors[L"初稿"] = 3;
        c.prefixColors[c.prefixes.front()] = 2;
        c.suffixColors[c.suffixes.front()] = PRESET_COLOR_COUNT;
        c.locationColors[normalize(c.favorites.front().path)] = 4;
        require(save(c, path));
        auto loaded = load(path);
        require(loaded.candidateRenames==c.candidateRenames);
        require(loaded.recentSubjects==c.recentSubjects);
        require(loaded.removedSubjects==c.removedSubjects);
        require(loaded.variableTemplate == c.variableTemplate && loaded.variableValues == c.variableValues);
        require(loaded.managerPositionSaved && loaded.managerX == -1300 && loaded.managerY == 240);
        require(loaded.dateFormat == 2 && loaded.numberDigits == 4 && loaded.upperVersion && loaded.modifierColors[L"初稿"] == 3);
        require(loaded.history.size()==1);
        require(loaded.prefixes == c.prefixes && loaded.suffixes == c.suffixes);
        require(loaded.modifierColors == c.modifierColors && loaded.prefixColors == c.prefixColors &&
                loaded.suffixColors == c.suffixColors && loaded.locationColors == c.locationColors);
        require(loaded.filenameBindings.size() == 1 &&
            loaded.filenameBindings[0].selector == c.filenameBindings[0].selector &&
            loaded.filenameBindings[0].executable == c.filenameBindings[0].executable);
        for (int i = 0; i < 23; ++i) {
            rememberLocation(c, {L"浏览", L"D:\\Browsed\\" + std::to_wstring(i), L"历史"});
            rememberSavedLocation(c, {L"保存", L"D:\\Saved\\" + std::to_wstring(i), L"最近保存"});
        }
        rememberLocation(c, {L"浏览", L"d:\\browsed\\21\\", L"历史"});
        rememberSavedLocation(c, {L"保存", L"d:\\saved\\21\\", L"最近保存"});
        rememberLocation(c, {}); rememberSavedLocation(c, {});
        require(c.history.size() == 20 && normalize(c.history.front().path) == L"d:\\browsed\\21");
        require(c.recentSaves.size() == 20 && normalize(c.recentSaves.front().path) == L"d:\\saved\\21");
        require(normalize(c.history.back().path) == L"d:\\browsed\\3" &&
                normalize(c.recentSaves.back().path) == L"d:\\saved\\3");
        require(save(c, path));
        loaded = load(path);
        require(loaded.recentSaves.size() == 20 && loaded.recentSaves.front().path == c.recentSaves.front().path);
        require(loaded.history.size() == 20 && loaded.history.front().path == c.history.front().path);
        require(rankLocations(c, L"", {}).front().sources.find(L"最近保存") != std::wstring::npos);
        c.prefixes.clear(); c.suffixes.clear();
        require(save(c, path));
        loaded = load(path);
        require(loaded.prefixes.empty() && loaded.suffixes.empty());
        c.modifiers.clear(); require(save(c, path)); require(load(path).modifiers.empty());
        // Match Windows profile behavior for old hand-edited numeric settings
        // and quoted/Unicode values, while reusing a section across all keys.
        auto profileFile=directory/L"profile-reader.ini";
        std::vector<std::wstring> spellings{L"",L" ",L"abc",L"-1",L"0010",L"010",L"0x10",L"+12",L"12junk",L"\"12\"",L"'12'",L" 12 ",L"4294967295"};
        for(size_t i=0;i<spellings.size();++i)
            require(WritePrivateProfileStringW(L"Case",std::to_wstring(i).c_str(),spellings[i].c_str(),profileFile.c_str())!=FALSE);
        ProfileReader profile(profileFile);
        for(size_t i=0;i<spellings.size();++i){auto key=std::to_wstring(i);
            wchar_t raw[128]{};GetPrivateProfileStringW(L"Case",key.c_str(),L"",raw,128,profileFile.c_str());
            require(profile.value(L"case",key)==raw);
            require(profile.integer(L"CASE",key,97)==static_cast<int>(GetPrivateProfileIntW(L"Case",key.c_str(),97,profileFile.c_str())));
        }
        require(profile.integer(L"missing",L"missing",17)==17);
        require(decode(encode(L"守卫=引号\"\r\n😀"))==L"守卫=引号\"\r\n😀");
        require(decode(L"FFFF000g").empty());
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace jump
