#pragma once
#include <d3d11.h>
#include <array>
#include <cstdint>
#include <ostream>
#include <type_traits>
namespace correction {
// Value-only snapshots: no borrowed COM references survive into the log queue.
enum class BindingCheck : unsigned {
 NotChecked,Passed,ContextType,MissingView,AliasedResource,ChildCount,
 InputStrideZero,InputStrideMaximum,InputStrideAlignment,OutputStride,
 ViewDevice,BufferQuery,SrvDimension,SrvFormat,SrvFirstElement,SrvRawFlag,
 SrvViewCapacity,SrvBufferCapacity,UavDimension,UavFirstElement,
 ResultFormat,ResultStride,ResultElements,UavFormat,UavRawFlag,UavViewCapacity,
 UavBufferCapacity,ConstantWidth,ConstantUsage,ConstantWriteAccess,ConstantDevice,
 SrvAlignment,SrvViewBounds,SrvRawResource,SrvBind,SrvViewEmpty,UavViewBounds
};
inline const char* checkName(BindingCheck c) {
 switch(c) {
 case BindingCheck::NotChecked:return "not_checked";case BindingCheck::Passed:return "passed";
 case BindingCheck::ContextType:return "context.immediate";
 case BindingCheck::MissingView:return "view.present";case BindingCheck::AliasedResource:return "resource.distinct";
 case BindingCheck::ChildCount:return "source.child_count_le_12";
 case BindingCheck::InputStrideZero:return "source.stride_nonzero";
 case BindingCheck::InputStrideMaximum:return "source.stride_le_64";
 case BindingCheck::InputStrideAlignment:return "source.stride_multiple_of_4";
 case BindingCheck::OutputStride:return "output.stride_equals_source_plus_4";
 case BindingCheck::ViewDevice:return "view.device_matches";
 case BindingCheck::BufferQuery:return "resource.query_buffer";
 case BindingCheck::SrvDimension:return "srv.dimension_bufferex";
 case BindingCheck::SrvFormat:return "srv.format_r32_typeless";
 case BindingCheck::SrvFirstElement:return "srv.first_element_zero";
 case BindingCheck::SrvRawFlag:return "srv.raw_flag";
 case BindingCheck::SrvViewCapacity:return "srv.view_bytes_sufficient";
 case BindingCheck::SrvBufferCapacity:return "srv.buffer_bytes_sufficient";
 case BindingCheck::UavDimension:return "uav.dimension_buffer";
 case BindingCheck::UavFirstElement:return "uav.first_element_zero";
 case BindingCheck::ResultFormat:return "result.format_unknown";
 case BindingCheck::ResultStride:return "result.structure_stride_112";
 case BindingCheck::ResultElements:return "result.element_count_1";
 case BindingCheck::UavFormat:return "uav.format_r32_typeless";
 case BindingCheck::UavRawFlag:return "uav.raw_flag";
 case BindingCheck::UavViewCapacity:return "uav.view_bytes_sufficient";
 case BindingCheck::UavBufferCapacity:return "uav.buffer_bytes_sufficient";
 case BindingCheck::ConstantWidth:return "cb0.byte_width_ge_336";
 case BindingCheck::ConstantUsage:return "cb0.usage_dynamic";
 case BindingCheck::ConstantWriteAccess:return "cb0.cpu_write_access";
 case BindingCheck::ConstantDevice:return "cb0.device_matches";
 case BindingCheck::SrvAlignment:return "srv.raw_offset_alignment_16";
 case BindingCheck::SrvViewBounds:return "srv.view_inside_backing_buffer";
 case BindingCheck::SrvRawResource:return "srv.resource_allows_raw_views";
 case BindingCheck::SrvBind:return "srv.resource_shader_bind";
 case BindingCheck::SrvViewEmpty:return "srv.view_nonempty";
 case BindingCheck::UavViewBounds:return "uav.view_inside_backing_buffer";
 }
 return "unknown";
}
struct SlotSnapshot {
 std::uint64_t view{},resource{},viewDevice{},resourceDevice{};
 bool resourceCaptured{},viewDescCaptured{},bufferDescCaptured{},bufferQueryAttempted{};
 HRESULT bufferQueryResult{};
 unsigned resourceDimension{},viewDimension{},format{},firstElement{},numElements{},flags{};
 std::array<unsigned,4> rawViewUnion{};
 D3D11_BUFFER_DESC buffer{};
 std::uint64_t requiredBytes{},viewOffsetBytes{},viewLengthBytes{};
 std::uint64_t vectorCopyReadEnd{},vectorCopyZeroFillBytes{};
};
struct ConstantSnapshot {
 bool captured{};std::uint64_t identity{},device{};D3D11_BUFFER_DESC desc{};
};
struct BindingTelemetry {
 bool attempted{},viewsCaptured{};
 unsigned contextType{},childCount{},sourceVertices{},sourceEntries{},recordCount{},inputStride{},outputStride{};
 std::uint64_t context{},expectedDevice{},selectedEntries{};
 ConstantSnapshot constant{};
 std::array<SlotSnapshot,10> slots{}; // t0,t1,u0..u7
 BindingCheck check=BindingCheck::NotChecked;
 int failedBinding=-1,relatedBinding=-1;
 std::uint64_t expected{},observed{};
};
inline bool failBinding(BindingTelemetry& t,BindingCheck c,int slot=-1,
 std::uint64_t expected=0,std::uint64_t observed=0,int related=-1) {
 t.check=c;t.failedBinding=slot;t.relatedBinding=related;t.expected=expected;t.observed=observed;return false;
}
inline bool checkedMul(std::uint64_t a,std::uint64_t b,std::uint64_t& out) {
 if(b&&a>UINT64_MAX/b)return false;out=a*b;return true;
}
inline bool checkedAdd(std::uint64_t a,std::uint64_t b,std::uint64_t& out) {
 if(a>UINT64_MAX-b)return false;out=a+b;return true;
}
// Only the two read-only input SRVs may share backing storage. Every pair
// involving an output UAV remains conservatively forbidden, even if disjoint.
inline bool validateBindingSnapshot(BindingTelemetry& t) {
 using C=BindingCheck;
 t.check=C::NotChecked;t.failedBinding=t.relatedBinding=-1;t.expected=t.observed=0;
 if(t.contextType!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return failBinding(t,C::ContextType,-1,D3D11_DEVICE_CONTEXT_IMMEDIATE,t.contextType);
 for(unsigned i=0;i<10;++i) {
  if(!t.slots[i].view)return failBinding(t,C::MissingView,i,1,0);
  for(unsigned j=0;j<i;++j)if(i>=2&&t.slots[i].resource==t.slots[j].resource)
   return failBinding(t,C::AliasedResource,i,1,0,j);
 }
 if(t.childCount>12)return failBinding(t,C::ChildCount,-1,12,t.childCount);
 if(!t.inputStride)return failBinding(t,C::InputStrideZero,-1,1,0);
 if(t.inputStride>64)return failBinding(t,C::InputStrideMaximum,-1,64,t.inputStride);
 if(t.inputStride%4)return failBinding(t,C::InputStrideAlignment,-1,0,t.inputStride%4);
 if(t.outputStride!=t.inputStride+4)return failBinding(t,C::OutputStride,-1,t.inputStride+4,t.outputStride);
 for(unsigned i=0;i<10;++i) {
  auto& s=t.slots[i];
  if(s.viewDevice!=t.expectedDevice)return failBinding(t,C::ViewDevice,i,t.expectedDevice,s.viewDevice);
  if(!s.bufferDescCaptured)return failBinding(t,C::BufferQuery,i,0,static_cast<std::uint32_t>(s.bufferQueryResult));
  if(i<2) {
   if(s.viewDimension!=D3D11_SRV_DIMENSION_BUFFEREX)return failBinding(t,C::SrvDimension,i,D3D11_SRV_DIMENSION_BUFFEREX,s.viewDimension);
   if(s.format!=DXGI_FORMAT_R32_TYPELESS)return failBinding(t,C::SrvFormat,i,DXGI_FORMAT_R32_TYPELESS,s.format);
   if(!(s.flags&D3D11_BUFFEREX_SRV_FLAG_RAW))return failBinding(t,C::SrvRawFlag,i,D3D11_BUFFEREX_SRV_FLAG_RAW,s.flags);
   if(s.flags!=D3D11_BUFFEREX_SRV_FLAG_RAW)return failBinding(t,C::SrvRawFlag,i,D3D11_BUFFEREX_SRV_FLAG_RAW,s.flags);
   if(!(s.buffer.MiscFlags&D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS))return failBinding(t,C::SrvRawResource,i,D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS,s.buffer.MiscFlags);
   if(!(s.buffer.BindFlags&D3D11_BIND_SHADER_RESOURCE))return failBinding(t,C::SrvBind,i,D3D11_BIND_SHADER_RESOURCE,s.buffer.BindFlags);
   std::uint64_t end{};
   if(!checkedMul(s.firstElement,4,s.viewOffsetBytes)||!checkedMul(s.numElements,4,s.viewLengthBytes)
      ||!checkedAdd(s.viewOffsetBytes,s.viewLengthBytes,end))return failBinding(t,C::SrvViewBounds,i);
   if(s.viewOffsetBytes%D3D11_RAW_UAV_SRV_BYTE_ALIGNMENT)return failBinding(t,C::SrvAlignment,i,0,s.viewOffsetBytes%D3D11_RAW_UAV_SRV_BYTE_ALIGNMENT);
   if(!s.numElements)return failBinding(t,C::SrvViewEmpty,i,1,0);
   if(end>s.buffer.ByteWidth)return failBinding(t,C::SrvViewBounds,i,s.buffer.ByteWidth,end);
   if(s.viewLengthBytes<s.requiredBytes)return failBinding(t,C::SrvViewCapacity,i,s.requiredBytes,s.viewLengthBytes);
   if(s.buffer.ByteWidth<s.requiredBytes)return failBinding(t,C::SrvBufferCapacity,i,s.requiredBytes,s.buffer.ByteWidth);
   if(i==0) {
    // The hash-pinned native loop copies ceil(sourceEntries/8) 16-byte blocks.
    // Only its final partial vector may exceed the logical view: D3D11 ld_raw
    // zero-fills out-of-view components. Valid index DWORDs must all be present.
    s.vectorCopyReadEnd=(std::uint64_t(t.sourceEntries)+7)/8*16;
    s.vectorCopyZeroFillBytes=s.vectorCopyReadEnd>s.viewLengthBytes?s.vectorCopyReadEnd-s.viewLengthBytes:0;
   }
  } else {
   if(s.viewDimension!=D3D11_UAV_DIMENSION_BUFFER)return failBinding(t,C::UavDimension,i,D3D11_UAV_DIMENSION_BUFFER,s.viewDimension);
   if(s.firstElement)return failBinding(t,C::UavFirstElement,i,0,s.firstElement);
   if(i==2) {
    if(s.format!=DXGI_FORMAT_UNKNOWN)return failBinding(t,C::ResultFormat,i,DXGI_FORMAT_UNKNOWN,s.format);
    if(s.buffer.StructureByteStride!=112)return failBinding(t,C::ResultStride,i,112,s.buffer.StructureByteStride);
    if(s.numElements!=1)return failBinding(t,C::ResultElements,i,1,s.numElements);
   } else {
    if(s.format!=DXGI_FORMAT_R32_TYPELESS)return failBinding(t,C::UavFormat,i,DXGI_FORMAT_R32_TYPELESS,s.format);
    if(!(s.flags&D3D11_BUFFER_UAV_FLAG_RAW))return failBinding(t,C::UavRawFlag,i,D3D11_BUFFER_UAV_FLAG_RAW,s.flags);
    if(s.viewLengthBytes<s.requiredBytes)return failBinding(t,C::UavViewCapacity,i,s.requiredBytes,s.viewLengthBytes);
   }
   if(s.buffer.ByteWidth<s.requiredBytes)return failBinding(t,C::UavBufferCapacity,i,s.requiredBytes,s.buffer.ByteWidth);
   if(s.viewLengthBytes>s.buffer.ByteWidth)return failBinding(t,C::UavViewBounds,i,s.buffer.ByteWidth,s.viewLengthBytes);
  }
 }
 t.check=C::Passed;return true;
}
inline void writeBufferDesc(std::ostream& o,const D3D11_BUFFER_DESC& b) {
 o<<"{\"byte_width\":"<<b.ByteWidth<<",\"usage\":"<<b.Usage<<",\"bind_flags\":"<<b.BindFlags
  <<",\"cpu_access_flags\":"<<b.CPUAccessFlags<<",\"misc_flags\":"<<b.MiscFlags<<",\"structure_stride\":"<<b.StructureByteStride<<'}';
}
inline void writeBindingSlot(std::ostream& o,int i) {
 if(i==-2)o<<"\"cb0\"";else if(i<0)o<<"null";else o<<'\"'<<(i<2?"t":"u")<<(i<2?i:i-2)<<'\"';
}
inline void writeBindingTelemetry(std::ostream& o,const BindingTelemetry& t) {
 o<<"{\"schema\":1,\"attempted\":"<<(t.attempted?"true":"false")<<",\"views_captured\":"<<(t.viewsCaptured?"true":"false")
  <<",\"first_check\":\""<<checkName(t.check)<<"\",\"failed_slot\":";
 writeBindingSlot(o,t.failedBinding);o<<",\"related_slot\":";writeBindingSlot(o,t.relatedBinding);
 o<<",\"expected\":"<<t.expected<<",\"observed\":"<<t.observed<<",\"context_identity\":"<<t.context
  <<",\"context_type\":"<<t.contextType<<",\"expected_device_identity\":"<<t.expectedDevice
  <<",\"source_children\":"<<t.childCount<<",\"source_vertices\":"<<t.sourceVertices<<",\"source_index_entries\":"<<t.sourceEntries
  <<",\"record_count\":"<<t.recordCount<<",\"selected_index_entries\":"<<t.selectedEntries
  <<",\"input_stride\":"<<t.inputStride<<",\"output_stride\":"<<t.outputStride;
 o<<",\"cb0\":{\"captured\":"<<(t.constant.captured?"true":"false")<<",\"identity\":"<<t.constant.identity
  <<",\"device_identity\":"<<t.constant.device<<",\"buffer_desc\":";
 if(t.constant.captured)writeBufferDesc(o,t.constant.desc);else o<<"null";o<<'}';
 o<<",\"slots\":[";
 for(unsigned i=0;i<10;++i) {
  if(i)o<<',';const auto& s=t.slots[i];
  o<<"{\"slot\":";writeBindingSlot(o,i);
  o<<",\"view_identity\":"<<s.view<<",\"resource_identity\":"<<s.resource
   <<",\"view_device_identity\":"<<s.viewDevice<<",\"resource_device_identity\":"<<s.resourceDevice
   <<",\"resource_captured\":"<<(s.resourceCaptured?"true":"false")
   <<",\"resource_dimension\":"<<s.resourceDimension
   <<",\"view_desc_captured\":"<<(s.viewDescCaptured?"true":"false")
   <<",\"view_dimension\":"<<s.viewDimension<<",\"format\":"<<s.format
   <<",\"first_element\":"<<s.firstElement<<",\"num_elements\":"<<s.numElements<<",\"view_flags\":"<<s.flags
   <<",\"raw_view_union\":[";
  for(unsigned j=0;j<4;++j){if(j)o<<',';o<<s.rawViewUnion[j];}
  o<<"],\"required_bytes\":"<<s.requiredBytes<<",\"view_offset_bytes\":"<<s.viewOffsetBytes
   <<",\"view_length_bytes\":"<<s.viewLengthBytes<<",\"vector_copy_read_end\":"<<s.vectorCopyReadEnd
   <<",\"vector_copy_zero_fill_bytes\":"<<s.vectorCopyZeroFillBytes<<",\"buffer_query_attempted\":"<<(s.bufferQueryAttempted?"true":"false")
   <<",\"buffer_query_hresult\":"<<static_cast<std::uint32_t>(s.bufferQueryResult)
   <<",\"buffer_desc\":";
  if(s.bufferDescCaptured)writeBufferDesc(o,s.buffer);else o<<"null";o<<'}';
 }
 o<<"]}";
}
}

