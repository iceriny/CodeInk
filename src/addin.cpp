#include "render.h"
#include "catalog.h"
#include "settings.h"
#include "ribbon.generated.h"
static HMODULE gModule=nullptr;static volatile LONG gObjects=0,gLocks=0;
static void show(HWND owner,const wchar_t*text,UINT type=MB_OK|MB_ICONINFORMATION){MessageBoxW(owner,text,L"CodeInk · 代码高亮",type);}
static int bound(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static void decimalText(wchar_t*out,float numberValue,const wchar_t*suffix=L""){
    int scaled=(int)(numberValue*100+0.5f);number(out,scaled/100);int tail=scaled%100;if(tail){wchar_t d[4]={'.',(wchar_t)('0'+tail/10),(wchar_t)('0'+tail%10),0};if(tail%10==0)d[2]=0;catstr(out,64,d);}catstr(out,64,suffix);
}
enum Callback {
    CConnect=1,CDisconnect,CAddins,CStartup,CShutdown,CCustomUI,CRibbonLoad,
    CHighlight,CFormat,CTable,CPlain,CClipboard,CBorders,
    CLanguage,CTheme,CFontFamily,CFontSize,CSpacing,CBackground,CBackgroundColor,CNumbers,CFirstLine,CTableRead,CMerge,CReplace,CThemeStyles,COutputMode,CWindowFrame,CWindowButtons,CWindowShadow,CWindowPadding,CWindowRadius,CWindowFill,CWindowColor,CBorderVertical,CBorderHorizontal,CBorderOuter,CLogging,
    GLanguage,GTheme,GFontFamily,GFontSize,GSpacing,GBackground,GBackgroundColor,GNumbers,GFirstLine,GTableRead,GMerge,GReplace,GThemeStyles,GOutputMode,GWindowFrame,GWindowButtons,GWindowShadow,GWindowPadding,GWindowRadius,GWindowFill,GWindowColor,GBorderVertical,GBorderHorizontal,GBorderOuter,GLogging,
    GWindowEnabled,GBackgroundEnabled,GBorderEnabled,GWindowLabel,
    GLanguageCount,GLanguageLabel,GThemeCount,GThemeLabel,GEnabled,GStatus,CImportTheme,CHelp,CAbout,CStatus,COpenLog,CClearLog,CAppearanceSettings,CLayoutSettings,GNumbersEnabled,CBasicCleanup,GBasicCleanup
};
static const wchar_t*METHODS[]={
    L"OnConnection",L"OnDisconnection",L"OnAddInsUpdate",L"OnStartupComplete",L"OnBeginShutdown",L"GetCustomUI",L"OnRibbonLoad",
    L"OnHighlight",L"OnFormat",L"OnTable",L"OnPlain",L"OnClipboard",L"OnBorders",
    L"OnLanguage",L"OnTheme",L"OnFontFamily",L"OnFontSize",L"OnSpacing",L"OnBackground",L"OnBackgroundColor",L"OnNumbers",L"OnFirstLine",L"OnTableRead",L"OnMerge",L"OnReplace",L"OnThemeStyles",L"OnOutputMode",L"OnWindowFrame",L"OnWindowButtons",L"OnWindowShadow",L"OnWindowPadding",L"OnWindowRadius",L"OnWindowFill",L"OnWindowColor",L"OnBorderVertical",L"OnBorderHorizontal",L"OnBorderOuter",L"OnLogging",
    L"GetLanguage",L"GetTheme",L"GetFontFamily",L"GetFontSize",L"GetSpacing",L"GetBackground",L"GetBackgroundColor",L"GetNumbers",L"GetFirstLine",L"GetTableRead",L"GetMerge",L"GetReplace",L"GetThemeStyles",L"GetOutputMode",L"GetWindowFrame",L"GetWindowButtons",L"GetWindowShadow",L"GetWindowPadding",L"GetWindowRadius",L"GetWindowFill",L"GetWindowColor",L"GetBorderVertical",L"GetBorderHorizontal",L"GetBorderOuter",L"GetLogging",
    L"GetWindowEnabled",L"GetBackgroundEnabled",L"GetBorderEnabled",L"GetWindowLabel",
    L"LanguageCount",L"LanguageLabel",L"ThemeCount",L"ThemeLabel",L"GetEnabled",L"GetStatus",L"OnImportTheme",L"OnHelp",L"OnAbout",L"OnStatus",L"OnOpenLog",L"OnClearLog",L"OnAppearanceSettings",L"OnLayoutSettings",L"GetNumbersEnabled",L"OnBasicCleanup",L"GetBasicCleanup"
};
class Addin final:public IDTExtensibility2,public IRibbonExtensibility{
    volatile LONG refs;Ref app,ribbon;Options opts;Catalog languages,themes;bool busy;wchar_t status[256];
public:
    Addin():refs(1),busy(false){InterlockedIncrement(&gObjects);copystr(status,256,L"尚未执行代码操作。");}
    ~Addin(){InterlockedDecrement(&gObjects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(guidEq(iid,IID_Unknown)||guidEq(iid,IID_Dispatch)||guidEq(iid,IID_Extensibility))*out=static_cast<IDTExtensibility2*>(this);else if(guidEq(iid,IID_Ribbon))*out=static_cast<IRibbonExtensibility*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&refs);if(!n){this->~Addin();dealloc(this);}return n;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT*n)override{if(!n)return E_POINTER;*n=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**out)override{if(out)*out=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPWSTR*names,UINT n,LCID,DISPID*ids)override{
        if(!names||!ids)return E_POINTER;bool unknown=false;for(UINT i=0;i<n;++i){ids[i]=-1;for(UINT j=0;j<sizeof(METHODS)/sizeof(METHODS[0]);++j)if(samei(names[i],METHODS[j])){ids[i]=(DISPID)j+1;break;}if(ids[i]<0)unknown=true;}return unknown?DISP_E_UNKNOWNNAME:S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*params,VARIANT*result,EXCEPINFO*,UINT*)override{
        UINT n=params?params->cArgs:0;VARIANT*a=params?params->rgvarg:nullptr;if(result)memset(result,0,sizeof(*result));
        if(id==CConnect){if(n<4)return DISP_E_BADPARAMCOUNT;return OnConnection(asDispatch(&a[n-1]),asLong(&a[n-2]),asDispatch(&a[n-3]),nullptr);}
        if(id==CDisconnect)return OnDisconnection(0,nullptr);if(id==CAddins)return OnAddInsUpdate(nullptr);if(id==CStartup)return OnStartupComplete(nullptr);if(id==CShutdown)return OnBeginShutdown(nullptr);
        if(id==CCustomUI){if(!result)return E_POINTER;result->vt=VT_BSTR;return GetCustomUI(nullptr,&result->bstrVal);}
        if(id==CRibbonLoad){if(n<1)return DISP_E_BADPARAMCOUNT;ribbon.set(asDispatch(&a[0]));return S_OK;}
        if(id==CHighlight||id==CFormat||id==CTable||id==CPlain||id==CClipboard||id==CBorders){run(id==CHighlight?Action::Color:id==CFormat?Action::Format:id==CTable?Action::Table:id==CPlain?Action::Plain:id==CBorders?Action::Borders:Action::Clipboard);return S_OK;}
        if((id>=CLanguage&&id<=CLogging)||id==CBasicCleanup){if(n<2)return DISP_E_BADPARAMCOUNT;change(id,a,n);return S_OK;}
        if(id==CImportTheme){importTheme();return S_OK;}if(id==CHelp){help();return S_OK;}if(id==CAbout){about();return S_OK;}
        if(id==CStatus){showStatus();return S_OK;}
        if(id==CAppearanceSettings||id==CLayoutSettings){settings(id==CAppearanceSettings);return S_OK;}
        if(id==COpenLog){openLog();return S_OK;}if(id==CClearLog){clearLog();return S_OK;}
        if(!result)return E_POINTER;wchar_t text[64];const wchar_t*value=nullptr;bool boolean=false;LONG integer=0;
        switch(id){
            case GLanguage:value=languages.display(opts.language);break;case GTheme:value=same(opts.theme,L"__custom__")?L"已导入的主题":themes.display(opts.theme);break;
            case GFontFamily:value=opts.font;break;case GFontSize:decimalText(text,opts.fontSize);value=text;break;
            case GSpacing:decimalText(text,opts.spacing,opts.spacingPoints?L" pt":L" 倍");value=text;break;
            case GBackground:integer=opts.background;break;
            case GBackgroundColor:{const wchar_t*hex=L"0123456789ABCDEF";text[0]='#';for(int i=0;i<6;++i)text[i+1]=hex[(opts.backgroundColor>>(20-i*4))&15];text[7]=0;value=text;break;}
            case GNumbers:boolean=true;integer=opts.lineNumbers;break;case GFirstLine:number(text,opts.firstLine);value=text;break;
            case GTableRead:integer=opts.tableRead;break;case GMerge:boolean=true;integer=opts.merge;break;case GReplace:boolean=true;integer=opts.replace;break;case GThemeStyles:boolean=true;integer=opts.themeStyles;break;
            case GOutputMode:integer=opts.outputMode;break;
            case GWindowFrame:boolean=true;integer=opts.windowFrame;break;
            case GWindowButtons:boolean=true;integer=opts.windowButtons;break;
            case GWindowShadow:boolean=true;integer=opts.windowShadow;break;
            case GWindowPadding:decimalText(text,opts.windowPadding);value=text;break;
            case GWindowRadius:decimalText(text,opts.windowRadius);value=text;break;
            case GWindowFill:integer=opts.windowFill;break;
            case GWindowColor:{const wchar_t*hex=L"0123456789ABCDEF";text[0]='#';for(int i=0;i<6;++i)text[i+1]=hex[(opts.windowColor>>(20-i*4))&15];text[7]=0;value=text;break;}
            case GBorderVertical:boolean=true;integer=opts.borderVertical;break;
            case GBorderHorizontal:boolean=true;integer=opts.borderHorizontal;break;
            case GBorderOuter:boolean=true;integer=opts.borderOuter;break;
            case GLogging:boolean=true;integer=opts.logging;break;
            case GWindowEnabled:boolean=true;integer=!busy&&opts.windowFrame;break;
            case GBackgroundEnabled:boolean=true;integer=!busy&&!opts.windowFrame;break;
            case GBorderEnabled:boolean=true;integer=!busy&&opts.outputMode==0;break;
            case GWindowLabel:value=L"模拟窗口";break;
            case GNumbersEnabled:boolean=true;integer=!busy&&opts.lineNumbers;break;
            case GBasicCleanup:boolean=true;integer=opts.basicCleanup;break;
            case GLanguageCount:integer=languages.count();break;case GLanguageLabel:if(n<2)return DISP_E_BADPARAMCOUNT;value=languages.label(asLong(&a[0]));break;
            case GThemeCount:integer=themes.count()+(opts.themePath[0]?1:0);break;case GThemeLabel:if(n<2)return DISP_E_BADPARAMCOUNT;value=asLong(&a[0])==themes.count()?L"已导入的主题":themes.label(asLong(&a[0]));break;
            case GEnabled:boolean=true;integer=!busy;break;case GStatus:value=status;break;default:return DISP_E_MEMBERNOTFOUND;
        }
        if(value){result->vt=VT_BSTR;result->bstrVal=SysAllocString(value);return result->bstrVal?S_OK:E_OUTOFMEMORY;}
        result->vt=boolean?VT_BOOL:VT_I4;if(boolean)result->boolVal=integer?-1:0;else result->lVal=integer;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnConnection(IDispatch*application,LONG,IDispatch*,SAFEARRAY**)override{
        app.set(application);read();languages.load(L"engine\\languages.tsv",false);themes.load(L"engine\\themes.tsv",true);logEvent(sizeof(void*)==8?L"CodeInk 1.0.1 connected (x64)":L"CodeInk 1.0.1 connected (x86)");return app?S_OK:E_POINTER;
    }
    HRESULT STDMETHODCALLTYPE OnDisconnection(LONG,SAFEARRAY**)override{app.take(nullptr);ribbon.take(nullptr);logEvent(L"Disconnected");return S_OK;}
    HRESULT STDMETHODCALLTYPE OnAddInsUpdate(SAFEARRAY**)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnStartupComplete(SAFEARRAY**)override{return S_OK;}HRESULT STDMETHODCALLTYPE OnBeginShutdown(SAFEARRAY**)override{app.take(nullptr);ribbon.take(nullptr);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCustomUI(BSTR,BSTR*xml)override{if(!xml)return E_POINTER;*xml=SysAllocString(RIBBON_XML);return *xml?S_OK:E_OUTOFMEMORY;}
private:
    void invalidate(){if(ribbon)invoke(ribbon,L"Invalidate",DISPATCH_METHOD,0,nullptr);}
    void read(){
        DWORD old=readNumber(CONFIG_KEY,L"Language",0);readRegText(CONFIG_KEY,L"LanguageId",opts.language,128,old==1?L"python":old==2?L"arduino":L"cpp");
        readRegText(CONFIG_KEY,L"ThemeId",opts.theme,128,L"github-light");readRegText(CONFIG_KEY,L"ThemeFile",opts.themePath,2048,L"");readRegText(CONFIG_KEY,L"FontFamily",opts.font,64,L"Consolas");
        opts.fontSize=bound(readNumber(CONFIG_KEY,L"FontSize100",1800),600,9600)/100.0f;opts.spacingPoints=readNumber(CONFIG_KEY,L"SpacingPoints",0)!=0;opts.spacing=bound(readNumber(CONFIG_KEY,L"Spacing100",120),opts.spacingPoints?600:80,opts.spacingPoints?20000:300)/100.0f;
        opts.background=bound(readNumber(CONFIG_KEY,L"BackgroundMode",0),0,2);opts.backgroundColor=readNumber(CONFIG_KEY,L"BackgroundColor",0xFFFFFF)&0xFFFFFF;opts.tableRead=bound(readNumber(CONFIG_KEY,L"TableRead",0),0,2);
        opts.lineNumbers=readNumber(CONFIG_KEY,L"LineNumbers",0)!=0;opts.merge=readNumber(CONFIG_KEY,L"Merge",0)!=0;opts.replace=readNumber(CONFIG_KEY,L"Replace",0)!=0;opts.themeStyles=readNumber(CONFIG_KEY,L"ThemeStyles",0)!=0;opts.firstLine=bound(readNumber(CONFIG_KEY,L"FirstLine",1),1,999999);
        opts.basicCleanup=readNumber(CONFIG_KEY,L"BasicCleanup",1)!=0;
        opts.outputMode=bound(readNumber(CONFIG_KEY,L"OutputMode",0),0,1);
        opts.windowFrame=readNumber(CONFIG_KEY,L"WindowFrame",0)!=0;
        opts.windowButtons=readNumber(CONFIG_KEY,L"WindowButtons",1)!=0;
        opts.windowShadow=readNumber(CONFIG_KEY,L"WindowShadow",1)!=0;
        opts.windowPadding=bound(readNumber(CONFIG_KEY,L"WindowPadding100",1400),0,6000)/100.0f;
        opts.windowRadius=bound(readNumber(CONFIG_KEY,L"WindowRadius100",800),0,4000)/100.0f;
        opts.windowFill=bound(readNumber(CONFIG_KEY,L"WindowFill",0),0,1);
        opts.windowColor=readNumber(CONFIG_KEY,L"WindowColor",0x282C34)&0xFFFFFF;
        opts.borderVertical=readNumber(CONFIG_KEY,L"BorderVertical",1)!=0;
        opts.borderHorizontal=readNumber(CONFIG_KEY,L"BorderHorizontal",0)!=0;
        opts.borderOuter=readNumber(CONFIG_KEY,L"BorderOuter",0)!=0;
        opts.logging=diagnosticLogging();
    }
    void save(){
        regText(CONFIG_KEY,L"LanguageId",opts.language);regText(CONFIG_KEY,L"ThemeId",opts.theme);regText(CONFIG_KEY,L"ThemeFile",opts.themePath);regText(CONFIG_KEY,L"FontFamily",opts.font);
        regNumber(CONFIG_KEY,L"FontSize100",(DWORD)(opts.fontSize*100+0.5f));regNumber(CONFIG_KEY,L"Spacing100",(DWORD)(opts.spacing*100+0.5f));regNumber(CONFIG_KEY,L"SpacingPoints",opts.spacingPoints);
        regNumber(CONFIG_KEY,L"BackgroundMode",opts.background);regNumber(CONFIG_KEY,L"BackgroundColor",opts.backgroundColor);regNumber(CONFIG_KEY,L"TableRead",opts.tableRead);regNumber(CONFIG_KEY,L"LineNumbers",opts.lineNumbers);regNumber(CONFIG_KEY,L"Merge",opts.merge);regNumber(CONFIG_KEY,L"Replace",opts.replace);regNumber(CONFIG_KEY,L"ThemeStyles",opts.themeStyles);regNumber(CONFIG_KEY,L"FirstLine",opts.firstLine);
        regNumber(CONFIG_KEY,L"BasicCleanup",opts.basicCleanup);
        regNumber(CONFIG_KEY,L"OutputMode",opts.outputMode);regNumber(CONFIG_KEY,L"WindowFrame",opts.windowFrame);
        regNumber(CONFIG_KEY,L"WindowButtons",opts.windowButtons);regNumber(CONFIG_KEY,L"WindowShadow",opts.windowShadow);
        regNumber(CONFIG_KEY,L"WindowPadding100",(DWORD)(opts.windowPadding*100+0.5f));regNumber(CONFIG_KEY,L"WindowRadius100",(DWORD)(opts.windowRadius*100+0.5f));
        regNumber(CONFIG_KEY,L"WindowFill",opts.windowFill);regNumber(CONFIG_KEY,L"WindowColor",opts.windowColor);
        regNumber(CONFIG_KEY,L"BorderVertical",opts.borderVertical);regNumber(CONFIG_KEY,L"BorderHorizontal",opts.borderHorizontal);regNumber(CONFIG_KEY,L"BorderOuter",opts.borderOuter);
        regNumber(CONFIG_KEY,L"DiagnosticLogging",opts.logging);invalidate();
    }
    void change(int id,VARIANT*a,UINT n){
        if(busy)return;const wchar_t*t=asText(&a[0]);bool ok=true;float v=0;int end=0;
        switch(id){
            case CLanguage:ok=t&&slen(t)<256;if(ok){const wchar_t*resolved=languages.resolve(t);ok=slen(resolved)<128;if(ok)copystr(opts.language,128,resolved);}break;
            case CTheme:ok=t&&slen(t)<256;if(ok){const wchar_t*resolved=same(t,L"已导入的主题")?L"__custom__":themes.resolve(t);ok=slen(resolved)<128;if(ok)copystr(opts.theme,128,resolved);}break;
            case CFontFamily:ok=t&&(samei(t,L"Consolas")||samei(t,L"Cascadia Mono")||samei(t,L"Courier New"));if(ok)copystr(opts.font,64,t);break;
            case CFontSize:ok=codeink::parseDecimal((const codeink::Ch*)t,v,end)&&(!t[end]||samei(t+end,L"pt")||same(t+end,L"磅"))&&v>=6&&v<=96;if(ok)opts.fontSize=v;break;
            case CSpacing:{ok=codeink::parseDecimal((const codeink::Ch*)t,v,end);bool points=ok&&(samei(t+end,L"pt")||same(t+end,L"磅"));ok=ok&&(points||!t[end]||samei(t+end,L"x")||same(t+end,L"倍"))&&(points?(v>=6&&v<=200):(v>=0.8f&&v<=3));if(ok){opts.spacing=v;opts.spacingPoints=points;}break;}
            case COutputMode:if(n<3)return;opts.outputMode=bound(asLong(&a[0]),0,1);break;
            case CWindowFrame:opts.windowFrame=asLong(&a[0])!=0;break;
            case CWindowButtons:opts.windowButtons=asLong(&a[0])!=0;break;
            case CWindowShadow:opts.windowShadow=asLong(&a[0])!=0;break;
            case CWindowPadding:case CWindowRadius:ok=codeink::parseDecimal((const codeink::Ch*)t,v,end)&&(!t[end]||samei(t+end,L"pt")||same(t+end,L"磅"))&&v>=0&&v<=(id==CWindowPadding?60:40);if(ok){if(id==CWindowPadding)opts.windowPadding=v;else opts.windowRadius=v;}break;
            case CWindowFill:if(n<3)return;opts.windowFill=bound(asLong(&a[0]),0,1);break;
            case CWindowColor:{ok=t&&slen(t)==7&&t[0]=='#';DWORD color=0;for(int i=1;ok&&i<7;++i){wchar_t c=t[i];int d=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;if(d<0)ok=false;else color=(color<<4)|d;}if(ok){opts.windowColor=color;opts.windowFill=1;}break;}
            case CBackground:if(n<3)return;opts.background=bound(asLong(&a[0]),0,2);break;
            case CBackgroundColor:{ok=t&&slen(t)==7&&t[0]=='#';DWORD color=0;for(int i=1;ok&&i<7;++i){wchar_t c=t[i];int d=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;if(d<0)ok=false;else color=(color<<4)|d;}if(ok){opts.backgroundColor=color;opts.background=2;}break;}
            case CBorderVertical:opts.borderVertical=asLong(&a[0])!=0;break;
            case CBorderHorizontal:opts.borderHorizontal=asLong(&a[0])!=0;break;
            case CBorderOuter:opts.borderOuter=asLong(&a[0])!=0;break;
            case CLogging:opts.logging=asLong(&a[0])!=0;break;
            case CBasicCleanup:opts.basicCleanup=asLong(&a[0])!=0;break;
            case CNumbers:opts.lineNumbers=asLong(&a[0])!=0;break;
            case CFirstLine:ok=codeink::parseDecimal((const codeink::Ch*)t,v,end)&&!t[end]&&v>=1&&v<=999999&&v==(int)v;if(ok)opts.firstLine=(int)v;break;
            case CTableRead:if(n<3)return;opts.tableRead=bound(asLong(&a[0]),0,2);break;
            case CMerge:opts.merge=asLong(&a[0])!=0;break;case CReplace:opts.replace=asLong(&a[0])!=0;break;case CThemeStyles:opts.themeStyles=asLong(&a[0])!=0;break;
        }
        if(!ok){
            const wchar_t* hint=L"输入内容无效，请检查后重试。原设置已保留。";
            switch(id){
                case CFontSize:hint=L"字号请输入 6～96 之间的数值，单位为磅。原设置已保留。";break;
                case CSpacing:hint=L"行距请输入 0.8～3 倍，或 6～200 pt，例如 1.2 倍、24 pt。原设置已保留。";break;
                case CFontFamily:hint=L"请选择 Consolas、Cascadia Mono 或 Courier New。原字体已保留。";break;
                case CBackgroundColor:case CWindowColor:hint=L"颜色请输入 # 和六位十六进制数字，例如 #F3F3F6。原颜色已保留。";break;
                case CFirstLine:hint=L"起始行号请输入 1～999999 之间的整数。原设置已保留。";break;
                case CWindowPadding:hint=L"内边距请输入 0～60 pt。原设置已保留。";break;
                case CWindowRadius:hint=L"圆角请输入 0～40 pt。原设置已保留。";break;
            }
            show(GetForegroundWindow(),hint,MB_OK|MB_ICONWARNING);
        }
        save();
        if(id==CLogging&&opts.logging)logEvent(L"Diagnostic logging explicitly enabled");
    }
    void run(Action action){
        if(busy||!app)return;busy=true;invalidate();HRESULT hr=E_FAIL;
        wchar_t*error=(wchar_t*)alloc(1600*2);if(!error){busy=false;invalidate();return;}
        {Progress progress(GetForegroundWindow(),L"CodeInk · 处理代码");hr=executeAction(app,opts,action,status,256,error,1600,progress);}
        if(failed(hr)){wchar_t hex[16];hexcode(hex,(DWORD)hr);catstr(error,1600,L"\r\n\r\n错误码：");catstr(error,1600,hex);show(GetForegroundWindow(),error,MB_OK|MB_ICONWARNING);copystr(status,256,error);logEvent(L"CodeInk action failed",hr);}
        dealloc(error);busy=false;invalidate();
    }
    void importTheme(){
        if(busy)return;wchar_t*file=(wchar_t*)alloc(2048*2);if(!file)return;file[0]=0;
        OPENFILENAMEW ofn={};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=GetForegroundWindow();ofn.lpstrFilter=L"VS Code 主题 (*.json;*.jsonc)\0*.json;*.jsonc\0\0";ofn.lpstrFile=file;ofn.nMaxFile=2048;ofn.lpstrTitle=L"导入 VS Code TextMate 主题";ofn.Flags=0x00081808;
        if(!GetOpenFileNameW(&ofn)){dealloc(file);return;}
        Options*candidate=(Options*)alloc(sizeof(Options));if(!candidate){dealloc(file);return;}new(candidate)Options(opts);copystr(candidate->theme,128,L"__custom__");copystr(candidate->themePath,2048,file);dealloc(file);
        busy=true;invalidate();HighlightResult result;BSTR sample=SysAllocString(L"int value = 42; // theme test");HRESULT hr=E_OUTOFMEMORY;
        if(sample){Progress progress(GetForegroundWindow(),L"CodeInk · 验证主题");hr=invokeEngine(*candidate,&sample,1,result,&progress);SysFreeString(sample);}
        if(!failed(hr)){opts=*candidate;copystr(status,256,L"主题已导入。点击“高亮排版”或“生成代码块”应用。");save();}else show(GetForegroundWindow(),result.error[0]?result.error:L"主题导入失败，原设置未改变。",MB_OK|MB_ICONWARNING);
        candidate->~Options();dealloc(candidate);busy=false;invalidate();
    }
    void help(){wchar_t*storage=(wchar_t*)alloc(2300*2);if(storage){wchar_t*dir=storage,*file=storage+1024;if(installDir(dir,1024)&&path(file,1276,dir,L"使用说明.html")){HINSTANCE r=ShellExecuteW(GetForegroundWindow(),L"open",file,nullptr,nullptr,1);dealloc(storage);if((LONG_PTR)r>32)return;}else dealloc(storage);}show(GetForegroundWindow(),L"未能打开使用说明。\r\n\r\n选中代码，选择语言和主题，再点击“高亮排版”或“生成代码块”。\r\n剪贴板内容可直接“粘贴为代码”。\r\n更新已有代码块时，勾选“替换原对象”后生成。\r\n\r\n可重新运行安装程序恢复帮助文件。");}
    void settings(bool appearance){
        if(busy)return;busy=true;invalidate();
        if(editSettings(GetForegroundWindow(),gModule,opts,appearance))save();
        busy=false;invalidate();
    }
    void openLog(){
        if(busy)return;wchar_t file[1200];
        if(!installDir(file,1200)||!catstr(file,1200,L"\\CodeInk.log")){
            show(GetForegroundWindow(),L"无法定位日志目录，请重新打开 PowerPoint 后重试。",MB_OK|MB_ICONWARNING);return;
        }
        if(GetFileAttributesW(file)!=0xFFFFFFFF){
            if((LONG_PTR)ShellExecuteW(GetForegroundWindow(),L"open",file,nullptr,nullptr,1)>32)return;
            show(GetForegroundWindow(),L"日志文件存在，但无法打开。请检查是否有可用的文本查看程序。",MB_OK|MB_ICONWARNING);return;
        }
        show(GetForegroundWindow(),opts.logging?L"没有可打开的日志。请重现问题后再查看。":L"没有可打开的日志。请在“帮助 → 问题排查”中开启“记录诊断日志”，再重现问题。");
    }
    void clearLog(){
        if(busy)return;
        if(MessageBoxW(GetForegroundWindow(),L"删除既有的 CodeInk 插件日志和引擎错误日志？\r\n不会修改课件、主题或当前日志开关。",L"清理 CodeInk 日志",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
        wchar_t file[1200];bool ok=true;
        const wchar_t*names[]={L"CodeInk.log",L"engine\\engine-error.txt"};
        for(int i=0;i<2&&ok;++i){ok=installDir(file,1200)&&catstr(file,1200,L"\\")&&catstr(file,1200,names[i]);if(ok&&!DeleteFileW(file)){DWORD e=GetLastError();ok=e==2||e==3;}}
        show(GetForegroundWindow(),ok?L"插件日志已清理。\r\n日志开关保持不变。":L"未能清理全部插件日志，请检查文件是否正被占用。",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONWARNING));
    }
    void showStatus(){
        wchar_t*text=(wchar_t*)alloc(2400*2);if(!text)return;
        copystr(text,2400,L"最近操作\r\n");catstr(text,2400,status);
        catstr(text,2400,L"\r\n\r\n语言与主题\r\n");
        if(languages.hasInstalledCatalog()&&themes.hasInstalledCatalog()){
            wchar_t n[24];number(n,languages.count());catstr(text,2400,L"目录已加载：");catstr(text,2400,n);
            number(n,themes.count());catstr(text,2400,L" 种语言，");catstr(text,2400,n);catstr(text,2400,L" 个主题。");
        }else catstr(text,2400,L"当前显示常用语言和主题，完整列表暂不可用。\r\n若找不到需要的选项，或无法完成高亮，请重新运行安装程序修复。");
        catstr(text,2400,L"\r\n\r\n诊断日志：");catstr(text,2400,opts.logging?L"已开启":L"已关闭");
        catstr(text,2400,sizeof(void*)==8?L"\r\n\r\n当前插件：64 位":L"\r\n\r\n当前插件：32 位");
        wchar_t dir[1024];catstr(text,2400,L"\r\n安装目录：");catstr(text,2400,installDir(dir,1024)?dir:L"暂时无法读取");
        MessageBoxW(GetForegroundWindow(),text,L"运行状态 · CodeInk",MB_OK|MB_ICONINFORMATION);dealloc(text);
    }
    void about(){
        MessageBoxW(GetForegroundWindow(),L"CodeInk\r\n让代码在 PowerPoint 中清晰呈现，保持可编辑。\r\n\r\n版本：1.0.1\r\n作者：Iceriny\r\n\r\n代码高亮在本机完成。",L"关于 CodeInk",MB_OK|MB_ICONINFORMATION);
    }

};
class Factory final:public IClassFactory{
    volatile LONG refs;
public:
    Factory():refs(1){InterlockedIncrement(&gObjects);}~Factory(){InterlockedDecrement(&gObjects);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(!guidEq(iid,IID_Unknown)&&!guidEq(iid,IID_ClassFactory))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return InterlockedIncrement(&refs);}
    ULONG STDMETHODCALLTYPE Release()override{LONG n=InterlockedDecrement(&refs);if(!n){this->~Factory();dealloc(this);}return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;void* mem=alloc(sizeof(Addin));if(!mem)return E_OUTOFMEMORY;Addin* p=new(mem) Addin();HRESULT hr=p->QueryInterface(iid,out);p->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock)override{if(lock)InterlockedIncrement(&gLocks);else InterlockedDecrement(&gLocks);return S_OK;}
};
extern "C" BOOL WINAPI DllMain(HMODULE module,DWORD reason,void*){if(reason==1){gModule=module;DisableThreadLibraryCalls(module);}return TRUE;}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out){if(!out)return E_POINTER;*out=nullptr;if(!guidEq(clsid,CLSID_CodeInk))return CLASS_E_CLASSNOTAVAILABLE;void* mem=alloc(sizeof(Factory));if(!mem)return E_OUTOFMEMORY;Factory* f=new(mem) Factory();HRESULT hr=f->QueryInterface(iid,out);f->Release();return hr;}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return !gObjects&&!gLocks?S_OK:S_FALSE;}
extern "C" HRESULT WINAPI DllRegisterServer(){wchar_t file[1024];DWORD n=GetModuleFileNameW(gModule,file,1024);if(!n||n>=1024)return E_FAIL;return registerCom(file,sizeof(void*)==8?KEY_WOW64_64KEY:KEY_WOW64_32KEY)?S_OK:E_FAIL;}
extern "C" HRESULT WINAPI DllUnregisterServer(){return unregisterCom(sizeof(void*)==8?KEY_WOW64_64KEY:KEY_WOW64_32KEY)?S_OK:E_FAIL;}
