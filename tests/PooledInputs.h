#pragma once
// Offline fixtures only. The plugin never copies geometry or creates replacement SRVs.
void pooledInputs(ID3D11Device* d,std::array<TestBuffer,2>& input,TestBuffer& pool,unsigned layout) {
 auto indices=readFile("validation/shader/indices.bin"),vertices=readFile("validation/shader/vertices.bin");
 const unsigned vertexOffset=layout==2?39752192:39407872,indexOffset=40134656;
 constexpr unsigned poolBytes=134217728,vertexViewBytes=726784;
 const unsigned indexViewBytes=layout==3?static_cast<unsigned>(indices.size()):259072;
 std::vector<unsigned char> data(poolBytes,0xa5);
 memcpy(data.data()+vertexOffset,vertices.data(),vertices.size());
 if(layout!=2)memcpy(data.data()+indexOffset,indices.data(),indices.size());
 D3D11_BUFFER_DESC b{};b.ByteWidth=poolBytes;b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
 b.BindFlags=D3D11_BIND_VERTEX_BUFFER|D3D11_BIND_INDEX_BUFFER|D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
 D3D11_SUBRESOURCE_DATA initial{data.data(),0,0};check(SUCCEEDED(d->CreateBuffer(&b,&initial,pool.buffer.put())));
 for(unsigned i=0;i<2;++i) {
  if(i==0&&layout==2) {
   // Mode-1 capture: standalone index input has exactly 258900 bytes.
   D3D11_BUFFER_DESC ib{};ib.ByteWidth=indices.size();ib.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   ib.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;D3D11_SUBRESOURCE_DATA source{indices.data(),0,0};
   check(SUCCEEDED(d->CreateBuffer(&ib,&source,input[0].buffer.put())));
  } else {input[i].buffer.p=pool.buffer.p;pool.buffer.p->AddRef();}
  D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=DXGI_FORMAT_R32_TYPELESS;v.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;
  v.BufferEx.FirstElement=i?vertexOffset/4:layout==2?0:indexOffset/4;
  v.BufferEx.NumElements=i?vertexViewBytes/4:layout==2?static_cast<unsigned>(indices.size()/4):indexViewBytes/4;
  v.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
  check(SUCCEEDED(d->CreateShaderResourceView(input[i].buffer.p,&v,input[i].srv.put())));
 }
}
void rawTailProbe(ID3D11Device* d,ID3D11DeviceContext* ctx,ID3D11ShaderResourceView* input) {
 using namespace correction;
 auto indices=readFile("validation/shader/indices.bin");unsigned end=static_cast<unsigned>(indices.size());
 // The index count has a four-byte tail in a final sixteen-byte copy vector.
 auto source=std::string("ByteAddressBuffer b:register(t0); RWByteAddressBuffer o:register(u0); [numthreads(1,1,1)] void main(){o.Store4(0,b.Load4(")
  +std::to_string(end-4)+"));}";
 Com<ID3DBlob> code,error;check(SUCCEEDED(D3DCompile(source.data(),source.size(),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,code.put(),error.put())));
 Com<ID3D11ComputeShader> shader;check(SUCCEEDED(d->CreateComputeShader(code.p->GetBufferPointer(),code.p->GetBufferSize(),nullptr,shader.put())));
 TestBuffer result;makeBuffer(d,result,std::vector<unsigned char>(16),false);
 ctx->CSSetShaderResources(0,1,&input);ctx->CSSetUnorderedAccessViews(0,1,&result.uav.p,nullptr);ctx->CSSetShader(shader.p,nullptr,0);ctx->Dispatch(1,1,1);
 ctx->ClearState();D3D11_BUFFER_DESC desc{};desc.ByteWidth=16;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 Com<ID3D11Buffer> staging;check(SUCCEEDED(d->CreateBuffer(&desc,nullptr,staging.put())));ctx->CopyResource(staging.p,result.buffer.p);
 D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(ctx->Map(staging.p,0,D3D11_MAP_READ,0,&mapped)));
 unsigned values[4]{},last{};memcpy(values,mapped.pData,16);ctx->Unmap(staging.p,0);
 memcpy(&last,indices.data()+indices.size()-4,4);
 check(values[0]==last);check(values[1]==0&&values[2]==0&&values[3]==0);
 std::cout<<"PASS: raw tail is view-bounded: last DWORD preserved; 12 bytes zero-filled despite poisoned pool neighbors\n";
}

