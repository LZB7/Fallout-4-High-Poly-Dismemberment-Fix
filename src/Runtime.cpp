// Correction runtime: the three synchronous interception points, checked reads,
// shader selection and the asynchronous message queue. Uses the Windows SDK
// D3D11 headers directly and never includes CommonLibF4.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Runtime.h"
#include "Correction.h"
#include "ShaderResources.h"
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <string>

#pragma intrinsic(_ReturnAddress)

namespace hpdf {
namespace {
using namespace diag;
constexpr char pluginName[] = "HighPolyDismembermentFix";
#ifdef HPDF_EXPERIMENTAL
constexpr char versionLabel[] = HPDF_VERSION_LABEL "-experimental";
#else
constexpr char versionLabel[] = HPDF_VERSION_LABEL;
#endif

RuntimeAddresses addr{};
std::atomic<bool> installed{};
bool verbose = false;

// Minimal asynchronous queue: render-thread callers never block on file I/O.
HANDLE wake{};
SRWLOCK logLock = SRWLOCK_INIT;
struct Message { char text[256]{}; };
Message messages[32];
unsigned head{}, tail{}, count{};
std::atomic<unsigned> onceFlags{};

void log(const char* text, unsigned category = 0) noexcept
{
	if (category && (onceFlags.fetch_or(category) & category)) return;
	if (!wake || !TryAcquireSRWLockExclusive(&logLock)) return;
	if (count < 32) {
		std::snprintf(messages[tail].text, 256, "%s", text);
		tail = (tail + 1) % 32;
		++count;
	}
	ReleaseSRWLockExclusive(&logLock);
	SetEvent(wake);
}
DWORD WINAPI logWorker(void*)
{
	for (;;) {
		WaitForSingleObject(wake, INFINITE);
		for (;;) {
			Message m{};
			AcquireSRWLockExclusive(&logLock);
			const bool ready = count != 0;
			if (ready) {
				m = messages[head];
				head = (head + 1) % 32;
				--count;
			}
			ReleaseSRWLockExclusive(&logLock);
			if (!ready) break;
			WriteLogLine(m.text);
		}
	}
}

template <class T> bool copy(std::uintptr_t p, T& value) noexcept
{
	SIZE_T n{};
	return p && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(p), &value, sizeof(T), &n) && n == sizeof(T);
}
template <class T> T field(std::uintptr_t p, std::size_t offset) noexcept
{
	T v{};
	if (p && p <= UINTPTR_MAX - offset) copy(p + offset, v);
	return v;
}
bool bytes(std::uintptr_t p, void* out, std::size_t size) noexcept
{
	SIZE_T n{};
	return p && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(p), out, size, &n) && n == size;
}

// Checked read-only reconstruction of the verified engine segment getters.
// Each row is five dwords; the shared map translates geometry segment to parent row.
bool sourceRanges(Context& c) noexcept
{
	// Require the same simple virtual getter used by the native setup. A subclass
	// with a different segment layout is logged without inventing a range mapping.
	const auto vtable = field<std::uintptr_t>(c.shape, 0);
	const auto getter = field<std::uintptr_t>(vtable, 0x1d8);
	if (getter != addr.segmentGetter) return false;
	constexpr std::array<std::uint8_t, 8> expected{ 0x48, 0x8b, 0x81, 0x70, 0x01, 0, 0, 0xc3 };
	std::array<std::uint8_t, 8> code{};
	if (!bytes(getter, code.data(), code.size()) || code != expected) return false;
	const auto seg = field<std::uintptr_t>(c.shape, 0x170);
	std::array<std::uint8_t, 0x40> prefix{};
	if (!bytes(seg, prefix.data(), prefix.size())) return false;
	auto u32 = [&](int off) { std::uint32_t v{}; memcpy(&v, prefix.data() + off, 4); return v; };
	auto ptr = [&](int off) { std::uintptr_t v{}; memcpy(&v, prefix.data() + off, 8); return v; };
	const auto table = ptr(0x18), shared = ptr(0x10);
	auto index = c.segment;
	if (index == u32(0x38)) index = 0;
	if (prefix[0x3d]) {
		c.ranges[0] = { 0, u32(0x34), true };
		c.childCount = 0;
		return true;
	}
	if (index >= u32(0x2c) || u32(0x2c) > 128) return false;
	if (shared) {
		auto mapping = field<std::uintptr_t>(shared, 0x18);
		if (!copy(mapping + 4ull * index, index)) return false;
	}
	if (index > 4096) return false;
	std::array<std::uint32_t, 5> parent{};
	if (!bytes(table + 20ull * index, parent.data(), 20)) return false;
	c.childCount = parent[3];
	if (c.childCount > 4096) return false;
	// Native child start/count getters reject child indices without shared data.
	// Do not invent child ranges from adjacent rows in that case.
	if (!shared && c.childCount) return false;
	if (!c.childCount) c.ranges[0] = { parent[0], parent[1], true };
	else
		for (std::size_t i = 0; i < std::min<std::uint32_t>(c.childCount, 12); ++i) {
			std::array<std::uint32_t, 5> row{};
			if (!bytes(table + 20ull * (index + 1 + i), row.data(), 20)) return false;
			c.ranges[i] = { row[0], row[1], true };
		}
	return true;
}
Context contextAt(std::uintptr_t state, std::uintptr_t shape) noexcept
{
	Context c{};
	c.state = state;
	c.shape = shape;
	std::array<std::uint8_t, 0xa0> snapshot{};
	if (!bytes(state, snapshot.data(), snapshot.size())) return c;
	c.segment = field<std::uint32_t>(state, 0x94);
	c.limb = field<std::uint32_t>(state, 0x8c);
	c.vertices = field<std::uint16_t>(shape, 0x164);
	c.triangles = field<std::uint32_t>(shape, 0x160);
	if (c.triangles <= UINT32_MAX / 3) c.entries = c.triangles * 3;
	c.rangesValid = sourceRanges(c);
	c.valid = state && shape && c.limb < 32 && c.segment <= 255;
	return c;
}

struct Scope { Context context{}; correction::Plan plan{}; bool uploaded{}; void* uploadRenderer{}; };
thread_local Scope* computeScope{};
thread_local unsigned nesting{};
using ShapeFn = std::int32_t (*)(void*, void*);
using UploadFn = void (*)(void*, const void*, std::uint32_t);
using RunFn = void (*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);
ShapeFn originalShape{};
UploadFn originalUpload{};
RunFn originalRun{};

correction::ShaderCache& shaderCache()
{
	// Never release COM objects from a CRT destructor under the loader lock.
	// Retire generations during ordinary dispatch on device change/removal;
	// the final generation and this pinned plugin live until process termination.
	static auto* cache = new correction::ShaderCache;
	return *cache;
}
struct alignas(8) OwnedDispatchWrapper { std::uint32_t technique{}, padding{}; ID3D11ComputeShader* shader{}; };
static_assert(offsetof(OwnedDispatchWrapper, shader) == 8);

ID3D11ComputeShader* selectCorrected(void* wrapper, Scope& scope, std::string& hash, const char*& reason, correction::BindingTelemetry& telemetry, correction::Com<ID3D11DeviceContext>& selectedContext)
{
	auto wp = reinterpret_cast<std::uintptr_t>(wrapper);
	if (field<std::uint32_t>(wp, 0) != 0) { reason = "unexpected technique"; return nullptr; }
	auto n = field<std::uint32_t>(wp, 0x10);
#ifdef HPDF_EXPERIMENTAL
	if (n < 44 || n > 4 * 1024 * 1024) { reason = "selected shader bytecode size outside bounded DXBC inspection"; return nullptr; }
#else
	if (n != 89632) { reason = "selected shader bytecode size mismatch"; return nullptr; }
#endif
	std::vector<unsigned char> b(n);
	if (!bytes(wp + 0x88, b.data(), b.size())) { reason = "selected shader bytecode unreadable"; return nullptr; }
	hash = correction::sha256(b.data(), b.size());
	if (hash != correction::nativeHash) {
		std::vector<unsigned char> normalized;
		if (!correction::normalizeNative(b, normalized)) { reason = "unrecognized shader: executable/signature chunks not proven equivalent; native fallback"; return nullptr; }
		b = std::move(normalized);
		log("Experimental DXBC container differs; complete executable/signature chunks match validated shader", 8);
	}
	if (!correction::shaderContract(b, false)) { reason = "selected shader bindings mismatch"; return nullptr; }
	auto tlsIndex = field<std::uint32_t>(addr.tlsIndex, 0);
	if (tlsIndex >= 4096) { reason = "TLS index invalid"; return nullptr; }
	auto tls = field<std::uintptr_t>(static_cast<std::uintptr_t>(__readgsqword(0x58)), 8ull * tlsIndex);
	auto ctx = field<ID3D11DeviceContext*>(tls, 0xb18);
	auto state = field<std::uintptr_t>(tls, 0xb20);
	auto constantBuffer = field<ID3D11Buffer*>(state, 0x1b40);
	auto nativeShader = field<ID3D11ComputeShader*>(wp, 8);
	if (!ctx || !nativeShader || !constantBuffer) { reason = "missing context, shader or constant buffer"; return nullptr; }
	telemetry.attempted = true;
	telemetry.context = reinterpret_cast<std::uintptr_t>(ctx);
	telemetry.contextType = ctx->GetType();
	if (ctx->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
		correction::failBinding(telemetry, correction::BindingCheck::ContextType, -1, D3D11_DEVICE_CONTEXT_IMMEDIATE, ctx->GetType());
		reason = "deferred context inspection unsupported; native fallback";
		return nullptr;
	}
	correction::Com<ID3D11Device> d, sd;
	ctx->GetDevice(d.put());
	nativeShader->GetDevice(sd.put());
	if (!d.p || d.p != sd.p) { reason = "device mismatch"; return nullptr; }
	if (FAILED(d.p->GetDeviceRemovedReason())) {
		auto removed = shaderCache().obtain(d.p, b);
		if (removed) removed->Release();
		reason = "device removed; native fallback";
		return nullptr;
	}
	D3D11_BUFFER_DESC cb{};
	constantBuffer->GetDesc(&cb);
	correction::Com<ID3D11Device> cbd;
	constantBuffer->GetDevice(cbd.put());
	telemetry.expectedDevice = reinterpret_cast<std::uintptr_t>(d.p);
	telemetry.constant = { true, reinterpret_cast<std::uintptr_t>(constantBuffer), reinterpret_cast<std::uintptr_t>(cbd.p), cb };
	using BC = correction::BindingCheck;
	auto cbFail = [&](BC check, std::uint64_t expected, std::uint64_t observed) -> ID3D11ComputeShader* {
		correction::failBinding(telemetry, check, -2, expected, observed);
		reason = correction::checkName(check);
		return nullptr;
	};
	if (cb.ByteWidth < 336) return cbFail(BC::ConstantWidth, 336, cb.ByteWidth);
	if (cb.Usage != D3D11_USAGE_DYNAMIC) return cbFail(BC::ConstantUsage, D3D11_USAGE_DYNAMIC, cb.Usage);
	if (!(cb.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE)) return cbFail(BC::ConstantWriteAccess, D3D11_CPU_ACCESS_WRITE, cb.CPUAccessFlags);
	if (cbd.p != d.p) return cbFail(BC::ConstantDevice, reinterpret_cast<std::uintptr_t>(d.p), reinterpret_cast<std::uintptr_t>(cbd.p));
	if (!correction::bindings(ctx, d.p, scope.context, scope.plan.words, &telemetry)) { reason = correction::checkName(telemetry.check); return nullptr; }
	auto shader = shaderCache().obtain(d.p, b);
	if (shader) {
		selectedContext.p = ctx;
		ctx->AddRef();
	}
	reason = shader ? "validated selected shader; plugin variant ready" : "shader creation/cache unavailable";
	return shader;
}
}  // namespace

std::int32_t ShapeHook(void* closure, void* slot)
{
	if (nesting >= 8) {
		auto previous = computeScope;
		computeScope = nullptr;
		auto result = originalShape(closure, slot);
		computeScope = previous;
		return result;
	}
	const auto error = GetLastError();
	Scope scope{};
	scope.context = contextAt(field<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(closure), 0x18), field<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(slot), 0));
	struct Guard
	{
		Scope* previous;
		Guard(Scope* s) : previous(computeScope) { computeScope = s; ++nesting; }
		~Guard() { computeScope = previous; --nesting; }
	} guard(&scope);
	SetLastError(error);
	return originalShape(closure, slot);
}

void UploadHook(void* renderer, const void* payload, std::uint32_t size)
{
	const auto error = GetLastError();
	if (computeScope) {
		computeScope->uploaded = false;
		std::array<std::uint32_t, 84> w{};
		if (size == 0x150 && bytes(reinterpret_cast<std::uintptr_t>(payload), w.data(), size)) {
			computeScope->plan = correction::prepare(correction::Mode::Enabled, computeScope->context, w);
			computeScope->uploaded = true;
			computeScope->uploadRenderer = renderer;
		}
	}
	SetLastError(error);
	originalUpload(renderer, payload, size);
}

void RunHook(void* renderer, void* wrapper, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
	const auto entryError = GetLastError();
	struct ErrorGuard { DWORD result; ~ErrorGuard() { SetLastError(result); } } error{ entryError };
	auto scope = computeScope;
	// Reached by the technique Run tail jump: the return address is Run's caller.
	if (!scope || !scope->uploaded || !installed.load() || reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != addr.callerReturn) {
		SetLastError(entryError);
		originalRun(renderer, wrapper, x, y, z);
		error.result = GetLastError();
		return;
	}
	scope->uploaded = false;
	const char* reason = scope->plan.reason;
	std::string hash;
	correction::BindingTelemetry telemetry{};
	correction::Com<ID3D11ComputeShader> shader;
	correction::Com<ID3D11DeviceContext> context;
	if (scope->plan.substitute) {
		if (renderer != scope->uploadRenderer || reinterpret_cast<std::uintptr_t>(renderer) != addr.renderer || x != 1 || y != 1 || z != 1)
			reason = "renderer or dispatch contract mismatch";
		else
			try {
				shader.p = selectCorrected(wrapper, *scope, hash, reason, telemetry, context);
			} catch (...) {
				reason = "shader preparation exception; native fallback";
			}
	} else if (scope->plan.eligible) {
		SetLastError(entryError);
		originalRun(renderer, wrapper, x, y, z);
		error.result = GetLastError();
		return;
	}
	if (shader.p) {
		log("Live DismemberCS validated; corrected shader ready", 1);
		OwnedDispatchWrapper selected{ 0, 0, shader.p };
		SetLastError(entryError);
		originalUpload(renderer, scope->plan.words.data(), 0x150);
		SetLastError(entryError);
		originalRun(renderer, &selected, x, y, z);
		error.result = GetLastError();
		correction::Com<ID3D11ComputeShader> bound;
		context.p->CSGetShader(bound.put(), nullptr, nullptr);
		if (bound.p != shader.p) log("Corrected dispatch returned with unexpected shader binding; no native rerun", 2);
		else if (verbose) log("High-start correction submitted");
	} else {
		char detail[256]{};
		std::snprintf(detail, sizeof(detail), "Native fallback: %s; binding slot=%d", reason, telemetry.failedBinding);
		log(detail, verbose ? 0 : 4);
		if (!hash.empty()) {
			std::string identity = "Selected shader SHA-256: " + hash;
			log(identity.c_str(), verbose ? 0 : 16);
		}
		SetLastError(entryError);
		originalRun(renderer, wrapper, x, y, z);
		error.result = GetLastError();
	}
}

Settings StartRuntime(const char* runtimeLabel) noexcept
{
	Settings settings{};
	HMODULE module{};
	// Pin: hooks and the trampoline reference this module for the process lifetime.
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(&StartRuntime), &module)) {
		settings.enabled = false;
		return settings;
	}
	wchar_t path[32768]{};
	auto size = GetModuleFileNameW(module, path, 32768);
	std::wstring dir(path, size);
	dir.resize(dir.find_last_of(L"\\/") + 1);
	auto ini = dir + L"HighPolyDismembermentFix.ini";
	settings.enabled = GetPrivateProfileIntW(L"General", L"Enabled", 1, ini.c_str()) != 0;
	settings.verbose = verbose = GetPrivateProfileIntW(L"General", L"Verbose", 0, ini.c_str()) != 0;
	wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (wake) {
		auto thread = CreateThread(nullptr, 0, logWorker, nullptr, 0, nullptr);
		if (thread) CloseHandle(thread);
	}
	char startup[160]{};
	std::snprintf(startup, sizeof(startup), "%s %s runtime=%s", pluginName, versionLabel, runtimeLabel);
	WriteLogLine(startup);
	return settings;
}

void ConfigureRuntime(const RuntimeAddresses& addresses) noexcept
{
	addr = addresses;
	originalShape = reinterpret_cast<ShapeFn>(addr.perShape);
	originalUpload = reinterpret_cast<UploadFn>(addr.setComputeConstants);
	originalRun = reinterpret_cast<RunFn>(addr.runComputeShader);
}

void SetInstalled(bool value) noexcept { installed.store(value); }
void Log(const char* text) noexcept { log(text); }
bool ReadMemory(std::uintptr_t address, void* out, std::size_t size) noexcept { return bytes(address, out, size); }
}  // namespace hpdf
