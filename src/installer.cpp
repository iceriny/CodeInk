#include "common.h"
#include "progress.h"
#include "payload.generated.h"
#include "bootstrap.generated.h"
// Current-user online bootstrap; runtime/highlighting remain local after setup.
static const wchar_t*UNINSTALL_KEY=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CodeInk";
static wchar_t directory[1024],filename[2400],self[1200],command[30000],message[2600],ps[1200],node[2200],bridge[2200],script[24000];
static DWORD lastCode=0;static bool prior32=false,prior64=false;static bool enginePrior[PAYLOAD_COUNT]={},engineBacked[PAYLOAD_COUNT]={};
static void tell(const wchar_t*t,UINT style=MB_OK|MB_ICONINFORMATION){MessageBoxW(nullptr,t,L"CodeInk 安装程序 · 1.0.0",style);}
static bool supportedSystem(){SYSTEM_INFO si={};GetNativeSystemInfo(&si);return si.wProcessorArchitecture==9;}
static const wchar_t*arguments(){const wchar_t*p=GetCommandLineW();while(*p==' '||*p=='\t')++p;if(*p=='"'){++p;while(*p&&*p!='"')++p;if(*p)++p;}else while(*p&&*p!=' '&&*p!='\t')++p;while(*p==' '||*p=='\t')++p;return p;}
static bool mode(const wchar_t*value){const wchar_t*p=arguments();int n=slen(value);for(int i=0;i<n;++i)if(!p[i]||p[i]!=value[i])return false;return !p[n]||p[n]==' '||p[n]=='\t';}
static bool exists(const wchar_t*p){return GetFileAttributesW(p)!=0xFFFFFFFF;}
static bool mkdir(const wchar_t*p){return CreateDirectoryW(p,nullptr)||GetLastError()==183;}
static bool officeClosed(){HANDLE h=CreateToolhelp32Snapshot(2,0);if(h==INVALID_HANDLE_VALUE)return false;PROCESSENTRY32W e={};e.dwSize=sizeof(e);bool found=false;if(Process32FirstW(h,&e))do{if(samei(e.szExeFile,L"POWERPNT.EXE")){found=true;break;}}while(Process32NextW(h,&e));CloseHandle(h);if(found)tell(L"请先保存课件并彻底退出 PowerPoint，再运行安装或卸载。程序不会强制结束 PowerPoint。",MB_OK|MB_ICONWARNING);return !found;}
static bool payload(bool engineOnly){for(unsigned i=0;i<PAYLOAD_COUNT;++i){const Payload&p=PAYLOADS[i];if(p.engine!=engineOnly)continue;if(!path(filename,2400,directory,p.name)||!writeFile(filename,p.bytes,p.length))return false;}return true;}
static bool psLiteral(wchar_t*out,int cap,const wchar_t*s){if(!catstr(out,cap,L"'"))return false;for(;*s;++s){wchar_t c[]={*s,0};if(!catstr(out,cap,c)||(*s=='\''&&!catstr(out,cap,L"'")))return false;}return catstr(out,cap,L"'");}
static bool engineSnapshot(){
    for(unsigned i=0;i<PAYLOAD_COUNT;++i)if(PAYLOADS[i].engine){
        if(!path(filename,2400,directory,PAYLOADS[i].name))return false;
        enginePrior[i]=exists(filename);if(enginePrior[i]){copystr(message,2600,filename);catstr(message,2600,L".previous");if(!CopyFileW(filename,message,FALSE))return false;}
        engineBacked[i]=true;
    }
    return true;
}
static void restoreEngine(){
    for(unsigned i=0;i<PAYLOAD_COUNT;++i)if(engineBacked[i]){
        path(filename,2400,directory,PAYLOADS[i].name);
        if(enginePrior[i]){copystr(message,2600,filename);catstr(message,2600,L".previous");CopyFileW(message,filename,FALSE);}
        else DeleteFileW(filename);
    }
}
static void discardEngineBackups(){for(unsigned i=0;i<PAYLOAD_COUNT;++i)if(engineBacked[i]&&enginePrior[i]){path(filename,2400,directory,PAYLOADS[i].name);catstr(filename,2400,L".previous");DeleteFileW(filename);}}
static bool manifest(Progress&p){
    command[0]=0;quoteArg(command,30000,node);path(filename,2400,directory,L"engine\\maintenance.mjs");quoteArg(command,30000,filename);quoteArg(command,30000,L"--manifest");quoteArg(command,30000,directory);
    return runChild(node,command,60000,&p,lastCode);
}
static bool deps(Progress&p){
    if(!enginePaths(node,2200,bridge,2200))return false;
    path(filename,2400,directory,L"runtime\\.codeink-verified-runtime.txt");
    if(exists(node)&&exists(filename)){
        command[0]=0;quoteArg(command,30000,node);quoteArg(command,30000,bridge);quoteArg(command,30000,L"--self-test");if(diagnosticLogging())quoteArg(command,30000,L"--diagnostics");
        p.text(L"正在离线检查已安装的高亮依赖…\r\n通过后直接复用现有运行时，不重复下载。");
        if(runChild(node,command,180000,&p,lastCode))return manifest(p);
        if(!p.pump())return false;
    }
    DWORD n=GetEnvironmentVariableW(L"WINDIR",ps,1000);if(!n||n>=1000||!catstr(ps,1200,L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"))return false;
    copystr(script,24000,L"& {\n");catstr(script,24000,BOOTSTRAP_SCRIPT);catstr(script,24000,L"\n} ");if(!psLiteral(script,24000,directory))return false;if(diagnosticLogging())catstr(script,24000,L" -Diagnostics");
    command[0]=0;if(!quoteArg(command,30000,ps)||!quoteArg(command,30000,L"-NoProfile")||!quoteArg(command,30000,L"-NonInteractive")||!quoteArg(command,30000,L"-Command")||!quoteArg(command,30000,script))return false;
    p.text(L"正在下载并校验私有 Node.js 运行时…\r\n本步骤访问 nodejs.org；不会更改系统 PATH 或安全策略。");
    if(!runChild(ps,command,900000,&p,lastCode))return false;
    if(!p.pump())return false;
    command[0]=0;quoteArg(command,30000,node);path(filename,2400,directory,L"runtime\\node-v24.21.0-win-x64\\node_modules\\npm\\bin\\npm-cli.js");quoteArg(command,30000,filename);quoteArg(command,30000,L"install");quoteArg(command,30000,L"--prefix");path(filename,2400,directory,L"engine");quoteArg(command,30000,filename);
    quoteArg(command,30000,L"--ignore-scripts");quoteArg(command,30000,L"--no-audit");quoteArg(command,30000,L"--no-fund");quoteArg(command,30000,L"--omit=dev");quoteArg(command,30000,L"--registry=https://registry.npmjs.org/");quoteArg(command,30000,L"--cache");path(filename,2400,directory,L"cache");quoteArg(command,30000,filename);
    if(!diagnosticLogging()){quoteArg(command,30000,L"--logs-max=0");quoteArg(command,30000,L"--loglevel=silent");quoteArg(command,30000,L"--timing=false");}
    p.text(L"正在安装固定版本的 Shiki 与 JSONC 依赖…\r\n访问 npm 官方仓库；不执行 npm 安装脚本。");
    if(!runChild(node,command,900000,&p,lastCode))return false;
    command[0]=0;quoteArg(command,30000,node);quoteArg(command,30000,bridge);quoteArg(command,30000,L"--self-test");if(diagnosticLogging())quoteArg(command,30000,L"--diagnostics");
    p.text(L"正在执行真实 Shiki 高亮自检并生成语言、主题目录…\r\n这一阶段在本机完成。");
    if(!runChild(node,command,180000,&p,lastCode))return false;
    return manifest(p);
}
static bool uninstallEntry(){path(filename,2400,directory,L"Uninstall.exe");if(!samei(self,filename)&&!CopyFileW(self,filename,FALSE))return false;command[0]=0;quoteArg(command,30000,filename);catstr(command,30000,L" --uninstall");bool ok=regText(UNINSTALL_KEY,L"DisplayName",L"CodeInk · PowerPoint 代码高亮");ok=regText(UNINSTALL_KEY,L"DisplayVersion",VERSION)&&ok;ok=regText(UNINSTALL_KEY,L"Publisher",L"Iceriny")&&ok;ok=regText(UNINSTALL_KEY,L"InstallLocation",directory)&&ok;ok=regText(UNINSTALL_KEY,L"UninstallString",command)&&ok;ok=regText(UNINSTALL_KEY,L"DisplayIcon",filename)&&ok;return regNumber(UNINSTALL_KEY,L"NoModify",1)&&regNumber(UNINSTALL_KEY,L"NoRepair",1)&&ok;}
static void failure(const wchar_t*action,DWORD code){wchar_t hex[16];hexcode(hex,code);copystr(message,2600,action);catstr(message,2600,L"\r\n\r\n错误码：");catstr(message,2600,hex);catstr(message,2600,L"\r\n安装目录：");catstr(message,2600,directory);catstr(message,2600,diagnosticLogging()?L"\r\n\r\n诊断日志已开启：下载问题见 bootstrap-error.txt，npm 安装问题见 cache\\_logs。":L"\r\n\r\n诊断日志已关闭（默认）。请保留本提示及错误码用于排查。");catstr(message,2600,L"\r\n请检查能否连接 nodejs.org 和 registry.npmjs.org，再重新运行安装器。\r\n不要关闭安全防护或降低 Office 安全设置；受组织策略限制时请交由管理员审核。");tell(message,MB_OK|MB_ICONERROR);}
static bool backup(){const wchar_t*names[]={L"CodeInk.x86.dll",L"CodeInk.x64.dll"};for(int i=0;i<2;++i){path(filename,2400,directory,names[i]);bool found=exists(filename);if(i==0)prior32=found;else prior64=found;if(found){copystr(message,2600,filename);catstr(message,2600,L".previous");if(!CopyFileW(filename,message,FALSE))return false;}}return true;}
static void rollback(){const wchar_t*names[]={L"CodeInk.x86.dll",L"CodeInk.x64.dll"};for(int i=0;i<2;++i){bool prior=i?prior64:prior32;DWORD view=i?KEY_WOW64_64KEY:KEY_WOW64_32KEY;path(filename,2400,directory,names[i]);if(prior){copystr(message,2600,filename);catstr(message,2600,L".previous");if(CopyFileW(message,filename,FALSE))registerCom(filename,view);}else{unregisterCom(view);DeleteFileW(filename);}}}
static void install(){
    if(!officeClosed())return;copystr(message,2600,L"安装 CodeInk 1.0.0\r\n\r\n为 PowerPoint 添加可编辑的代码高亮功能。\r\n\r\n安装到当前用户，自动配置所需依赖。首次安装或修复依赖需要联网；日常使用在本机完成。\r\n\r\n安装目录：\r\n");catstr(message,2600,directory);catstr(message,2600,L"\r\n\r\n是否继续？");if(MessageBoxW(nullptr,message,L"安装 CodeInk",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
    if(!mkdir(directory)){failure(L"无法建立安装目录。",GetLastError());return;}path(filename,2400,directory,L"engine");if(!mkdir(filename)||!engineSnapshot()){failure(L"无法备份本地高亮适配层。",GetLastError());return;}if(!payload(true)){lastCode=GetLastError();restoreEngine();failure(L"无法写入本地高亮适配层，已尝试恢复旧文件。",lastCode);return;}
    bool ok=false;{Progress p(nullptr,L"CodeInk · 安装本地依赖");ok=deps(p);}if(!ok){restoreEngine();failure(L"依赖安装或 Shiki 自检未完成。尚未替换插件 DLL，并已尝试恢复旧高亮适配层。",lastCode);return;}
    // Recheck: the user may have opened PowerPoint while downloading dependencies.
    if(!officeClosed()){restoreEngine();return;}if(!backup()){restoreEngine();failure(L"无法备份已有插件，升级已停止。",GetLastError());return;}
    if(!payload(false)){lastCode=GetLastError();rollback();restoreEngine();failure(L"插件文件写入失败，已尝试恢复旧 DLL。",lastCode);return;}
    path(filename,2400,directory,L"CodeInk.x86.dll");ok=registerCom(filename,KEY_WOW64_32KEY);path(filename,2400,directory,L"CodeInk.x64.dll");ok=registerCom(filename,KEY_WOW64_64KEY)&&ok;
    if(ok){Progress p(nullptr,L"CodeInk · COM 加载自检");const wchar_t*checks[]={L"CodeInk.Check32.exe",L"CodeInk.Check64.exe"};for(int i=0;i<2&&ok;++i){path(filename,2400,directory,checks[i]);command[0]=0;quoteArg(command,30000,filename);ok=runChild(filename,command,90000,&p,lastCode);}}
    if(!ok){rollback();restoreEngine();failure(L"COM 注册或加载自检未通过，已尝试恢复旧插件 DLL。",lastCode);return;}
    if(!uninstallEntry()){failure(L"插件已写入，但卸载信息写入失败。请重新运行安装器修复。",GetLastError());return;}
    path(filename,2400,directory,L"CodeInk.x86.dll.previous");DeleteFileW(filename);path(filename,2400,directory,L"CodeInk.x64.dll.previous");DeleteFileW(filename);
    path(filename,2400,directory,L"bootstrap-error.txt");DeleteFileW(filename);discardEngineBackups();logEvent(L"CodeInk 1.0.0 installed; Shiki self-test and COM checks passed");
    tell(L"CodeInk 已安装。\r\n\r\n重新打开 PowerPoint，在“代码高亮”选项卡中选择语言和主题，再点击“高亮排版”。\r\n\r\n更多用法见“帮助 → 使用说明”。");
}
static bool safeRelative(const wchar_t*p){if(!p||!*p||*p=='\\'||*p=='/')return false;for(const wchar_t*q=p;*q;++q)if(*q==':'||*q=='/'||*q=='\t'||*q=='\n'||(*q=='.'&&q[1]=='.'))return false;const wchar_t*allowed[]={L"runtime",L"engine\\node_modules",L"cache",L"_downloads"};for(int i=0;i<4;++i){int n=slen(allowed[i]),j=0;while(j<n&&p[j]==allowed[i][j])++j;if(j==n&&(!p[j]||p[j]=='\\'))return true;}return false;}
static bool removeManaged(){path(filename,2400,directory,L"managed-files.tsv");FileBytes data;if(!data.load(filename,8*1024*1024))return true;if(data.length%2)return false;wchar_t*txt=(wchar_t*)data.bytes;int n=data.length/2,i=txt[0]==0xFEFF?1:0;bool ok=true;while(i<n){int end=i;while(end<n&&txt[end]!='\r'&&txt[end]!='\n')++end;wchar_t kind=txt[i];bool valid=end-i>2&&txt[i+1]=='\t';txt[end]=0;if(valid&&safeRelative(txt+i+2)&&path(filename,2400,directory,txt+i+2)){if(kind=='F'){if(!DeleteFileW(filename)&&GetLastError()!=2&&GetLastError()!=3)ok=false;}else if(kind=='D')RemoveDirectoryW(filename);}i=end+1;while(i<n&&(txt[i]=='\r'||txt[i]=='\n'))++i;}return ok;}
static bool removeKnown(){bool ok=removeManaged();for(unsigned i=0;i<PAYLOAD_COUNT;++i)if(PAYLOADS[i].engine&&path(filename,2400,directory,PAYLOADS[i].name)){catstr(filename,2400,L".previous");if(!DeleteFileW(filename)&&GetLastError()!=2&&GetLastError()!=3)ok=false;}for(unsigned i=0;i<PAYLOAD_COUNT;++i)if(path(filename,2400,directory,PAYLOADS[i].name)&&!DeleteFileW(filename)&&GetLastError()!=2&&GetLastError()!=3)ok=false;const wchar_t*extra[]={L"CodeInk.log",L"Uninstall.exe",L"managed-files.tsv",L"bootstrap-error.txt",L"CodeInk.x86.dll.previous",L"CodeInk.x64.dll.previous",L"engine\\languages.tsv",L"engine\\themes.tsv",L"engine\\catalog-summary.json",L"engine\\self-test-passed.txt",L"engine\\engine-error.txt",L"engine\\package-lock.json"};for(unsigned i=0;i<sizeof(extra)/sizeof(extra[0]);++i){path(filename,2400,directory,extra[i]);if(!DeleteFileW(filename)&&GetLastError()!=2&&GetLastError()!=3)ok=false;}path(filename,2400,directory,L"engine");RemoveDirectoryW(filename);RemoveDirectoryW(directory);return ok;}
static DWORD parentPid(){const wchar_t*p=arguments();while(*p&&*p!=' '&&*p!='\t')++p;while(*p==' '||*p=='\t')++p;DWORD id=0;while(*p>='0'&&*p<='9'){id=id*10+(*p-'0');++p;}return id;}
static void cleanup(){DWORD id=parentPid();if(id){HANDLE h=OpenProcess(0x00100000,FALSE,id);if(h){WaitForSingleObject(h,15000);CloseHandle(h);}}if(!officeClosed())return;bool ok=unregisterCom(KEY_WOW64_32KEY);ok=unregisterCom(KEY_WOW64_64KEY)&&ok;ok=deleteKey(UNINSTALL_KEY)&&ok;ok=deleteKey(CONFIG_KEY)&&ok;Sleep(250);ok=removeKnown()&&ok;if(ok)tell(L"CodeInk 已卸载。\r\n课件、原生代码表格和外部导入的主题文件没有被修改。安装后自行添加的未知文件将保留。\r\n临时目录中的卸载器副本可随 Windows 临时文件一起清理。");else failure(L"部分文件或注册项未能清理，请关闭 PowerPoint 后重试。",GetLastError());}
static void uninstall(){if(!officeClosed())return;if(MessageBoxW(nullptr,L"卸载 CodeInk、私有运行时、安装依赖与本地偏好？\r\n\r\n已保存的课件和外部主题文件不会被修改。",L"卸载 CodeInk",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;DWORD n=GetTempPathW(2000,filename);if(!n||n>=2000)return;wchar_t id[16];number(id,(int)GetCurrentProcessId());catstr(filename,2400,L"CodeInk-Uninstall-");catstr(filename,2400,id);catstr(filename,2400,L".exe");if(!CopyFileW(self,filename,TRUE)){failure(L"无法建立临时卸载器。",GetLastError());return;}command[0]=0;quoteArg(command,30000,filename);catstr(command,30000,L" --cleanup ");catstr(command,30000,id);STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};if(!CreateProcessW(filename,command,nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){failure(L"无法启动卸载清理进程。",GetLastError());return;}CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
extern "C" void WINAPI EntryPoint(){if(!supportedSystem()){tell(L"此安装包面向 Intel / AMD 的 64 位 Windows，支持其中的 32 / 64 位桌面版 PowerPoint；不支持 ARM64 原生 Office、32 位 Windows、Mac 或网页版。",MB_OK|MB_ICONWARNING);ExitProcess(1);}if(!installDir(directory,1024)){tell(L"无法获取当前用户的 LOCALAPPDATA 目录。",MB_OK|MB_ICONERROR);ExitProcess(1);}DWORD n=GetModuleFileNameW(nullptr,self,1200);if(!n||n>=1200)ExitProcess(1);if(mode(L"--cleanup"))cleanup();else if(mode(L"--uninstall"))uninstall();else{const wchar_t*name=self;for(const wchar_t*p=self;*p;++p)if(*p=='\\'||*p=='/')name=p+1;if(samei(name,L"Uninstall.exe"))uninstall();else install();}ExitProcess(0);}
