#include "fonts.h"

namespace {
struct FontProbe {
    HANDLE dc;
    FontProbe():dc(CreateCompatibleDC(nullptr)){}
    ~FontProbe(){if(dc)DeleteDC(dc);}
    bool monospaced(const wchar_t*name){
        if(!dc||!name||!name[0]||name[0]=='@'||slen(name)>=32)return false;
        LOGFONTW description={};description.lfHeight=-32;description.lfCharSet=1;copystr(description.lfFaceName,32,name);
        HANDLE font=CreateFontIndirectW(&description);if(!font)return false;HANDLE previous=SelectObject(dc,font);
        if(!previous||previous==INVALID_HANDLE_VALUE){DeleteObject(font);return false;}
        wchar_t actual[64];bool ok=GetTextFaceW(dc,64,actual)>0&&samei(actual,name);LONG width=0;
        // Measure advances, not ink bounds. A substituted or proportional face
        // must never pass just because its name contains "Mono".
        const wchar_t sample[]=L" iWm0";
        for(int pass=0;pass<2&&ok;++pass)for(int i=0;i<(pass?95:5)&&ok;++i){
            wchar_t c=pass?(wchar_t)(32+i):sample[i];SIZE size={};
            ok=GetTextExtentPoint32W(dc,&c,1,&size)&&size.cx>0;
            if(ok){if(!width)width=size.cx;else ok=size.cx==width;}
        }
        SelectObject(dc,previous);DeleteObject(font);return ok;
    }
};
}
struct FontCatalog::Enumeration {FontCatalog*catalog;FontProbe*probe;};
int WINAPI FontCatalog::enumerate(const LOGFONTW*font,const void*,DWORD type,LPARAM parameter){
    auto&state=*(Enumeration*)parameter;auto&catalog=*state.catalog;const wchar_t*name=font->lfFaceName;
    if(type&3)return 1; // Raster and printer device fonts are not Office text faces.
    if(catalog.used>=catalog.names.size())return 0;
    if(!catalog.find(name)&&state.probe->monospaced(name))copystr(catalog.names[catalog.used++].text,64,name);
    return 1;
}
const wchar_t*FontCatalog::find(const wchar_t*name)const{
    if(!name)return nullptr;for(int i=0;i<used;++i)if(samei(name,names[i].text))return names[i].text;return nullptr;
}
bool FontCatalog::load(IDispatch*application){
    used=0;native=false;names.clear();Ref bars,control,temporary;
    HRESULT source=object(application,L"CommandBars",bars);
    Value identity[]={Value::integer(4),Value::integer(1728)};
    HRESULT hr=failed(source)?source:object(bars,L"FindControl",control,2,identity);
    int total=failed(hr)?0:getInt(control,L"ListCount");
    if(total<1&&bars){
        // Office retains the native Font command even when its old toolbar is
        // absent. A hidden temporary bar exposes its list without changing it.
        wchar_t title[64],tick[24];copystr(title,64,L"CodeInk_Fonts_");number(tick,(int)(GetTickCount()&0x7FFFFFFF));catstr(title,64,tick);
        Value args[]={Value::text(title),Value::integer(1),Value::boolean(false),Value::boolean(true)};
        hr=object(bars,L"Add",temporary,4,args);
        Ref controls;if(!failed(hr))hr=object(temporary,L"Controls",controls);
        if(!failed(hr))hr=object(controls,L"Add",control,2,identity);
        total=failed(hr)?0:getInt(control,L"ListCount");
    }
    bool ok=total>0&&total<=10000&&names.make(total);
    if(ok){FontProbe probe;
        for(int i=1;i<=total;++i){Value text,index=Value::integer(i);
            if(failed(invoke(control,L"List",DISPATCH_PROPERTYGET,1,&index,&text))||text.v.vt!=VT_BSTR){ok=false;break;}
            const wchar_t*name=text.v.bstrVal;if(slen(name)>=64||find(name)||!probe.monospaced(name))continue;
            copystr(names[used++].text,64,name);
        }
    }
    if(temporary&&failed(invoke(temporary,L"Delete",DISPATCH_METHOD,0,nullptr))){used=0;names.clear();return false;}
    if(ok&&used){native=true;return true;}
    // Modern PowerPoint can expose the Font command with an empty List. Enum in
    // the add-in's own process also sees fonts privately registered by Office,
    // rather than relying on a hard-coded list or another application's fonts.
    used=0;if(!names.make(10000))return false;FontProbe probe;if(!probe.dc)return false;
    Enumeration state{this,&probe};LOGFONTW filter={};filter.lfCharSet=1;
    EnumFontFamiliesExW(probe.dc,&filter,enumerate,(LPARAM)&state,0);
    return used>0;
}
