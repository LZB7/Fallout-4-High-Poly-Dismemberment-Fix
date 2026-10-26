#include "SdkPrefix.h"
#include "RuntimeProfiles.h"
#include "Correction.h"
#include "ShaderResources.h"
#include "f4se/PluginAPI.h"
#include "f4se_common/BranchTrampoline.h"
#include "f4se_common/f4se_version.h"
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <string>
namespace {
using namespace diag;
constexpr char pluginName[]="HighPolyDismembermentFix";
#ifdef HPDF_EXPERIMENTAL
constexpr char versionLabel[]="1.0.1-experimental.1";
#else
constexpr char versionLabel[]="1.0.0";
#endif
std::uintptr_t base{};const release::Profile* profile{};HMODULE module{};
bool enabled=true,verbose=false;std::atomic<bool> installed{};
std::wstring logPath;
HANDLE wake{};SRWLOCK logLock=SRWLOCK_INIT;
struct Message {char text[256]{};};Message messages[32];unsigned head{},tail{},count{};
std::atomic<unsigned> onceFlags{};
void log(const char* text,unsigned category=0) noexcept {
 if(category && (onceFlags.fetch_or(category)&category))return;
 if(!wake||!TryAcquireSRWLockExclusive(&logLock))return;
 if(count<32){std::snprintf(messages[tail].text,256,"%s",text);tail=(tail+1)%32;++count;}
 ReleaseSRWLockExclusive(&logLock);SetEvent(wake);
}
DWORD WINAPI logWorker(void*) {
 FILE* file=_wfopen(logPath.c_str(),L"w");
 for(;;){WaitForSingleObject(wake,INFINITE);for(;;){Message m{};
 AcquireSRWLockExclusive(&logLock);bool ready=count!=0;
 if(ready){m=messages[head];head=(head+1)%32;--count;}ReleaseSRWLockExclusive(&logLock);
 if(!ready)break;if(file){std::fprintf(file,"%s\n",m.text);std::fflush(file);}
 }}
}
void startLog() {
 wchar_t path[32768]{};auto size=GetModuleFileNameW(module,path,32768);
 std::wstring dir(path,size);dir.resize(dir.find_last_of(L"\\/")+1);
 auto ini=dir+L"HighPolyDismembermentFix.ini";
 enabled=GetPrivateProfileIntW(L"General",L"Enabled",1,ini.c_str())!=0;
 verbose=GetPrivateProfileIntW(L"General",L"Verbose",0,ini.c_str())!=0;
 logPath=dir+L"HighPolyDismembermentFix.log";wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(wake){auto thread=CreateThread(nullptr,0,logWorker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
 std::string startup=std::string(pluginName)+" "+versionLabel+" runtime="+profile->name;log(startup.c_str());
}
template<class T> bool copy(std::uintptr_t p,T& value) noexcept {
 SIZE_T n{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&value,sizeof(T),&n)&&n==sizeof(T);
}
template<class T> T field(std::uintptr_t p,std::size_t offset) noexcept {
 T v{};if(p&&p<=UINTPTR_MAX-offset)copy(p+offset,v);return v;
}
bool bytes(std::uintptr_t p,void* out,std::size_t size) noexcept {
 SIZE_T n{};return p&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,size,&n)&&n==size;
}
// Checked read-only reconstruction of the verified engine segment getters.
// Each row is five dwords; the shared map translates geometry segment to parent row.
bool sourceRanges(Context& c) noexcept {
    // Require the same simple virtual getter used by the native setup. A subclass
    // with a different segment layout is logged without inventing a range mapping.
    const auto vtable=field<std::uintptr_t>(c.shape,0);
    const auto getter=field<std::uintptr_t>(vtable,0x1d8);
    if(getter!=base+profile->getter)return false;
    constexpr std::array<std::uint8_t,8> expected{0x48,0x8b,0x81,0x70,0x01,0,0,0xc3};
    std::array<std::uint8_t,8> code{};
    if(!bytes(getter,code.data(),code.size())||code!=expected)return false;
    const auto seg=field<std::uintptr_t>(c.shape,0x170);
    std::array<std::uint8_t,0x40> prefix{};if(!bytes(seg,prefix.data(),prefix.size()))return false;
    auto u32=[&](int off){std::uint32_t v{};memcpy(&v,prefix.data()+off,4);return v;};
    auto ptr=[&](int off){std::uintptr_t v{};memcpy(&v,prefix.data()+off,8);return v;};
    const auto table=ptr(0x18), shared=ptr(0x10);auto index=c.segment;
    if(index==u32(0x38))index=0;
    if(prefix[0x3d]) {c.ranges[0]={0,u32(0x34),true};c.childCount=0;return true;}
    if(index>=u32(0x2c)||u32(0x2c)>128)return false;
    if(shared) {auto mapping=field<std::uintptr_t>(shared,0x18);if(!copy(mapping+4ull*index,index))return false;}
    if(index>4096)return false;
    std::array<std::uint32_t,5> parent{};if(!bytes(table+20ull*index,parent.data(),20))return false;
    c.childCount=parent[3];if(c.childCount>4096)return false;
    // Native child start/count getters reject child indices without shared data.
    // Do not invent child ranges from adjacent rows in that case.
    if(!shared && c.childCount)return false;
    if(!c.childCount)c.ranges[0]={parent[0],parent[1],true};
    else for(std::size_t i=0;i<std::min<std::uint32_t>(c.childCount,12);++i) {
        std::array<std::uint32_t,5> row{};
        if(!bytes(table+20ull*(index+1+i),row.data(),20))return false;
        c.ranges[i]={row[0],row[1],true};
    }
    return true;
}
Context contextAt(std::uintptr_t state,std::uintptr_t shape) noexcept {
 Context c{};c.state=state;c.shape=shape;
 std::array<std::uint8_t,0xa0> snapshot{};
 if(!bytes(state,snapshot.data(),snapshot.size()))return c;
 c.segment=field<std::uint32_t>(state,0x94);c.limb=field<std::uint32_t>(state,0x8c);
 c.vertices=field<std::uint16_t>(shape,0x164);c.triangles=field<std::uint32_t>(shape,0x160);
 if(c.triangles<=UINT32_MAX/3)c.entries=c.triangles*3;
 c.rangesValid=sourceRanges(c);c.valid=state&&shape&&c.limb<32&&c.segment<=255;return c;
}
struct Scope {Context context{};correction::Plan plan{};bool uploaded{};void* uploadRenderer{};};
thread_local Scope* computeScope{};thread_local unsigned nesting{};
using ShapeFn=std::int32_t(*)(void*,void*);
using UploadFn=void(*)(void*,const void*,std::uint32_t);
using RunFn=void(*)(void*,void*,std::uint32_t,std::uint32_t,std::uint32_t);
ShapeFn originalShape{};UploadFn originalUpload{};RunFn originalRun{};
std::int32_t shapeHook(void* closure,void* slot) {
 if(nesting>=8){auto previous=computeScope;computeScope=nullptr;auto result=originalShape(closure,slot);computeScope=previous;return result;}
 const auto error=GetLastError();Scope scope{};
 scope.context=contextAt(field<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(closure),0x18),field<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(slot),0));
 struct Guard {Scope* previous;Guard(Scope* s):previous(computeScope){computeScope=s;++nesting;}~Guard(){computeScope=previous;--nesting;}}guard(&scope);
 SetLastError(error);return originalShape(closure,slot);
}
void uploadHook(void* renderer,const void* payload,std::uint32_t size) {
 const auto error=GetLastError();
 if(computeScope){computeScope->uploaded=false;std::array<std::uint32_t,84>w{};
 if(size==0x150&&bytes(reinterpret_cast<std::uintptr_t>(payload),w.data(),size)){
 computeScope->plan=correction::prepare(correction::Mode::Enabled,computeScope->context,w);
 computeScope->uploaded=true;computeScope->uploadRenderer=renderer;}}
 SetLastError(error);originalUpload(renderer,payload,size);
}
correction::ShaderCache& shaderCache() {
 // Never release COM objects from a CRT destructor under the loader lock.
 // Retire generations during ordinary dispatch on device change/removal;
 // the final generation and this pinned plugin live until process termination.
 static auto* cache=new correction::ShaderCache;return *cache;
}
struct alignas(8) OwnedDispatchWrapper {std::uint32_t technique{},padding{};ID3D11ComputeShader* shader{};};
static_assert(offsetof(OwnedDispatchWrapper,shader)==8);
ID3D11ComputeShader* selectCorrected(void* wrapper,Scope& scope,std::string& hash,const char*& reason,correction::BindingTelemetry& telemetry,correction::Com<ID3D11DeviceContext>& selectedContext) {
 auto wp=reinterpret_cast<std::uintptr_t>(wrapper);
 if(field<std::uint32_t>(wp,0)!=0){reason="unexpected technique";return nullptr;}
 auto n=field<std::uint32_t>(wp,0x10);
#ifdef HPDF_EXPERIMENTAL
 if(n<44||n>4*1024*1024){reason="selected shader bytecode size outside bounded DXBC inspection";return nullptr;}
#else
 if(n!=89632){reason="selected shader bytecode size mismatch";return nullptr;}
#endif
 std::vector<unsigned char>b(n);
 if(!bytes(wp+0x88,b.data(),b.size())){reason="selected shader bytecode unreadable";return nullptr;}
 hash=correction::sha256(b.data(),b.size());
 if(hash!=correction::nativeHash){
  std::vector<unsigned char> normalized;
  if(!correction::normalizeNative(b,normalized)){reason="unrecognized shader: executable/signature chunks not proven equivalent; native fallback";return nullptr;}
  b=std::move(normalized);log("Experimental DXBC container differs; complete executable/signature chunks match validated shader",8);
 }
 if(!correction::shaderContract(b,false)){reason="selected shader bindings mismatch";return nullptr;}
 auto tlsIndex=field<std::uint32_t>(base,profile->tlsIndex);
 if(tlsIndex>=4096){reason="TLS index invalid";return nullptr;}
 auto tls=field<std::uintptr_t>(static_cast<std::uintptr_t>(__readgsqword(0x58)),8ull*tlsIndex);
 auto ctx=field<ID3D11DeviceContext*>(tls,0xb18);
 auto state=field<std::uintptr_t>(tls,0xb20);
 auto constantBuffer=field<ID3D11Buffer*>(state,0x1b40);
 auto nativeShader=field<ID3D11ComputeShader*>(wp,8);
 if(!ctx||!nativeShader||!constantBuffer){reason="missing context, shader or constant buffer";return nullptr;}
 telemetry.attempted=true;telemetry.context=reinterpret_cast<std::uintptr_t>(ctx);telemetry.contextType=ctx->GetType();
 if(ctx->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE){
  correction::failBinding(telemetry,correction::BindingCheck::ContextType,-1,D3D11_DEVICE_CONTEXT_IMMEDIATE,ctx->GetType());
  reason="deferred context inspection unsupported; native fallback";return nullptr;
 }
 correction::Com<ID3D11Device> d,sd;ctx->GetDevice(d.put());nativeShader->GetDevice(sd.put());
 if(!d.p||d.p!=sd.p){reason="device mismatch";return nullptr;}
 if(FAILED(d.p->GetDeviceRemovedReason())) {
  auto removed=shaderCache().obtain(d.p,b);if(removed)removed->Release();
  reason="device removed; native fallback";return nullptr;
 }
 D3D11_BUFFER_DESC cb{};constantBuffer->GetDesc(&cb);
 correction::Com<ID3D11Device> cbd;constantBuffer->GetDevice(cbd.put());
 telemetry.expectedDevice=reinterpret_cast<std::uintptr_t>(d.p);
 telemetry.constant={true,reinterpret_cast<std::uintptr_t>(constantBuffer),reinterpret_cast<std::uintptr_t>(cbd.p),cb};
 using BC=correction::BindingCheck;
 auto cbFail=[&](BC check,std::uint64_t expected,std::uint64_t observed){
  correction::failBinding(telemetry,check,-2,expected,observed);reason=correction::checkName(check);return nullptr;
 };
 if(cb.ByteWidth<336)return cbFail(BC::ConstantWidth,336,cb.ByteWidth);
 if(cb.Usage!=D3D11_USAGE_DYNAMIC)return cbFail(BC::ConstantUsage,D3D11_USAGE_DYNAMIC,cb.Usage);
 if(!(cb.CPUAccessFlags&D3D11_CPU_ACCESS_WRITE))return cbFail(BC::ConstantWriteAccess,D3D11_CPU_ACCESS_WRITE,cb.CPUAccessFlags);
 if(cbd.p!=d.p)return cbFail(BC::ConstantDevice,reinterpret_cast<std::uintptr_t>(d.p),reinterpret_cast<std::uintptr_t>(cbd.p));
 if(!correction::bindings(ctx,d.p,scope.context,scope.plan.words,&telemetry)){reason=correction::checkName(telemetry.check);return nullptr;}
 auto shader=shaderCache().obtain(d.p,b);
 if(shader){selectedContext.p=ctx;ctx->AddRef();}
 reason=shader?"validated selected shader; plugin variant ready":"shader creation/cache unavailable";
 return shader;
}
void runHook(void* renderer,void* wrapper,std::uint32_t x,std::uint32_t y,std::uint32_t z) {
 const auto entryError=GetLastError();
 struct ErrorGuard {DWORD result;~ErrorGuard(){SetLastError(result);}}error{entryError};
 auto scope=computeScope;
 if(!scope||!scope->uploaded||!installed.load()||reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))!=base+profile->callerReturn){
 SetLastError(entryError);originalRun(renderer,wrapper,x,y,z);error.result=GetLastError();return;}
 scope->uploaded=false;const char* reason=scope->plan.reason;std::string hash;
 correction::BindingTelemetry telemetry{};correction::Com<ID3D11ComputeShader> shader;
 correction::Com<ID3D11DeviceContext> context;
 if(scope->plan.substitute){
 if(renderer!=scope->uploadRenderer||reinterpret_cast<std::uintptr_t>(renderer)!=base+profile->renderer||x!=1||y!=1||z!=1)reason="renderer or dispatch contract mismatch";
 else try {shader.p=selectCorrected(wrapper,*scope,hash,reason,telemetry,context);}catch(...){reason="shader preparation exception; native fallback";}
 } else if(scope->plan.eligible){SetLastError(entryError);originalRun(renderer,wrapper,x,y,z);error.result=GetLastError();return;}
 if(shader.p){
 log("Live DismemberCS validated; corrected shader ready",1);
 OwnedDispatchWrapper selected{0,0,shader.p};
 SetLastError(entryError);originalUpload(renderer,scope->plan.words.data(),0x150);
 SetLastError(entryError);originalRun(renderer,&selected,x,y,z);error.result=GetLastError();
 correction::Com<ID3D11ComputeShader> bound;context.p->CSGetShader(bound.put(),nullptr,nullptr);
 if(bound.p!=shader.p)log("Corrected dispatch returned with unexpected shader binding; no native rerun",2);
 else if(verbose)log("High-start correction submitted");
 }else{
 char detail[256]{};std::snprintf(detail,sizeof(detail),"Native fallback: %s; binding slot=%d",reason,telemetry.failedBinding);
 log(detail,verbose?0:4);
 if(!hash.empty()){std::string identity="Selected shader SHA-256: "+hash;log(identity.c_str(),verbose?0:16);}
 SetLastError(entryError);originalRun(renderer,wrapper,x,y,z);error.result=GetLastError();
 }
}
std::uint64_t fnv(const void* p,std::size_t size){std::uint64_t h=14695981039346656037ull;for(std::size_t i=0;i<size;++i){h^=static_cast<const unsigned char*>(p)[i];h*=1099511628211ull;}return h;}
bool validate() {
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt->OptionalHeader.SizeOfImage!=profile->imageSize)return false;
 for(unsigned i=0;i<profile->count;++i){auto& s=profile->contracts[i];if(std::uint64_t(s.rva)+s.size>profile->imageSize||fnv(reinterpret_cast<void*>(base+s.rva),s.size)!=s.hash){log(s.name);return false;}}
 for(auto call:{profile->shape,profile->upload})if(field<unsigned char>(base+call.site,0)!=0xe8||base+call.site+5+field<std::int32_t>(base+call.site,1)!=base+call.target)return false;
 return field<unsigned char>(base+profile->tail,0)==0xe9&&base+profile->tail+5+field<std::int32_t>(base+profile->tail,1)==base+profile->run;
}
bool install(const F4SEInterface* f4se,PluginHandle plugin) {
 if(!validate()){log("Exact runtime code/layout contract rejected; zero hooks installed");return false;}
 auto api=static_cast<F4SETrampolineInterface*>(f4se->QueryInterface(kInterface_Trampoline));
 if(!api||api->interfaceVersion<1||!api->AllocateFromBranchPool)return false;
 auto pool=api->AllocateFromBranchPool(plugin,96);if(!pool)return false;
 const std::array<std::uint32_t,3> sites{profile->shape.site,profile->upload.site,profile->tail};
 const std::array<std::uintptr_t,3> wrappers{reinterpret_cast<std::uintptr_t>(shapeHook),reinterpret_cast<std::uintptr_t>(uploadHook),reinterpret_cast<std::uintptr_t>(runHook)};
 for(auto rva:sites){auto d=reinterpret_cast<std::intptr_t>(pool)-static_cast<std::intptr_t>(base+rva+5);if(d<INT32_MIN+96ll||d>INT32_MAX-96ll)return false;}
 auto trampoline=new BranchTrampoline;trampoline->SetBase(96,pool); // F4SE owns pool; retain wrapper until process exit.
 std::array<DWORD,3> protection{};std::array<std::array<unsigned char,5>,3> saved{};unsigned writable=0;
 for(;writable<3;++writable){auto p=reinterpret_cast<void*>(base+sites[writable]);memcpy(saved[writable].data(),p,5);if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&protection[writable]))break;}
 originalShape=reinterpret_cast<ShapeFn>(base+profile->shape.target);originalUpload=reinterpret_cast<UploadFn>(base+profile->upload.target);originalRun=reinterpret_cast<RunFn>(base+profile->run);
 bool ok=writable==3;
 if(ok){ok=trampoline->Write5Call(base+sites[0],wrappers[0])&&trampoline->Write5Call(base+sites[1],wrappers[1])&&trampoline->Write5Branch(base+sites[2],wrappers[2]);}
 if(ok)for(unsigned i=0;i<3;++i){auto site=base+sites[i];auto stub=site+5+field<std::int32_t>(site,1);constexpr std::array<unsigned char,6> expected{0xff,0x25,0,0,0,0};std::array<unsigned char,6> observed{};ok=field<unsigned char>(site,0)==(i==2?0xe9:0xe8)&&bytes(stub,observed.data(),6)&&observed==expected&&field<std::uintptr_t>(stub,6)==wrappers[i]&&ok;}
 if(!ok)for(unsigned i=0;i<writable;++i)memcpy(reinterpret_cast<void*>(base+sites[i]),saved[i].data(),5);
 // All pages were already made writable; undo every hook if protection restore fails.
 bool restore=true;for(unsigned i=0;i<writable;++i){DWORD unused{};if(!VirtualProtect(reinterpret_cast<void*>(base+sites[i]),5,protection[i],&unused))restore=false;}
 if(!restore){for(unsigned i=0;i<writable;++i){DWORD unused{};auto p=reinterpret_cast<void*>(base+sites[i]);if(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&unused)){memcpy(p,saved[i].data(),5);VirtualProtect(p,5,protection[i],&unused);}}ok=false;}
 FlushInstructionCache(GetCurrentProcess(),nullptr,0);installed.store(ok);log(ok?"Three correction hooks installed":"Hook installation failed; native processing retained");return ok;
}
}
static_assert(sizeof(F4SEPluginVersionData)==0x45c);
extern "C" __declspec(dllexport) constinit F4SEPluginVersionData F4SEPlugin_Version=[]() constexpr {
 F4SEPluginVersionData v{};v.dataVersion=1;
#ifdef HPDF_EXPERIMENTAL
 v.pluginVersion=MAKE_EXE_VERSION(1,0,1);v.compatibleVersions[0]=0x010b0dd0;v.compatibleVersions[1]=0x010b0f00;v.seVersionRequired=MAKE_EXE_VERSION(0,7,8);
#else
 v.pluginVersion=MAKE_EXE_VERSION(1,0,0);v.compatibleVersions[0]=0x010b0f00;v.seVersionRequired=MAKE_EXE_VERSION(0,7,9);
#endif
 constexpr char n[]="HighPolyDismembermentFix",a[]="High-Poly Dismemberment Fix contributors";
 for(unsigned i=0;i<sizeof(n);++i)v.name[i]=n[i];for(unsigned i=0;i<sizeof(a);++i)v.author[i]=a[i];
 v.structureIndependence=F4SEPluginVersionData::kStructureIndependence_1_11_137Layout;return v;}();
extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SEInterface* f4se){
 try{
 if(!f4se||f4se->isEditor||!f4se->QueryInterface||!f4se->GetPluginHandle)return false;
 profile=release::select(f4se->runtimeVersion);if(!profile||f4se->f4seVersion<profile->minF4se)return false;
 base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&F4SEPlugin_Load),&module))return false;
 startLog();if(!enabled){log("Disabled in configuration; zero hooks installed");return true;}
 if(!install(f4se,f4se->GetPluginHandle())){log("Correction disabled: startup validation or installation failed");return false;}return true;
 }catch(...){log("Startup exception; correction unavailable");return false;}
}
