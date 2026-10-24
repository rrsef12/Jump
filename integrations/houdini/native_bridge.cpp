// Version-pinned adapter to Houdini's existing RE/UI file chooser.
// These exported UI functions are not all in the public HDK headers. Resolve
// symbols by name, require an exact Houdini build, and never use memory offsets.
#include <SI/AP_Interface.h>
#include <UI/UI_Value.h>
#include <QtWidgets/QWidget>
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <stdexcept>

namespace {
template<class T> T symbol(const wchar_t *module, const char *name) {
    auto address=GetProcAddress(GetModuleHandleW(module),name);
    if(!address) throw std::runtime_error("Houdini UI symbol unavailable");
    return reinterpret_cast<T>(address);
}
struct Chooser { void *native=nullptr; AP_Interface *app=nullptr; };
Chooser find(void *widget) {
    if(!widget) throw std::runtime_error("No original chooser");
    auto qt=static_cast<QWidget*>(widget);
    if(!qt->isVisible() || qt->objectName()!=QStringLiteral("RE_Window") ||
       (qt->windowTitle()!=QStringLiteral("Save As") && qt->windowTitle()!=QStringLiteral("另存为")))
        throw std::runtime_error("Original chooser has closed");
    auto root=symbol<void*(*)(void*,bool)>(L"libUI.dll","?getWindowWithID@UI_Window@@SAPEAV1@PEAX_N@Z")(widget,false);
    auto count=symbol<int(*)(const void*)>(L"libUI.dll","?getNumChildren@UI_Feel@@QEBAHXZ");
    auto child=symbol<UI_Object*(*)(const void*,int)>(L"libUI.dll","?getChild@UI_Feel@@QEBAPEAV1@H@Z");
    std::vector<UI_Object*> queue{static_cast<UI_Object*>(root)};
    std::unordered_set<UI_Object*> seen;
    Chooser result;
    for(size_t i=0;i<queue.size() && seen.size()<4096;++i) {
        auto object=queue[i];
        if(!object || !seen.insert(object).second) continue;
        if(std::string(object->className())=="SI_FileChooserFeel") {
            if(result.native) throw std::runtime_error("Ambiguous original chooser");
            result.native=symbol<void*(*)(void*)>(L"libSI.dll","?getFileChooser@SI_FileChooserFeel@@UEAAPEAVSI_FileChooser@@XZ")(object);
            result.app=dynamic_cast<AP_Interface*>(static_cast<UI_Object*>(result.native));
        }
        int children=count(object);
        if(children<0 || children>1024) throw std::runtime_error("Invalid original layout");
        for(int n=0;n<children;++n) queue.push_back(child(object,n));
    }
    if(!result.app) throw std::runtime_error("No original Houdini file chooser");
    auto mode=symbol<int(*)(const void*)>(L"libSI.dll","?getMode@SI_FileChooser@@QEBA?AW4SI_FileChooserMode@@XZ")(result.native);
    auto pattern=result.app->findValueSymbol("pattern.val"); UT_String filter;
    if(pattern) pattern->getValue(filter);
    if(mode!=1 || !pattern || std::string(filter.c_str()).find("*.hip")==std::string::npos)
        throw std::runtime_error("Not a Houdini scene Save As chooser");
    return result;
}
std::string value(const Chooser &chooser,const char *key) {
    auto field=chooser.app->findValueSymbol(key);
    if(!field) throw std::runtime_error("Missing original field");
    UT_String text;field->getValue(text);
    std::string out=text.c_str();
    if(out.find_first_of("\r\n")!=out.npos) throw std::runtime_error("Unsupported line break");
    return out;
}
std::string snapshot(const Chooser &c) {
    return "OK\n"+value(c,"fileField.val")+"\n"+value(c,"fullDirPath.val")+"\n"+value(c,"pattern.val");
}
std::string failure(const std::exception &e) {return std::string("ERROR\n")+e.what();}
}
extern "C" __declspec(dllexport) const char *jump_houdini_version() {return JUMP_HOUDINI_VERSION;}
extern "C" __declspec(dllexport) const char *jump_houdini_inspect(void *widget) {
    static thread_local std::string result;
    try {result=snapshot(find(widget));} catch(const std::exception &e) {result=failure(e);}
    return result.c_str();
}
extern "C" __declspec(dllexport) const char *jump_houdini_apply(void *widget,const char *name,const char *directory,bool submit) {
    static thread_local std::string result;
    try {
        auto chooser=find(widget);
        if(!name || !*name || std::string(name).find_first_of("\\/:*?\"<>|\r\n")!=std::string::npos)
            throw std::runtime_error("Invalid filename");
        if(directory && *directory) {
            auto path=std::string(directory)+"/"+name;
            symbol<void(*)(void*,const char*)>(L"libSI.dll","?setFullPath@SI_FileChooser@@QEAAXPEBD@Z")(chooser.native,path.c_str());
        } else symbol<void(*)(void*,const char*)>(L"libSI.dll","?setFileField@SI_FileChooser@@QEAAXPEBD@Z")(chooser.native,name);
        auto readback=value(chooser,"fileField.val");
        auto slash=readback.find_last_of("/\\");
        if((slash==readback.npos?readback:readback.substr(slash+1))!=name)
            throw std::runtime_error("Original filename readback failed");
        result=snapshot(chooser);
        if(submit) {
            auto accept=chooser.app->findValueSymbol("accept.button.val");
            if(!accept) throw std::runtime_error("Original Accept button unavailable");
            accept->setValue(1);accept->changed(nullptr);
        }
    } catch(const std::exception &e) {result=failure(e);}
    return result.c_str();
}
