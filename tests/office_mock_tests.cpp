// Executes the REAL renderer against a deliberately small IDispatch simulator.
// This checks object creation / offsets / grouping / cleanup, not Office visuals.
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include "../src/render.cpp"
using U=std::u16string;
static U u(const wchar_t*s){return U((const char16_t*)s,(const char16_t*)s+slen(s));}
static const wchar_t* w(const U&s){return (const wchar_t*)s.c_str();}
static U text(const VARIANT&v){return v.vt==VT_BSTR?U((char16_t*)v.bstrVal,SysStringLen(v.bstrVal)):U();}
extern "C" HANDLE GetProcessHeap(){return (HANDLE)1;}
extern "C" void* HeapAlloc(HANDLE,DWORD,SIZE_T n){return std::calloc(1,n);}
extern "C" BOOL HeapFree(HANDLE,DWORD,void*p){std::free(p);return 1;}
extern "C" BSTR SysAllocStringLen(LPCWSTR src,UINT n){auto*p=(UINT*)std::malloc(4+(n+1)*2);assert(p);*p=n;BSTR b=(BSTR)(p+1);if(src)std::memcpy(b,src,n*2);else std::memset(b,0,n*2);b[n]=0;return b;}
extern "C" BSTR SysAllocString(LPCWSTR s){return SysAllocStringLen(s,slen(s));}
extern "C" UINT SysStringLen(BSTR s){return s?((UINT*)s)[-1]:0;}
extern "C" void SysFreeString(BSTR s){if(s)std::free((UINT*)s-1);}
struct SAFEARRAY {std::vector<VARIANT> values;};
extern "C" HRESULT VariantClear(VARIANT*v){
    if(v->vt==VT_BSTR)SysFreeString(v->bstrVal);else if(v->vt==VT_DISPATCH&&v->pdispVal)v->pdispVal->Release();
    else if(v->vt==(0x2000|VT_VARIANT)&&v->parray){for(auto&e:v->parray->values)VariantClear(&e);delete v->parray;}
    std::memset(v,0,sizeof(*v));return S_OK;
}
extern "C" HRESULT VariantCopy(VARIANT*dst,const VARIANT*src){VariantClear(dst);*dst=*src;
    if(src->vt==VT_BSTR)dst->bstrVal=SysAllocStringLen(src->bstrVal,SysStringLen(src->bstrVal));
    if(src->vt==VT_DISPATCH&&src->pdispVal)src->pdispVal->AddRef();
    if(src->vt==(0x2000|VT_VARIANT)){dst->parray=new SAFEARRAY;dst->parray->values.resize(src->parray->values.size());for(size_t i=0;i<dst->parray->values.size();++i)VariantCopy(&dst->parray->values[i],&src->parray->values[i]);}
    return S_OK;
}
extern "C" SAFEARRAY* SafeArrayCreateVector(WORD type,LONG lower,ULONG count){assert(type==VT_VARIANT&&lower==0);auto*a=new SAFEARRAY;a->values.resize(count);return a;}
extern "C" HRESULT SafeArrayPutElement(SAFEARRAY*a,LONG*i,void*v){assert(*i>=0&&(size_t)*i<a->values.size());return VariantCopy(&a->values[*i],(VARIANT*)v);}
static int pumps=0,cancelAfter=-1;
Progress::Progress(HWND,const wchar_t*):window(nullptr),label(nullptr),owner(nullptr),stopped(false),ownDisable(false){}
Progress::~Progress(){}
void Progress::text(const wchar_t*){}
bool Progress::pump(){return !stopped&&(cancelAfter<0||pumps++<cancelAfter);}
static U clipboardText=u"clipboard code";
extern "C" HWND GetForegroundWindow(){return nullptr;}
extern "C" BOOL OpenClipboard(HWND){return TRUE;}
extern "C" BOOL CloseClipboard(){return TRUE;}
extern "C" HANDLE GetClipboardData(UINT){return (HANDLE)1;}
extern "C" void* GlobalLock(HANDLE){return (void*)clipboardText.c_str();}
extern "C" BOOL GlobalUnlock(HANDLE){return TRUE;}
extern "C" SIZE_T GlobalSize(HANDLE){return (clipboardText.size()+1)*2;}
extern "C" LONG RegOpenKeyExW(HKEY,LPCWSTR,DWORD,DWORD,HKEY*){return 2;}
extern "C" LONG RegQueryValueExW(HKEY,LPCWSTR,DWORD*,DWORD*,BYTE*,DWORD*){return 2;}
extern "C" LONG RegCloseKey(HKEY){return 0;}
extern "C" DWORD GetEnvironmentVariableW(LPCWSTR,LPWSTR,DWORD){return 0;}
extern "C" HANDLE CreateFileW(LPCWSTR,DWORD,DWORD,void*,DWORD,DWORD,HANDLE){return INVALID_HANDLE_VALUE;}
extern "C" BOOL WriteFile(HANDLE,const void*,DWORD,DWORD*,void*){return FALSE;}
extern "C" BOOL CloseHandle(HANDLE){return TRUE;}
extern "C" int WideCharToMultiByte(UINT,DWORD,LPCWSTR,int,char*,int,const char*,BOOL*){return 0;}
static bool engineFails=false;
HRESULT invokeEngine(const Options&,const BSTR* texts,int n,HighlightResult&result,Progress*){
    if(engineFails){copystr(result.error,1100,L"engine failed");return E_FAIL;}
    result.fg=0x222222;result.bg=0xFFFFFF;result.gutter=0x777777;
    result.items.make(n);for(int i=0;i<n;++i){result.items[i].length=SysStringLen(texts[i]);result.items[i].tokens.make(1);result.items[i].tokens[0]={0,(DWORD)SysStringLen(texts[i]),0xABCDEF,0};}return S_OK;
}
struct Node;
struct World {
    std::vector<std::unique_ptr<Node>> nodes;int nextId=1;bool failGroup=false,ignoreBorderVisible=true;float minRow=0;
    Node* make(const char*kind,Node*owner=nullptr);
    Node* shape(int type,float x,float y,float width,float height);
    std::vector<Node*> top()const;
};
struct Node final:IDispatch {
    World*world;std::string kind;Node*owner=nullptr,*parentGroup=nullptr;int id=0,type=0,rs=0,rl=-1,rows=0,cols=0;
    U code,name;std::vector<Node*> members;std::map<U,Value> props;std::map<U,Node*> links;
    std::vector<DWORD> ink;std::vector<float> widths,heights;bool dead=false;
    HRESULT QueryInterface(REFIID,void**out)override{*out=this;return S_OK;}ULONG AddRef()override{return 1;}ULONG Release()override{return 1;}
    HRESULT GetTypeInfoCount(UINT*n)override{*n=0;return S_OK;}HRESULT GetTypeInfo(UINT,LCID,ITypeInfo**)override{return E_NOTIMPL;}
    HRESULT GetIDsOfNames(REFIID,LPWSTR*names,UINT n,LCID,DISPID*ids)override;
    HRESULT Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*)override;
    Node* link(const U&name,const char*kind){auto it=links.find(name);if(it!=links.end())return it->second;return links[name]=world->make(kind,this);}
    float value(const U&key,float fallback=0){auto it=props.find(key);return it==props.end()?fallback:it->second.realValue(fallback);}
    Node* textShape(){Node*p=this;while(p&&p->kind!="shape")p=p->owner;return p;}
    void geometry(float x,float y,float w,float h){props[u"Left"]=Value::real(x);props[u"Top"]=Value::real(y);props[u"Width"]=Value::real(w);props[u"Height"]=Value::real(h);}
};
Node*World::make(const char*k,Node*owner){auto n=std::make_unique<Node>();n->world=this;n->kind=k;n->owner=owner;auto*p=n.get();nodes.push_back(std::move(n));return p;}
Node*World::shape(int type,float x,float y,float width,float height){Node*n=make("shape");n->id=nextId++;n->type=type;n->name=u"Shape ";for(char c:std::to_string(n->id))n->name+=(char16_t)c;n->geometry(x,y,width,height);return n;}
std::vector<Node*>World::top()const{std::vector<Node*>r;for(auto&n:nodes)if(n->kind=="shape"&&!n->dead&&!n->parentGroup&&!n->owner)r.push_back(n.get());return r;}
static std::vector<U> methodNames;
HRESULT Node::GetIDsOfNames(REFIID,LPWSTR*names,UINT n,LCID,DISPID*ids){for(UINT i=0;i<n;++i){U name=u(names[i]);auto it=std::find(methodNames.begin(),methodNames.end(),name);if(it==methodNames.end()){methodNames.push_back(name);ids[i]=methodNames.size();}else ids[i]=(DISPID)(it-methodNames.begin()+1);}return S_OK;}
static HRESULT val(VARIANT*r,const Value&v){return VariantCopy(r,&v.v);}
static HRESULT obj(VARIANT*r,Node*n){r->vt=VT_DISPATCH;r->pdispVal=n;return S_OK;}
HRESULT Node::Invoke(DISPID id,REFIID,LCID,WORD flags,DISPPARAMS*p,VARIANT*r,EXCEPINFO*,UINT*){
    const U&m=methodNames[id-1];int argc=p?p->cArgs:0;auto arg=[&](int i)->VARIANT&{assert(i<argc);return p->rgvarg[argc-1-i];};
    auto integer=[&](int i){return asLong(&arg(i));};auto real=[&](int i){auto&v=arg(i);return v.vt==VT_R4?v.fltVal:(float)v.lVal;};
    if(flags&DISPATCH_PROPERTYPUT){
        if(kind=="border"&&m==u"Visible"&&integer(0)==0&&world->ignoreBorderVisible){props[m]=Value::integer(-1);return S_OK;}
        if(m==u"Text"&&kind=="range"){owner->owner->code=text(arg(0));owner->owner->ink.assign(owner->owner->code.size(),0);return S_OK;}
        if(m==u"RGB"&&kind=="fontcolor"){
            Node*range=owner->owner;Node*s=range->textShape();int len=range->rl<0?(int)s->code.size():range->rl;
            for(int i=range->rs;i<range->rs+len;++i){assert(i>=0&&(size_t)i<s->ink.size());s->ink[i]=integer(0);}return S_OK;
        }
        if(m==u"Width"&&kind=="column"){owner->owner->widths[rs]=real(0);return S_OK;}
        if(m==u"Height"&&kind=="row"){owner->owner->heights[rs]=std::max(real(0),world->minRow);return S_OK;}
        if(m==u"Item"&&kind=="adjustments"){assert(argc==2&&integer(0)==1);props[u"radius"]=Value::real(real(1));return S_OK;}
        props[m].clear();VariantCopy(&props[m].v,&arg(argc-1));return S_OK;
    }
    if(m==u"Delete"&&kind=="shape"){dead=true;return S_OK;}
    if(m==u"ZOrder"||m==u"Select"||m==u"Solid")return S_OK;
    if(m==u"ParentGroup"){if(!parentGroup)return E_FAIL;return obj(r,parentGroup);}
    if(m==u"Parent"){return owner?obj(r,owner):E_FAIL;}
    if(m==u"Type")return val(r,Value::integer(type));
    if(m==u"Id")return val(r,Value::integer(this->id));
    if(m==u"Name")return val(r,Value::text(w(name)));
    if(m==u"HasTable")return val(r,Value::integer(type==19?-1:0));
    if(m==u"HasTextFrame")return val(r,Value::integer(kind=="shape"&&type!=6&&type!=19?-1:0));
    if(m==u"HasText")return val(r,Value::integer(!textShape()->code.empty()?-1:0));
    if(m==u"TextFrame")return obj(r,link(m,"frame"));
    if(m==u"TextRange")return obj(r,link(m,"range"));
    if(m==u"Font")return obj(r,link(m,"font"));
    if(m==u"Color"&&kind=="font")return obj(r,link(m,"fontcolor"));
    if(m==u"ParagraphFormat")return obj(r,link(m,"paragraph"));
    if(m==u"Bullet")return obj(r,link(m,"bullet"));
    if(m==u"Fill")return obj(r,link(m,"fill"));
    if(m==u"ForeColor")return obj(r,link(m,"color"));
    if(m==u"Line")return obj(r,link(m,"line"));
    if(m==u"Shadow")return obj(r,link(m,"shadow"));
    if(m==u"Adjustments")return obj(r,link(m,"adjustments"));
    if(m==u"GroupItems"){if(type!=6)return E_FAIL;Node*g=link(m,"groupitems");g->members=members;return obj(r,g);}
    if(m==u"Text"&&kind=="range")return val(r,Value::text(w(textShape()->code)));
    if((m==u"BoundWidth"||m==u"BoundHeight")&&kind=="range"){
        float fs=link(u"Font","font")->value(u"Size",18);
        return val(r,Value::real(m==u"BoundWidth"?textShape()->code.size()*fs*0.5f:fs*1.1f));
    }
    if(m==u"Characters"&&kind=="range"){
        Node*q=world->make("range",owner);q->rs=integer(0)-1;q->rl=integer(1);assert(q->rs>=0&&(size_t)(q->rs+q->rl)<=textShape()->code.size());return obj(r,q);
    }
    if(m==u"AddTextbox"||m==u"AddShape"){
        Node*q=world->shape(m==u"AddTextbox"?17:1,real(1),real(2),real(3),real(4));q->props[u"AutoShapeType"]=Value::integer(integer(0));return obj(r,q);
    }
    if(m==u"AddTable"){
        Node*q=world->shape(19,real(2),real(3),real(4),real(5));Node*t=q->link(u"Table","table");t->rows=integer(0);t->cols=integer(1);
        t->widths.assign(t->cols,real(4)/t->cols);t->heights.assign(t->rows,real(5)/t->rows);
        for(int row=0;row<t->rows;++row)for(int col=0;col<t->cols;++col){Node*cell=world->make("cell",t);Node*c=world->shape(17,real(2),real(3)+row*t->heights[row],t->widths[col],t->heights[row]);c->owner=cell;cell->links[u"Shape"]=c;t->members.push_back(cell);}return obj(r,q);
    }
    if(m==u"Table")return obj(r,links.at(m));
    if(m==u"Rows")return obj(r,link(m,"rows"));
    if(m==u"Columns")return obj(r,link(m,"columns"));
    if(m==u"Cell"&&kind=="table")return obj(r,members.at((integer(0)-1)*cols+integer(1)-1));
    if(m==u"Shape"&&kind=="cell")return obj(r,links.at(m));
    if(m==u"Borders")return obj(r,link(m,"borders"));
    if(m==u"Count"){
        int n=kind=="shapes"?world->top().size():kind=="rows"?owner->rows:kind=="columns"?owner->cols:members.size();return val(r,Value::integer(n));
    }
    if(m==u"Item"){
        int i=integer(0)-1;
        if(kind=="rows"||kind=="columns"){Node*q=world->make(kind=="rows"?"row":"column",this);q->rs=i;return obj(r,q);}
        if(kind=="borders"){assert(i>=0&&i<6);U key(1,(char16_t)(u'1'+i));return obj(r,link(key,"border"));}
        if(kind=="shapes")return obj(r,world->top().at(i));
        return obj(r,members.at(i));
    }
    if(m==u"Range"&&kind=="shapes"){
        auto&v=arg(0);assert(v.vt==(0x2000|VT_VARIANT));Node*q=world->make("shaperange");
        for(auto&e:v.parray->values){auto all=world->top();auto it=std::find_if(all.begin(),all.end(),[&](Node*s){return s->name==text(e);});assert(it!=all.end());q->members.push_back(*it);}return obj(r,q);
    }
    if(m==u"Group"&&kind=="shaperange"){
        // Real Office rejects native tables: fail the test if renderer attempts it.
        for(Node*s:members)assert(s->type!=19);
        if(world->failGroup)return E_FAIL;
        float x=1e6,y=1e6,right=-1e6,bottom=-1e6;
        for(Node*s:members){x=std::min(x,s->value(u"Left"));y=std::min(y,s->value(u"Top"));right=std::max(right,s->value(u"Left")+s->value(u"Width"));bottom=std::max(bottom,s->value(u"Top")+s->value(u"Height"));}
        Node*q=world->shape(6,x,y,right-x,bottom-y);q->members=members;for(Node*s:members)s->parentGroup=q;return obj(r,q);
    }
    if(kind=="shape"&&type==19&&(m==u"Width"||m==u"Height")){
        float sum=0;Node*t=links.at(u"Table");for(float v:m==u"Width"?t->widths:t->heights)sum+=v;return val(r,Value::real(sum));
    }
    auto it=props.find(m);if(it!=props.end())return val(r,it->second);
    auto linkIt=links.find(m);if(linkIt!=links.end())return obj(r,linkIt->second);
    // Unsupported optional property reads fail instead of inventing a successful value.
    return DISP_E_MEMBERNOTFOUND;
}
struct Case {
    World world;Options opts;Capture source;HighlightItem item;HighlightResult colors;CreatedBlock out;Progress progress{nullptr,L"test"};Node*shapes,*original;
    Case(const U&code,bool numbers=false){opts.replace=true;opts.lineNumbers=numbers;shapes=world.make("shapes");original=world.shape(17,30,30,100,30);original->code=code;original->ink.assign(code.size(),0);source.shape.set(original);source.text=Value::text(w(code));source.left=30;source.top=30;item.length=code.size();item.tokens.make(1);item.tokens[0]={0,(DWORD)code.size(),0xABCDEF,0};colors.fg=0x222222;colors.bg=0x282C34;colors.gutter=0x777777;cancelAfter=-1;pumps=0;}
    HRESULT run(){return createBlock(shapes,source,item,colors,opts,out,progress);}
    Node*table(){return ((Node*)out.content.p)->links.at(u"Table");}
    Node*border(int r,int c,int side){return table()->members.at(r*table()->cols+c)->link(u"Borders","borders")->link(U(1,(char16_t)(u'0'+side)),"border");}
    bool visible(int r,int c,int side){return border(r,c,side)->value(u"Visible",-1)!=0&&border(r,c,side)->value(u"Transparency",0)<1;}
    Node*cell(int r,int c){Node*t=table();return t->members.at(r*t->cols+c)->links.at(u"Shape");}
};
static float fontSize(Case& c){return c.cell(0,c.opts.lineNumbers?1:0)->link(u"TextFrame","frame")->link(u"TextRange","range")->link(u"Font","font")->value(u"Size");}
static Node* appFor(Case& c,bool selected){
    Node*app=c.world.make("app"),*pres=app->link(u"ActivePresentation","presentation");
    Node*setup=pres->link(u"PageSetup","setup");setup->props[u"SlideWidth"]=Value::real(960);setup->props[u"SlideHeight"]=Value::real(540);
    Node*window=app->link(u"ActiveWindow","window"),*selection=window->link(u"Selection","selection");
    Node*range=selection->link(u"ShapeRange","selectionrange");if(selected)range->members.push_back(c.original);
    window->link(u"View","view")->link(u"Slide","slide")->links[u"Shapes"]=c.shapes;return app;
}
static int passed=0;
static void pass(const char*s){++passed;std::printf("PASS %02d: %s\n",passed,s);}
int main(){
    {
        Case c(u"a\r\rb\r",true);assert(c.run()==S_OK);assert(c.table()->rows==4&&c.table()->cols==2);
        assert(c.cell(0,1)->code==u"a"&&c.cell(1,1)->code.empty()&&c.cell(2,1)->code==u"b"&&c.cell(3,1)->code.empty());
        assert(c.cell(3,0)->code==u"4");assert(!c.original->dead);pass("native N-row table preserves interior and final blank rows");
    }
    {
        Case c(u"one\rtwo\rthree",true);c.opts.firstLine=98;assert(c.run()==S_OK);
        assert(c.cell(0,0)->code==u"98"&&c.cell(2,0)->code==u"100");assert(std::abs(c.table()->widths[0]-39)<0.01f);
        assert(c.table()->widths[0]<c.table()->widths[1]);pass("gutter uses measured font width and 98-to-100 digit boundary");
    }
    {
        Case c(u"/* a\r中😀\r*/\rint x;",false);c.item.tokens.make(2);c.item.tokens[0]={0,11,0x00AA00,0};c.item.tokens[1]={12,3,0xFF0000,0};
        assert(c.run()==S_OK);assert(c.cell(1,0)->code==u"中😀");for(auto color:c.cell(1,0)->ink)assert(color==(DWORD)rgb(0x00AA00));
        assert(c.cell(3,0)->ink[0]==(DWORD)rgb(0xFF0000));
        Capture rec;const wchar_t*msg=nullptr;assert(captureShape(c.out.content,rec,c.opts,false,&msg)==S_OK);assert(text(rec.text.v)==text(c.source.text.v));
        pass("full-document tokens map into row-local UTF-16 ranges and table reads back exactly");
    }
    {
        Case c(u"let x = 1;\r\rx++;",true);c.opts.windowFrame=true;assert(c.run()==S_OK);
        Node*decor=(Node*)c.out.decoration.p;assert(decor&&decor->type==6&&decor->members.size()==4);
        Node*panel=decor->members[0];assert(panel->links.at(u"Shadow")->value(u"Visible")==-1);
        for(int row=0;row<3;++row)for(int col=0;col<2;++col)assert(c.cell(row,col)->links.at(u"Fill")->value(u"Visible")==0);
        pass("native table stays native; only frame and buttons are grouped, cells transparent");
    }
    {
        Case c(u"a\r\rb",true);c.opts.outputMode=1;c.opts.windowFrame=true;assert(c.run()==S_OK);
        Node*g=(Node*)c.out.content.p;assert(g->type==6&&g->members.size()==10&&!c.out.decoration);
        for(Node*s:g->members)assert(s->type!=19);
        Capture rec;const wchar_t*msg=nullptr;assert(captureShape(g,rec,c.opts,false,&msg)==S_OK);assert(rec.grouped&&rec.gutter&&rec.rows==3);assert(text(rec.text.v)==u"a\r\rb");
        Ref child;child.set(g->members.back());wholeGroup(child);assert(child.p==g);
        // Rebuilding from the ordinary group's visible text does not need object names.
        for(Node*s:g->members)s->name=u"renamed by user";
        assert(captureShape(g,rec,c.opts,false,&msg)==S_OK&&text(rec.text.v)==u"a\r\rb");
        pass("editable row text boxes and frame truly group; child selection and stateless readback work");
    }
    {
        Case c(u"a\rb",false);c.opts.outputMode=1;c.opts.windowFrame=false;c.opts.background=0;assert(c.run()==S_OK);
        Node*g=(Node*)c.out.content.p;assert(g->type==6&&g->members.size()==2);for(Node*s:g->members)assert(s->type==17);
        pass("window off generates only editable code, no decorative background or buttons");
    }
    {
        Case c(u"a\rb",false);c.opts.outputMode=1;c.opts.windowFrame=true;c.opts.windowButtons=false;c.opts.windowShadow=false;c.opts.windowFill=1;c.opts.windowColor=0xF3F3F6;
        assert(c.run()==S_OK);Node*g=(Node*)c.out.content.p;assert(g->members.size()==3);Node*panel=g->members[0];
        assert(panel->links.at(u"Shadow")->value(u"Visible")==0);assert((DWORD)panel->links.at(u"Fill")->links.at(u"ForeColor")->value(u"RGB")== (DWORD)rgb(0xF3F3F6));
        pass("buttons and shadow can be disabled; window color is independent");
    }
    {
        Case c(u"a\rb",false);c.opts.windowFrame=true;c.world.minRow=40;assert(c.run()==S_OK);
        Node*panel=((Node*)c.out.decoration.p)->members[0];assert(std::abs(panel->value(u"Height")-(80+28+29.7f))<0.05f);
        pass("frame resizes to actual native table height instead of covering or clipping it");
    }
    {
        Case c(u"a\rb",true);c.opts.windowFrame=true;cancelAfter=0;assert(failed(c.run()));c.out.cleanup();
        assert(c.world.top().size()==1&&c.world.top()[0]==c.original&&!c.original->dead);cancelAfter=-1;
        pass("cancel removes newly created table and frame; original source stays intact");
    }
    {
        Case c(u"a\rb",true);c.opts.outputMode=1;c.opts.windowFrame=true;c.world.failGroup=true;assert(failed(c.run()));c.out.cleanup();
        assert(c.world.top().size()==1&&!c.original->dead);pass("group failure cleans pending editable shapes without deleting the source");
    }
    {
        Case c(u"a\rb",true);c.opts.windowFrame=true;assert(c.run()==S_OK);
        CaptureSet set;set.objects.make(1);set.count=1;const wchar_t*msg=nullptr;
        assert(captureShape(c.out.content,set.objects[0],c.opts,false,&msg)==S_OK);
        float codeTop=set.objects[0].top;Items<Ref> selected;selected.make(2);selected[0]=c.out.content;selected[1]=c.out.decoration;
        attachExplicitFrames(set,selected,1);assert(!set.objects[0].decoration&&set.objects[0].top==codeTop);
        attachExplicitFrames(set,selected,2);assert(set.objects[0].decoration.p==c.out.decoration.p);
        assert(set.objects[0].top==30&&set.objects[0].left==30&&set.objects[0].top<codeTop);
        assert(!c.original->dead&&!((Node*)c.out.decoration.p)->dead);
        assert(captureShape(c.out.content,set.objects[0],c.opts,false,&msg)==S_OK&&!set.objects[0].decoration);
        pass("only explicitly selected table-plus-window is paired for replacement; reread clears stale pairing");
    }
    {
        Case c(u"alpha\r\rbeta",true);c.opts.outputMode=1;c.opts.windowFrame=true;assert(c.run()==S_OK);
        Capture source;const wchar_t*msg=nullptr;assert(captureShape(c.out.content,source,c.opts,false,&msg)==S_OK);
        c.opts.windowFrame=false;CreatedBlock rebuilt;
        assert(createBlock(c.shapes,source,c.item,c.colors,c.opts,rebuilt,c.progress)==S_OK);
        Node*g=(Node*)rebuilt.content.p;assert(g->type==6&&g->members.size()==6);
        for(Node*part:g->members)assert(part->type==17);
        Capture again;assert(captureShape(g,again,c.opts,false,&msg)==S_OK&&text(again.text.v)==u"alpha\r\rbeta");
        pass("window can be removed by stateless rebuild while keeping editable code and blank rows");
    }
    {
        Case c(u"a\rb\rc",true);assert(c.run()==S_OK);
        for(int r=0;r<3;++r)for(int col=0;col<2;++col)for(int side=1;side<=6;++side)
            assert(c.visible(r,col,side)==((col==0&&side==4)||(col==1&&side==2)));
        assert(c.border(0,0,4)->value(u"Weight")==0.75f);
        pass("default border is only the gutter/code separator; no outer/horizontal/diagonal borders");
    }
    {
        Case c(u"a\rb\rc",true);c.opts.borderVertical=false;c.opts.borderHorizontal=true;assert(c.run()==S_OK);
        for(int r=0;r<3;++r)for(int col=0;col<2;++col)for(int side=1;side<=6;++side)
            assert(c.visible(r,col,side)==((side==1&&r>0)||(side==3&&r<2)));
        pass("horizontal-only uses shared interior row edges without drawing an outer frame");
    }
    {
        Case c(u"a\rb\rc",true);c.opts.borderVertical=false;c.opts.borderOuter=true;assert(c.run()==S_OK);
        for(int r=0;r<3;++r)for(int col=0;col<2;++col)for(int side=1;side<=6;++side)
            assert(c.visible(r,col,side)==((side==1&&r==0)||(side==3&&r==2)||(side==2&&col==0)||(side==4&&col==1)));
        pass("outer-only is exactly the four table perimeter edges");
    }
    {
        // All 8 user-visible toggle combinations, including adjacent shared edges.
        for(int flags=0;flags<8;++flags){Case c(u"a\rb",true);c.opts.borderVertical=flags&1;c.opts.borderHorizontal=flags&2;c.opts.borderOuter=flags&4;assert(c.run()==S_OK);
            for(int r=0;r<2;++r){assert(c.visible(r,0,4)==bool(flags&1));assert(c.visible(r,1,2)==bool(flags&1));
                assert(c.visible(r,0,2)==bool(flags&4));assert(c.visible(r,1,4)==bool(flags&4));}
            for(int col=0;col<2;++col){assert(c.visible(0,col,3)==bool(flags&2));assert(c.visible(1,col,1)==bool(flags&2));
                assert(c.visible(0,col,1)==bool(flags&4));assert(c.visible(1,col,3)==bool(flags&4));}}
        pass("all 8 border combinations agree across neighbouring cells; toggles are independent");
    }
    {
        Case c(u"a\rb",false);assert(c.run()==S_OK);
        for(int r=0;r<2;++r)for(int side=1;side<=6;++side)assert(!c.visible(r,0,side));
        pass("without line numbers, the vertical toggle does not create a stray outer border");
    }
    {
        Case c(u"single",false);c.opts.borderHorizontal=true;assert(c.run()==S_OK);
        for(int side=1;side<=6;++side)assert(!c.visible(0,0,side));
        c.opts.borderOuter=true;assert(applyTableBorders(c.table(),1,1,false,c.opts,c.colors,c.progress)==S_OK);
        for(int side=1;side<=6;++side)assert(c.visible(0,0,side)==(side<=4));
        pass("single-row/single-column tables distinguish internal edges from the perimeter");
    }
    {
        Case c(u"a\rb",true);c.opts.windowFrame=true;assert(c.run()==S_OK);
        Capture captured;const wchar_t*msg=nullptr;assert(captureShape(c.out.content,captured,c.opts,false,&msg)==S_OK);
        auto code=c.cell(0,1)->code;auto ink=c.cell(0,1)->ink;auto widths=c.table()->widths;auto heights=c.table()->heights;
        auto fill=c.cell(0,1)->links.at(u"Fill")->value(u"Visible");auto size=c.cell(0,1)->links.at(u"TextFrame")->links.at(u"TextRange")->links.at(u"Font")->value(u"Size");
        c.opts.borderVertical=false;c.opts.borderHorizontal=false;c.opts.borderOuter=false;c.opts.fontSize=72;c.opts.background=2;
        assert(recolor(captured,c.item,c.colors,c.opts,Action::Borders,c.progress)==S_OK);
        assert(c.cell(0,1)->code==code&&c.cell(0,1)->ink==ink&&c.table()->widths==widths&&c.table()->heights==heights);
        assert(c.cell(0,1)->links.at(u"Fill")->value(u"Visible")==fill);
        assert(c.cell(0,1)->links.at(u"TextFrame")->links.at(u"TextRange")->links.at(u"Font")->value(u"Size")==size);
        for(int r=0;r<2;++r)for(int col=0;col<2;++col)for(int side=1;side<=6;++side)assert(!c.visible(r,col,side));
        assert(c.out.decoration&&!((Node*)c.out.decoration.p)->dead);
        pass("border-only update preserves text, syntax colors, font size, fill, dimensions and window");
    }
    {
        Case c(u"a\rb",true);assert(c.run()==S_OK);Capture captured;const wchar_t*msg=nullptr;assert(captureShape(c.out.content,captured,c.opts,false,&msg)==S_OK);
        c.opts.borderVertical=false;c.opts.borderHorizontal=true;c.opts.borderOuter=true;
        assert(recolor(captured,c.item,c.colors,c.opts,Action::Format,c.progress)==S_OK);
        assert(!c.visible(0,0,4)&&c.visible(0,0,1)&&c.visible(0,0,3));
        pass("normal highlighter formatting also honours the selected table-border settings");
    }
    {
        Case c(U(300,u'x'));c.opts.fontSize=24;
        c.source.left=945;c.source.top=535;
        assert(c.run()==S_OK&&fontSize(c)==24&&!c.original->dead);
        Node*result=(Node*)c.out.content.p;
        assert(result->value(u"Left")==945&&result->value(u"Top")==535&&result->value(u"Width")>960);
        pass("wide code preserves explicit font size and source origin beyond slide edges");
    }
    {
        Case c(u"abc");c.opts.fontSize=60;assert(c.run()==S_OK&&fontSize(c)==60);
        Case small(u"abc");small.opts.fontSize=6;assert(small.run()==S_OK&&fontSize(small)==6);
        pass("small and large explicit font sizes are preserved");
    }
    {
        Case c(U(40,u'x'));c.opts.replace=false;Node*obstacle=c.world.shape(17,0,0,630,540);obstacle->code=u"occupied";
        assert(c.run()==S_OK&&fontSize(c)==18);
        assert(((Node*)c.out.content.p)->value(u"Left")==30&&((Node*)c.out.content.p)->value(u"Top")==30&&!c.original->dead);
        pass("occupied slides preserve source origin and explicit font size without replacement");
    }
    {
        Case c(u"abc");c.opts.replace=false;Node*obstacle=c.world.shape(17,0,0,960,540);obstacle->code=u"occupied";
        assert(c.run()==S_OK&&fontSize(c)==c.opts.fontSize);
        assert(c.out.content&&!c.original->dead);pass("full slides preserve the configured font size");
    }
    for(int mode:{0,1})for(bool window:{false,true}){
        U code;for(int i=0;i<40;++i)code+=u"a long code line\r";
        Case c(code,true);c.opts.outputMode=mode;c.opts.windowFrame=window;c.opts.fontSize=36;
        c.source.left=-20;c.source.top=500;
        assert(c.run()==S_OK);
        Node*root=(Node*)(c.out.decoration?c.out.decoration.p:c.out.content.p);
        assert(root->value(u"Left")==-20&&root->value(u"Top")==500);
        Capture captured;const wchar_t*msg=nullptr;
        assert(captureShape(c.out.content,captured,c.opts,false,&msg)==S_OK);
        for(int i=0;i<captured.segments.size();++i){Ref font;
            assert(object(captured.segments[i].range,L"Font",font)==S_OK&&getFloat(font,L"Size")==36);
        }
        pass("both output modes retain font size and origin for tall blocks with or without a window");
    }
    for(bool selected:{false,true})for(bool replace:{false,true}){
        Case c(u"original text");c.opts.replace=replace;c.opts.fontSize=42;Node*obstacle=c.world.shape(17,0,0,960,540);obstacle->code=u"occupied";
        Node*app=appFor(c,selected);wchar_t status[256],error[1600];clipboardText=u"pasted code";
        assert(executeAction(app,c.opts,Action::Clipboard,status,256,error,1600,c.progress)==S_OK);
        assert(c.original->dead==(selected&&replace)&&!obstacle->dead);bool pasted=false;
        for(Node*shape:c.world.top())if(shape->type==19){
            Node*cell=shape->links.at(u"Table")->members[0]->links.at(u"Shape");
            pasted|=cell->code==u"pasted code";
            assert(cell->links.at(u"TextFrame")->links.at(u"TextRange")->links.at(u"Font")->value(u"Size")==42);
            assert(shape->value(u"Left")==(selected?30:48)&&shape->value(u"Top")==(selected?30:48));
        }
        assert(pasted&&error[0]==0);pass(selected?"clipboard uses selection origin with replacement enabled or disabled":"clipboard without selection uses the default origin and explicit font size");
    }
    {
        Case c(u"keep original");Node*app=appFor(c,true);wchar_t status[256],error[1600];engineFails=true;
        assert(failed(executeAction(app,c.opts,Action::Clipboard,status,256,error,1600,c.progress))&&!c.original->dead);engineFails=false;
        pass("clipboard replacement preserves selected originals if highlighting fails");
    }
    {
        Case c(u"if(x!=0){\rvalue=1+2;\r}\r");Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,Action::Format,status,256,error,1600,c.progress)==S_OK);
        assert(c.original->code==u"if (x != 0) {\r    value = 1 + 2;\r}\r");
        for(auto color:c.original->ink)assert(color==(DWORD)rgb(0xABCDEF));
        pass("default cleanup writes complete text and uses new UTF-16 offsets for highlighting");
    }
    for(Action action:{Action::Format,Action::Color,Action::Plain}){
        Case c(u"x=1+2;");c.opts.basicCleanup=action!=Action::Format;Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,action,status,256,error,1600,c.progress)==S_OK&&c.original->code==u"x=1+2;");
        pass("opt-out and color-only actions leave source spacing unchanged");
    }
    for(Action action:{Action::Clipboard,Action::Table}){
        Case c(u"{\rx=1;\r\r}\r",true);clipboardText=u"{\rx=1;\r\r}\r";
        Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,action,status,256,error,1600,c.progress)==S_OK&&c.original->dead);
        Node*created=nullptr;for(Node*shape:c.world.top())if(shape->type==19)created=shape;assert(created);
        Capture capture;const wchar_t*msg=nullptr;assert(captureShape(created,capture,c.opts,false,&msg)==S_OK);
        assert(text(capture.text.v)==u"{\r    x = 1;\r\r}\r"&&capture.gutter&&capture.rows==5);
        // Formatting the generated table again must not lose indentation at cell boundaries.
        Node*selection=app->links.at(u"ActiveWindow")->links.at(u"Selection")->links.at(u"ShapeRange");selection->members={created};
        assert(executeAction(app,c.opts,Action::Format,status,256,error,1600,c.progress)==S_OK);
        assert(captureShape(created,capture,c.opts,false,&msg)==S_OK&&text(capture.text.v)==u"{\r    x = 1;\r\r}\r");
        pass("generated/pasted rows retain blanks, final newline and gutter through repeated cleanup");
    }
    {
        Case c(u"x=1;");Node*app=appFor(c,true);wchar_t status[256],error[1600];engineFails=true;
        assert(failed(executeAction(app,c.opts,Action::Format,status,256,error,1600,c.progress))&&c.original->code==u"x=1;");engineFails=false;
        pass("engine failure after cleanup preserves the original source");
    }
    {
        Case c(u"s=\"a\tb\";");Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,Action::Format,status,256,error,1600,c.progress)==S_OK&&c.original->code==u"s = \"a\tb\";");
        pass("literal tabs survive the entire default cleanup path");
    }
    for(int mode:{0,1}){
        Case c(u"cout<<i<<\"\u00a0 \"<<n1<<\"   \"<<n2<<endl;",true);c.opts.outputMode=mode;
        Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,Action::Table,status,256,error,1600,c.progress)==S_OK);
        Node*created=nullptr;for(Node*shape:c.world.top())if(shape->type==(mode?6:19))created=shape;assert(created);
        Capture rec;const wchar_t*msg=nullptr;assert(captureShape(created,rec,c.opts,false,&msg)==S_OK);
        assert(text(rec.text.v)==u"cout << i << \"\u00a0 \" << n1 << \"   \" << n2 << endl;"&&rec.gutter);
        for(int i=0;i<rec.segments.size();++i)if(!rec.segments[i].number){Node*shape=(Node*)rec.segments[i].shape.p;for(auto color:shape->ink)assert(color==(DWORD)rgb(0xABCDEF));}
        pass("stream operators format in native table/grouped output while literal spaces and colors survive");
    }
    {
        Case c(u"s=\"a\tb\";");c.opts.basicCleanup=false;Node*app=appFor(c,true);wchar_t status[256],error[1600];
        assert(executeAction(app,c.opts,Action::Format,status,256,error,1600,c.progress)==S_OK&&c.original->code==u"s=\"a\tb\";");
        pass("disabling cleanup preserves literal tabs and source spacing");
    }
    std::printf("%d renderer integration simulations passed. NOT a Windows/PowerPoint runtime test.\n",passed);
}
