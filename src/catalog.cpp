#include "catalog.h"
void Catalog::load(const wchar_t*file,bool themes){
    used=0;installed=false;entries.clear();FileBytes data;wchar_t*storage=(wchar_t*)alloc(2224*2);bool ok=false;if(storage){wchar_t*root=storage,*filename=storage+1024;ok=installDir(root,1024)&&path(filename,1200,root,file)&&data.load(filename,512*1024)&&data.length>=2&&(data.length%2==0);dealloc(storage);}
    if(ok){const wchar_t* text=(const wchar_t*)data.bytes;int n=data.length/2,start=text[0]==0xFEFF?1:0,count=0;
        for(int i=start;i<n;++i)if(text[i]=='\n')++count;++count;if(count>600)ok=false;
        if(ok&&entries.make(count))for(int i=start;i<n;){int end=i;while(end<n&&text[end]!='\r'&&text[end]!='\n')++end;int tab=i;while(tab<end&&text[tab]!='\t')++tab;
            if(used<entries.size()&&tab>i&&tab-i<128&&end-tab-1<192&&tab<end){CatalogItem&v=entries[used++];memcpy(v.id,text+i,(tab-i)*2);v.id[tab-i]=0;memcpy(v.name,text+tab+1,(end-tab-1)*2);v.name[end-tab-1]=0;copystr(v.label,256,v.name);}
            i=end;while(i<n&&(text[i]=='\r'||text[i]=='\n'))++i;
        }
    }
    if(used){installed=true;return;}
    const wchar_t*langs[]={L"cpp",L"python",L"javascript",L"typescript",L"c",L"csharp",L"java",L"rust",L"go",L"html",L"css",L"json",L"yaml",L"sql",L"powershell",L"bash",L"markdown",L"arduino",L"text"};
    const wchar_t*ts[]={L"github-light",L"light-plus",L"github-dark",L"dark-plus",L"one-dark-pro",L"dracula",L"nord",L"monokai",L"catppuccin-latte",L"catppuccin-mocha",L"vitesse-light",L"vitesse-dark"};
    int n=themes?12:19;if(entries.make(n))for(int i=0;i<n;++i){const wchar_t*id=themes?ts[i]:langs[i];copystr(entries[i].id,128,id);copystr(entries[i].name,192,id);copystr(entries[i].label,256,id);++used;}
}
const wchar_t* Catalog::resolve(const wchar_t*input)const{for(int i=0;i<used;++i)if(samei(input,entries[i].id)||samei(input,entries[i].name)||samei(input,entries[i].label))return entries[i].id;return input;}

const wchar_t* Catalog::display(const wchar_t* id)const{for(int i=0;i<used;++i)if(samei(id,entries[i].id))return entries[i].name;return id;}
