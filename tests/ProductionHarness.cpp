// Private mapped PE copies and synthetic objects only. Never execute game code.
#include "../src/Plugin.cpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
template<class T> void put(void* p,unsigned off,T value){memcpy(static_cast<unsigned char*>(p)+off,&value,sizeof(value));}
void* pool{};
void* allocatePool(PluginHandle,std::size_t size){CHECK(size==96);return pool;}
F4SETrampolineInterface api{1,allocatePool,nullptr};
void* queryApi(UInt32 id){return id==kInterface_Trampoline?&api:nullptr;}
unsigned uploads{};
unsigned runs{};
void runMock(void* renderer,void* wrapper,std::uint32_t x,std::uint32_t y,std::uint32_t z){CHECK(renderer==reinterpret_cast<void*>(1));CHECK(wrapper==reinterpret_cast<void*>(2));CHECK(x==3&&y==4&&z==5);CHECK(GetLastError()==17);++runs;SetLastError(919);}
void uploadMock(void* renderer,const void*,std::uint32_t n){CHECK(renderer==reinterpret_cast<void*>(1));CHECK(n==336);CHECK(GetLastError()==123);++uploads;SetLastError(456);}
std::array<std::uint32_t,84> payload{};
std::int32_t shapeMock(void*,void*){CHECK(computeScope!=nullptr);SetLastError(123);uploadHook(reinterpret_cast<void*>(1),payload.data(),336);CHECK(GetLastError()==456);CHECK(computeScope->uploaded);SetLastError(789);return 7;}
int main(int argc,char** argv){
 CHECK(argc==3);CHECK(release::select(0)==nullptr);CHECK(release::select(0x010a0a30)==nullptr);
 CHECK(release::select(0x010b0f00)==&release::profile240);
#ifdef HPDF_EXPERIMENTAL
 CHECK(release::select(0x010b0dd0)==&release::profile221);
#else
 CHECK(release::select(0x010b0dd0)==nullptr);
#endif
 for(int arg=1;arg<=2;++arg){
  profile=arg==1?&release::profile240:&release::profile221;
  std::ifstream f(argv[arg],std::ios::binary);std::vector<unsigned char> raw{std::istreambuf_iterator<char>(f),{}};CHECK(raw.size()>1000);
  auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(raw.data());auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(raw.data()+dos->e_lfanew);
  const auto allocation=std::size_t(nt->OptionalHeader.SizeOfImage)+0x10000;
  auto image=static_cast<unsigned char*>(VirtualAlloc(nullptr,allocation,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));CHECK(image);
  memcpy(image,raw.data(),nt->OptionalHeader.SizeOfHeaders);auto section=IMAGE_FIRST_SECTION(nt);
  for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)memcpy(image+section[i].VirtualAddress,raw.data()+section[i].PointerToRawData,section[i].SizeOfRawData);
  base=reinterpret_cast<std::uintptr_t>(image);CHECK(validate());
  image[profile->shape.site]^=1;CHECK(!validate());image[profile->shape.site]^=1;CHECK(validate());
  pool=image+nt->OptionalHeader.SizeOfImage;F4SEInterface fake{};fake.QueryInterface=queryApi;CHECK(install(&fake,1));CHECK(installed.load());
  CHECK(image[profile->shape.site]==0xe8);CHECK(image[profile->upload.site]==0xe8);CHECK(image[profile->tail]==0xe9);
  originalRun=runMock;SetLastError(17);
  // Execute only our patched E9+pool stub, never the original engine function.
  reinterpret_cast<RunFn>(base+profile->tail)(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),3,4,5);
  CHECK(GetLastError()==919);
  CHECK(!validate()); // A second install sees the changed code, not a clean engine.
  // Validate source mapping and synchronous payload scopes against this runtime getter.
  std::array<unsigned char,0xa0> state{};std::array<unsigned char,0x180> shape{};std::array<unsigned char,0x40> segment{};
  std::array<std::uintptr_t,64> vt{};vt[0x1d8/8]=base+profile->getter;
  std::array<std::uint32_t,5> rows{95535,100,0,0,0};
  put(shape.data(),0,reinterpret_cast<std::uintptr_t>(vt.data()));put(shape.data(),0x170,reinterpret_cast<std::uintptr_t>(segment.data()));
  put(shape.data(),0x160,std::uint32_t{43150});put(shape.data(),0x164,std::uint16_t{22708});
  put(segment.data(),0x18,reinterpret_cast<std::uintptr_t>(rows.data()));put(segment.data(),0x2c,std::uint32_t{1});
  auto c=contextAt(reinterpret_cast<std::uintptr_t>(state.data()),reinterpret_cast<std::uintptr_t>(shape.data()));CHECK(c.valid&&c.rangesValid);CHECK(c.ranges[0].start==95535);
  ++vt[0x1d8/8];CHECK(!contextAt(reinterpret_cast<std::uintptr_t>(state.data()),reinterpret_cast<std::uintptr_t>(shape.data())).rangesValid);--vt[0x1d8/8];
  std::array<std::uintptr_t,4> closure{};closure[3]=reinterpret_cast<std::uintptr_t>(state.data());auto slot=reinterpret_cast<std::uintptr_t>(shape.data());
  payload[32]=1;payload[33]=1;payload[34]=129450;payload[36]=(95535u<<16)|100;
  originalShape=shapeMock;originalUpload=uploadMock;CHECK(shapeHook(closure.data(),&slot)==7);CHECK(GetLastError()==789);CHECK(computeScope==nullptr);CHECK(nesting==0);
  VirtualFree(image,0,MEM_RELEASE);
 }
 CHECK(uploads==2);CHECK(runs==2);std::cout<<"PASS "<<checks<<" production profile/hook/scope checks\n";
}
