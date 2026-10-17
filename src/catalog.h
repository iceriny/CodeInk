#pragma once
#include "common.h"
struct CatalogItem { wchar_t id[128],name[192],label[256];CatalogItem(){id[0]=name[0]=label[0]=0;} };
class Catalog {
    Items<CatalogItem> entries;int used;bool installed;
public:
    Catalog():used(0),installed(false){}
    void load(const wchar_t* file,bool themes);
    int count()const{return used;}
    const wchar_t* label(int i)const{return i>=0&&i<used?entries[i].label:L"";}
    const wchar_t* resolve(const wchar_t* input)const;
    const wchar_t* display(const wchar_t* id)const;
    bool hasInstalledCatalog()const{return installed;}
};
