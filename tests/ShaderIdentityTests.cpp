#include "ShaderTransform.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstdlib>
unsigned checks{};
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
void checksum(std::vector<unsigned char>& b){DWORD c[4]{};CalculateDXBCChecksum(b.data(),static_cast<DWORD>(b.size()),c);memcpy(b.data()+4,c,16);}
int main(){using namespace correction;
 std::ifstream f("validation/shader/native.dxbc",std::ios::binary);std::vector<unsigned char> native{std::istreambuf_iterator<char>(f),{}},out;
 CHECK(normalizeNative(native,out));CHECK(out==native);
 // Reorder all three chunks and add a private metadata chunk; instructions and
 // interface chunks remain identical. The complete container hash now differs.
 std::vector<unsigned char> varied(48,0);memcpy(varied.data(),"DXBC",4);put(varied,20,1);put(varied,28,4);
 unsigned index=0;for(auto old:{76u,60u,44u}){put(varied,32+4*index++,static_cast<std::uint32_t>(varied.size()));auto n=word(native,old+4)+8;varied.insert(varied.end(),native.begin()+old,native.begin()+old+n);}
 put(varied,44,static_cast<std::uint32_t>(varied.size()));const unsigned char metadata[]{'P','R','I','V',4,0,0,0,1,2,3,4};varied.insert(varied.end(),std::begin(metadata),std::end(metadata));put(varied,24,static_cast<std::uint32_t>(varied.size()));checksum(varied);
 CHECK(sha256(varied.data(),varied.size())!=nativeHash);CHECK(normalizeNative(varied,out));CHECK(out==native);
 std::vector<unsigned char>a,b;CHECK(transform(native,a));CHECK(transform(out,b));CHECK(a==b);
 auto bad=varied;bad[48+8+200]^=1;checksum(bad);CHECK(!normalizeNative(bad,out)); // changed executable, even with valid checksum
 bad=varied;put(bad,36,word(bad,32));checksum(bad);CHECK(!normalizeNative(bad,out)); // overlapping/duplicate chunks
 bad=varied;put(bad,32,0xfffffffcu);checksum(bad);CHECK(!normalizeNative(bad,out));
 bad=varied;bad.back()^=1;CHECK(!normalizeNative(bad,out)); // corrupt container checksum
 bad=varied;auto last=word(bad,44);memcpy(bad.data()+last,"SHDR",4);checksum(bad);CHECK(!normalizeNative(bad,out)); // extra executable program
 bad=varied;auto signature=word(bad,40);bad[signature+8]^=1;checksum(bad);CHECK(!normalizeNative(bad,out));
 std::cout<<"PASS "<<checks<<" complete-program identity/container rejection checks\n";
}
