#include "engine.h"
// Installation-time COM activation check; does not launch or automate PowerPoint.
extern "C" void WINAPI EntryPoint(){
    HRESULT hr=CoInitializeEx(nullptr,2);if(failed(hr)){ExitProcess((UINT)hr);return;}
    IDTExtensibility2* addin=nullptr;
    hr=CoCreateInstance(CLSID_CodeInk,nullptr,1,IID_Extensibility,(void**)&addin);
    if(!failed(hr)&&addin){
        IRibbonExtensibility* ribbon=nullptr;hr=addin->QueryInterface(IID_Ribbon,(void**)&ribbon);
        if(!failed(hr)&&ribbon){BSTR xml=nullptr;hr=ribbon->GetCustomUI(nullptr,&xml);if(!failed(hr)&&(!xml||SysStringLen(xml)<100))hr=E_FAIL;if(xml)SysFreeString(xml);ribbon->Release();}
        if(!failed(hr)){
            GUID nullId={};wchar_t* name=(wchar_t*)L"GetLanguage";DISPID id=0;
            hr=addin->GetIDsOfNames(nullId,&name,1,0x0409,&id);
            if(!failed(hr)){DISPPARAMS params={};VARIANT result={};hr=addin->Invoke(id,nullId,0x0409,1,&params,&result,nullptr,nullptr);if(!failed(hr)&&(result.vt!=VT_BSTR||!same(result.bstrVal,L"cpp")))hr=E_FAIL;VariantClear(&result);}
        }
        addin->Release();
    }
    if(!failed(hr)){
        Options*o=(Options*)alloc(sizeof(Options));HighlightResult*out=(HighlightResult*)alloc(sizeof(HighlightResult));
        if(!o||!out){dealloc(o);dealloc(out);hr=E_OUTOFMEMORY;}
        else{new(o)Options();new(out)HighlightResult();BSTR samples[]={SysAllocString(L"#include <string>\r\nint main(){ /* 中文😀 */ return 42; }"),SysAllocString(L"const char* s = \"hello\";\r// test\nint x=1;")};
            hr=samples[0]&&samples[1]?invokeEngine(*o,samples,2,*out,nullptr):E_OUTOFMEMORY;
            if(!failed(hr)&&(!out->items.size()||!out->items[0].tokens.size()))hr=E_FAIL;
            SysFreeString(samples[0]);SysFreeString(samples[1]);out->~HighlightResult();o->~Options();dealloc(out);dealloc(o);
        }
    }
    logEvent(sizeof(void*)==8?L"COM + native Shiki bridge self-check (x64)":L"COM + native Shiki bridge self-check (x86)",hr);
    CoUninitialize();ExitProcess((UINT)hr);
}
