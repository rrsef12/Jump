#include <windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <iostream>
#include <string>
using Microsoft::WRL::ComPtr;
static std::string utf8(BSTR value) { if (!value) return {}; int len = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr); std::string out(len, '\0'); WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), len, nullptr, nullptr); out.pop_back(); return out; }
int wmain(int argc, wchar_t** argv) {
    std::cout << "start " << argc << std::endl;
    if (argc < 2) return 2;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    std::cout << "init" << std::endl;
    ComPtr<IUIAutomation> uia; auto hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia));
    if (FAILED(hr)) { std::cout << "create " << std::hex << hr << std::endl; return 3; }
    std::cout << "created " << (bool)uia << std::endl;
    ComPtr<IUIAutomationElement> root; hr = uia->ElementFromHandle(reinterpret_cast<HWND>(_wcstoui64(argv[1], nullptr, 16)), &root);
    if (FAILED(hr)) { std::cout << "root " << std::hex << hr << std::endl; return 4; }
    std::cout << "root " << (bool)root << std::endl;
    ComPtr<IUIAutomationCondition> all; uia->CreateTrueCondition(&all); ComPtr<IUIAutomationElementArray> list; hr = root->FindAll(TreeScope_Descendants, all.Get(), &list);
    if (FAILED(hr)) { std::cout << "find " << std::hex << hr << std::endl; return 5; }
    std::cout << "list " << (bool)list << std::endl;
    int length = 0; list->get_Length(&length);
    for (int i = 0; i < length; ++i) {
        ComPtr<IUIAutomationElement> el; list->GetElement(i, &el); CONTROLTYPEID type = 0; el->get_CurrentControlType(&type);
        if (type != UIA_EditControlTypeId && type != UIA_ComboBoxControlTypeId && type != UIA_ButtonControlTypeId) continue;
        BSTR id=nullptr,name=nullptr,cls=nullptr; el->get_CurrentAutomationId(&id);el->get_CurrentName(&name);el->get_CurrentClassName(&cls);
        std::cout << type << " id=" << utf8(id) << " class=" << utf8(cls) << " name=" << utf8(name);
        ComPtr<IUIAutomationValuePattern> value; if (SUCCEEDED(el->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(&value))) && value) { BSTR text=nullptr;value->get_CurrentValue(&text); std::cout << " value=" << utf8(text); SysFreeString(text); }
        std::cout << "\n"; SysFreeString(id);SysFreeString(name);SysFreeString(cls);
    }
    CoUninitialize();
}
