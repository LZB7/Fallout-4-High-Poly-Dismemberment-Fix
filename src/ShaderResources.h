#pragma once
#include "ShaderTransform.h"
#include "BindingTelemetry.h"
namespace correction {
inline bool sameDevice(ID3D11DeviceChild* child,ID3D11Device* device) {
 Com<ID3D11Device> d;child->GetDevice(d.put());return d.p==device;
}
inline bool bufferDesc(ID3D11View* v,ID3D11Device* d,D3D11_BUFFER_DESC& desc) {
 if(!v||!sameDevice(v,d))return false;
 Com<ID3D11Resource>r;v->GetResource(r.put());Com<ID3D11Buffer>b;
 if(FAILED(r.p->QueryInterface(__uuidof(ID3D11Buffer),reinterpret_cast<void**>(b.put()))))return false;
 b.p->GetDesc(&desc);return true;
}
inline bool bindings(ID3D11DeviceContext* ctx,ID3D11Device* device,
 const diag::Context& c,const std::array<std::uint32_t,84>& w,BindingTelemetry* report=nullptr) {
 BindingTelemetry local{};auto& t=report?*report:local;
 auto constant=t.constant;t={};t.constant=constant;t.attempted=true;
 t.context=reinterpret_cast<std::uintptr_t>(ctx);t.expectedDevice=reinterpret_cast<std::uintptr_t>(device);
 t.contextType=ctx->GetType();t.childCount=c.childCount;t.sourceVertices=c.vertices;t.sourceEntries=c.entries;
 t.recordCount=w[33];t.inputStride=w[20];t.outputStride=w[28];
 // Deferred context getters are unsupported. Preserve the original rejection.
 if(t.contextType!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return validateBindingSnapshot(t);
 struct Views {
  ID3D11ShaderResourceView* s[2]{};ID3D11UnorderedAccessView* u[8]{};
  ~Views(){for(auto x:s)if(x)x->Release();for(auto x:u)if(x)x->Release();}
 } v;
 ctx->CSGetShaderResources(0,2,v.s);ctx->CSGetUnorderedAccessViews(0,8,v.u);t.viewsCaptured=true;
 for(unsigned i=0;i<std::min(w[33],12u);++i)t.selectedEntries+=3ull*c.ranges[i].triangles;
 for(unsigned i=0;i<10;++i) {
  auto& s=t.slots[i];
  s.requiredBytes=i==0?std::uint64_t(c.entries)*2:i==1?std::uint64_t(c.vertices)*w[20]:
   i==2?112:i==3?2*t.selectedEntries*w[28]:i==4?std::uint64_t(c.entries)*2:
   i==6?4*t.selectedEntries:8*t.selectedEntries;
  ID3D11View* view=i<2?static_cast<ID3D11View*>(v.s[i]):static_cast<ID3D11View*>(v.u[i-2]);
  s.view=reinterpret_cast<std::uintptr_t>(view);if(!view)continue;
  Com<ID3D11Device> vd;view->GetDevice(vd.put());s.viewDevice=reinterpret_cast<std::uintptr_t>(vd.p);
  Com<ID3D11Resource> r;view->GetResource(r.put());s.resource=reinterpret_cast<std::uintptr_t>(r.p);
  if(r.p) {
   s.resourceCaptured=true;D3D11_RESOURCE_DIMENSION dimension{};r.p->GetType(&dimension);s.resourceDimension=dimension;
   Com<ID3D11Device> rd;r.p->GetDevice(rd.put());s.resourceDevice=reinterpret_cast<std::uintptr_t>(rd.p);
   Com<ID3D11Buffer> buffer;s.bufferQueryAttempted=true;
   s.bufferQueryResult=r.p->QueryInterface(__uuidof(ID3D11Buffer),reinterpret_cast<void**>(buffer.put()));
   if(SUCCEEDED(s.bufferQueryResult)&&buffer.p){buffer.p->GetDesc(&s.buffer);s.bufferDescCaptured=true;}
  } else {s.bufferQueryAttempted=true;s.bufferQueryResult=E_POINTER;}
  if(i<2) {
   D3D11_SHADER_RESOURCE_VIEW_DESC d{};v.s[i]->GetDesc(&d);
   s.viewDimension=d.ViewDimension;s.format=d.Format;s.firstElement=d.BufferEx.FirstElement;
   s.numElements=d.BufferEx.NumElements;s.flags=d.BufferEx.Flags;
   static_assert(sizeof(d)-offsetof(D3D11_SHADER_RESOURCE_VIEW_DESC,Buffer)>=sizeof(s.rawViewUnion));
   memcpy(s.rawViewUnion.data(),&d.Buffer,sizeof(s.rawViewUnion));
  } else {
   D3D11_UNORDERED_ACCESS_VIEW_DESC d{};v.u[i-2]->GetDesc(&d);
   s.viewDimension=d.ViewDimension;s.format=d.Format;s.firstElement=d.Buffer.FirstElement;
   s.numElements=d.Buffer.NumElements;s.flags=d.Buffer.Flags;
   static_assert(sizeof(d)-offsetof(D3D11_UNORDERED_ACCESS_VIEW_DESC,Buffer)>=12);
   memcpy(s.rawViewUnion.data(),&d.Buffer,12);
  }
  s.viewDescCaptured=true;
  auto elementBytes=i==2?std::uint64_t(s.buffer.StructureByteStride):4ull;
  s.viewOffsetBytes=std::uint64_t(s.firstElement)*elementBytes;s.viewLengthBytes=std::uint64_t(s.numElements)*elementBytes;
 }
 return validateBindingSnapshot(t);
}
// One cache generation per device. A local AddRef protects each in-flight dispatch.
// Releasing an old generation never waits for the GPU; D3D retains submitted refs.
struct ShaderCache {
 SRWLOCK lock=SRWLOCK_INIT;Com<ID3D11Device> device;Com<ID3D11ComputeShader> shader;
 ID3D11ComputeShader* obtain(ID3D11Device* d,const std::vector<unsigned char>& native) {
  if(!TryAcquireSRWLockExclusive(&lock))return nullptr;
  struct Unlock {SRWLOCK* p;~Unlock(){ReleaseSRWLockExclusive(p);}}unlock{&lock};
  if(FAILED(d->GetDeviceRemovedReason())){if(device.p==d){shader.put();device.put();}return nullptr;}
  if(device.p!=d) {
   shader.put();device.put();std::vector<unsigned char> variant;
   if(!transform(native,variant)||!shaderContract(variant,true)
      ||FAILED(d->CreateComputeShader(variant.data(),variant.size(),nullptr,shader.put())))return nullptr;
   device.p=d;d->AddRef();
  }
  if(!shader.p)return nullptr;shader.p->AddRef();return shader.p;
 }
};
}

