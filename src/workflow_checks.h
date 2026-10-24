#pragma once
#include "naming_worker.h"
#include "naming_session.h"
#include "save_verification.h"
#include "structured_naming.h"
#include "naming_tools.h"
#include "save_target.h"
#include "config_writer.h"
#include <fstream>
#include <source_location>

namespace jump {
inline bool saveAssistChecks(const std::filesystem::path &directory){
    std::filesystem::create_directories(directory);
    try{
        auto require=[](bool ok,const std::source_location where=std::source_location::current()){
            if(!ok)throw std::runtime_error("save assistance check failed at line "+std::to_string(where.line()));};
        Config config=defaults();auto scheme=gameNamingTemplates()[0];
        config.projects.push_back({L"test",L"测试",directory.wstring()});config.activeProject=L"test";
        scheme.locations={{L"type",L"SM",L"Models"},{L"usage",L"BC",L"Textures"}};
        require(namingSchemaError(scheme).empty());require(deserializeStructure(serializeStructure(scheme))==scheme);
        auto disabled=scheme;disabled.fields[0].enabled=false;disabled.recommendLocations=disabled.warnExisting=false;
        require(deserializeStructure(serializeStructure(disabled))==disabled);
        auto disabledSession=namingSession(disabled,L"Rock");
        require(!namingFieldVisible(disabledSession,0,config)&&!namingFieldVisible(disabledSession,3,config));
        require(composeStructure(disabledSession,config).name==L"Rock_A"&&namingRecommendedLocations(disabledSession,config).empty());
        require(namingTrialPreview(disabled,config,L"Rock.txt").items[0].explanation.find(L"已在方案中关闭")!=std::wstring::npos);
        disabled.fields[2].enabled=false;disabled.fields[2].validation=2;
        require(namingSchemaError(disabled).empty()&&deserializeStructure(serializeStructure(disabled))==disabled);
        auto invalidDisabled=disabled;invalidDisabled.fields[2].defaultValue=invalidDisabled.fields[2].value=L"bad/name";
        require(!namingSchemaError(invalidDisabled).empty());
        auto session=namingSession(scheme,L"Rock");auto locations=namingRecommendedLocations(session,config);
        require(locations.size()==1&&normalize(locations[0].path)==normalize((directory/L"Models").wstring()));
        session.fields[0].value=L"T";locations=namingRecommendedLocations(session,config);
        require(locations.size()==1&&locations[0].name==L"Textures");
        session.fields[3].omitted=true;require(namingRecommendedLocations(session,config).empty());
        config.activeProject.clear();require(namingRecommendedLocations(namingSession(scheme,L"Rock"),config).empty());
        scheme.locations[0].directory=(directory/L"Models").wstring();
        require(namingRecommendedLocations(namingSession(scheme,L"Rock"),config).size()==1);
        auto invalid=scheme;invalid.locations[0].directory=L"..\\escape";require(!namingSchemaError(invalid).empty());
        invalid=scheme;invalid.locations[0].field=L"missing";require(!namingSchemaError(invalid).empty());
        auto report=namingTrialPreview(scheme,config,L"Rock.txt\nGuard.fbx\nbad/name.png");
        require(report.items.size()==3&&report.items[0].name==L"SM_Rock_A.txt"&&report.items[1].name==L"SM_Guard_A.fbx"&&
            report.items[0].explanation.find(L"才会加入")!=std::wstring::npos&&report.items[2].error.find(L"不要带文件夹路径")!=std::wstring::npos);
        auto missing=basicStructure(L"");missing.fields[2].required=true;
        require(namingTrialPreview(missing,config,L"Rock.txt").items[0].error==L"请返回方案填写“文件类别”。");
        auto restricted=namingTrialPreview(scheme,config,L"守卫.fbx");require(restricted.items[0].errorField==1&&restricted.items[0].settingsPage==1);
        Config saved=defaults();Recipe recipe;recipe.name=scheme.name;recipe.value=L"Asset";recipe.complete=recipe.namingOnly=true;recipe.structure=serializeStructure(scheme);saved.recipes.push_back(recipe);
        auto file=directory/L"routes.ini";require(save(saved,file));auto loaded=load(file);
        require(!loaded.recipes.empty());require(deserializeStructure(loaded.recipes.back().structure).locations==scheme.locations);
        auto target=directory/L"existing.txt";std::ofstream(target)<<"existing";
        auto info=inspectSaveTarget({1,2,target.wstring()});require(info.exists&&!info.folder&&info.bytes==8&&!info.error);
        require(saveTargetDetails(info).find(L"8 B")!=std::wstring::npos);
        info=inspectSaveTarget({1,2,directory.wstring()});require(info.exists&&info.folder);
        info=inspectSaveTarget({1,2,(directory/L"missing.txt").wstring()});require(!info.exists&&!info.error);
        SaveTargetWorker worker(nullptr,WM_APP+15);worker.request({0,2,target.wstring()});worker.cancel();
        worker.request({0,3,(directory/L"missing.txt").wstring()});std::optional<SaveTargetInfo> result;
        for(int i=0;i<100&&!result;++i){result=worker.take();if(!result)Sleep(5);}
        require(result&&result->request.generation==3&&!result->exists);return true;
    }catch(const std::exception &error){std::ofstream(directory/L"failure.txt")<<error.what();return false;}
}
inline void namingDefaultsChecks(const std::filesystem::path &directory){
    auto require=[](bool ok,const std::source_location where=std::source_location::current()){
        if(!ok)throw std::runtime_error("naming defaults check failed at line "+std::to_string(where.line()));};
    auto cfg=defaults();require(defaultNamingScheme(cfg).id==L"asset-v2");
    require(setDefaultNamingScheme(cfg,L"game-assets-v1"));auto catalog=cfg.namingCatalog;
    require(setDefaultNamingScheme(cfg,L"game-textures-v1",L"C:\\Tools\\BLENDER.EXE"));
    require(setDefaultNamingScheme(cfg,L"asset-v2",L"Photoshop.exe"));
    require(defaultNamingScheme(cfg,L"D:\\OtherVersion\\Blender.exe").id==L"game-textures-v1");
    require(defaultNamingScheme(cfg,L"PHOTOSHOP.EXE").id==L"asset-v2");
    require(defaultNamingScheme(cfg,L"Other.exe").id==L"game-assets-v1");
    cfg.activeNamingScheme=L"game-project-v1";
    require(defaultNamingScheme(cfg).id==L"game-assets-v1"&&cfg.namingCatalog==catalog);
    require(!setDefaultNamingScheme(cfg,L"missing",L"Blender.exe"));
    require(!setDefaultNamingScheme(cfg,L"asset-v2",L"not-an-application.txt"));
    auto file=directory/L"naming-defaults.ini";require(save(cfg,file));auto loaded=load(file);
    require(loaded.defaultNamingScheme==cfg.defaultNamingScheme&&loaded.applicationNamingSchemes==cfg.applicationNamingSchemes);
    require(defaultNamingScheme(loaded,L"blender.exe").id==L"game-textures-v1");
    require(WritePrivateProfileStringW(L"Jump",L"defaultNamingScheme",nullptr,file.c_str())!=FALSE);
    auto migrated=load(file);require(migrated.defaultNamingScheme==cfg.activeNamingScheme);
    migrated.activeNamingScheme=L"asset-v2";require(save(migrated,file));
    require(load(file).defaultNamingScheme==L"game-project-v1");
    cfg.applicationNamingSchemes[L"blender.exe"]=L"missing";
    require(defaultNamingScheme(cfg,L"blender.exe").id==L"game-assets-v1");
    cfg.defaultNamingScheme=L"missing";require(defaultNamingScheme(cfg,L"blender.exe").id==L"asset-v2");
    auto custom=gameNamingTemplates()[0];custom.id=L"custom-default";custom.name=L"Custom default";
    Recipe recipe;recipe.name=custom.name;recipe.structure=serializeStructure(custom);recipe.namingOnly=true;cfg.recipes.push_back(recipe);
    require(setDefaultNamingScheme(cfg,custom.id)&&setDefaultNamingScheme(cfg,custom.id,L"Blender.exe"));
    cfg.recipes.back().name=L"Renamed default";require(defaultNamingScheme(cfg,L"blender.exe").name==L"Renamed default");
    cfg.recipes.pop_back();removeNamingSchemeDefaults(cfg,custom.id);
    require(cfg.defaultNamingScheme==L"asset-v2"&&!cfg.applicationNamingSchemes.contains(L"blender.exe"));
    require(defaultNamingScheme(cfg,L"Photoshop.exe").id==L"asset-v2");
}
inline void persistencePerformanceChecks(const std::filesystem::path &directory){
    auto require=[](bool ok,const std::source_location where=std::source_location::current()){
        if(!ok)throw std::runtime_error("persistence/cache check failed at line "+std::to_string(where.line()));};
    auto cfg=defaults();namingSchemeCatalog(cfg);auto initial=cfg.namingCatalog;
    require(&namingCatalog(cfg)==initial.get());
    rememberSubject(cfg,L"",L"Recent");cfg.activeNamingScheme=L"asset-v2";
    require(&namingCatalog(cfg)==initial.get());
    auto rollback=cfg;auto custom=namingDefinition(gameNamingTemplates()[0]);custom.name=L"Custom";
    Recipe recipe;recipe.name=custom.name;recipe.value=L"Asset";recipe.namingOnly=true;recipe.structure=serializeStructure(custom);
    cfg.recipes.push_back(recipe);require(namingSchemeCatalog(cfg).front().name==L"Custom");
    require(cfg.namingCatalog!=initial&&namingCatalog(cfg).recipeIndices.at(custom.id)==0);
    auto saved=cfg.namingCatalog;cfg.recipes[0].name=L"Renamed";
    require(namingSchemeCatalog(cfg).front().name==L"Renamed"&&cfg.namingCatalog!=saved);
    custom.fields[0].defaultValue=L"SK";cfg.recipes[0].structure=serializeStructure(custom);
    require(namingSchemeCatalog(cfg).front().fields[0].defaultValue==L"SK");
    cfg.candidateRenames.push_back({custom.id,L"type",L"SK",L"Skeletal"});
    require(namingSchemeCatalog(cfg).front().fields[0].defaultValue==L"Skeletal");
    cfg=rollback;require(&namingCatalog(cfg)==initial.get());
    cfg.numberDigits=4;require(namingSchemeCatalog(cfg).back().digits==4);
    cfg.dateFormat=2;cfg.upperVersion=true;cfg.modifiers.push_back(L"UniqueStatus");
    const auto &asset=namingSchemeCatalog(cfg).back();require(asset.dateFormat==2&&asset.upper);
    require(asset.fields[4].candidates.back().value==L"UniqueStatus");
    struct Gate {std::mutex mutex;std::condition_variable changed;bool started=false,release=false;std::vector<std::wstring> writes;} gate;
    {
        ConfigWriter writer(directory/L"writer-order.ini",nullptr,0,[&](const Config &snapshot,const auto &){
            std::unique_lock lock(gate.mutex);
            if(gate.writes.empty()){
                gate.started=true;gate.changed.notify_all();
                if(!gate.changed.wait_for(lock,std::chrono::seconds(3),[&]{return gate.release;}))return false;
            }
            gate.writes.push_back(snapshot.variableTemplate);return snapshot.variableTemplate!=L"fail";
        });
        cfg.variableTemplate=L"first";writer.request(cfg,true);
        bool started;
        {std::unique_lock lock(gate.mutex);started=gate.changed.wait_for(lock,std::chrono::seconds(2),[&]{return gate.started;});}
        cfg.variableTemplate=L"superseded";writer.request(cfg);
        cfg.variableTemplate=L"latest";writer.request(cfg);
        {std::lock_guard lock(gate.mutex);gate.release=true;gate.changed.notify_all();}
        require(started&&writer.flush());
        require(gate.writes==std::vector<std::wstring>{L"first",L"latest"});
        cfg.variableTemplate=L"fail";require(!writer.saveNow(cfg));
        cfg.variableTemplate=L"retry";require(writer.saveNow(cfg));
        cfg.variableTemplate=L"exit";writer.request(cfg);
    }
    require(gate.writes.back()==L"exit");
    auto file=directory/L"writer-roundtrip.ini";
    {ConfigWriter writer(file,nullptr,0);rememberSubject(cfg,L"",L"ExitRecord");writer.request(cfg);}
    require(load(file).recentSubjects.front().value==L"ExitRecord");
}
inline bool legacyNamingMigrationChecks(const std::filesystem::path &directory){
    auto file=directory/L"legacy-migration.ini";
    auto require=[](bool ok,const std::source_location where=std::source_location::current()){
        if(!ok)throw std::runtime_error("legacy naming migration failed at line "+std::to_string(where.line()));};
    auto cfg=defaults();auto scheme=namingDefinition(gameNamingTemplates()[0]);
    scheme.id=L"migrated-0-0";scheme.name=L"旧名称";
    Recipe saved;saved.name=scheme.name;saved.value=L"Asset";saved.namingOnly=saved.complete=true;saved.structure=serializeStructure(scheme);
    cfg.recipes.push_back(saved);cfg.activeNamingScheme=scheme.id;
    // Existing data above the normal creation limit must never be truncated on reload.
    for(int i=0;i<101;++i)cfg.recipes.push_back({L"流程"+std::to_wstring(i),directory.wstring(),L"Saved"});
    require(save(cfg,file));
    auto write=[&](const std::wstring &section,const std::wstring &key,const std::wstring &value){
        require(WritePrivateProfileStringW(section.c_str(),key.c_str(),value.c_str(),file.c_str())!=FALSE);};
    auto encode=[](const std::wstring &value){std::wstring result;wchar_t unit[5]{};
        for(auto ch:value){swprintf_s(unit,L"%04X",static_cast<unsigned>(ch));result+=unit;}return result;};
    write(L"Jump",L"groups",L"1");write(L"Jump",L"structuredPreferred",L"0");
    write(L"Group0",L"id",encode(L"old"));write(L"Group0",L"name",encode(L"旧名称"));write(L"Group0",L"presets",L"56");
    for(int i=0;i<56;++i){auto section=L"Group0.Name"+std::to_wstring(i);auto value=L"Legacy"+std::to_wstring(i==55?0:i);
        write(section,L"name",encode(value));write(section,L"value",encode(value));write(section,L"keywords",encode(L"旧关键词"));}
    auto migrated=load(file);require(migrated.recipes.size()==104&&migrated.activeNamingScheme==cfg.activeNamingScheme);
    size_t candidates=0;
    for(size_t i=102;i<migrated.recipes.size();++i){auto definition=deserializeStructure(migrated.recipes[i].structure);
        require(migrated.recipes[i].namingOnly);
        require(definition.enabled&&definition.id!=scheme.id&&definition.name!=scheme.name&&definition.counter==0);
        require(definition.fields.size()==1&&definition.fields[0].candidates.size()<=50);
        candidates+=definition.fields[0].candidates.size();
        require(composeStructure(namingSession(definition,L"Original"),migrated).name==L"Original");}
    require(candidates==55);require(backupConfig(file));require(save(migrated,file));auto roundtrip=load(file);
    require(roundtrip.recipes.size()==migrated.recipes.size()&&roundtrip.activeNamingScheme==cfg.activeNamingScheme);
    for(size_t i=102;i<roundtrip.recipes.size();++i)
        require(roundtrip.recipes[i].namingOnly&&roundtrip.recipes[i].structure==migrated.recipes[i].structure);
    auto owner=L"Recipe102";int parts=GetPrivateProfileIntW(owner,L"structureParts",0,file.c_str());require(parts>0);
    auto lastPart=std::wstring(owner)+L".Structure"+std::to_wstring(parts-1);
    require(GetPrivateProfileIntW(owner,L"namingOnly",-1,file.c_str())==1);
    require(GetPrivateProfileIntW(lastPart.c_str(),L"namingOnly",-1,file.c_str())==-1);
    // Recover schemes written by the old chunked writer, without reclassifying workflows.
    require(WritePrivateProfileStringW(owner,L"namingOnly",nullptr,file.c_str())!=FALSE);
    write(lastPart,L"namingOnly",L"1");require(load(file).recipes[102].namingOnly);
    write(owner,L"namingOnly",L"0");require(!load(file).recipes[102].namingOnly);
    require(save(roundtrip,file));
    wchar_t removed[32]{};GetPrivateProfileStringW(L"Jump",L"groups",L"removed",removed,32,file.c_str());
    require(std::wstring(removed)==L"removed");
    GetPrivateProfileStringW(L"Jump",L"structuredPreferred",L"removed",removed,32,file.c_str());require(std::wstring(removed)==L"removed");
    auto backups=configBackups(file);require(!backups.empty());
    return true;
}
inline bool workflowModelChecks(const std::filesystem::path &directory) {
    try {
        auto require = [](bool ok) { if (!ok) throw std::runtime_error("workflow check failed"); };
        persistencePerformanceChecks(directory);
        namingDefaultsChecks(directory);
        require(legacyNamingMigrationChecks(directory));
        Project project{L"harbor", L"海港", L"D:\\Harbor", L"项目=海港\n作者=李", L"work", L"delivery\\characters"};
        require(projectPath(project, project.delivery) == L"D:\\Harbor\\delivery\\characters");
        require(projectPath(project, L"..\\escape").empty());
        require(projectPath(project, L"C:\\escape").empty());
        require(projectPath(project, L"\\escape").empty());
        auto values = mergedVariables(L"项目=默认\n角色=守卫", project.variables);
        require(smartNamingPreview(L"角色", project.root, L".txt", {0, false, L"{项目}_{角色}_{作者}", values}).name == L"海港_守卫_李");
        require(!smartNamingPreview(L"角色", project.root, L".txt", {0, false, L"{name}", mergedVariables(L"作者=A\n作者=B", L"")}).error.empty());
        auto cfg = defaults(); cfg.projects.push_back(project); cfg.activeProject = project.id;
        auto structure=basicStructure(L"Guard_v012");
        require(structure.id==L"asset-v2"&&structure.fields[1].candidates.empty());
        require(structure.fields[1].value==L"Guard");
        structure.fields[2].value=L"模型";structure.fields[4].value=L"工作";
        require(composeStructure(structure,cfg).name==L"海港_Guard_模型_工作");
        structure.fields[3].value=L"Guard";
        require(composeStructure(structure,cfg).name==L"海港_Guard_模型_Guard_工作");
        require(deserializeStructure(serializeStructure(structure))==structure);
        require(!deserializeStructure(L"999999999999999:x").enabled);
        auto duplicate=structure;duplicate.fields[2].id=duplicate.fields[1].id;
        require(!deserializeStructure(serializeStructure(duplicate)).enabled);
        auto engine=gameNamingTemplates()[0];
        require(namingSchemaError(engine).empty()&&composeStructure(engine,cfg).name==L"SM_Rock_A");
        engine.fields[0].value=L"T";engine.fields[3].value=L"N";
        require(composeStructure(engine,cfg).name==L"T_Rock_A_N");
        engine.fields[0].value=L"t";engine.fields[0].casing=1;
        require(composeStructure(engine,cfg).name==L"T_Rock_A_N");
        engine.fields[0].value=L"SM";engine.fields[3].value=L"bad/name";
        require(composeStructure(engine,cfg).name==L"SM_Rock_A"); // Hidden values are not emitted or validated.
        engine.fields[0].conditionField=L"usage";engine.fields[0].conditionValue=L"N";
        require(!namingSchemaError(engine).empty());
        auto numbered=gameNamingTemplates()[2];numbered.fields[2].casing=1;
        numbered.fields[3].prefix=L"LOD";numbered.fields[3].ownJoiner=true;numbered.fields[3].joiner=L"-";
        require(composeStructure(numbered,cfg).name==L"PRJ_Prop_ROCK-LOD001");
        numbered.fields[3].value=L"1234";require(!composeStructure(numbered,cfg).error.empty());
        numbered.fields[3].value.clear();require(composeStructure(numbered,cfg).name==L"PRJ_Prop_ROCK");
        auto definition=namingDefinition(gameNamingTemplates()[0]);
        auto fixed=gameNamingTemplates()[1].fields[0];fixed.source=0;require(namingFieldFixed(fixed));
        auto session=namingSession(definition,L"Tree");require(composeStructure(session,cfg).name==L"SM_Tree_A");
        require(session.fields[1].defaultValue.empty()&&session.counter==0);
        require(deserializeStructure(serializeStructure(definition))==definition);
        auto revision=definition;revision.fields[0].defaultValue=L"SK";
        auto revised=reviseNamingSession(revision,definition,session,L"Tree");
        require(composeStructure(revised,cfg).name==L"SK_Tree_A");
        session.fields[0].value=L"T";session.fields[3].value=L"N";
        revised=reviseNamingSession(revision,definition,session,L"Tree");
        require(composeStructure(revised,cfg).name==L"T_Tree_A_N");
        revision.fields[0].source=revision.fields[0].defaultSource=3;
        revised=reviseNamingSession(revision,definition,session,L"Tree");
        require(composeStructure(revised,cfg).name==L"SK_Tree_A");
        auto badOptions=definition;badOptions.digits=1;require(!namingSchemaError(badOptions).empty());
        SmartNaming exampleCounter{1};exampleCounter.digits=4;exampleCounter.upperVersion=true;
        require(numberingExample(L"Rock_v007",exampleCounter)==L"Rock_V0008");
        exampleCounter.counter=2;exampleCounter.digits=3;
        require(numberingExample(L"PRJ_Prop_Rock_001",exampleCounter)==L"PRJ_Prop_Rock_002");
        require(numberingExample(L"Rock_20261018",exampleCounter)==L"Rock_20261018_001");
        auto old=packNaming({L"JNS1",L"old-schema",L"旧方案",L"_",L"1",L"0",L"0",L"0",L"3",L"0",L"0",
            packNaming({L"subject",L"主体",L"Legacy",L"1",L"0"})});
        require(composeStructure(deserializeStructure(old),cfg).name==L"Legacy");
        session.literal=true;session.literalName=L"Manual_Name";
        require(composeStructure(session,cfg).name==L"Manual_Name"&&deserializeStructure(serializeStructure(session))==session);
        require(basicStructure(L"Guard_1001").fields[1].value==L"Guard_1001");
        require(basicStructure(L"Guard_20261018").fields[1].value==L"Guard_20261018");
        auto missing=structure;missing.fields[1].value.clear();require(!composeStructure(missing,cfg).error.empty());
        Recipe recipe{L"交付", project.delivery, L"角色", false, true};
        recipe.complete = recipe.relativeDirectory = recipe.variables = true;
        recipe.structure=serializeStructure(structure);
        recipe.namingOnly=true;cfg.activeNamingScheme=structure.id;
        recipe.counter = 1; recipe.digits = 4; recipe.upperVersion = true; recipe.dateFormat = 2;
        recipe.prefix = L"P_"; recipe.suffix = L"_LOD0"; recipe.stageValue = L"定稿";
        recipe.expression = L"{项目}_{name}_{作者}"; recipe.variableValues = L"作者=李";
        recipe.ruleEnabled = recipe.ruleRegex = true; recipe.pattern = L" +"; recipe.replacement = L"_";
        cfg.recipes = {recipe};
        for (int i = 0; i < 55; ++i) rememberSavedFile(cfg, {L"D:\\Harbor\\file" + std::to_wstring(i) + L".txt", L"test.exe", L"2026-10-17 12:00:00"});
        require(cfg.savedFiles.size() == 50);
        rememberSavedFile(cfg, {L"D:\\Harbor\\file10.txt", L"other.exe", L"later"});
        require(cfg.savedFiles.size() == 50 && cfg.savedFiles.front().process == L"other.exe");
        auto file = directory / L"workflow-roundtrip.ini";
        auto largeStructure=structure;
        for(auto &field:largeStructure.fields)for(int i=0;i<12;++i)
            field.candidates.push_back({std::wstring(40,L'名'),std::wstring(70,L'A')+std::to_wstring(i),std::wstring(40,L'词')});
        cfg.savedFiles.front().structure=serializeStructure(largeStructure);
        require(cfg.savedFiles.front().structure.size()>8192);
        require(save(cfg, file)); auto loaded = load(file);
        require(loaded.savedFiles.front().structure==cfg.savedFiles.front().structure);
        require(deserializeStructure(loaded.savedFiles.front().structure)==largeStructure);
        HANDLE reader=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        require(reader!=INVALID_HANDLE_VALUE);
        std::thread releaseReader([reader]{Sleep(45);CloseHandle(reader);});
        bool savedWithReader=save(cfg,file);releaseReader.join();require(savedWithReader);
        require(loaded.projects.size() == 1 && activeProject(loaded)->root == project.root);
        require(loaded.savedFiles.size() == 50 && loaded.savedFiles.front().verifiedAt == L"later");
        const auto &r = loaded.recipes.front();
        require(deserializeStructure(r.structure)==structure);
        require(r.namingOnly&&loaded.activeNamingScheme==structure.id);
        require(r.complete && r.relativeDirectory && r.variables && r.date && r.counter == 1 && r.digits == 4 && r.upperVersion && r.dateFormat == 2);
        require(r.expression == recipe.expression && r.pattern == recipe.pattern && r.ruleEnabled && r.ruleRegex && r.prefix == recipe.prefix && r.stageValue == recipe.stageValue);
        for (int i = 0; i < 7; ++i) require(backupConfig(file));
        auto backups = configBackups(file); require(backups.size() == 5 && load(backups.front()).recipes.front().expression == recipe.expression);
        auto package = cfg; package.presetPackage = true; 
        auto packageFile = directory / L"recipe-only-package.ini";
        require(save(package, packageFile));
        require(load(packageFile, true).recipes.size() == 1);
        require(!load(packageFile).presetPackage);
        auto metadataFile = directory / L"metadata-baseline.txt";
        std::ofstream(metadataFile).put('x');
        auto baseline = readSaveStamp(metadataFile.wstring());
        require(baseline.baselineKnown && baseline.existed && !savedStampChanged(baseline));
        std::ofstream(metadataFile, std::ios::app).put('y'); require(savedStampChanged(baseline));
        SaveStamp unknown; unknown.file = metadataFile.wstring();
        require(!savedStampChanged(unknown)); // An unreadable baseline is never evidence of creation.
        NamingHistory history; NamingSnapshot a; a.parts.base = L"原始";
        auto b = a; b.parts.stage = L"定稿"; auto c = b; c.counter = 1;
        history.checkpoint(a); history.checkpoint(b);
        require(history.move(c, false) == b); require(history.move(b, false) == a);
        require(history.move(a, true) == b); history.checkpoint(b);
        require(!history.canRedo());
        NamingHistory cancelled;NamingSnapshot original;original.structure=basicStructure(L"Guard");
        auto start=cancelled.mark();cancelled.checkpoint(original);
        auto edited=original;edited.structure.fields[1].value=L"Temporary";cancelled.checkpoint(edited);
        auto stage=edited;stage.structure.fields[4].value=L"工作";
        cancelled.cancelField(start,stage.structure.id,original.structure.fields[1]);stage.structure.fields[1]=original.structure.fields[1];
        require(cancelled.move(stage,false)==original);
        // Deterministic slow-directory simulation: a blocked old request must not
        // prevent a newer request, publish stale results, or delay cancellation.
        struct Gate { std::mutex mutex; std::condition_variable changed; bool started = false, release = false, done = false; };
        auto gate = std::make_shared<Gate>();
        NamingWorker worker(nullptr, WM_APP + 10, [gate](const NamingRequest &request, const std::function<bool()> &) {
            if (request.source == L"slow") {
                std::unique_lock lock(gate->mutex); gate->started = true; gate->changed.notify_all();
                gate->changed.wait_for(lock, std::chrono::seconds(3), [&] { return gate->release; });
                gate->done = true; gate->changed.notify_all();
            }
            return RenamePreview{request.source};
        });
        NamingRequest request; request.source = L"slow"; worker.request(request);
        { std::unique_lock lock(gate->mutex); require(gate->changed.wait_for(lock, std::chrono::seconds(2), [&] { return gate->started; })); }
        request.source = L"latest"; auto id = worker.request(request);
        std::optional<NamingResult> result;
        for (int i = 0; i < 200 && !result; ++i) { result = worker.take(); if (!result) Sleep(5); }
        { std::lock_guard lock(gate->mutex); gate->release = true; gate->changed.notify_all(); }
        require(result && result->request.id == id && result->preview.name == L"latest");
        { std::unique_lock lock(gate->mutex); require(gate->changed.wait_for(lock, std::chrono::seconds(2), [&] { return gate->done; })); }
        worker.cancel(); require(!worker.take());
        require(!smartNamingPreview(L"角色", L"", L".txt", {1}).error.empty());
        return true;
    } catch (const std::exception &error) { std::ofstream(directory/L"workflow-failure.txt")<<error.what();return false; }
}
} // namespace jump
