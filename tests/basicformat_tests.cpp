#include "basicformat.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
using namespace codeink;
using U = std::u16string;
static U tidy(const U& source, const Ch* language = u"cpp") {
    std::vector<Ch> buffer(50001);
    int n = basicFormat(source.data(), source.size(), language, buffer.data(), buffer.size());
    assert(n >= 0); return U(buffer.data(), n);
}
static void check(const U& source, const U& expected, const Ch* language = u"cpp") {
    U result = tidy(source, language);
    if (result != expected) {
        auto print=[](const U& s){for(Ch c:s)std::cerr<<(c=='\r'?"\\r":c=='\n'?"\\n":std::string(1,(char)c));std::cerr<<'\n';};
        print(source); print(result); print(expected); assert(false);
    }
    assert(tidy(result, language) == result);
    assert(lineCount(source.data(), source.size()) == lineCount(result.data(), result.size()));
}
int main() {
    check(u"int main(){\rint x=1+2;\r  if(x!=0){\rx+=3;\r}\r}",
          u"int main() {\r    int x = 1 + 2;\r    if (x != 0) {\r        x += 3;\r    }\r}");
    check(u"x=-1; f(a,b,  c); i++; --j; a->b; ns::name;",
          u"x = -1; f(a, b, c); i++; --j; a->b; ns::name;");
    check(u"std::vector<int> x; int* p=&x; double n=1e-3;",
          u"std::vector<int> x; int* p = &x; double n = 1e-3;");
    check(u"auto s=R\"tag(a=1,\r\t  \"x\" { })tag\"; x=2;",
          u"auto s = R\"tag(a=1,\r\t  \"x\" { })tag\"; x = 2;");
    check(u"auto s=u8R\"tag(a=1,\t b)tag\";", u"auto s = u8R\"tag(a=1,\t b)tag\";");
    check(u"s=\"a=b,  c\\\"d\t\"; // x=1,  y  \r/* { x=1,\r\t } */\rx=2;",
          u"s = \"a=b,  c\\\"d\t\"; // x=1,  y  \r/* { x=1,\r\t } */\rx = 2;");
    check(u"#define SUM(x,y) \\\r ((x)+(y))\rx=2;", u"#define SUM(x,y) \\\r ((x)+(y))\rx=2;");
    check(u"def f(a,b):\n  x=1+2\n  if x!=0:\n    print(a,b)\n  return x\n",
          u"def f(a, b):\n  x = 1 + 2\n  if x != 0:\n    print(a, b)\n  return x\n", u"python");
    check(u"if ok:\r\tx=1\r        y=2", u"if ok:\r        x = 1\r        y = 2", u"python");
    check(u"s='''{ a=1,\r\t  x }'''\rx=2 # a=1,  b  ", u"s = '''{ a=1,\r\t  x }'''\rx = 2 # a=1,  b  ", u"python");
    check(u"s=@\"a=1,\"\"  b\"; x=2;", u"s = @\"a=1,\"\"  b\"; x = 2;", u"csharp");
    check(u"const r=/a=1,  [}]/g;\nx=2;", u"const r = /a=1,  [}]/g;\nx = 2;", u"js");
    check(u"const s=`a=${`b=${x+1}`}`; x=2;", u"const s = `a=${`b=${x+1}`}`; x=2;", u"js");
    check(u"{\n\"a\":1,\n\"b\":{\n\"c\":2\n}\n}", u"{\n    \"a\": 1,\n    \"b\": {\n        \"c\": 2\n    }\n}", u"json");
    for (const Ch* lang : {u"bash",u"powershell",u"yaml",u"markdown",u"makefile",u"html",u"tsx",u"rust",u"text"})
        check(u"\t x=1,  y\n  content  \n", u"\t x=1,  y\n  content  \n", lang);
    check(u"x=\"unterminated  \t", u"x = \"unterminated  \t");
    check(u"x=1\r\n\r\n",u"x = 1\r\n\r\n");
    check(u"a?" u"?=b; c>>>=1; d&&=e; x<=>y;", u"a ?" u"?= b; c >>>= 1; d &&= e; x <=> y;", u"ts");
    check(u"x=a*b/2; y=a[1:2];", u"x = a * b / 2; y = a[1:2];", u"python");
    check(u"s=f\"value={item[\"key\"]}\"; x=1", u"s = f\"value={item[\"key\"]}\"; x=1", u"python");
    check(u"if (x:=1):\n    y=2", u"if (x := 1):\n    y = 2", u"python");
    // Regression from the user: literal NBSPs/spaces are data, the stream
    // operators outside those strings should all get a single space.
    check(u"cout<<i<<\"\u00a0 \"<<n1<<\"\u00a0\u00a0 \"<<n2<<endl;",
          u"cout << i << \"\u00a0 \" << n1 << \"\u00a0\u00a0 \" << n2 << endl;");
    check(u"cin>>a>>b; mask=1<<n; flags>>=2; x<<=1;",
          u"cin >> a >> b; mask = 1 << n; flags >>= 2; x <<= 1;");
    check(u"std::vector<std::pair<int,int>> v; cout<<v.size()<<endl;",
          u"std::vector<std::pair<int, int>> v; cout << v.size() << endl;");
    check(u"std::map<int,std::vector<long>> v; f<int>(a,b);",
          u"std::map<int, std::vector<long>> v; f<int>(a, b);");
    check(u"if(a<b&&b>c||x<=y){result=a*b+c/d-e%f;}",
          u"if (a < b && b > c || x <= y) { result = a * b + c / d - e % f; }");
    check(u"v=(a&b)|(c^d); bits&=mask; bits|=flag; bits^=1;",
          u"v = (a & b) | (c ^ d); bits &= mask; bits |= flag; bits ^= 1;");
    check(u"int* p=&a; const Foo& r=*p; int&& rr=1; return -n;",
          u"int* p = &a; const Foo& r = *p; int&& rr = 1; return -n;");
    check(u"x=i+++j; y=--i+-j; z=!flag; t=~mask;",
          u"x = i++ + j; y = --i + -j; z = !flag; t = ~mask;");
    check(u"answer=ok?a:b; nested=a?b?c:d:e;",
          u"answer = ok ? a : b; nested = a ? b ? c : d : e;");
    check(u"x=ok?std::max(a,b):arr[i];", u"x = ok ? std::max(a, b) : arr[i];");
    check(u"for( int i=0;i<n;i++ ){f( a[i] ,b );}else{g();}",
          u"for (int i = 0; i < n; i++) { f(a[i], b); } else { g(); }");
    check(u"for(;;){break;}", u"for (;;) { break; }");
    check(u"for(auto& item:items){use(item);}", u"for (auto& item : items) { use(item); }");
    check(u"obj.method( a[ i ], b ); p->member; obj.*ptr; p->*member;",
          u"obj.method(a[i], b); p->member; obj.*ptr; p->*member;");
    check(u"bool operator<<(int x){return x<3;}", u"bool operator<<(int x) { return x < 3; }");
    check(u"n=1.25e-3+0x1.fp+2;", u"n = 1.25e-3 + 0x1.fp+2;");
    check(u"x=a**b; y=a//b; x**=2; y//=2; f(*args,**kw);",
          u"x = a ** b; y = a // b; x **= 2; y //= 2; f(*args, **kw);", u"python");
    check(u"const x=a??b; x?" u"?=c; a&&=b; c||=d; n>>>=1; x=obj?.value;",
          u"const x = a ?? b; x ?" u"?= c; a &&= b; c ||= d; n >>>= 1; x = obj?.value;", u"js");
    check(u"int? n=null; var x=n??0;", u"int? n = null; var x = n ?? 0;", u"csharp");
    check(u"{\n\"a\":[\n1,\n{\n\"b\":2\n}\n]\n}",
          u"{\n    \"a\": [\n        1,\n        {\n            \"b\": 2\n        }\n    ]\n}", u"json");
    check(u"cout\n<<a\n<<b;", u"cout\n    << a\n    << b;");
    check(u"auto value=\nfoo(\na,\nb\n);", u"auto value =\n    foo(\n    a,\n    b\n);");
    check(u"template<typename T=int>\nT f(T x){\nreturn x+\n1;\n}",
          u"template<typename T=int>\nT f(T x) {\n    return x +\n        1;\n}");
    check(u"std::array<int,(1<<2)> a; int(*fp)(int,int); int(&ref)[2]=a2;",
          u"std::array<int, (1<<2)> a; int (*fp)(int, int); int (&ref)[2] = a2;");
    check(u"#define stringify(x) #x\ncout<<stringify(a+b);",
          u"#define stringify(x) #x\ncout << stringify(a+b);");
    check(u"#define stringify(x) #x\ncout<<stringify /* keep */\n(a+b);",
          u"#define stringify(x) #x\ncout << stringify /* keep */\n(a+b);");
    check(u"// continued\\\nx=1;", u"// continued\\\nx=1;");
    check(u"int a<:2:>=<%1,2%>;", u"int a<:2:>=<%1,2%>;");
    check(u"auto x=0xe+value;", u"auto x=0xe+value;");
    check(u"cout<<STR(a+b)<<WIDTH*HEIGHT;", u"cout << STR(a+b) << WIDTH * HEIGHT;");
    check(u"n=1'000+2;", u"n = 1'000 + 2;");
    check(u"def f(x=1,y:int=2):\n  return g(x = x,y = y)", u"def f(x=1, y: int = 2):\n  return g(x=x, y=y)", u"python");
    check(u"", u"");
    Ch small[4] = {}; assert(basicFormat(u"x=1",3,u"cpp",small,4)==-1);
    // Exercise bounds/termination with deterministic incomplete snippets.
    unsigned seed=31; const U alphabet=u"ab01 =+-*/(){}[]\"'`#,:;\t\r\n";
    for(int t=0;t<3000;++t){ U source; for(int j=0;j<t%97;++j){seed=seed*1664525+1013904223;source+=alphabet[seed%alphabet.size()];}
        U result=tidy(source,t%2?u"cpp":u"python");
        assert(lineCount(source.data(),source.size())==lineCount(result.data(),result.size()));
    }
    std::cout << "PASS: basic cleanup, literal/comment protection, language guards, idempotence, line preservation, capacity and 3000 incomplete snippets\n";
}
