// Frozen 0.2.0 predicates, for differential regression only.
#pragma once
namespace correction {
inline bool legacyBindings(ID3D11DeviceContext* ctx,ID3D11Device* device,
 const diag::Context& c,const std::array<std::uint32_t,84>& w) {
 if(ctx->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
 struct Views {
  ID3D11ShaderResourceView* s[2]{};ID3D11UnorderedAccessView* u[8]{};
  ~Views(){for(auto x:s)if(x)x->Release();for(auto x:u)if(x)x->Release();}
 } v;
 ctx->CSGetShaderResources(0,2,v.s);ctx->CSGetUnorderedAccessViews(0,8,v.u);
 // Never accept an SRV/UAV alias or aliased output streams. This cannot be
 // repaired merely by widening starts and would violate the native contract.
 std::array<ID3D11Resource*,10> identities{};
 struct Release {std::array<ID3D11Resource*,10>& a;~Release(){for(auto p:a)if(p)p->Release();}}release{identities};
 for(unsigned i=0;i<10;++i) {
  ID3D11View* view=i<2?static_cast<ID3D11View*>(v.s[i]):static_cast<ID3D11View*>(v.u[i-2]);
  if(!view)return false;view->GetResource(&identities[i]);
  for(unsigned j=0;j<i;++j)if(identities[i]==identities[j])return false;
 }
 if(c.childCount>12||w[20]==0||w[20]>64||w[20]%4||w[28]!=w[20]+4)return false;
 std::uint64_t entries{};for(unsigned i=0;i<w[33];++i)entries+=3ull*c.ranges[i].triangles;
 for(unsigned i=0;i<2;++i) {
  D3D11_BUFFER_DESC b{};if(!bufferDesc(v.s[i],device,b))return false;
  D3D11_SHADER_RESOURCE_VIEW_DESC d{};v.s[i]->GetDesc(&d);
  auto need=i?std::uint64_t(c.vertices)*w[20]:std::uint64_t(c.entries)*2;
  if(d.ViewDimension!=D3D11_SRV_DIMENSION_BUFFEREX||d.Format!=DXGI_FORMAT_R32_TYPELESS
   ||d.BufferEx.FirstElement||!(d.BufferEx.Flags&D3D11_BUFFEREX_SRV_FLAG_RAW)
   ||std::uint64_t(d.BufferEx.NumElements)*4<need||b.ByteWidth<need)return false;
 }
 for(unsigned i=0;i<8;++i) {
  D3D11_BUFFER_DESC b{};if(!bufferDesc(v.u[i],device,b))return false;
  D3D11_UNORDERED_ACCESS_VIEW_DESC d{};v.u[i]->GetDesc(&d);
  if(d.ViewDimension!=D3D11_UAV_DIMENSION_BUFFER||d.Buffer.FirstElement)return false;
  std::uint64_t need=i==0?112:i==1?2*entries*w[28]:i==2?std::uint64_t(c.entries)*2:
                       i==4?4*entries:8*entries;
  if(i==0) {
   if(d.Format!=DXGI_FORMAT_UNKNOWN||b.StructureByteStride!=112||d.Buffer.NumElements!=1)return false;
  } else if(d.Format!=DXGI_FORMAT_R32_TYPELESS||!(d.Buffer.Flags&D3D11_BUFFER_UAV_FLAG_RAW)
      ||std::uint64_t(d.Buffer.NumElements)*4<need)return false;
  if(b.ByteWidth<need)return false;
 }
 return true;
}
}
