#pragma once
// Pure snapshot predicates exercise precise rejection names without attempting to
// create invalid D3D views. Real binding comparison stays in ShaderReplay.h.
void telemetryPredicates(const correction::BindingTelemetry& base) {
 using namespace correction;using C=BindingCheck;
 check(std::is_trivially_copyable_v<BindingTelemetry>);
 auto rejected=[&](auto change,C expected,int slot,int related=-1) {
  auto t=base;change(t);check(!validateBindingSnapshot(t));
  check(t.check==expected);check(t.failedBinding==slot);check(t.relatedBinding==related);
 };
 auto t=base;check(validateBindingSnapshot(t)&&t.check==C::Passed);
 rejected([](auto& v){v.contextType=D3D11_DEVICE_CONTEXT_DEFERRED;},C::ContextType,-1);
 rejected([](auto& v){v.slots[9].view=0;},C::MissingView,9);
 rejected([](auto& v){v.slots[7].resource=v.slots[5].resource;},C::AliasedResource,7,5);
 // Disjoint view windows on one resource remain rejected, exactly as in 0.2.0.
 rejected([](auto& v){v.slots[7].resource=v.slots[5].resource;v.slots[7].viewOffsetBytes=0x10000000;},C::AliasedResource,7,5);
 rejected([](auto& v){v.childCount=13;},C::ChildCount,-1);
 rejected([](auto& v){v.inputStride=0;},C::InputStrideZero,-1);
 rejected([](auto& v){v.inputStride=68;},C::InputStrideMaximum,-1);
 rejected([](auto& v){v.inputStride=33;},C::InputStrideAlignment,-1);
 rejected([](auto& v){v.outputStride=64;},C::OutputStride,-1);
 rejected([](auto& v){v.slots[1].viewDevice=0;},C::ViewDevice,1);
 rejected([](auto& v){v.slots[1].bufferDescCaptured=false;v.slots[1].bufferQueryResult=E_NOINTERFACE;},C::BufferQuery,1);
 rejected([](auto& v){v.slots[0].viewDimension=D3D11_SRV_DIMENSION_BUFFER;},C::SrvDimension,0);
 rejected([](auto& v){v.slots[1].format=DXGI_FORMAT_R32_UINT;},C::SrvFormat,1);
 rejected([](auto& v){v.slots[1].firstElement=1;},C::SrvAlignment,1);
 rejected([](auto& v){v.slots[1].firstElement=0xfffffffc;},C::SrvViewBounds,1);
 rejected([](auto& v){v.slots[0].buffer.MiscFlags=0;},C::SrvRawResource,0);
 rejected([](auto& v){v.slots[0].buffer.BindFlags=0;},C::SrvBind,0);
 rejected([](auto& v){v.slots[0].flags=3;},C::SrvRawFlag,0);
 rejected([](auto& v){v.slots[2].resource=v.slots[0].resource;},C::AliasedResource,2,0);
 rejected([](auto& v){v.slots[0].flags=0;},C::SrvRawFlag,0);
 rejected([](auto& v){v.slots[0].numElements=static_cast<unsigned>(v.slots[0].requiredBytes/4)-1;},C::SrvViewCapacity,0);
 rejected([](auto& v){v.slots[0].buffer.ByteWidth=0;},C::SrvViewBounds,0);
 rejected([](auto& v){v.slots[5].viewDimension=D3D11_UAV_DIMENSION_TEXTURE2D;},C::UavDimension,5);
 rejected([](auto& v){v.slots[5].firstElement=1;},C::UavFirstElement,5);
 rejected([](auto& v){v.slots[2].format=DXGI_FORMAT_R32_UINT;},C::ResultFormat,2);
 rejected([](auto& v){v.slots[2].buffer.StructureByteStride=16;},C::ResultStride,2);
 rejected([](auto& v){v.slots[2].numElements=2;},C::ResultElements,2);
 rejected([](auto& v){v.slots[6].format=DXGI_FORMAT_R32_UINT;},C::UavFormat,6);
 rejected([](auto& v){v.slots[6].flags=0;},C::UavRawFlag,6);
 rejected([](auto& v){v.slots[6].viewLengthBytes=0;},C::UavViewCapacity,6);
 rejected([](auto& v){v.slots[2].buffer.ByteWidth=0;},C::UavBufferCapacity,2);
 // Multiple violations must preserve the old early-failure ordering.
 rejected([](auto& v){v.slots[0].format=0;v.slots[9].view=0;},C::MissingView,9);
 rejected([](auto& v){v.slots[0].format=0;v.slots[0].firstElement=1;},C::SrvFormat,0);
 auto offset=base;offset.slots[1].firstElement=4;offset.slots[1].buffer.ByteWidth+=16;
 check(validateBindingSnapshot(offset));check(offset.slots[1].viewOffsetBytes==16);
 auto shared=base;shared.slots[0].resource=shared.slots[1].resource;
 shared.slots[0].buffer.ByteWidth=shared.slots[1].buffer.ByteWidth;
 check(validateBindingSnapshot(shared)); // read-only sharing does not create a write hazard
 std::uint64_t checked{};check(!checkedAdd(UINT64_MAX,1,checked));check(!checkedMul(UINT64_MAX,2,checked));
 check(checkedMul(9938048,4,checked)&&checked==39752192);
 auto deferred=base;deferred.contextType=D3D11_DEVICE_CONTEXT_DEFERRED;
 check(!validateBindingSnapshot(deferred)&&deferred.check==C::ContextType);
}

