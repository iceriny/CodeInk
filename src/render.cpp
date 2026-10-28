#include "render.h"
#include "basicformat.h"
using namespace codeink;
static constexpr int MAX_CODE=50000,MAX_CELLS=512,MAX_ROWS=200;
static LONG rgb(DWORD x){return (LONG)(((x&255)<<16)|(x&0xFF00)|((x>>16)&255));}
static HRESULT cellShape(IDispatch*t,int row,int col,Ref& out){Value a[]={Value::integer(row),Value::integer(col)};Ref cell;HRESULT hr=object(t,L"Cell",cell,2,a);return failed(hr)?hr:object(cell,L"Shape",out);}
static HRESULT shapeRange(IDispatch*s,Ref& out){Ref f;HRESULT hr=object(s,L"TextFrame",f);return failed(hr)?hr:object(f,L"TextRange",out);}
static int count(IDispatch*t,const wchar_t*which){Ref c;return failed(object(t,which,c))?0:getInt(c,L"Count");}
static HRESULT setBstr(Value& value,BSTR text){if(!text)return E_OUTOFMEMORY;value.clear();value.v.vt=VT_BSTR;value.v.bstrVal=text;return S_OK;}
static HRESULT normalized(Value& value,BSTR source,bool expandTabs=false){
    int n=(int)SysStringLen(source);
    if(!expandTabs){
        if(n>MAX_CODE)return E_FAIL;
        BSTR b=SysAllocStringLen(nullptr,n);if(!b)return E_OUTOFMEMORY;int k=0;
        for(int i=0;i<n;++i){wchar_t c=source[i];if(c=='\r'&&i+1<n&&source[i+1]=='\n')++i;
            b[k++]=(c=='\n'||c=='\v'||c==0x2028||c==0x2029)?'\r':c;}
        Value temp;HRESULT hr=setBstr(temp,b);if(failed(hr))return hr;return setBstr(value,SysAllocStringLen(b,k));
    }
    int len=normalizedLength((const Ch*)source,n);if(len<0||len>MAX_CODE)return E_FAIL;
    BSTR b=SysAllocStringLen(nullptr,len);if(!b)return E_OUTOFMEMORY;normalize((const Ch*)source,n,(Ch*)b,len+1);return setBstr(value,b);
}
static HRESULT tidyText(Value& value,const Options&o){
    Value source;HRESULT hr=normalized(source,value.v.bstrVal,false);if(failed(hr))return hr;
    Items<Ch> buffer;if(!buffer.make(MAX_CODE+1))return E_OUTOFMEMORY;
    int n=basicFormat((const Ch*)source.v.bstrVal,SysStringLen(source.v.bstrVal),(const Ch*)o.language,buffer.data(),MAX_CODE+1);
    // If added whitespace would exceed the existing limit, keep the source.
    if(n<0){value=source;return S_OK;}
    return setBstr(value,SysAllocStringLen((const wchar_t*)buffer.data(),n));
}
static HRESULT tidyCapture(Capture&cap,const Options&o){
    // Several code columns represent separate cells, not one source line.
    if(cap.cols-(cap.gutter?1:0)>1){
        for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];if(s.number)continue;
            HRESULT hr=tidyText(s.text,o);if(failed(hr))return hr;s.length=SysStringLen(s.text.v.bstrVal);}
    }else{
        Value formatted=cap.text;HRESULT hr=tidyText(formatted,o);if(failed(hr))return hr;
        BSTR source=formatted.v.bstrVal;int total=SysStringLen(source),offset=0;
        for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];if(s.number)continue;
            int breaks=lineCount((const Ch*)s.text.v.bstrVal,SysStringLen(s.text.v.bstrVal))-1,end=offset;
            while(end<total){if(source[end]=='\r'){if(!breaks)break;--breaks;}++end;}
            hr=setBstr(s.text,SysAllocStringLen(source+offset,end-offset));if(failed(hr))return hr;
            s.length=end-offset;offset=end<total?end+1:end;
        }
    }
    return S_OK;
}
static HRESULT prepareSegment(Segment& seg,bool format){
    HRESULT hr=shapeRange(seg.shape,seg.range);if(failed(hr)||failed(hr=getText(seg.range,L"Text",seg.original)))return hr;
    if(SysStringLen(seg.original.v.bstrVal)>MAX_CODE)return E_FAIL;
    if(format&&!seg.number)hr=normalized(seg.text,seg.original.v.bstrVal);else seg.text=seg.original;
    seg.length=(int)SysStringLen(seg.text.v.bstrVal);return hr;
}
static HRESULT concatenate(Capture& cap){
    int total=0,used=0,previousRow=-1;
    for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];if(s.number)continue;total+=s.length+(used++?1:0);if(total>MAX_CODE)return E_FAIL;}
    BSTR b=SysAllocStringLen(nullptr,total);if(!b)return E_OUTOFMEMORY;int k=0;used=0;
    for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];if(s.number)continue;if(used++)b[k++]=s.row==previousRow?'\t':'\r';s.start=k;memcpy(b+k,s.text.v.bstrVal,s.length*2);k+=s.length;previousRow=s.row;}
    b[k]=0;return setBstr(cap.text,b);
}
static bool hasGutter(Capture& cap){
    if(cap.cols!=2)return false;int next=-1,n=0;
    for(int r=0;r<cap.rows;++r){Segment& s=cap.segments[r*cap.cols];if(!numberSequence((const Ch*)s.original.v.bstrVal,(int)SysStringLen(s.original.v.bstrVal),next,n))return false;}
    return n>0;
}
// Read ordinary groups by visible text geometry, not by names, tags or source caches.
// Empty text boxes are meaningful code rows; blank auto-shapes are decorations.
static HRESULT textLeaves(IDispatch*shape,Items<Ref>&leaves,int&used,int depth){
    if(depth>16)return E_FAIL;
    if(getInt(shape,L"Type")==6){
        Ref children;HRESULT hr=object(shape,L"GroupItems",children);if(failed(hr))return hr;
        int n=getInt(children,L"Count");if(n<0||n>512)return E_FAIL;
        for(int i=1;i<=n;++i){Value index=Value::integer(i);Ref child;
            if(failed(hr=object(children,L"Item",child,1,&index))||failed(hr=textLeaves(child,leaves,used,depth+1)))return hr;}
        return S_OK;
    }
    if(getInt(shape,L"HasTable"))return E_FAIL; // Do not flatten a foreign nested table silently.
    if(!getInt(shape,L"HasTextFrame"))return S_OK;
    Ref frame;if(failed(object(shape,L"TextFrame",frame)))return E_FAIL;
    if(!getInt(frame,L"HasText")&&getInt(shape,L"Type")!=17)return S_OK;
    if(used>=leaves.size())return E_FAIL;leaves[used++].set(shape);return S_OK;
}
static void readingOrder(Items<Ref>&ordered,int n){
    for(int i=1;i<n;++i)for(int j=i;j>0;--j){
        float ay=getFloat(ordered[j-1],L"Top"),by=getFloat(ordered[j],L"Top");
        float ax=getFloat(ordered[j-1],L"Left"),bx=getFloat(ordered[j],L"Left");
        bool swap=ay>by+3||((ay-by<3&&by-ay<3)&&ax>bx);if(!swap)break;
        Ref tmp=ordered[j];ordered[j]=ordered[j-1];ordered[j-1]=tmp;
    }
}
static void wholeGroup(Ref&shape){
    for(int i=0;i<16;++i){Ref parent;
        if(failed(object(shape,L"ParentGroup",parent))||getInt(parent,L"Type")!=6)break;
        if(getInt(parent,L"Id",-1)==getInt(shape,L"Id",-2))break;shape=parent;
    }
}
static HRESULT captureGroup(IDispatch*shape,Capture&cap,const Options&o,bool format,const wchar_t**msg){
    Items<Ref> leaves;if(!leaves.make(MAX_CELLS))return E_OUTOFMEMORY;int n=0;
    HRESULT hr=textLeaves(shape,leaves,n,0);if(failed(hr)){*msg=L"组合过于复杂或包含嵌套表格，请先取消组合或单独选取代码对象。";return hr;}
    if(!n)return S_FALSE;readingOrder(leaves,n);if(!cap.segments.make(n))return E_OUTOFMEMORY;
    cap.grouped=true;cap.rows=0;cap.cols=0;float previousY=-1e6f;int col=0;
    for(int i=0;i<n;++i){Segment&seg=cap.segments[i];float y=getFloat(leaves[i],L"Top");
        if(i==0||y-previousY>3||previousY-y>3){++cap.rows;col=0;previousY=y;}
        seg.row=cap.rows-1;seg.col=col++;if(col>cap.cols)cap.cols=col;
        seg.shape=leaves[i];if(failed(hr=prepareSegment(seg,false)))return hr;
    }
    if(cap.rows>MAX_ROWS){*msg=L"每个代码组合最多读取 200 行，请分批处理。";return E_FAIL;}
    bool rectangular=cap.rows*cap.cols==n;
    cap.gutter=rectangular&&(o.tableRead==2?cap.cols>=2:o.tableRead==0&&hasGutter(cap));
    for(int i=0;i<n;++i){Segment&seg=cap.segments[i];seg.number=cap.gutter&&seg.col==0;
        if(format&&!seg.number){if(failed(hr=normalized(seg.text,seg.original.v.bstrVal)))return hr;seg.length=SysStringLen(seg.text.v.bstrVal);}}
    return S_OK;
}
static HRESULT captureShape(IDispatch* shape,Capture& cap,const Options&o,bool format,const wchar_t**msg){
    cap.table.take(nullptr);cap.decoration.take(nullptr);cap.segments.clear();cap.text.clear();cap.gutter=false;cap.grouped=false;cap.rows=cap.cols=0;cap.shape.set(shape);cap.left=getFloat(shape,L"Left",48);cap.top=getFloat(shape,L"Top",48);
    HRESULT hr=S_OK;
    if(getInt(shape,L"Type")==6){hr=captureGroup(shape,cap,o,format,msg);if(hr!=S_OK)return hr;}else if(getInt(shape,L"HasTable",0)){
        hr=object(shape,L"Table",cap.table);if(failed(hr))return hr;cap.rows=count(cap.table,L"Rows");cap.cols=count(cap.table,L"Columns");
        if(cap.rows<1||cap.cols<1||cap.rows>MAX_ROWS||cap.cols>32||cap.rows*cap.cols>MAX_CELLS){*msg=L"表格超过 200 行或 512 个单元格，请缩小处理范围。";return E_FAIL;}
        if(!cap.segments.make(cap.rows*cap.cols))return E_OUTOFMEMORY;
        for(int r=0;r<cap.rows;++r)for(int c=0;c<cap.cols;++c){Segment&s=cap.segments[r*cap.cols+c];s.row=r;s.col=c;hr=cellShape(cap.table,r+1,c+1,s.shape);if(failed(hr)||failed(hr=prepareSegment(s,false)))return hr;}
        cap.gutter=o.tableRead==2?cap.cols>=2:o.tableRead==0&&hasGutter(cap);
        for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];s.number=cap.gutter&&s.col==0;if(format&&!s.number){hr=normalized(s.text,s.original.v.bstrVal);if(failed(hr))return hr;s.length=(int)SysStringLen(s.text.v.bstrVal);}}
    }else{
        if(getInt(shape,L"HasTextFrame",0)==0)return S_FALSE;
        if(!cap.segments.make(1))return E_OUTOFMEMORY;cap.segments[0].shape.set(shape);hr=prepareSegment(cap.segments[0],format);if(failed(hr))return hr;
    }
    hr=concatenate(cap);if(failed(hr)){*msg=L"每个对象最多 50,000 个 UTF-16 字符单位，请拆分代码。";return hr;}
    return SysStringLen(cap.text.v.bstrVal)?S_OK:S_FALSE;
}
static bool isWindowDecoration(IDispatch*shape){
    int type=getInt(shape,L"Type");
    if(type==1){Ref frame;return getInt(shape,L"AutoShapeType")==5&&!failed(object(shape,L"TextFrame",frame))&&!getInt(frame,L"HasText");}
    if(type!=6)return false;Ref items;if(failed(object(shape,L"GroupItems",items))||getInt(items,L"Count")!=4)return false;
    int panels=0,dots=0;
    for(int i=1;i<=4;++i){Ref item,frame;Value index=Value::integer(i);
        if(failed(object(items,L"Item",item,1,&index))||getInt(item,L"Type")!=1||failed(object(item,L"TextFrame",frame))||getInt(frame,L"HasText"))return false;
        int shapeType=getInt(item,L"AutoShapeType");if(shapeType==5)++panels;else if(shapeType==9)++dots;else return false;
    }
    return panels==1&&dots==3;
}
static void attachExplicitFrames(CaptureSet&set,Items<Ref>&selected,int n){
    // No persistent link: only pair an explicitly selected, window-like decoration
    // with exactly one selected native table that it closely encloses.
    for(int i=0;i<n;++i){IDispatch*frame=selected[i];if(!isWindowDecoration(frame))continue;
        float x=getFloat(frame,L"Left"),y=getFloat(frame,L"Top"),w=getFloat(frame,L"Width"),h=getFloat(frame,L"Height");int found=-1;
        for(int j=0;j<set.count;++j){Capture&c=set.objects[j];if(!c.table)continue;
            float l=c.left-x,t=c.top-y,r=x+w-c.left-getFloat(c.shape,L"Width"),b=y+h-c.top-getFloat(c.shape,L"Height");
            if(l>=-0.5f&&l<=61&&r>=-0.5f&&r<=61&&b>=-0.5f&&b<=61&&t>=-0.5f&&t<=240){if(found>=0){found=-2;break;}found=j;}
        }
        if(found>=0&&!set.objects[found].decoration){Capture&c=set.objects[found];c.decoration.set(frame);c.left=x;c.top=y;}
    }
}
HRESULT captureSelection(IDispatch* app,CaptureSet& set,const Options&o,bool format,const wchar_t**msg){
    *msg=L"请在普通编辑视图中选中文字、文本框或表格；选中部分文字也会处理它所属的整个组件。";
    Ref window,selection,shapes;HRESULT hr=object(app,L"ActiveWindow",window);if(failed(hr)||failed(hr=object(window,L"Selection",selection)))return hr;
    const wchar_t* which=getInt(selection,L"HasChildShapeRange",0)?L"ChildShapeRange":L"ShapeRange";
    hr=object(selection,which,shapes);
    if(failed(hr)){
        // Some Office hosts do not expose ShapeRange while editing a text run.
        // Climb TextRange -> TextFrame -> Shape; never use the partial TextRange.
        Ref text,frame,shape;
        if(failed(object(selection,L"TextRange",text))||failed(object(text,L"Parent",frame))||failed(object(frame,L"Parent",shape)))return hr;
        if(!set.objects.make(1))return E_OUTOFMEMORY;wholeGroup(shape);hr=captureShape(shape,set.objects[0],o,format,msg);if(hr==S_OK)set.count=1;return hr==S_FALSE?E_FAIL:hr;
    }
    int n=getInt(shapes,L"Count");if(n<1||n>32){*msg=L"请选中 1～32 个组件；更多对象请分批处理。";return E_FAIL;}
    if(!set.objects.make(n))return E_OUTOFMEMORY;
    // Sort selected shapes in reading order (top, then left), not selection / z-order.
    Items<Ref> ordered;if(!ordered.make(n))return E_OUTOFMEMORY;
    int unique=0;
    for(int i=0;i<n;++i){Value index=Value::integer(i+1);Ref candidate;
        hr=object(shapes,L"Item",candidate,1,&index);if(failed(hr))return hr;wholeGroup(candidate);
        bool duplicate=false;int id=getInt(candidate,L"Id",-1);
        for(int j=0;j<unique;++j)if(id>=0&&id==getInt(ordered[j],L"Id",-2)){duplicate=true;break;}
        if(!duplicate)ordered[unique++]=candidate;
    }
    n=unique;readingOrder(ordered,n);
    int total=0;
    for(int i=0;i<n;++i){hr=captureShape(ordered[i],set.objects[set.count],o,format,msg);if(failed(hr))return hr;if(hr==S_FALSE){++set.skipped;continue;}total+=(int)SysStringLen(set.objects[set.count].text.v.bstrVal);if(total>400000){*msg=L"每批最多处理 400,000 个 UTF-16 字符单位，请分批处理。";return E_FAIL;}++set.count;}
    if(!set.count){*msg=L"选中的对象没有可编辑文字。图片、SmartArt 和无文本的装饰组合不会被转换。";return E_FAIL;}
    attachExplicitFrames(set,ordered,n);
    *msg=nullptr;return S_OK;
}
static HRESULT colorRange(IDispatch* range,DWORD color){Ref font,c;HRESULT hr=object(range,L"Font",font);if(failed(hr)||failed(hr=object(font,L"Color",c)))return hr;return put(c,L"RGB",Value::integer(rgb(color)));}
static HRESULT fontStyle(IDispatch*range,DWORD flags){Ref font;HRESULT hr=object(range,L"Font",font);if(failed(hr))return hr;hr=put(font,L"Italic",Value::integer((flags&1)?-1:0));if(!failed(hr))hr=put(font,L"Bold",Value::integer((flags&2)?-1:0));if(!failed(hr))hr=put(font,L"Underline",Value::integer((flags&4)?-1:0));return hr;}
static HRESULT applyTokens(IDispatch*range,const HighlightItem& result,int global,int length,const HighlightResult& all,const Options&o,bool plain,Progress&p){
    HRESULT hr=colorRange(range,all.fg);if(failed(hr))return hr;
    if(o.themeStyles&&!plain){hr=fontStyle(range,0);if(failed(hr))return hr;}if(plain)return S_OK;
    int end=global+length;
    for(int i=0;i<result.tokens.size();++i){const ColorToken&t=result.tokens[i];if((int)t.start>=end)break;if((int)(t.start+t.length)<=global)continue;
        if(t.color==all.fg&&(!o.themeStyles||t.style==0))continue;
        int a=(int)t.start>global?(int)t.start:global,b=(int)(t.start+t.length)<end?(int)(t.start+t.length):end;if(a>=b)continue;
        Value args[]={Value::integer(a-global+1),Value::integer(b-a)};Ref chars;hr=object(range,L"Characters",chars,2,args);if(failed(hr)||failed(hr=colorRange(chars,t.color)))return hr;
        if(o.themeStyles&&failed(hr=fontStyle(chars,t.style)))return hr;
        if(i%48==0&&!p.pump())return (HRESULT)0x80004004;
    }
    return S_OK;
}
static float lineHeight(const Options&o,float fs){return o.spacingPoints?o.spacing:fs*o.spacing;}
static HRESULT background(IDispatch*shape,const Options&o,const HighlightResult&colors){
    Ref fill;HRESULT hr=object(shape,L"Fill",fill);if(failed(hr))return hr;
    if(o.background==0){put(fill,L"Transparency",Value::real(1));return put(fill,L"Visible",Value::integer(0));}
    hr=invoke(fill,L"Solid",DISPATCH_METHOD,0,nullptr);if(failed(hr))return hr;put(fill,L"Visible",Value::integer(-1));put(fill,L"Transparency",Value::real(0));
    Ref color;hr=object(fill,L"ForeColor",color);return failed(hr)?hr:put(color,L"RGB",Value::integer(rgb(o.background==1?colors.bg:o.backgroundColor)));
}
// PpBorderType: top=1, left=2, bottom=3, right=4, diagonal down/up=5/6.
// Neighbouring cells receive identical shared-edge settings, avoiding order-dependent
// disappearance. Vertical means ONLY the gutter/code separator, not every column.
static DWORD tableBorderColor(const Options&o,const HighlightResult&colors){
    DWORD bg=o.windowFrame?(o.windowFill?o.windowColor:colors.bg):o.background==2?o.backgroundColor:colors.bg;
    DWORD value=0;
    for(int shift=0;shift<=16;shift+=8){DWORD fgPart=(colors.fg>>shift)&255,bgPart=(bg>>shift)&255;value|=((fgPart*35+bgPart*65+50)/100)<<shift;}
    return value;
}
static HRESULT tableCellBorders(IDispatch*table,int row,int col,int rows,int cols,bool gutter,const Options&o,DWORD color){
    Value a[]={Value::integer(row),Value::integer(col)};Ref cell,borders;HRESULT hr=object(table,L"Cell",cell,2,a);
    if(failed(hr)||failed(hr=object(cell,L"Borders",borders)))return hr;
    const bool visible[]={
        row==1?o.borderOuter:o.borderHorizontal,
        col==1?o.borderOuter:(gutter&&col==2&&o.borderVertical),
        row==rows?o.borderOuter:o.borderHorizontal,
        col==cols?o.borderOuter:(gutter&&col==1&&o.borderVertical),
        false,false
    };
    for(int i=1;i<=6;++i){Value index=Value::integer(i);Ref line,fore;
        if(failed(hr=object(borders,L"Item",line,1,&index)))return hr;
        if(visible[i-1]){
            if(failed(hr=object(line,L"ForeColor",fore))||failed(hr=put(fore,L"RGB",Value::integer(rgb(color))))
                ||failed(hr=put(line,L"Weight",Value::real(0.75f)))||failed(hr=put(line,L"DashStyle",Value::integer(1)))
                ||failed(hr=put(line,L"Style",Value::integer(1)))||failed(hr=put(line,L"Transparency",Value::real(0))))return hr;
        }
        if(failed(hr=put(line,L"Visible",Value::integer(visible[i-1]?-1:0))))return hr;
        // PowerPoint can keep rendering table borders after Visible=false.
        // Explicit transparency also suppresses those inherited/style borders.
        if(failed(hr=put(line,L"Transparency",Value::real(visible[i-1]?0.0f:1.0f))))return hr;
    }
    return S_OK;
}
static HRESULT applyTableBorders(IDispatch*table,int rows,int cols,bool gutter,const Options&o,const HighlightResult&colors,Progress&p){
    if(!table||rows<1||cols<1)return E_FAIL;DWORD color=tableBorderColor(o,colors);
    for(int r=1;r<=rows;++r){if(!p.pump())return (HRESULT)0x80004004;
        for(int c=1;c<=cols;++c){HRESULT hr=tableCellBorders(table,r,c,rows,cols,gutter,o,color);if(failed(hr))return hr;}}
    return S_OK;
}
static HRESULT typography(IDispatch*shape,IDispatch*range,const Options&o,float fs,bool gutter,bool compact,const HighlightResult&colors,bool transparent=false){
    Ref frame,font,para;HRESULT hr=object(shape,L"TextFrame",frame);if(failed(hr))return hr;
    put(frame,L"AutoSize",Value::integer(0));put(frame,L"WordWrap",Value::integer(0));put(frame,L"VerticalAnchor",Value::integer(1));
    put(frame,L"MarginLeft",Value::real(gutter?4:8));put(frame,L"MarginRight",Value::real(gutter?6:8));put(frame,L"MarginTop",Value::real(compact?0:8));put(frame,L"MarginBottom",Value::real(compact?0:8));
    if(failed(hr=object(range,L"Font",font)))return hr;
    if(failed(hr=put(font,L"Name",Value::text(o.font)))||failed(hr=put(font,L"Size",Value::real(fs))))return hr;
    put(font,L"NameFarEast",Value::text(o.font));
    if(failed(hr=object(range,L"ParagraphFormat",para)))return hr;
    put(para,L"Alignment",Value::integer(gutter?3:1));put(para,L"SpaceBefore",Value::real(0));put(para,L"SpaceAfter",Value::real(0));
    if(failed(hr=put(para,L"LineRuleWithin",Value::integer(o.spacingPoints?0:-1)))||failed(hr=put(para,L"SpaceWithin",Value::real(o.spacing))))return hr;
    Ref bullet;if(!failed(object(para,L"Bullet",bullet)))put(bullet,L"Visible",Value::integer(0));
    if(transparent){Ref fill;if(failed(hr=object(shape,L"Fill",fill))||failed(hr=put(fill,L"Visible",Value::integer(0))))return hr;}
    else if(failed(hr=background(shape,o,colors)))return hr;
    return gutter?colorRange(range,colors.gutter):S_OK;
}
static Value makeNumbers(int first,int lines){
    if(lines<1||lines>MAX_ROWS+MAX_CODE)return Value();
    // Seven digits plus separator per line; firstLine is validated in the UI.
    wchar_t* b=(wchar_t*)alloc((SIZE_T)(lines*12+1)*2);if(!b)return Value();int k=0;
    for(int i=0;i<lines;++i){wchar_t num[24];number(num,first+i);int len=slen(num);memcpy(b+k,num,len*2);k+=len;if(i+1<lines)b[k++]='\r';}
    Value result=Value::textLen(b,k);dealloc(b);return result;
}
static HRESULT verifyAndWrite(Segment&s,bool format){
    Value current;HRESULT hr=getText(s.range,L"Text",current);if(failed(hr))return hr;
    if(SysStringLen(current.v.bstrVal)!=SysStringLen(s.original.v.bstrVal)||!same(current.v.bstrVal,s.original.v.bstrVal))return E_FAIL;
    if(format&&!same(s.original.v.bstrVal,s.text.v.bstrVal)){
        hr=put(s.range,L"Text",s.text);if(failed(hr))return hr;
        if(failed(hr=getText(s.range,L"Text",current)))return hr;
        if(SysStringLen(current.v.bstrVal)!=SysStringLen(s.text.v.bstrVal)||!same(current.v.bstrVal,s.text.v.bstrVal))return E_FAIL;
    }
    return S_OK;
}
static HRESULT recolor(Capture&cap,const HighlightItem&item,const HighlightResult&all,const Options&o,Action action,Progress&p){
    if(action==Action::Borders)return applyTableBorders(cap.table,cap.rows,cap.cols,cap.gutter,o,all,p);
    const bool format=action==Action::Format,plain=action==Action::Plain;HRESULT hr=S_OK;int next=o.firstLine;
    if(format&&cap.table){background(cap.shape,o,all);const wchar_t*props[]={L"FirstRow",L"LastRow",L"FirstCol",L"LastCol",L"HorizBanding",L"VertBanding"};for(int j=0;j<6;++j)put(cap.table,props[j],Value::boolean(false));}
    for(int i=0;i<cap.segments.size();++i){Segment&s=cap.segments[i];if(!p.pump())return (HRESULT)0x80004004;
        if(failed(hr=verifyAndWrite(s,format)))return hr;
        if(format){
            if(s.number){int lines=1;for(int j=i+1;j<cap.segments.size()&&cap.segments[j].row==s.row;++j){int n=lineCount((const Ch*)cap.segments[j].text.v.bstrVal,cap.segments[j].length);if(n>lines)lines=n;}Value nums=makeNumbers(next,lines);if(nums.v.vt!=VT_BSTR)return E_OUTOFMEMORY;hr=put(s.range,L"Text",nums);if(failed(hr))return hr;next+=lines;}
            if(failed(hr=typography(s.shape,s.range,o,o.fontSize,s.number,cap.rows>1,all)))return hr;

        }
        if(s.number)hr=colorRange(s.range,plain?all.fg:all.gutter);else hr=applyTokens(s.range,item,s.start,s.length,all,o,plain,p);
        if(failed(hr))return hr;
    }
    if(format&&cap.table){
        if(failed(hr=applyTableBorders(cap.table,cap.rows,cap.cols,cap.gutter,o,all,p)))return hr;
        Ref rows;if(!failed(object(cap.table,L"Rows",rows)))for(int r=0;r<cap.rows;++r){
            int lines=1;for(int j=0;j<cap.cols;++j){Segment&s=cap.segments[r*cap.cols+j];if(s.number)continue;
                int n=lineCount((const Ch*)s.text.v.bstrVal,s.length);if(n>lines)lines=n;}
            Value index=Value::integer(r+1);Ref row;if(!failed(object(rows,L"Item",row,1,&index)))
                put(row,L"Height",Value::real(lines*lineHeight(o,o.fontSize)+(cap.rows>1?0:16)));
        }
        if(cap.gutter&&cap.cols==2){
            float measured=0;int numberCount=0;
            for(int i=0;i<cap.segments.size();++i)if(cap.segments[i].number){float w=getFloat(cap.segments[i].range,L"BoundWidth");if(w>measured)measured=w;
                numberCount+=lineCount((const Ch*)cap.segments[i].text.v.bstrVal,cap.segments[i].length);}
            float gutter=narrowGutterWidth(o.firstLine,numberCount,o.fontSize,measured),width=getFloat(cap.shape,L"Width");
            if(width-gutter<48)return E_FAIL;
            Ref columns,one,two;Value first=Value::integer(1),second=Value::integer(2);
            if(failed(hr=object(cap.table,L"Columns",columns))||failed(hr=object(columns,L"Item",one,1,&first))||failed(hr=object(columns,L"Item",two,1,&second)))return hr;
            if(failed(hr=put(one,L"Width",Value::real(gutter)))||failed(hr=put(two,L"Width",Value::real(width-gutter))))return hr;
        }
    }
    return S_OK;
}
static HRESULT getSlide(IDispatch*app,Ref&shapes){
    Ref window,view,slide;HRESULT hr=object(app,L"ActiveWindow",window);
    if(!failed(hr))hr=object(window,L"View",view);
    if(!failed(hr))hr=object(view,L"Slide",slide);
    if(!failed(hr))hr=object(slide,L"Shapes",shapes);return hr;
}
// Temporary objects are tracked until grouped. Failure removes ONLY newly created
// output; source deletion is deferred until the entire batch succeeds.
struct CreatedBlock {
    Ref content,decoration;Items<Ref> pending;int used;
    CreatedBlock():used(0){}
    HRESULT add(const Ref&shape){if(used>=pending.size())return E_OUTOFMEMORY;pending[used++]=shape;return S_OK;}
    void cleanup(){
        if(content)invoke(content,L"Delete",DISPATCH_METHOD,0,nullptr);
        if(decoration)invoke(decoration,L"Delete",DISPATCH_METHOD,0,nullptr);
        for(int i=used-1;i>=0;--i)if(pending[i])invoke(pending[i],L"Delete",DISPATCH_METHOD,0,nullptr);
        content.take(nullptr);decoration.take(nullptr);pending.clear();used=0;
    }
};
static HRESULT groupObjects(IDispatch*shapes,Items<Ref>&parts,int n,Ref&group){
    if(n<1)return E_FAIL;if(n==1){group=parts[0];return S_OK;}
    Value array;array.v.vt=0x2000|VT_VARIANT;array.v.parray=SafeArrayCreateVector(VT_VARIANT,0,(ULONG)n);
    if(!array.v.parray)return E_OUTOFMEMORY;
    for(int i=0;i<n;++i){Value name;HRESULT hr=getText(parts[i],L"Name",name);if(failed(hr))return hr;
        LONG index=i;if(failed(hr=SafeArrayPutElement(array.v.parray,&index,&name.v)))return hr;}
    Ref range;HRESULT hr=object(shapes,L"Range",range,1,&array);return failed(hr)?hr:object(range,L"Group",group);
}
static HRESULT flatFill(IDispatch*shape,DWORD color){
    Ref fill,c,line;HRESULT hr=object(shape,L"Fill",fill);if(failed(hr))return hr;
    if(failed(hr=invoke(fill,L"Solid",DISPATCH_METHOD,0,nullptr))||failed(hr=put(fill,L"Visible",Value::integer(-1)))||failed(hr=put(fill,L"Transparency",Value::real(0))))return hr;
    if(failed(hr=object(fill,L"ForeColor",c))||failed(hr=put(c,L"RGB",Value::integer(rgb(color)))))return hr;
    if(failed(hr=object(shape,L"Line",line)))return hr;return put(line,L"Visible",Value::integer(0));
}
static HRESULT roundness(IDispatch*panel,float width,float height,float radius){
    float shortest=width<height?width:height;if(shortest<=0)return E_FAIL;
    float amount=radius/shortest;if(amount>0.5f)amount=0.5f;if(amount<0)amount=0;
    Ref adjustments;HRESULT hr=object(panel,L"Adjustments",adjustments);if(failed(hr))return hr;
    Value values[]={Value::integer(1),Value::real(amount)};
    return invoke(adjustments,L"Item",DISPATCH_PROPERTYPUT,2,values);
}
static HRESULT makePanel(IDispatch*shapes,float left,float top,float width,float height,const Options&o,const HighlightResult&colors,CreatedBlock&out,Ref&panel){
    const bool window=o.windowFrame;
    Value a[]={Value::integer(window?5:1),Value::real(left),Value::real(top),Value::real(width),Value::real(height)}; // rounded rectangle / rectangle
    HRESULT hr=object(shapes,L"AddShape",panel,5,a);if(failed(hr))return hr;if(failed(hr=out.add(panel)))return hr;
    DWORD color=window?(o.windowFill?o.windowColor:colors.bg):(o.background==1?colors.bg:o.backgroundColor);
    if(failed(hr=flatFill(panel,color)))return hr;
    if(window&&failed(hr=roundness(panel,width,height,o.windowRadius)))return hr;
    Ref shadow;if(failed(hr=object(panel,L"Shadow",shadow)))return hr;
    if(failed(hr=put(shadow,L"Visible",Value::integer(window&&o.windowShadow?-1:0))))return hr;
    if(window&&o.windowShadow){
        Ref c;if(failed(hr=object(shadow,L"ForeColor",c))||failed(hr=put(c,L"RGB",Value::integer(0))))return hr;
        if(failed(hr=put(shadow,L"Blur",Value::real(16)))||failed(hr=put(shadow,L"OffsetX",Value::real(0)))||failed(hr=put(shadow,L"OffsetY",Value::real(6)))||failed(hr=put(shadow,L"Transparency",Value::real(0.72f))))return hr;
    }
    return S_OK;
}
static HRESULT makeButtons(IDispatch*shapes,float left,float top,const WindowGeometry&g,const Options&o,CreatedBlock&out){
    if(!o.windowFrame||!o.windowButtons)return S_OK;
    const DWORD colors[]={0xFF5F57,0xFEBC2E,0x28C840};
    float gap=g.dotSize*0.6f;
    for(int i=0;i<3;++i){Ref dot;
        Value a[]={Value::integer(9),Value::real(left+o.windowPadding+i*(g.dotSize+gap)),Value::real(top+o.windowPadding+(g.headerHeight-g.dotSize)*0.5f),Value::real(g.dotSize),Value::real(g.dotSize)};
        HRESULT hr=object(shapes,L"AddShape",dot,5,a);if(failed(hr))return hr;if(failed(hr=out.add(dot))||failed(hr=flatFill(dot,colors[i])))return hr;
        // Decorative only. No action settings, macros or hyperlinks are attached.
        Ref shadow;if(!failed(object(dot,L"Shadow",shadow)))put(shadow,L"Visible",Value::integer(0));
    }
    return S_OK;
}
static HRESULT addTextBox(IDispatch*shapes,float left,float top,float width,float height,Ref&shape){
    Value a[]={Value::integer(1),Value::real(left),Value::real(top),Value::real(width),Value::real(height)};
    return object(shapes,L"AddTextbox",shape,5,a);
}
static float measureNumber(IDispatch*shapes,int last,const Options&o,float fs){
    Ref shape,range,font,frame;float width=-1;
    // A single off-slide, short-lived text box measures the installed Office font.
    // It is removed immediately after measurement.
    if(!failed(addTextBox(shapes,-10000,-10000,1000,200,shape))){
        wchar_t text[24];number(text,last);
        if(!failed(shapeRange(shape,range))&&!failed(put(range,L"Text",Value::text(text)))&&!failed(object(range,L"Font",font))){
            put(font,L"Name",Value::text(o.font));put(font,L"Size",Value::real(fs));fontStyle(range,0);
            if(!failed(object(shape,L"TextFrame",frame))){put(frame,L"WordWrap",Value::integer(0));put(frame,L"AutoSize",Value::integer(0));}
            width=getFloat(range,L"BoundWidth",-1);
        }
        invoke(shape,L"Delete",DISPATCH_METHOD,0,nullptr);
    }
    return width;
}
static HRESULT setColumns(IDispatch*table,float gutter,float width){
    Ref columns,one,two;Value first=Value::integer(1),second=Value::integer(2);HRESULT hr;
    if(failed(hr=object(table,L"Columns",columns))||failed(hr=object(columns,L"Item",one,1,&first))||failed(hr=object(columns,L"Item",two,1,&second)))return hr;
    if(failed(hr=put(one,L"Width",Value::real(gutter))))return hr;
    return put(two,L"Width",Value::real(width-gutter));
}
static HRESULT writeLine(IDispatch*shape,BSTR source,LineSpan line,const HighlightItem&item,const HighlightResult&colors,const Options&o,float fs,bool transparent,Progress&p){
    Ref range;HRESULT hr=shapeRange(shape,range);if(failed(hr))return hr;
    Value text=Value::textLen(source+line.start,line.length);if(!text.v.bstrVal)return E_OUTOFMEMORY;
    if(failed(hr=put(range,L"Text",text)))return hr;
    Value actual;if(failed(hr=getText(range,L"Text",actual)))return hr;
    if(SysStringLen(actual.v.bstrVal)!=(UINT)line.length||!same(actual.v.bstrVal,text.v.bstrVal))return E_FAIL;
    if(failed(hr=typography(shape,range,o,fs,false,true,colors,transparent))||failed(hr=fontStyle(range,0)))return hr;
    // Preserve multiline lexical state by slicing tokens from the FULL document.
    return applyTokens(range,item,line.start,line.length,colors,o,false,p);
}
static HRESULT writeNumber(IDispatch*shape,int value,const HighlightResult&colors,const Options&o,float fs,bool transparent){
    Ref range;HRESULT hr=shapeRange(shape,range);if(failed(hr))return hr;wchar_t text[24];number(text,value);
    if(failed(hr=put(range,L"Text",Value::text(text)))||failed(hr=typography(shape,range,o,fs,true,true,colors,transparent)))return hr;
    return fontStyle(range,0);
}
static HRESULT createBlock(IDispatch*shapes,Capture&source,const HighlightItem&item,const HighlightResult&colors,const Options&o,CreatedBlock&out,Progress&p){
    BSTR text=source.text.v.bstrVal;int length=SysStringLen(text),lines=lineCount((const Ch*)text,length),cols=displayColumns((const Ch*)text,length);
    if(lines>MAX_ROWS)return E_FAIL;if(cols<6)cols=6;Items<LineSpan> spans;if(!spans.make(lines))return E_OUTOFMEMORY;
    if(splitLines((const Ch*)text,length,spans.data(),lines)!=lines)return E_FAIL;
    if(!out.pending.make(lines*(o.lineNumbers?2:1)+4))return E_OUTOFMEMORY;
    const float fs=o.fontSize;
    const float measured=o.lineNumbers?measureNumber(shapes,o.firstLine+lines-1,o,fs):-1;
    const float gutter=o.lineNumbers?narrowGutterWidth(o.firstLine,lines,fs,measured):0;
    const float pitch=lineHeight(o,fs),height=lines*pitch;
    float width=cols*fs*0.62f+18+gutter;if(width<180)width=180;
    const float left=source.left,top=source.top;
    auto geo=[&](float w,float h,float size){return o.windowFrame?windowGeometry(w,h,size,o.windowPadding,o.windowButtons):WindowGeometry{w,h,0,0,0,0};};
    WindowGeometry g=geo(width,height,fs);
    HRESULT hr=S_OK;Ref panel;
    bool panelNeeded=o.windowFrame||(o.outputMode==1&&o.background!=0);
    if(panelNeeded){if(failed(hr=makePanel(shapes,left,top,g.width,g.height,o,colors,out,panel))||failed(hr=makeButtons(shapes,left,top,g,o,out)))return hr;}
    float x=left+g.contentX,y=top+g.contentY;
    if(o.outputMode==0){
        // Every source line, including an empty line, is a separate NATIVE table row.
        Value a[]={Value::integer(lines),Value::integer(o.lineNumbers?2:1),Value::real(x),Value::real(y),Value::real(width),Value::real(height)};
        if(failed(hr=object(shapes,L"AddTable",out.content,6,a)))return hr;Ref table;
        if(failed(hr=object(out.content,L"Table",table)))return hr;
        const wchar_t*props[]={L"FirstRow",L"LastRow",L"FirstCol",L"LastCol",L"HorizBanding",L"VertBanding"};for(int i=0;i<6;++i)put(table,props[i],Value::boolean(false));
        if(!o.windowFrame)background(out.content,o,colors);
        else {Ref fill;if(!failed(object(out.content,L"Fill",fill)))put(fill,L"Visible",Value::integer(0));}
        if(o.lineNumbers&&failed(hr=setColumns(table,gutter,width)))return hr;
        Ref rows;if(failed(hr=object(table,L"Rows",rows)))return hr;
        for(int r=0;r<lines;++r){if(!p.pump())return (HRESULT)0x80004004;Ref shape;
            int codeCol=o.lineNumbers?2:1;
            if(failed(hr=cellShape(table,r+1,codeCol,shape))||failed(hr=writeLine(shape,text,spans[r],item,colors,o,fs,o.windowFrame,p)))return hr;
            if(o.lineNumbers){if(failed(hr=cellShape(table,r+1,1,shape))||failed(hr=writeNumber(shape,o.firstLine+r,colors,o,fs,o.windowFrame)))return hr;}
            Ref row;Value index=Value::integer(r+1);if(failed(hr=object(rows,L"Item",row,1,&index))||failed(hr=put(row,L"Height",Value::real(pitch))))return hr;
        }
        if(failed(hr=applyTableBorders(table,lines,o.lineNumbers?2:1,o.lineNumbers,o,colors,p)))return hr;
        // Office can enforce a larger minimum height for the chosen installed font.
        float actualHeight=getFloat(out.content,L"Height",height);
        if(actualHeight!=height)g=geo(width,actualHeight,fs);
        if(panel&&actualHeight!=height){if(failed(hr=put(panel,L"Height",Value::real(g.height))))return hr;
            if(o.windowFrame&&failed(hr=roundness(panel,g.width,g.height,o.windowRadius)))return hr;}
        if(out.used){if(failed(hr=groupObjects(shapes,out.pending,out.used,out.decoration)))return hr;out.pending.clear();out.used=0;
            Value front=Value::integer(0);if(failed(hr=invoke(out.content,L"ZOrder",DISPATCH_METHOD,1,&front)))return hr;}
    }else{
        for(int r=0;r<lines;++r){if(!p.pump())return (HRESULT)0x80004004;Ref shape;
            if(o.lineNumbers){if(failed(hr=addTextBox(shapes,x,y+r*pitch,gutter,pitch,shape))||failed(hr=out.add(shape))||failed(hr=writeNumber(shape,o.firstLine+r,colors,o,fs,true)))return hr;}
            if(failed(hr=addTextBox(shapes,x+gutter,y+r*pitch,width-gutter,pitch,shape))||failed(hr=out.add(shape))||failed(hr=writeLine(shape,text,spans[r],item,colors,o,fs,true,p)))return hr;
            Ref line;if(!failed(object(shape,L"Line",line)))put(line,L"Visible",Value::integer(0));
        }
        if(failed(hr=groupObjects(shapes,out.pending,out.used,out.content)))return hr;out.pending.clear();out.used=0;
    }
    return S_OK;
}

static HRESULT clipboardCapture(CaptureSet&set,bool expandTabs=true){
    if(!OpenClipboard(GetForegroundWindow()))return E_FAIL;HANDLE h=GetClipboardData(13);wchar_t*p=h?(wchar_t*)GlobalLock(h):nullptr;HRESULT hr=E_FAIL;
    if(p){SIZE_T max=GlobalSize(h)/2;int n=0;while((SIZE_T)n<max&&n<=MAX_CODE&&p[n])++n;if(n>0&&n<=MAX_CODE&&(SIZE_T)n<max&&set.objects.make(1)){Value original=Value::textLen(p,n);hr=normalized(set.objects[0].text,original.v.bstrVal,expandTabs);if(!failed(hr))set.count=1;}GlobalUnlock(h);}CloseClipboard();return hr;
}
static HRESULT flattenForTable(CaptureSet&set,bool merge,bool expandTabs=true){
    if(merge&&set.count>1){int total=0;for(int i=0;i<set.count;++i){total+=(int)SysStringLen(set.objects[i].text.v.bstrVal)+(i?2:0);if(total>MAX_CODE)return E_FAIL;}BSTR b=SysAllocStringLen(nullptr,total);if(!b)return E_OUTOFMEMORY;int k=0;
        for(int i=0;i<set.count;++i){if(i){b[k++]='\r';b[k++]='\r';}BSTR t=set.objects[i].text.v.bstrVal;int n=SysStringLen(t);memcpy(b+k,t,n*2);k+=n;}b[k]=0;Value value;setBstr(value,b);return normalized(set.objects[0].text,value.v.bstrVal,expandTabs);
    }
    for(int i=0;i<set.count;++i){Value source=set.objects[i].text;HRESULT hr=normalized(set.objects[i].text,source.v.bstrVal,expandTabs);if(failed(hr))return hr;}return S_OK;
}
HRESULT executeAction(IDispatch*app,const Options&o,Action action,wchar_t*status,int sc,wchar_t*error,int ec,Progress&p){
    copystr(error,ec,L"操作未完成。若已改变部分格式，请按 Ctrl+Z 撤销本次操作。");
    const bool table=action==Action::Table||action==Action::Clipboard;const wchar_t*msg=nullptr;CaptureSet set,clipboardTargets;
    HRESULT hr=action==Action::Clipboard?clipboardCapture(set,false):captureSelection(app,set,o,action==Action::Format&&!o.basicCleanup,&msg);
    if(failed(hr)){copystr(error,ec,msg?msg:L"无法读取剪贴板的纯文本代码。");return hr;}
    if(action==Action::Clipboard){
        const wchar_t*ignored=nullptr;
        if(captureSelection(app,clipboardTargets,o,false,&ignored)==S_OK&&clipboardTargets.count){
            // Clipboard text remains the source; selection only supplies the
            // anchor and explicit replacement targets, never the pasted content.
            Capture&target=clipboardTargets.objects[0];Capture&clip=set.objects[0];
            clip.shape=target.shape;clip.decoration=target.decoration;clip.left=target.left;clip.top=target.top;
        }else clipboardTargets.count=0;
    }
    if(action==Action::Borders)for(int i=0;i<set.count;++i)if(!set.objects[i].table){
        copystr(error,ec,L"仅应用边框需要选中原生表格。请选择表格后重试；本次没有修改任何对象。\r\n可组合文本框不具有表格边框，三个边框选项不会给文本框额外画线。");return E_FAIL;}
    if(action==Action::Format)for(int i=0;i<set.count;++i)if(set.objects[i].grouped){
        copystr(error,ec,L"此对象是组合代码。修改字体、行距或容器，请勾选“替换原对象”，再点击“生成代码块”。\r\n只修改字色可直接点击“仅改配色”。本次未改动对象。");return E_FAIL;}

    int outputCount=table&&o.merge?1:set.count;
    if(table&&failed(hr=flattenForTable(set,o.merge,false))){copystr(error,ec,L"合并后超过 50,000 个字符，请拆分代码。");return hr;}
    if(o.basicCleanup&&(table||action==Action::Format))for(int i=0;i<outputCount;++i){
        hr=table?tidyText(set.objects[i].text,o):tidyCapture(set.objects[i],o);
        if(!failed(hr)&&!table)hr=concatenate(set.objects[i]);
        if(failed(hr)){copystr(error,ec,L"代码整理未完成，原文未改动。请减少本次处理内容后重试。");return hr;}
    }
    if(table)for(int i=0;i<outputCount;++i)if(lineCount((const Ch*)set.objects[i].text.v.bstrVal,SysStringLen(set.objects[i].text.v.bstrVal))>MAX_ROWS){copystr(error,ec,L"每张代码表格最多 200 行；请把较长代码拆到多页。");return E_FAIL;}
    Ref shapes;if(table&&failed(hr=getSlide(app,shapes))){copystr(error,ec,L"请切换到普通编辑视图中的一页幻灯片。");return hr;}
    Items<BSTR> texts;if(!texts.make(outputCount))return E_OUTOFMEMORY;for(int i=0;i<outputCount;++i)texts[i]=set.objects[i].text.v.bstrVal;
    HighlightResult result;
    hr=invokeEngine(o,texts.data(),outputCount,result,&p);if(failed(hr)){copystr(error,ec,result.error);return hr;}
    if(!p.pump()){copystr(error,ec,L"已取消；原文未改动。");return (HRESULT)0x80004004;}
    invoke(app,L"StartNewUndoEntry",DISPATCH_METHOD,0,nullptr);
    Items<CreatedBlock> created;if(table&&!created.make(outputCount))return E_OUTOFMEMORY;
    for(int i=0;i<outputCount;++i){wchar_t label[160],num[24];copystr(label,160,L"正在写入 PowerPoint 原生文字与格式：");number(num,i+1);catstr(label,160,num);catstr(label,160,L" / ");number(num,outputCount);catstr(label,160,num);catstr(label,160,L"\r\n取消后可用 Ctrl+Z 撤销已完成的原地修改。");p.text(label);
        hr=table?createBlock(shapes,set.objects[i],result.items[i],result,o,created[i],p):recolor(set.objects[i],result.items[i],result,o,action,p);
        if(failed(hr))break;
    }
    if(failed(hr)){if(table){for(int i=0;i<created.size();++i)created[i].cleanup();copystr(error,ec,L"未完成的代码块及新建底框已尝试移除，原对象保留。可能是字符校验、取消、组合或 Office 格式写入失败。");}return hr;}
    if(table){
        // Source deletion is opt-in and only happens after every output succeeded.
        CaptureSet&targets=action==Action::Clipboard?clipboardTargets:set;
        if(o.replace)for(int i=0;i<targets.count;++i)if(targets.objects[i].shape){hr=invoke(targets.objects[i].shape,L"Delete",DISPATCH_METHOD,0,nullptr);if(failed(hr)){copystr(error,ec,L"代码块已生成，但部分原对象未能删除；请检查页面或按 Ctrl+Z 撤销。");return hr;}
            if(targets.objects[i].decoration){hr=invoke(targets.objects[i].decoration,L"Delete",DISPATCH_METHOD,0,nullptr);if(failed(hr)){copystr(error,ec,L"新代码块已生成，但旧底框未能删除；请检查页面或按 Ctrl+Z 撤销。");return hr;}}}
        bool first=true;
        for(int i=0;i<created.size();++i){
            Value replace=Value::integer(first?-1:0);invoke(created[i].content,L"Select",DISPATCH_METHOD,1,&replace);first=false;
            if(created[i].decoration){Value add=Value::integer(0);invoke(created[i].decoration,L"Select",DISPATCH_METHOD,1,&add);}
        }
    }
    wchar_t n[24];copystr(status,sc,L"已处理 ");number(n,outputCount);catstr(status,sc,n);catstr(status,sc,table?(o.outputMode?L" 个可组合代码块":L" 张逐行代码表格"):action==Action::Borders?L" 张表格的边框":L" 个组件");
    if(set.skipped){catstr(status,sc,L"；跳过 ");number(n,set.skipped);catstr(status,sc,n);catstr(status,sc,L" 个无文本对象");}
    if(table&&o.windowFrame&&o.outputMode==0)catstr(status,sc,L"；表格与底框共同选中（非组合）");

    logEvent(table?L"Batch editable code blocks created":action==Action::Borders?L"Table borders updated":L"Batch native text formatting applied");error[0]=0;return S_OK;
}
