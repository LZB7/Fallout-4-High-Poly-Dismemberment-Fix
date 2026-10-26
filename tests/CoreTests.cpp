#include "Core.h"
#include <iostream>
#include <limits>
#include <cstdlib>
int checks{};
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
int main() {
    using namespace diag;
    CHECK(supported(runtime240)); CHECK(!supported(0x010B0DD0)); CHECK(!supported(0));
    for(auto s:{58020u,64488u,66309u,67983u,98403u,65535u,65538u}) {
        auto d=decode({s,12,true},(s<<16)|12,129450);
        CHECK(d.matches); CHECK(d.startAligned);CHECK(d.lossy==(s>65535));
        CHECK(d.retainedStart==(s&0xffff));CHECK(d.inBounds);
    }
    CHECK(!decode({65538,1,true},(65538u<<16)|1,100000).consumedAligned);
    CHECK(decode({0,65536,true},0,300000).lossy);
    CHECK(!decode({0xffffffff,0xffffffff,true},0xffffffff,0xffffffff).inBounds);
    CHECK(decode({0xffffffff,0xffffffff,true},0xffffffff,0xffffffff).fullEnd==17179869180ull);
    CHECK(!decode({3,2,true},3,100).matches);
    CHECK(!decode({3,2,false},(3u<<16)|2,100).matches);
    CHECK(quote(std::string("a\"\\\n\0",5))=="\"a\\\"\\\\\\u000a\\u0000\"");
    Correlator<2> c; Context a{};a.state=1;a.epoch=1;a.id=10; Context b=a;b.id=11;
    c.put(100,a,1);c.put(101,b,2); Context out{};
    CHECK(c.get(1,100,1,3,out));CHECK(out.id==10);
    CHECK(!c.get(2,100,1,3,out));CHECK(!c.get(1,100,2,3,out));
    c.put(100,b,4);CHECK(c.replacements==1);CHECK(c.get(1,100,1,4,out)&&out.id==11);
    c.put(102,a,5);CHECK(c.evictions==1);
    c.retire(1,1);CHECK(!c.get(1,102,1,5,out));
    c.put(103,a,6);CHECK(!c.get(1,103,1,120007,out));CHECK(c.expired==1);
    c.put(104,a,120008);c.clear();CHECK(!c.get(1,104,1,120009,out));
    std::cout<<checks<<" checks passed\n";
}
