#pragma once
// Boundary between the CommonLibF4 entry point (Plugin.cpp, Hooks.cpp) and the
// correction runtime (Runtime.cpp), which uses the Windows SDK D3D11 headers.
// Only plain types cross it.
#include <cstddef>
#include <cstdint>

namespace hpdf {
struct RuntimeAddresses
{
	std::uintptr_t perShape{};             // original per-shape compute target
	std::uintptr_t setComputeConstants{};  // original constant upload target
	std::uintptr_t runComputeShader{};     // original tail-jump target
	std::uintptr_t renderer{};             // BSGraphics::Renderer singleton
	std::uintptr_t callerReturn{};         // per-shape return address after technique Run
	std::uintptr_t tlsIndex{};             // address of the game's TLS index
	std::uintptr_t segmentGetter{};        // BSSubIndexTriShape segment-data getter
};

struct Settings
{
	bool enabled{ true };
	bool verbose{ false };
};

// Reads HighPolyDismembermentFix.ini beside the DLL, pins the module and
// starts the asynchronous message queue.
Settings StartRuntime(const char* runtimeLabel) noexcept;
// Must precede hook installation; the hooks forward to these originals.
void ConfigureRuntime(const RuntimeAddresses& addresses) noexcept;
void SetInstalled(bool installed) noexcept;
// Non-blocking; drops messages when the queue is busy or full.
void Log(const char* text) noexcept;
// Checked read of process memory; false instead of faulting.
bool ReadMemory(std::uintptr_t address, void* out, std::size_t size) noexcept;

std::int32_t ShapeHook(void* closure, void* slot);
void UploadHook(void* renderer, const void* payload, std::uint32_t size);
void RunHook(void* renderer, void* wrapper, std::uint32_t x, std::uint32_t y, std::uint32_t z);

// Implemented by Plugin.cpp: writes one line to the F4SE plugin log.
void WriteLogLine(const char* text) noexcept;
}  // namespace hpdf
