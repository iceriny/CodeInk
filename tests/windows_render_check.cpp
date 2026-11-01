// Native COM round trip on one new, hidden, unsaved presentation. Existing
// presentations, selection and preferences are never written by this check.
#include <cstdio>
#include "../src/render.cpp"
Progress::Progress(HWND,const wchar_t*):window(nullptr),label(nullptr),owner(nullptr),stopped(false),ownDisable(false){}
Progress::~Progress(){}
void Progress::text(const wchar_t*){}
bool Progress::pump(){return true;}
HRESULT invokeEngine(const Options&,const BSTR*,int,HighlightResult&,Progress*){return E_NOTIMPL;}

static HRESULT verify(IDispatch*shapes,int mode,bool numbers,bool window){
    Options options;options.outputMode=mode;options.lineNumbers=numbers;options.windowFrame=window;options.columnCount=window?3:2;
    Capture source;source.text=Value::text(L"// 1\r\r// 3\r// 4\r// 5\r// 6\r");
    HighlightItem item;item.length=SysStringLen(source.text.v.bstrVal);if(!item.tokens.make(1))return E_OUTOFMEMORY;
    item.tokens[0]={0,(DWORD)item.length,0x00AA00,0};HighlightResult colors;colors.fg=0x222222;colors.bg=0xFFFFFF;colors.gutter=0x777777;
    CreatedBlock block;Progress progress(nullptr,L"CodeInk test");HRESULT hr=createBlock(shapes,source,item,colors,options,block,progress);
    if(failed(hr))std::printf("  createBlock: 0x%08X\n",(unsigned)hr);
    if(!failed(hr)){
        Capture captured;const wchar_t*message=nullptr;hr=captureShape(block.content,captured,options,false,&message);
        if(hr!=S_OK||captured.codeColumns!=options.columnCount||!same(captured.text.v.bstrVal,source.text.v.bstrVal))
            std::printf("  capture: 0x%08X, columns=%d, length=%u (expected %u)\n",(unsigned)hr,captured.codeColumns,SysStringLen(captured.text.v.bstrVal),SysStringLen(source.text.v.bstrVal));
        if(hr!=S_OK||captured.codeColumns!=options.columnCount||!same(captured.text.v.bstrVal,source.text.v.bstrVal))hr=E_FAIL;
        if(!failed(hr)){hr=recolor(captured,item,colors,options,Action::Color,progress);if(failed(hr))std::printf("  recolor: 0x%08X\n",(unsigned)hr);}
        if(!failed(hr)&&mode==0){hr=tidyCapture(captured,options);if(!failed(hr))hr=concatenate(captured);if(!failed(hr))hr=recolor(captured,item,colors,options,Action::Format,progress);if(failed(hr))std::printf("  format: 0x%08X\n",(unsigned)hr);}
        if(!failed(hr)){
            hr=captureShape(block.content,captured,options,false,&message);
            if(hr!=S_OK||!same(captured.text.v.bstrVal,source.text.v.bstrVal))hr=E_FAIL;
        }
    }
    block.cleanup();std::printf("%s: %s, numbers=%d, window=%d (0x%08X)\n",failed(hr)?"FAIL":"PASS",mode?"text boxes":"table",numbers,window,(unsigned)hr);return hr;
}
int main(){
    if(failed(CoInitializeEx(nullptr,2)))return 1;int result=1;
    {
        const GUID powerpoint={0x91493441,0x5A91,0x11CF,{0x87,0x00,0x00,0xAA,0x00,0x60,0x26,0x3B}};
        Ref app,presentations,presentation,slides,slide,shapes;HRESULT hr=CoCreateInstance(powerpoint,nullptr,4,IID_Dispatch,(void**)&app.p);
        if(!failed(hr))hr=object(app,L"Presentations",presentations);
        Value hidden=Value::integer(0);if(!failed(hr))hr=object(presentations,L"Add",presentation,1,&hidden);
        if(!failed(hr))hr=object(presentation,L"Slides",slides);
        Value args[]={Value::integer(1),Value::integer(12)};if(!failed(hr))hr=object(slides,L"Add",slide,2,args);
        if(!failed(hr))hr=object(slide,L"Shapes",shapes);
        if(!failed(hr))for(int mode=0;mode<2&&!failed(hr);++mode)for(int numbers=0;numbers<2&&!failed(hr);++numbers)for(int window=0;window<2&&!failed(hr);++window)hr=verify(shapes,mode,numbers!=0,window!=0);
        if(failed(hr))std::printf("Native renderer check failed: 0x%08X\n",(unsigned)hr);else result=0;
        if(presentation){put(presentation,L"Saved",Value::integer(-1));if(failed(invoke(presentation,L"Close",DISPATCH_METHOD,0,nullptr)))result=1;}
    }
    CoUninitialize();return result;
}
