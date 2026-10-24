#pragma once
#include "model.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <cwctype>

namespace jump {
// Length-prefixed strings preserve separators, Unicode, and user-defined labels.
inline std::wstring packNaming(const std::vector<std::wstring> &values) {
    std::wstring result;
    for (const auto &v : values) result += std::to_wstring(v.size()) + L":" + v;
    return result;
}
inline bool unpackNaming(const std::wstring &text, std::vector<std::wstring> &values) {
    if (text.size() > 120000) return false;
    size_t at = 0;
    while (at < text.size()) {
        size_t length = 0, digits = 0;
        while (at < text.size() && text[at] >= L'0' && text[at] <= L'9') {
            if (++digits > 6) return false;
            length = length * 10 + text[at++] - L'0';
        }
        if (!digits || at >= text.size() || text[at++] != L':' || length > text.size() - at || values.size() >= 2000) return false;
        values.push_back(text.substr(at, length)); at += length;
    }
    return true;
}
inline std::wstring namingSchemaError(const StructuredNaming &s);
inline void upgradeNamingFieldLabel(NamingField &field) {
    // One upgrade path for built-ins, saved copies and saved-file records.
    // Match both identity and the old default so custom labels remain intact.
    struct Rename {const wchar_t *id,*before,*after;};
    static constexpr Rename names[]{
        {L"project",L"项目",L"项目名称"},{L"subject",L"主体",L"主要名称"},
        {L"type",L"类型",L"文件类别"},{L"variant",L"变体",L"补充说明"},
        {L"stage",L"阶段",L"当前状态"}};
    for(const auto &name:names)if(field.id==name.id&&field.label==name.before){field.label=name.after;break;}
}
inline std::wstring serializeStructure(const StructuredNaming &s) {
    std::vector<std::wstring> metadata{s.primaryField,s.literal?L"1":L"0",s.literalName};
    if(!s.locations.empty()||!s.recommendLocations||!s.warnExisting){
        std::vector<std::wstring> rules;
        for(const auto &r:s.locations)rules.push_back(packNaming({r.field,r.value,r.directory}));
        metadata.push_back(packNaming(rules));
    }
    if(!s.recommendLocations||!s.warnExisting)metadata.push_back(packNaming({s.recommendLocations?L"1":L"0",s.warnExisting?L"1":L"0"}));
    std::vector<std::wstring> v{L"JNS2", s.id, s.name, s.separator, s.enabled ? L"1" : L"0",
        s.free ? L"1" : L"0", s.compact ? L"1" : L"0", std::to_wstring(s.counter),
        std::to_wstring(s.digits), std::to_wstring(s.dateFormat), s.upper ? L"1" : L"0",
        packNaming(metadata)};
    for (const auto &f : s.fields) {
        std::vector<std::wstring> meta{f.defaultValue,f.joiner,f.prefix,f.suffix,f.conditionField,f.conditionValue,
            f.quick?L"1":L"0",f.ownJoiner?L"1":L"0",std::to_wstring(f.casing),std::to_wstring(f.validation),
            std::to_wstring(f.width),std::to_wstring(f.defaultSource)};
        if(f.omitted||!f.enabled)meta.push_back(f.omitted?L"1":L"0");
        if(!f.enabled)meta.push_back(L"0");
        std::vector<std::wstring> field{f.id, f.label, f.value, f.required ? L"1" : L"0", std::to_wstring(f.source),
            packNaming(meta)};
        for (const auto &p : f.candidates) field.push_back(packNaming({p.name,p.value,p.keywords}));
        v.push_back(packNaming(field));
    }
    return packNaming(v);
}
inline StructuredNaming deserializeStructure(const std::wstring &text) {
    StructuredNaming s;
    std::vector<std::wstring> v;
    if (!unpackNaming(text,v) || v.empty()) return s;
    bool modern=v[0]==L"JNS2";
    if((!modern&&v[0]!=L"JNS1")||v.size()<(modern?13u:12u)||v.size()>(modern?20u:19u))return s;
    if (v[1].empty() || v[2].empty() || v[2].size()>80 || v[3].size()>3 ||
        v[7].size()!=1 || v[7][0]<L'0' || v[7][0]>L'2' ||
        (v[8]!=L"2" && v[8]!=L"3" && v[8]!=L"4") ||
        (v[9]!=L"0" && v[9]!=L"1" && v[9]!=L"2")) return s;
    s.id=v[1];s.name=v[2];s.separator=v[3];s.enabled=v[4]==L"1";s.free=v[5]==L"1";s.compact=v[6]==L"1";
    s.counter=v[7][0]-L'0';s.digits=v[8][0]-L'0';s.dateFormat=v[9][0]-L'0';s.upper=v[10]==L"1";
    if(modern){std::vector<std::wstring> meta;if(!unpackNaming(v[11],meta)||meta.size()<3||meta.size()>5)return {};
        s.primaryField=meta[0];s.literal=meta[1]==L"1";s.literalName=meta[2];
        if(meta.size()>=4){std::vector<std::wstring> rules;if(!unpackNaming(meta[3],rules)||rules.size()>20)return {};
            for(const auto &entry:rules){std::vector<std::wstring> rule;if(!unpackNaming(entry,rule)||rule.size()!=3)return {};
                s.locations.push_back({rule[0],rule[1],rule[2]});}}
        if(meta.size()==5){std::vector<std::wstring> flags;if(!unpackNaming(meta[4],flags)||flags.size()!=2)return {};
            for(const auto &flag:flags)if(flag!=L"0"&&flag!=L"1")return {};
            s.recommendLocations=flags[0]==L"1";s.warnExisting=flags[1]==L"1";}
    }
    std::set<std::wstring> ids;
    for(size_t i=modern?12:11;i<v.size();++i){
        std::vector<std::wstring> f;
        if(!unpackNaming(v[i],f)||f.size()<(modern?6u:5u)||f.size()>(modern?56u:55u)||f[0].empty()||f[0].size()>40||!ids.insert(f[0]).second||
           f[1].empty()||f[1].size()>40||f[2].size()>255||(!f[2].empty()&&!validName(f[2]))||
           (f[4]!=L"0"&&f[4]!=L"1"&&f[4]!=L"2"&&f[4]!=L"3"))return {};
        NamingField field{f[0],f[1],f[2],f[3]==L"1",f[4][0]-L'0'};
        if(modern){
            std::vector<std::wstring> m;if(!unpackNaming(f[5],m)||m.size()<12||m.size()>14)return {};
            auto digit=[](const std::wstring &n,int max){return n.size()==1&&n[0]>=L'0'&&n[0]<=L'0'+max;};
            if(!digit(m[8],2)||!digit(m[9],3)||!digit(m[10],8)||(m[11]!=L"-1"&&!digit(m[11],3)))return {};
            field.defaultValue=m[0];field.joiner=m[1];field.prefix=m[2];field.suffix=m[3];
            field.conditionField=m[4];field.conditionValue=m[5];field.quick=m[6]==L"1";field.ownJoiner=m[7]==L"1";
            field.casing=m[8][0]-L'0';field.validation=m[9][0]-L'0';field.width=m[10][0]-L'0';
            field.defaultSource=m[11]==L"-1"?-1:m[11][0]-L'0';
            if(m.size()>=13){if(m[12]!=L"0"&&m[12]!=L"1")return {};field.omitted=m[12]==L"1";}
            if(m.size()==14){if(m[13]!=L"0"&&m[13]!=L"1")return {};field.enabled=m[13]==L"1";}
        }else{
            field.quick=(s.id!=L"asset-v1"&&s.id!=L"asset-v2")||field.id==L"subject"||field.id==L"type"||field.id==L"stage";
        }
        for(size_t j=modern?6:5;j<f.size();++j){std::vector<std::wstring> p;if(!unpackNaming(f[j],p)||p.size()!=3||!validName(p[1]))return {};field.candidates.push_back({p[0],p[1],p[2]});}
        upgradeNamingFieldLabel(field);s.fields.push_back(std::move(field));
    }
    if(!modern&&std::none_of(s.fields.begin(),s.fields.end(),[&](const auto &f){return f.id==s.primaryField;}))
        s.primaryField=s.fields.front().id;
    if(!namingSchemaError(s).empty())return {};
    return s;
}
inline std::wstring fieldValue(const NamingField &f,const Config &cfg,int dateFormat=0){
    if(f.omitted)return {};
    if(f.source==2)return today(dateFormat);
    if(f.source==1){const auto *p=activeProject(cfg);return p?p->name:L"";}
    if(f.source==3)return f.defaultValue;
    return f.value;
}
inline bool namingFieldFixed(const NamingField &f){return f.source==3||f.defaultSource==3;}
inline int namingFieldDefaultSource(const NamingField &f){return f.defaultSource<0?f.source:f.defaultSource;}
inline bool namingFieldCanOmit(const StructuredNaming &s,size_t index){
    return index<s.fields.size()&&!namingFieldFixed(s.fields[index])&&
        (s.fields[index].id!=s.primaryField||!s.fields[index].required);
}
inline std::wstring namingFieldOutput(const NamingField &f,std::wstring value);
inline bool namingFieldVisible(const StructuredNaming &s,size_t index,const Config &cfg,unsigned visiting=0){
    if(index>=s.fields.size()||(visiting&(1u<<index)))return false;
    const auto &f=s.fields[index];if(!f.enabled)return false;if(f.conditionField.empty())return true;
    for(size_t i=0;i<s.fields.size();++i)if(s.fields[i].id==f.conditionField)
        return namingFieldVisible(s,i,cfg,visiting|(1u<<index))&&namingFieldOutput(s.fields[i],fieldValue(s.fields[i],cfg,s.dateFormat))==f.conditionValue;
    return false;
}
inline std::wstring namingFieldError(const NamingField &f,const std::wstring &value){
    if(value.empty())return f.required?L"请填写"+f.label:L"";
    if(!validName(value)||value.size()>255)return f.label+L"包含无效字符或过长";
    auto number=[](wchar_t ch){return ch>=L'0'&&ch<=L'9';};
    if(f.validation==1&&std::any_of(value.begin(),value.end(),[&](wchar_t ch){return !number(ch)&&!(ch>=L'A'&&ch<=L'Z')&&!(ch>=L'a'&&ch<=L'z')&&ch!=L'_';}))return f.label+L"仅允许英文字母、数字和下划线";
    if((f.validation==2||f.width)&&!std::all_of(value.begin(),value.end(),number))return f.label+L"仅允许数字";
    if(f.validation==3&&std::any_of(value.begin(),value.end(),[](wchar_t ch){return iswspace(ch)!=0;}))return f.label+L"不能包含空格";
    if(f.width&&value.size()>static_cast<size_t>(f.width))return f.label+L"不能超过"+std::to_wstring(f.width)+L"位";
    return {};
}
inline std::wstring namingLocationPathError(const std::wstring &text){
    if(text.empty()||text.size()>1024||text.find_first_of(L"*?\"<>|\r\n\t")!=text.npos)return L"请填写有效的文件夹路径";
    std::filesystem::path path(text);
    if(path.is_absolute()){
        for(const auto &part:path.relative_path())if(part==L"..")return L"目录中不能包含 ..";
        if(path.relative_path().wstring().find(L':')!=std::wstring::npos)return L"目录中包含无效的冒号";
    }else{
        Project check;check.root=L"C:\\Project";
        if(projectPath(check,text).empty())return L"项目内目录请填写相对路径，如 Models；不能使用 ..";
    }
    return {};
}
inline std::wstring namingSchemaError(const StructuredNaming &s){
    auto literal=[](const std::wstring &v,size_t limit){return v.size()<=limit&&v.find_first_of(L"\\/:*?\"<>|\r\n\t")==v.npos;};
    if(s.id.empty()||s.id.size()>80||trim(s.name).empty()||s.name.size()>80||s.fields.empty()||s.fields.size()>8||!literal(s.separator,3))return L"请设置方案名称、1–8 个字段和有效分隔符";
    if(s.counter<0||s.counter>2||s.digits<2||s.digits>4||s.dateFormat<0||s.dateFormat>2)return L"请检查方案的编号和日期格式";
    std::set<std::wstring> ids;
    for(const auto &f:s.fields){
        if(f.omitted&&(namingFieldFixed(f)||(f.id==s.primaryField&&f.required)))return L"必填的主要名称和固定文字不能取消";
        if(f.id.empty()||f.id.size()>40||!ids.insert(f.id).second||f.label.empty()||f.label.size()>40||f.source<0||f.source>3||f.defaultSource < -1||f.defaultSource>3||f.casing<0||f.casing>2||f.validation<0||f.validation>3||f.width<0||f.width>8||!literal(f.joiner,3)||!literal(f.prefix,40)||!literal(f.suffix,40)||f.defaultValue.size()>120||f.conditionValue.size()>120||f.candidates.size()>50)
            return L"请检查字段名称、连接符和输出设置";
        auto check=f;check.required=false;
        if(!f.enabled){check.validation=0;check.width=0;}
        auto error=namingFieldError(check,f.defaultValue);if(!error.empty())return L"默认值："+error;
        std::set<std::wstring> outputs;
        for(const auto &p:f.candidates){error=namingFieldError(check,p.value);if(p.value.empty()||!error.empty()||p.name.size()>80)return L"候选："+(error.empty()?L"显示名称过长或输出为空":error);
            if(!outputs.insert(namingFieldOutput(f,p.value)).second)return f.label+L"存在输出结果相同的候选";}
    }
    if(!s.primaryField.empty()&&!ids.contains(s.primaryField))return L"原名填入字段已不存在";
    if(s.locations.size()>20)return L"目录推荐最多 20 条规则";
    std::set<std::wstring> routes;
    for(const auto &r:s.locations){
        if(!ids.contains(r.field)||r.value.empty()||r.value.size()>120||!validName(r.value))return L"目录推荐需要有效的命名内容和匹配文字";
        auto error=namingLocationPathError(r.directory);if(!error.empty())return error;
        if(!routes.insert(packNaming({r.field,r.value,normalize(r.directory)})).second)return L"存在重复的目录推荐规则";
    }
    for(size_t i=0;i<s.fields.size();++i){
        std::set<std::wstring> chain;size_t at=i;
        while(!s.fields[at].conditionField.empty()){
            if(!chain.insert(s.fields[at].id).second)return L"出现条件不能相互循环依赖";
            auto found=std::find_if(s.fields.begin(),s.fields.end(),[&](const auto &f){return f.id==s.fields[at].conditionField;});
            if(found==s.fields.end()||s.fields[at].conditionValue.empty())return L"出现条件需要有效字段和匹配值";
            at=static_cast<size_t>(found-s.fields.begin());
        }
    }
    if(s.literal&&!validName(s.literalName))return L"手动名称无效";
    return {};
}
inline std::wstring namingFieldOutput(const NamingField &f,std::wstring value){
    if(value.empty())return {};
    if(f.width&&value.size()<static_cast<size_t>(f.width))value.insert(0,f.width-value.size(),L'0');
    if(f.casing)for(auto &ch:value)ch=static_cast<wchar_t>(f.casing==1?towupper(ch):towlower(ch));
    return f.prefix+value+f.suffix;
}
// Display order is shared by every scheme; output always follows fields order.
inline std::vector<size_t> namingDisplayOrder(const StructuredNaming &s,const Config &cfg,bool quick){
    std::vector<size_t> order;
    for(size_t i=0;i<s.fields.size();++i){const auto &f=s.fields[i];
        if(!namingFieldVisible(s,i,cfg))continue;
        if(!quick||f.quick||f.id==s.primaryField||(!f.omitted&&!namingFieldError(f,fieldValue(f,cfg,s.dateFormat)).empty()))order.push_back(i);
    }
    std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){
        auto rank=[&](size_t i){return s.fields[i].id==s.primaryField?0:s.fields[i].quick?1:2;};
        return rank(a)<rank(b);
    });
    return order;
}
inline StructuredNaming namingSession(StructuredNaming s,const std::wstring &stem){
    s.free=s.literal=false;s.literalName.clear();
    for(auto &f:s.fields){f.omitted=false;f.value=f.defaultValue;f.source=f.defaultSource=namingFieldDefaultSource(f);
        if(f.id==s.primaryField&&f.source==0)f.value=stem;}
    return s;
}
inline StructuredNaming namingDefinition(StructuredNaming s){
    s.free=s.literal=false;s.literalName.clear();
    for(auto &f:s.fields){f.omitted=false;if(f.defaultSource<0)f.defaultSource=f.source;f.source=f.defaultSource;f.value=f.defaultValue;}
    return s;
}
// Updating reusable rules keeps explicit per-file values. Fields still following
// an old default adopt its replacement; a changed source always uses the new rule.
inline StructuredNaming reviseNamingSession(const StructuredNaming &definition,const StructuredNaming &previous,
                                            const StructuredNaming &current,const std::wstring &stem){
    auto next=namingSession(definition,stem);
    if(current.literal)return next;
    for(size_t i=0;i<next.fields.size();++i){auto &field=next.fields[i];
        auto old=std::find_if(previous.fields.begin(),previous.fields.end(),[&](const auto &f){return f.id==field.id;});
        auto value=std::find_if(current.fields.begin(),current.fields.end(),[&](const auto &f){return f.id==field.id;});
        if(old==previous.fields.end()||value==current.fields.end()||field.source==3||
           field.defaultSource!=old->defaultSource||
           (field.id==definition.primaryField)!=(field.id==previous.primaryField)||
           (value->omitted&&!namingFieldCanOmit(next,i)))continue;
        if(value->omitted||field.defaultValue==old->defaultValue||value->source!=old->source||value->value!=old->defaultValue){
            field.value=value->value;field.source=value->source;field.omitted=value->omitted;
        }
    }
    return next;
}
inline NamingField *structuredSuffixField(StructuredNaming &s,const Config &cfg,const std::wstring &suffix){
    if(!s.enabled||s.free||suffix.size()<2||suffix.front()!=L'_'||s.separator!=L"_")return nullptr;
    auto value=suffix.substr(1);NamingField *match=nullptr;
    for(size_t i=0;i<s.fields.size();++i){auto &field=s.fields[i];
        if(field.id==s.primaryField||namingFieldFixed(field)||!namingFieldVisible(s,i,cfg)||
           (field.ownJoiner&&field.joiner!=L"_")||namingFieldOutput(field,value)!=value||!namingFieldError(field,value).empty())continue;
        if(value==fieldValue(field,cfg,s.dateFormat)||
            std::any_of(field.candidates.begin(),field.candidates.end(),[&](const auto &p){return p.value==value;})||
            ((field.source==2||namingFieldDefaultSource(field)==2)&&value==today(s.dateFormat))){
            if(match)return nullptr; // Ambiguous values remain ordinary suffixes.
            match=&field;
        }
    }
    return match;
}
inline RenamePreview composeStructure(const StructuredNaming &s,const Config &cfg){
    RenamePreview r;
    if(s.literal){r.name=s.literalName;if(!validName(r.name))r.error=L"手动名称无效";return r;}
    for(size_t i=0;i<s.fields.size();++i){const auto &f=s.fields[i];if(f.omitted||!namingFieldVisible(s,i,cfg))continue;
        auto value=fieldValue(f,cfg,s.dateFormat);r.error=namingFieldError(f,value);if(!r.error.empty())return r;
        if(value.empty())continue;value=namingFieldOutput(f,value);
        if(!r.name.empty())r.name+=f.ownJoiner?f.joiner:s.separator;
        r.matches.push_back({r.name.size(),value.size()});r.name+=value;
    }
    if(!validName(r.name))r.error=L"请填写主要名称，或缩短组合后的名称。";
    return r;
}
inline std::vector<Location> namingRecommendedLocations(const StructuredNaming &s,const Config &cfg){
    std::vector<Location> result;std::set<std::wstring> seen;
    if(!s.enabled||!s.recommendLocations||s.free||s.literal)return result;
    for(const auto &r:s.locations){
        auto found=std::find_if(s.fields.begin(),s.fields.end(),[&](const auto &f){return f.id==r.field;});
        if(found==s.fields.end()||found->omitted||!namingFieldVisible(s,found-s.fields.begin(),cfg))continue;
        auto value=fieldValue(*found,cfg,s.dateFormat);
        if(value.empty()||!namingFieldError(*found,value).empty()||namingFieldOutput(*found,value)!=r.value)continue;
        std::wstring path=r.directory;
        if(!std::filesystem::path(path).is_absolute()){
            auto project=activeProject(cfg);if(!project)continue;path=projectPath(*project,path);
        }
        if(path.empty()||!seen.insert(normalize(path)).second)continue;
        auto name=std::filesystem::path(path).filename().wstring();if(name.empty())name=path;
        result.push_back({name,path,L"命名推荐 · "+found->label+L" = "+value});
    }
    return result;
}
inline StructuredNaming basicStructure(const std::wstring &stem,const Config *cfg=nullptr){
    // New identity keeps the former mixed subject history out of the categorized defaults.
    StructuredNaming s;s.id=L"asset-v2";s.name=L"资产文件";s.enabled=true;
    s.fields={{L"project",L"项目名称",L"",false,1},{L"subject",L"主要名称",stem,true,0},
        {L"type",L"文件类别",L"",false,0,{{L"模型",L"模型"},{L"材质",L"材质"},{L"绑定",L"绑定"},
            {L"角色设定",L"角色设定"},{L"场景设定",L"场景设定"},{L"道具设定",L"道具设定"},
            {L"角色参考",L"角色参考"},{L"场景参考",L"场景参考"},{L"世界设定",L"世界设定"}}},
        {L"variant",L"补充说明",L"",false,0,{{L"夜巡",L"夜巡"},{L"重甲",L"重甲"}}},
        {L"stage",L"当前状态",L"",false,0,{{L"工作",L"工作"},{L"审核",L"审核"},{L"交付",L"交付"},{L"初稿",L"初稿"},{L"定稿",L"定稿"},{L"预览",L"预览"}}},
        {L"date",L"日期",L"",false,0}};
    for(auto &f:s.fields){f.quick=f.id==L"subject"||f.id==L"type"||f.id==L"stage";f.defaultSource=f.source;}
    // Legacy libraries become template data, including when this template is copied.
    if(cfg)for(const auto &value:cfg->modifiers)
        if(s.fields[4].candidates.size()<50&&std::none_of(s.fields[4].candidates.begin(),s.fields[4].candidates.end(),[&](const auto &p){return p.value==value;}))
            s.fields[4].candidates.push_back({value,value});
    // Remove only an unambiguous trailing _vNNN marker. Dates/LOD/UDIM remain intact.
    auto at=stem.find_last_of(L'_');
    if(at!=stem.npos&&at+2<stem.size()&&(stem[at+1]==L'v'||stem[at+1]==L'V')&&stem.size()-at-2<=9&&
        std::all_of(stem.begin()+at+2,stem.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))s.fields[1].value=stem.substr(0,at);
    return s;
}
inline std::vector<StructuredNaming> gameNamingTemplates(){
    StructuredNaming engine;engine.id=L"game-assets-v1";engine.name=L"游戏引擎资产";engine.enabled=true;engine.counter=0;
    engine.fields={{L"type",L"文件类别",L"SM",true,0,{{L"静态模型",L"SM"},{L"骨骼模型",L"SK"},{L"材质",L"M"},{L"材质实例",L"MI"},{L"贴图",L"T"}}},
        {L"subject",L"主要名称",L"Rock",true},{L"variant",L"补充说明",L"A",false,0,{{L"A",L"A"},{L"B",L"B"},{L"C",L"C"}}},
        {L"usage",L"贴图用途",L"BC",true,0,{{L"颜色",L"BC"},{L"法线",L"N"},{L"通道打包",L"ORM"}}}};
    for(auto &f:engine.fields){f.defaultSource=0;f.validation=1;f.defaultValue=f.value;}
    engine.fields[1].defaultValue.clear();engine.fields[3].conditionField=L"type";engine.fields[3].conditionValue=L"T";
    auto texture=engine;texture.id=L"game-textures-v1";texture.name=L"贴图资源";
    texture.fields[0].defaultValue=texture.fields[0].value=L"T";
    texture.fields[0].defaultSource=texture.fields[0].source=3;texture.fields[0].quick=false;
    texture.fields[3].conditionField.clear();texture.fields[3].conditionValue.clear();
    StructuredNaming project;project.id=L"game-project-v1";project.name=L"项目分类与编号";project.enabled=true;project.counter=0;
    project.fields={{L"project",L"项目前缀",L"PRJ",true},{L"category",L"分类",L"Prop",true,0,{{L"道具",L"Prop"},{L"场景",L"Env"},{L"角色",L"Char"}}},
        {L"subject",L"主要名称",L"Rock",true},{L"number",L"编号",L"1",false}};
    for(auto &f:project.fields){f.defaultSource=0;f.validation=1;f.defaultValue=f.value;}
    project.fields[2].defaultValue.clear();project.fields[3].validation=2;project.fields[3].width=3;
    return {engine,texture,project};
}
inline std::vector<StructuredNaming> builtinNamingSchemes(const Config &cfg){
    auto schemes=gameNamingTemplates();auto asset=basicStructure(L"",&cfg);
    asset.digits=cfg.numberDigits;asset.dateFormat=cfg.dateFormat;asset.upper=cfg.upperVersion;
    schemes.push_back(std::move(asset));return schemes;
}
inline bool builtinNamingScheme(const std::wstring &id){
    return id==L"asset-v2"||id==L"game-assets-v1"||id==L"game-textures-v1"||id==L"game-project-v1";
}
// Every selector/editor reads this catalog. Saved definitions override defaults
// by identity; simply viewing an available scheme never writes configuration.
struct NamingCatalog {
    struct Input {
        std::wstring name,structure;
        bool namingOnly=false;
        bool operator==(const Input &) const = default;
    };
    std::vector<Input> inputs;
    std::vector<std::wstring> modifiers;
    std::vector<CandidateRename> renames;
    int digits=3,dateFormat=0;
    bool upper=false;
    std::vector<StructuredNaming> schemes;
    std::map<std::wstring,size_t> recipeIndices;
    bool matches(const Config &cfg) const {
        if(inputs.size()!=cfg.recipes.size()||modifiers!=cfg.modifiers||renames!=cfg.candidateRenames||
           digits!=cfg.numberDigits||dateFormat!=cfg.dateFormat||upper!=cfg.upperVersion)return false;
        for(size_t i=0;i<inputs.size();++i){const auto &r=cfg.recipes[i];const auto &old=inputs[i];
            if(r.namingOnly!=old.namingOnly||(r.namingOnly&&(r.name!=old.name||r.structure!=old.structure)))return false;
        }
        return true;
    }
};
inline const NamingCatalog &namingCatalog(const Config &cfg){
    // Compare the actual inputs so imports, rollback and in-place edits all use
    // the same invalidation path, without relying on every caller to bump a flag.
    if(cfg.namingCatalog&&cfg.namingCatalog->matches(cfg))return *cfg.namingCatalog;
    auto cache=std::make_shared<NamingCatalog>();
    cache->modifiers=cfg.modifiers;cache->renames=cfg.candidateRenames;
    cache->digits=cfg.numberDigits;cache->dateFormat=cfg.dateFormat;cache->upper=cfg.upperVersion;
    auto &schemes=cache->schemes;
    auto add=[&](StructuredNaming s){
        if(!s.enabled||std::any_of(schemes.begin(),schemes.end(),[&](const auto &v){return v.id==s.id;}))return;
        s=namingDefinition(std::move(s));
        for(auto &field:s.fields){
            auto original=field;
            for(auto &candidate:field.candidates)for(const auto &rename:cfg.candidateRenames)
                if(rename.schema==s.id&&rename.field==field.id&&rename.from==candidate.value){
                    if(original.defaultValue==candidate.value)field.value=field.defaultValue=rename.to;
                    auto from=namingFieldOutput(original,candidate.value),to=namingFieldOutput(field,rename.to);
                    for(auto &other:s.fields)if(other.conditionField==field.id&&other.conditionValue==from)other.conditionValue=to;
                    for(auto &rule:s.locations)if(rule.field==field.id&&rule.value==from)rule.value=to;
                    candidate.name=candidate.value=rename.to;break;
                }
        }
        schemes.push_back(std::move(s));
    };
    for(size_t i=0;i<cfg.recipes.size();++i){const auto &recipe=cfg.recipes[i];
        cache->inputs.push_back({recipe.namingOnly?recipe.name:L"",recipe.namingOnly?recipe.structure:L"",recipe.namingOnly});
        if(recipe.namingOnly){auto s=deserializeStructure(recipe.structure);
            if(!s.id.empty())cache->recipeIndices.try_emplace(s.id,i);
            s.name=recipe.name;add(std::move(s));}
    }
    for(auto s:builtinNamingSchemes(cfg))add(std::move(s));
    cfg.namingCatalog=std::move(cache);return *cfg.namingCatalog;
}
inline const std::vector<StructuredNaming> &namingSchemeCatalog(const Config &cfg){
    return namingCatalog(cfg).schemes;
}
inline const StructuredNaming &defaultNamingScheme(const Config &cfg,const std::wstring &executable=L""){
    const auto &schemes=namingSchemeCatalog(cfg);
    auto find=[&](const std::wstring &id)->const StructuredNaming*{
        for(const auto &scheme:schemes)if(scheme.id==id)return &scheme;
        return nullptr;
    };
    auto application=cfg.applicationNamingSchemes.find(namingApplicationKey(executable));
    if(application!=cfg.applicationNamingSchemes.end())if(auto scheme=find(application->second))return *scheme;
    if(auto scheme=find(cfg.defaultNamingScheme))return *scheme;
    // The asset scheme is always present, including after a missing/corrupt custom definition.
    return *find(L"asset-v2");
}
inline bool setDefaultNamingScheme(Config &cfg,const std::wstring &id,const std::wstring &executable=L""){
    const auto &schemes=namingSchemeCatalog(cfg);
    if(std::none_of(schemes.begin(),schemes.end(),[&](const auto &scheme){return scheme.id==id;}))return false;
    if(executable.empty()){cfg.defaultNamingScheme=id;return true;}
    auto application=namingApplicationKey(executable);if(application.empty())return false;
    if(!cfg.applicationNamingSchemes.contains(application)&&cfg.applicationNamingSchemes.size()>=100)return false;
    cfg.applicationNamingSchemes[application]=id;return true;
}
inline std::vector<std::wstring> namingSchemeApplications(const Config &cfg,const std::wstring &id){
    std::vector<std::wstring> applications;
    for(const auto &[application,scheme]:cfg.applicationNamingSchemes)if(scheme==id)applications.push_back(application);
    return applications;
}
inline bool setNamingSchemeApplications(Config &cfg,const std::wstring &id,const std::vector<std::wstring> &applications){
    const auto &schemes=namingSchemeCatalog(cfg);
    if(std::none_of(schemes.begin(),schemes.end(),[&](const auto &scheme){return scheme.id==id;}))return false;
    auto updated=cfg.applicationNamingSchemes;
    std::erase_if(updated,[&](const auto &entry){return entry.second==id;});
    for(const auto &executable:applications){
        auto application=namingApplicationKey(executable);if(application.empty())return false;
        updated[application]=id;
    }
    if(updated.size()>100)return false;
    cfg.applicationNamingSchemes=std::move(updated);return true;
}
inline void removeNamingSchemeDefaults(Config &cfg,const std::wstring &id){
    if(cfg.defaultNamingScheme==id)cfg.defaultNamingScheme=L"asset-v2";
    if(cfg.activeNamingScheme==id)cfg.activeNamingScheme.clear();
    std::erase_if(cfg.applicationNamingSchemes,[&](const auto &entry){return entry.second==id;});
}
} // namespace jump
