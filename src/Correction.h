#pragma once
#include "Core.h"
#include <array>
#include <cstdint>
namespace correction {
enum class Mode : unsigned { Off=0, Enabled=1, Zero=2 };
struct Plan {
 bool eligible{}, substitute{}; std::uint32_t sideband{};
 const char* reason="correction disabled";
 std::array<std::uint32_t,84> words{};
};
inline std::uint32_t reconstruct(std::uint32_t packed,std::uint32_t sideband,unsigned i) {
 return (packed>>16)|(((sideband>>(2*i))&3u)<<16);
}
inline Plan prepare(Mode mode,const diag::Context& c,const std::array<std::uint32_t,84>& w) {
 Plan p{};p.words=w;
 if(mode==Mode::Off)return p;
 auto reject=[&](const char* s){p.reason=s;return p;};
 if(!c.valid||!c.rangesValid)return reject("unknown source mapping");
 if(c.childCount>12)return reject("source child count exceeds shader records");
 if(w[32]!=1||w[33]==0||w[33]>12||w[34]!=c.entries)return reject("constant header mismatch");
 bool high=false;
 for(unsigned i=0;i<w[33];++i) {
  auto r=c.ranges[i];
  if(!r.valid)return reject("unknown active record");
  if(r.start>0x3ffff)return reject("start exceeds 18 bits");
  if(r.triangles>0xffff)return reject("count exceeds 16 bits");
  if(r.start%3)return reject("unaligned source start");
  if(std::uint64_t(r.start)+3ull*r.triangles>c.entries)return reject("source range out of bounds");
  if(w[36+4*i]!=((r.start<<16)|(r.triangles&0xffff)))return reject("packed record differs from source");
  // Preserve all three native flag words, including disabled/empty records.
  for(unsigned f=1;f<=3;++f)if(w[36+4*i+f]>1)return reject("unknown record flag");
  p.sideband|=((r.start>>16)&3u)<<(2*i);high|=r.start>65535;
 }
 if(mode==Mode::Zero&&high)return reject("zero-sideband test requires native-representable starts");
 p.eligible=true;p.substitute=high||mode==Mode::Zero;
 p.reason=p.substitute?"eligible; awaiting shader/resource validation":"native-representable operation";
 p.words[35]=mode==Mode::Zero?0:p.sideband;
 return p;
}
}

