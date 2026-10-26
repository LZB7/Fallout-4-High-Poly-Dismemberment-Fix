#include "ShaderTransform.h"
#include "Correction.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <source_location>
unsigned checks{};
void check(bool v,std::source_location at=std::source_location::current()){++checks;if(!v){std::cerr<<"FAIL "<<checks<<" "<<at.file_name()<<":"<<at.line()<<"\n";std::exit(1);}}
#include "ShaderReplay.h"
int main(){
 using namespace correction;
 diag::Context c{};c.valid=c.rangesValid=true;c.entries=400000;
 std::array<std::uint32_t,84>w{};w[32]=1;w[33]=12;w[34]=c.entries;w[35]=0xdeadc0de;
 std::array<unsigned,12> starts{0,58020,64488,65535,65538,66309,67983,95535,101898,196608,262140,262143};
 for(unsigned i=0;i<12;++i){c.ranges[i]={starts[i],1,true};w[36+4*i]=(starts[i]<<16)|1;w[38+4*i]=i%2;}
 auto p=prepare(Mode::Enabled,c,w);check(p.eligible&&p.substitute);
 for(unsigned i=0;i<12;++i)check(reconstruct(w[36+4*i],p.sideband,i)==starts[i]);
 for(unsigned i=0;i<84;++i)if(i!=35)check(p.words[i]==w[i]);
 check(!prepare(Mode::Off,c,w).eligible);check(!prepare(Mode::Zero,c,w).eligible);
 c.ranges[0].start=262144;check(!prepare(Mode::Enabled,c,w).eligible);c.ranges[0]={0,65536,true};check(!prepare(Mode::Enabled,c,w).eligible);
 c.ranges[0]={1,1,true};check(!prepare(Mode::Enabled,c,w).eligible);
 c.ranges[0]={0,1,false};check(!prepare(Mode::Enabled,c,w).eligible);
 c.ranges[0]={399999,1,true};check(!prepare(Mode::Enabled,c,w).eligible);
 c.ranges[0]={0,0,true};w[36]=0;check(prepare(Mode::Enabled,c,w).eligible);
 w[33]=1;auto low=prepare(Mode::Enabled,c,w);check(low.eligible&&!low.substitute);
 auto zero=prepare(Mode::Zero,c,w);check(zero.eligible&&zero.substitute&&zero.words[35]==0);
 w[33]=13;check(!prepare(Mode::Enabled,c,w).eligible);w[33]=1;
 w[37]=2;check(!prepare(Mode::Enabled,c,w).eligible);w[37]=0;
 std::ifstream f("validation/shader/native.dxbc",std::ios::binary);std::vector<unsigned char>b{std::istreambuf_iterator<char>(f),{}},v;
 check(transform(b,v));check(shaderContract(b,false));check(shaderContract(v,true));
 DWORD h[4]{};CalculateDXBCChecksum(b.data(),b.size(),h);check(memcmp(h,b.data()+4,16)==0);
 CalculateDXBCChecksum(v.data(),v.size(),h);check(memcmp(h,v.data()+4,16)==0);
 auto bad=b;bad[90]^=1;std::vector<unsigned char>rejected;check(!transform(bad,rejected));
 // Every original instruction byte is retained; only temp declaration/length change.
 auto restored=v;restored.erase(restored.begin()+84+478*4,restored.begin()+84+478*4+144);
 put(restored,24,b.size());put(restored,80,89548);put(restored,88,22387);put(restored,84+41*4,73);
 memcpy(restored.data()+4,b.data()+4,16);check(restored==b);
 std::ofstream("validation/shader/corrected.dxbc",std::ios::binary).write((char*)v.data(),v.size());
 Com<ID3DBlob> asmBlob;check(SUCCEEDED(D3DDisassemble(v.data(),v.size(),0,nullptr,asmBlob.put())));
 std::ofstream("validation/shader/corrected.asm").write((char*)asmBlob.p->GetBufferPointer(),asmBlob.p->GetBufferSize());
 Com<ID3D11Device>device;Com<ID3D11DeviceContext>context;D3D_FEATURE_LEVEL level{};
 HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,device.put(),&level,context.put());
 check(SUCCEEDED(hr));Com<ID3D11ComputeShader> original,corrected;
 check(SUCCEEDED(device.p->CreateComputeShader(b.data(),b.size(),nullptr,original.put())));
 check(SUCCEEDED(device.p->CreateComputeShader(v.data(),v.size(),nullptr,corrected.put())));
 differential(device.p,context.p,original.p,corrected.p,b);
 std::ofstream("validation/shader-validation.json")<<"{\"checks\":"<<checks<<",\"native_sha256\":\""<<sha256(b.data(),b.size())
  <<"\",\"variant_sha256\":\""<<sha256(v.data(),v.size())<<"\",\"variant_bytes\":"<<v.size()
  <<",\"stock_instructions_preserved\":true,\"checksum_validated\":true,\"d3d11_warp_creation_passed\":true,\"gameplay\":false}\n";
 std::cout<<"PASS "<<checks<<" correction/shader checks; native + corrected shaders created on D3D11 WARP\n";
}

