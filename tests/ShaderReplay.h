// Offline D3D11 WARP replay: synchronous staging reads occur only in this test.
#include "ShaderResources.h"
#include "LegacyBindings.h"
#include "BindingTelemetryTests.h"
std::vector<unsigned char> readFile(const char* path) {
 std::ifstream f(path,std::ios::binary);check(bool(f));
 return {std::istreambuf_iterator<char>(f),{}};
}
struct TestBuffer {
 correction::Com<ID3D11Buffer> buffer;
 correction::Com<ID3D11ShaderResourceView> srv;
 correction::Com<ID3D11UnorderedAccessView> uav;
};
void makeBuffer(ID3D11Device* device,TestBuffer& out,std::vector<unsigned char> data,bool input,bool structured=false) {
 data.resize((data.size()+(input?15:3))&~std::size_t(input?15:3));
 D3D11_BUFFER_DESC b{};b.ByteWidth=data.size();b.BindFlags=input?D3D11_BIND_SHADER_RESOURCE:D3D11_BIND_UNORDERED_ACCESS;
 b.MiscFlags=structured?D3D11_RESOURCE_MISC_BUFFER_STRUCTURED:D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
 b.StructureByteStride=structured?112:0;
 // Structured buffers must have an exact multiple of their stride.
 if(structured){b.ByteWidth=112;data.resize(112);}
 D3D11_SUBRESOURCE_DATA initial{data.data(),0,0};
 check(SUCCEEDED(device->CreateBuffer(&b,&initial,out.buffer.put())));
 if(input) {
  D3D11_SHADER_RESOURCE_VIEW_DESC d{};d.Format=DXGI_FORMAT_R32_TYPELESS;d.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;
  d.BufferEx.NumElements=b.ByteWidth/4;d.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
  check(SUCCEEDED(device->CreateShaderResourceView(out.buffer.p,&d,out.srv.put())));
 } else {
  D3D11_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=structured?DXGI_FORMAT_UNKNOWN:DXGI_FORMAT_R32_TYPELESS;
  d.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;d.Buffer.NumElements=structured?1:b.ByteWidth/4;
  d.Buffer.Flags=structured?0:D3D11_BUFFER_UAV_FLAG_RAW;
  check(SUCCEEDED(device->CreateUnorderedAccessView(out.buffer.p,&d,out.uav.put())));
 }
}
#include "PooledInputs.h"
std::array<std::vector<unsigned char>,8> replay(ID3D11Device* device,ID3D11DeviceContext* ctx,
 ID3D11ComputeShader* shader,bool thigh,unsigned sideband,unsigned layout=0,bool right=false) {
 using namespace correction;
 auto constantBytes=readFile(right?"validation/shader/right-thigh-constants.bin":thigh?"validation/shader/thigh-constants.bin":"validation/shader/arm-constants.bin");
 std::array<unsigned,84>w{};memcpy(w.data(),constantBytes.data(),336);w[35]=sideband;
 diag::Context c{};c.valid=c.rangesValid=true;c.vertices=22708;c.triangles=43150;c.entries=129450;c.childCount=w[33];
 std::uint64_t entries{};
 constexpr unsigned rightStarts[]{61620,64488,66309,67983,71415};
 for(unsigned i=0;i<w[33];++i){c.ranges[i]={right?rightStarts[i]:(w[36+4*i]>>16)+(thigh?65536:0),w[36+4*i]&65535,true};entries+=3ull*c.ranges[i].triangles;}
 auto plan=prepare(Mode::Enabled,c,w);check(plan.eligible);
 if(sideband){check(plan.sideband==sideband);for(unsigned i=0;i<w[33];++i)check(reconstruct(w[36+4*i],sideband,i)==c.ranges[i].start);}
 std::array<TestBuffer,2> input;std::array<TestBuffer,8> output;TestBuffer pool;
 if(layout)pooledInputs(device,input,pool,layout);
 else {
  makeBuffer(device,input[0],readFile("validation/shader/indices.bin"),true);
  makeBuffer(device,input[1],readFile("validation/shader/vertices.bin"),true);
 }
 if(layout==3)rawTailProbe(device,ctx,input[0].srv.p);
 std::array<std::uint64_t,8> sizes{112,entries*2*w[28],c.entries*2ull,entries*8,entries*4,entries*8,entries*8,entries*8};
 for(unsigned i=0;i<8;++i) {
  std::vector<unsigned char> data(sizes[i]);
  if(!i){auto words=reinterpret_cast<unsigned*>(data.data());words[0]=words[1]=words[2]=0x7f7fffff;
   words[4]=words[5]=words[6]=0xff7fffff;words[15]=0xff;}
  makeBuffer(device,output[i],data,false,i==0);
 }
 Com<ID3D11Buffer>cb;D3D11_BUFFER_DESC bd{};bd.ByteWidth=336;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 D3D11_SUBRESOURCE_DATA initial{w.data(),0,0};check(SUCCEEDED(device->CreateBuffer(&bd,&initial,cb.put())));
 ctx->CSSetConstantBuffers(0,1,&cb.p);
 ID3D11ShaderResourceView* srvs[]{input[0].srv.p,input[1].srv.p};
 ID3D11UnorderedAccessView* uavs[8]{};for(unsigned i=0;i<8;++i)uavs[i]=output[i].uav.p;
 ctx->CSSetShaderResources(0,2,srvs);ctx->CSSetUnorderedAccessViews(0,8,uavs,nullptr);
 BindingTelemetry telemetry{};
 check(bindings(ctx,device,c,w,&telemetry));check(legacyBindings(ctx,device,c,w)==(layout==0));
 check(telemetry.attempted&&telemetry.viewsCaptured&&telemetry.check==BindingCheck::Passed);
 for(unsigned i=0;i<10;++i)check(telemetry.slots[i].viewDescCaptured&&telemetry.slots[i].bufferDescCaptured);
 telemetryPredicates(telemetry);
 // Negative resource contract: missing source and missing destination are rejected.
 ID3D11ShaderResourceView* noSrv{};ctx->CSSetShaderResources(1,1,&noSrv);check(!bindings(ctx,device,c,w,&telemetry));
 check(!legacyBindings(ctx,device,c,w));check(telemetry.check==BindingCheck::MissingView&&telemetry.failedBinding==1);
 check(telemetry.slots[9].bufferDescCaptured); // Capture other slots despite early rejection.
 ctx->CSSetShaderResources(0,2,srvs);
 ID3D11UnorderedAccessView* noUav{};ctx->CSSetUnorderedAccessViews(7,1,&noUav,nullptr);check(!bindings(ctx,device,c,w,&telemetry));
 check(!legacyBindings(ctx,device,c,w));check(telemetry.check==BindingCheck::MissingView&&telemetry.failedBinding==9);
 ctx->CSSetUnorderedAccessViews(0,8,uavs,nullptr);
 auto alias=uavs[6];ctx->CSSetUnorderedAccessViews(7,1,&alias,nullptr);check(!bindings(ctx,device,c,w,&telemetry));
 check(!legacyBindings(ctx,device,c,w));
 check(telemetry.check==BindingCheck::AliasedResource||telemetry.check==BindingCheck::MissingView);
 ctx->CSSetUnorderedAccessViews(0,8,uavs,nullptr);
 // Valid D3D subview with insufficient logical capacity remains rejected.
 D3D11_SHADER_RESOURCE_VIEW_DESC subDesc{};srvs[0]->GetDesc(&subDesc);
 D3D11_BUFFER_DESC inputDesc{};input[0].buffer.p->GetDesc(&inputDesc);
 subDesc.BufferEx.FirstElement=4;subDesc.BufferEx.NumElements=std::min(c.entries/2-1,inputDesc.ByteWidth/4-4);
 Com<ID3D11ShaderResourceView> subview;
 check(SUCCEEDED(device->CreateShaderResourceView(input[0].buffer.p,&subDesc,subview.put())));
 ctx->CSSetShaderResources(0,1,&subview.p);
 check(!bindings(ctx,device,c,w,&telemetry)&&!legacyBindings(ctx,device,c,w));
 check(telemetry.check==BindingCheck::SrvViewCapacity&&telemetry.failedBinding==0);
 check(telemetry.slots[0].viewOffsetBytes==16);
 ctx->CSSetShaderResources(0,2,srvs);
 ctx->CSSetShader(shader,nullptr,0);ctx->Dispatch(1,1,1);
 Com<ID3D11ComputeShader> selectedAfterDispatch;ctx->CSGetShader(selectedAfterDispatch.put(),nullptr,nullptr);
 check(selectedAfterDispatch.p==shader); // Same state observation used by runtime telemetry.
 ID3D11ShaderResourceView* emptySrvs[2]{};ID3D11UnorderedAccessView* emptyUavs[8]{};
 ctx->CSSetShaderResources(0,2,emptySrvs);ctx->CSSetUnorderedAccessViews(0,8,emptyUavs,nullptr);ctx->CSSetShader(nullptr,nullptr,0);
 std::array<std::vector<unsigned char>,8> result;
 for(unsigned i=0;i<8;++i) {
  D3D11_BUFFER_DESC desc{};output[i].buffer.p->GetDesc(&desc);auto width=desc.ByteWidth;
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.MiscFlags=0;desc.StructureByteStride=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  Com<ID3D11Buffer> staging;check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,staging.put())));
  ctx->CopyResource(staging.p,output[i].buffer.p);D3D11_MAPPED_SUBRESOURCE mapped{};
  check(SUCCEEDED(ctx->Map(staging.p,0,D3D11_MAP_READ,0,&mapped)));
  result[i].assign((unsigned char*)mapped.pData,(unsigned char*)mapped.pData+width);ctx->Unmap(staging.p,0);
 }
 unsigned count{};memcpy(&count,result[0].data()+96,4);check(count<=2*entries);
 std::cout<<(right?"right thigh":thigh?"left thigh":"arm")<<" layout "<<layout<<" sideband "<<sideband<<": "<<count<<" generated vertices\n";
 return result;
}
void differential(ID3D11Device* d,ID3D11DeviceContext* ctx,ID3D11ComputeShader* native,ID3D11ComputeShader* patched,const std::vector<unsigned char>& bytes) {
 auto a=replay(d,ctx,native,false,0),b=replay(d,ctx,patched,false,0);check(a==b);
 auto c=replay(d,ctx,native,true,0),e=replay(d,ctx,patched,true,341);check(c!=e);
 for(unsigned layout:{1u,2u,3u}) {
  auto pa=replay(d,ctx,native,false,0,layout),pb=replay(d,ctx,patched,false,0,layout);
  check(pa==pb);check(pa==a); // All eight complete outputs, not just counts.
  auto pc=replay(d,ctx,native,true,0,layout),pe=replay(d,ctx,patched,true,341,layout);
  check(pc==c);check(pe==e);check(pc!=pe);
 }
 auto rightNative=replay(d,ctx,native,true,0,0,true),rightFixed=replay(d,ctx,patched,true,336,0,true);
 check(rightNative!=rightFixed);
 for(unsigned layout:{1u,2u}) {
  check(replay(d,ctx,native,true,0,layout,true)==rightNative);
  check(replay(d,ctx,patched,true,336,layout,true)==rightFixed);
 }
 auto index=readFile("validation/shader/indices.bin");
 correction::Com<ID3D11DeviceContext> deferred;check(SUCCEEDED(d->CreateDeferredContext(0,deferred.put())));
 diag::Context unused{};std::array<unsigned,84> unusedWords{};
 check(!correction::bindings(deferred.p,d,unused,unusedWords));
 // u2 begins as the full index copy, then cutting intentionally edits selected
 // ranges. The prefix outside the left-leg parent must remain untouched.
 check(std::equal(index.begin(),index.begin()+95535*2,e[2].begin()));
 check(!std::equal(index.begin()+6939*2,index.begin()+54681*2,c[2].begin()+6939*2));
 check(std::equal(index.begin(),index.begin()+61620*2,rightFixed[2].begin()));
 check(std::equal(index.begin()+95535*2,index.end(),rightFixed[2].begin()+95535*2));
 correction::ShaderCache cache;correction::Com<ID3D11ComputeShader> first,second;
 first.p=cache.obtain(d,bytes);second.p=cache.obtain(d,bytes);check(first.p&&first.p==second.p);
 AcquireSRWLockExclusive(&cache.lock);check(cache.obtain(d,bytes)==nullptr);ReleaseSRWLockExclusive(&cache.lock);
 correction::Com<ID3D11Device> secondDevice;correction::Com<ID3D11DeviceContext> secondContext;
 check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,secondDevice.put(),nullptr,secondContext.put())));
 correction::Com<ID3D11ComputeShader> generation;generation.p=cache.obtain(secondDevice.p,bytes);
 check(generation.p&&cache.device.p==secondDevice.p);check(correction::sameDevice(first.p,d));
 check(correction::sameDevice(generation.p,secondDevice.p));
 ctx->ClearState();
 unsigned armActual{},nativeThigh{},correctedThigh{};
 memcpy(&armActual,a[0].data()+96,4);memcpy(&nativeThigh,c[0].data()+96,4);memcpy(&correctedThigh,e[0].data()+96,4);
 std::ofstream("validation/shader-replay.json")<<"{\"device\":\"D3D11 WARP\",\"arm_zero_sideband_all_eight_outputs_identical\":true,\"arm_generated_vertices\":"<<armActual
  <<",\"native_thigh_generated_vertices\":"<<nativeThigh<<",\"corrected_thigh_generated_vertices\":"<<correctedThigh
  <<",\"corrected_unrelated_index_prefix_unchanged\":true,\"native_torso_indices_changed\":true,\"pooled_zero_equivalence\":true,\"pooled_high_start_matches_standalone\":true,\"raw_tail_zero_fill_verified\":true,\"observed_shared_offsets_tested\":true,\"observed_vertex_first_element\":9938048,\"right_thigh_unrelated_indices_unchanged\":true,\"gameplay\":false}\n";
 std::cout<<"PASS: zero-sideband arm output byte-identical across all 8 UAVs; high-start thigh preserves unrelated index prefix while native modifies torso\n";
}

