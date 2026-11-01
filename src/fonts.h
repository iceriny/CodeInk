#pragma once
#include "com.h"

class FontCatalog {
    struct Name { wchar_t text[64]; };
    Items<Name> names;int used=0;bool native=false;
    struct Enumeration;
    static int WINAPI enumerate(const LOGFONTW*,const void*,DWORD,LPARAM);
public:
    bool load(IDispatch*application);
    int count()const{return used;}
    const wchar_t*label(int index)const{return index>=0&&index<used?names[index].text:L"";}
    const wchar_t*find(const wchar_t*name)const;
    bool nativeSource()const{return native;}
};
