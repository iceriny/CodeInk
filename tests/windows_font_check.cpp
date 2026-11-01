// Read-only native integration check: no slides, selection or saved preferences
// are changed. The production catalog removes its own temporary font toolbar.
#include <cstdio>
#include "fonts.h"
int main(){
    HRESULT hr=CoInitializeEx(nullptr,2);if(failed(hr))return 1;
    const GUID powerpoint={0x91493441,0x5A91,0x11CF,{0x87,0x00,0x00,0xAA,0x00,0x60,0x26,0x3B}};
    int result=1;
    {
        Ref app;hr=CoCreateInstance(powerpoint,nullptr,4,IID_Dispatch,(void**)&app.p);
        if(failed(hr))std::printf("PowerPoint connection failed: 0x%08X\n",(unsigned)hr);
        else{
            FontCatalog fonts;if(!fonts.load(app))std::puts("Could not read the native PowerPoint monospace font list.");
            else{
                std::printf("PASS: %d usable monospace fonts; source: %s\n",fonts.count(),fonts.nativeSource()?"PowerPoint Font command":"host font resources (native Font list was empty)");
                for(int i=0;i<fonts.count();++i){char name[256];int bytes=WideCharToMultiByte(65001,0,fonts.label(i),-1,name,256,nullptr,nullptr);if(bytes>0)std::puts(name);}
                result=0;
            }
        }
    }
    CoUninitialize();return result;
}
