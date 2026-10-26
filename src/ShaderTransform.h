#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cstring>
#include "../vendor/dxbc-checksum/DXBCChecksum.h"
namespace correction {
inline constexpr char nativeHash[]="99a02d014da6f41ad6808afc1ca9b3c6be5f6eeac30f22586ee6ecdda986fdbf";
template<class T> struct Com {
 T* p{};~Com(){if(p)p->Release();} Com()=default;Com(const Com&)=delete;
 Com& operator=(const Com&)=delete;
 T** put(){if(p){p->Release();p=nullptr;}return &p;}
};
inline std::string sha256(const void* p,std::size_t n) {
 BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};std::array<unsigned char,32> digest{};
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 auto status=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0);
 if(status>=0)status=BCryptHashData(hash,(PUCHAR)p,static_cast<ULONG>(n),0);
 if(status>=0)status=BCryptFinishHash(hash,digest.data(),32,0);
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 if(status<0)return {};std::string out;constexpr char h[]="0123456789abcdef";
 for(auto v:digest){out+=h[v>>4];out+=h[v&15];}return out;
}
inline std::uint32_t word(const std::vector<unsigned char>& b,std::size_t o) {
 std::uint32_t v{};if(o<=b.size()&&b.size()-o>=4)memcpy(&v,b.data()+o,4);return v;
}
inline void put(std::vector<unsigned char>& b,std::size_t o,std::uint32_t v){memcpy(b.data()+o,&v,4);}
// Experimental container equivalence only: never infer compatibility from a
// similar interface or a few opcodes. Reconstruct the exact known container
// from identical full executable/signature chunks and require its known hash.
inline bool normalizeNative(const std::vector<unsigned char>& b,std::vector<unsigned char>& out) {
 if(b.size()==89632&&sha256(b.data(),b.size())==nativeHash){out=b;return true;}
#ifndef HPDF_EXPERIMENTAL
 return false;
#else
 if(b.size()<44||b.size()>4*1024*1024||memcmp(b.data(),"DXBC",4)||word(b,20)!=1||word(b,24)!=b.size())return false;
 auto count=word(b,28);if(!count||count>32||32+4ull*count>b.size())return false;
 auto checksumInput=b;DWORD checksum[4]{};CalculateDXBCChecksum(checksumInput.data(),static_cast<DWORD>(checksumInput.size()),checksum);
 if(memcmp(checksum,b.data()+4,16))return false;
 std::array<std::uint32_t,3> selected{};std::vector<std::pair<std::uint32_t,std::uint32_t>> spans;
 for(unsigned i=0;i<count;++i){auto offset=word(b,32+4*i);
  if(offset%4||offset<32+4*count||std::uint64_t(offset)+8>b.size())return false;
  auto size=word(b,offset+4);auto end=std::uint64_t(offset)+8+size;if(end>b.size())return false;
  for(auto range:spans)if(offset<range.second&&end>range.first)return false;
  spans.emplace_back(offset,static_cast<std::uint32_t>(end));
  int index=!memcmp(b.data()+offset,"ISGN",4)?0:!memcmp(b.data()+offset,"OSGN",4)?1:!memcmp(b.data()+offset,"SHEX",4)?2:-1;
  if(index>=0){if(selected[index])return false;selected[index]=offset;}
  else if(memcmp(b.data()+offset,"SDBG",4)&&memcmp(b.data()+offset,"SPDB",4)&&memcmp(b.data()+offset,"STAT",4)&&memcmp(b.data()+offset,"PRIV",4)&&memcmp(b.data()+offset,"ILDN",4))return false;
 }
 if(!selected[0]||!selected[1]||!selected[2]||word(b,selected[0]+4)!=8||word(b,selected[1]+4)!=8||word(b,selected[2]+4)!=89548)return false;
 out.assign(89632,0);memcpy(out.data(),"DXBC",4);put(out,20,1);put(out,24,89632);put(out,28,3);put(out,32,44);put(out,36,60);put(out,40,76);
 memcpy(out.data()+44,b.data()+selected[0],16);memcpy(out.data()+60,b.data()+selected[1],16);memcpy(out.data()+76,b.data()+selected[2],89556);
 CalculateDXBCChecksum(out.data(),static_cast<DWORD>(out.size()),checksum);memcpy(out.data()+4,checksum,16);
 if(sha256(out.data(),out.size())!=nativeHash){out.clear();return false;}return true;
#endif
}
// A hash-pinned token transformation, not a source reconstruction. All stock
// instructions remain byte-identical except dcl_temps 73->74 and code length.
inline bool transform(const std::vector<unsigned char>& native,std::vector<unsigned char>& result) {
 if(native.size()!=89632||sha256(native.data(),native.size())!=nativeHash)return false;
 if(word(native,24)!=native.size()||word(native,28)!=3||word(native,40)!=76
    ||memcmp(native.data()+76,"SHEX",4)||word(native,80)!=89548)return false;
 const auto code=84u;const auto count=word(native,code+4);
 if(count!=22387||word(native,code)!=0x50050)return false; // cs_5_0
 unsigned pos=2;bool found=false;
 while(pos<count) {
  auto token=word(native,code+pos*4);auto len=(token>>24)&127;
  if(!len||len>count-pos)return false;
  if(pos==471) {
   constexpr std::array<std::uint32_t,7> expected{0x07000055,0x100082,13,0x10000a,14,0x4001,16};
   if(memcmp(native.data()+code+pos*4,expected.data(),28))return false;found=true;
  }
  pos+=len;
 }
 if(!found||word(native,code+40*4)!=0x02000068||word(native,code+41*4)!=73)return false;
 // r73.x = ((cb0[8].w >> (2*r12.w)) & 3) << 16; r13.w |= r73.x.
 // Insert immediately after the sole range-start unpack, before end calculation.
 constexpr std::array<std::uint32_t,36> patch{
  0x07000029,0x100012,73,0x10003a,12,0x4001,1,
  0x08000055,0x100012,73,0x20803a,0,8,0x10000a,73,
  0x07000001,0x100012,73,0x10000a,73,0x4001,3,
  0x07000029,0x100012,73,0x10000a,73,0x4001,16,
  0x0700003c,0x100082,13,0x10003a,13,0x10000a,73};
 result=native;const auto at=code+478*4;
 result.insert(result.begin()+at,reinterpret_cast<const unsigned char*>(patch.data()),
               reinterpret_cast<const unsigned char*>(patch.data()+patch.size()));
 put(result,code+41*4,74);put(result,code+4,count+patch.size());
 put(result,80,89548+patch.size()*4);put(result,24,static_cast<std::uint32_t>(result.size()));
 DWORD hash[4]{};CalculateDXBCChecksum(result.data(),static_cast<DWORD>(result.size()),hash);
 memcpy(result.data()+4,hash,16);return true;
}
inline bool shaderContract(const std::vector<unsigned char>& b,bool modified) {
 Com<ID3DBlob> text;
 if(FAILED(D3DDisassemble(b.data(),b.size(),0,nullptr,text.put())))return false;
 std::string s(static_cast<const char*>(text.p->GetBufferPointer()),text.p->GetBufferSize());
 for(auto part:{"cs_5_0","dcl_constantbuffer CB0[21], immediateIndexed","dcl_resource_raw t0","dcl_resource_raw t1",
 "dcl_uav_structured u0, 112","dcl_thread_group 1, 1, 1"})
  if(s.find(part)==std::string::npos)return false;
 for(unsigned i=1;i<8;++i)if(s.find("dcl_uav_raw u"+std::to_string(i))==std::string::npos)return false;
 if(!modified&&s.find("cb0[8].w")!=std::string::npos)return false;
 return !modified||s.find("ushr r73.x, cb0[8].w, r73.x")!=std::string::npos;
}
}

