#include "basicformat.h"
#include <fstream>
#include <string>
#include <vector>
#include <cassert>
// Self-contained executable fixture. The runner compiles and executes both
// versions, comparing stdout, rather than only checking whitespace snapshots.
int main(int argc, char** argv) {
    assert(argc == 3);
    std::u16string source = uR"fixture(#include <iostream>
#include <vector>
#include <utility>
#include <array>
#define STR(x) #x
#define stringify(x) #x
template<typename T=int> T twice(T x){return x+x;}
int add(int a,int b){return a+b;}
struct Foo {int n;};
int main(){
std::vector<std::pair<int,int>> v={{1,2},{3,4}};
int i=1,j=2,a=7,b=3,n1=4,n2=5;
std::cout<<i<<"  "<<n1<<"   "<<n2<<std::endl;
int mask=(1<<3)|(a&b); mask^=2; mask>>=1;
std::cout<<mask<<" "<<(a<b?j:i)<<" "<<v[0].first<<"\n";
int* p=&a; const Foo& ref=Foo{9};
std::cout<<*p<<" "<<ref.n<<" "<<a*b+(a/b)<<"\n";
for(int k=0;k<2;k++){std::cout<<k;}
std::cout<<" "<<i+++j<<" "<<--i+-j<<"\n";
std::cout<<1.25e-3+0x1.fp+2<<" "<<1'000+2<<"\n";
std::cout<<STR(a+b)<<" "<<STR((a,b))<<" "<<R"tag(<<   >>  x=1)tag"<<"\n";
std::cout<<(true?std::max(a,b):v[1].first)<<"\n";
std::array<int,(1<<2)> arr={1,2,3,4};
int(*fp)(int,int)=add; int raw[2]={6,7}; int(&refArray)[2]=raw;
std::cout<<fp(arr[1],refArray[0])<<" "<<twice<>(3)<<" "<<stringify(a+b)<<"\n";
std::cout<<stringify /* macro call */
(a+b)<<"\n";
int multi=
add(
a,
b
)+
2;
std::cout
<<multi
<<"\n";
return 0;
}
)fixture";
    std::vector<char16_t> buffer(50001);
    int n = codeink::basicFormat(source.data(), source.size(), u"cpp", buffer.data(), buffer.size());
    assert(n >= 0);
    auto write = [](const char* path, const std::u16string& code) {
        std::ofstream out(path, std::ios::binary);
        for (char16_t c : code) { assert(c < 128); out.put((char)c); }
        assert(out.good());
    };
    write(argv[1], source); write(argv[2], std::u16string(buffer.data(), n));
}
