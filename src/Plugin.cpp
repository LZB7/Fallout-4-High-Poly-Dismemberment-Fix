// F4SE entry point (CommonLibF4): resolves Address Library IDs, verifies every
// interception contract in src/Sites.h and installs the three hooks.
// One DLL serves OG 1.10.163, NG 1.10.980/984 and AE 1.11.137-240.
#include "F4SE/F4SE.h"
#include "REL/IDDB.h"

#include "Runtime.h"
#include "Sites.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <span>

namespace hpdf {
void WriteLogLine(const char* text) noexcept
{
	try {
		REX::INFO("{}", text);
	} catch (...) {
	}
}

namespace {
// IDDB::offset() aborts the game on a missing ID; resolve without it so an
// unknown build fails closed with zero hooks instead.
struct IdTable : REL::IDDB
{
	static std::uintptr_t resolve(std::uint64_t id) noexcept
	{
		const auto* db = REL::IDDB::GetSingleton();
		if (!db) return 0;
		const auto base = REX::FModule::GetExecutingModule().GetBaseAddress();
		constexpr auto v0 = &IdTable::get_id2offset<REL::IDDB::MAPPING>;
		if (const auto table = (db->*v0)(); !table.empty()) {
			const auto it = std::lower_bound(table.begin(), table.end(), id, [](const REL::IDDB::MAPPING& m, std::uint64_t key) { return m.id < key; });
			return it != table.end() && it->id == id && it->offset ? base + static_cast<std::uintptr_t>(it->offset) : 0;
		}
		constexpr auto v5 = &IdTable::get_id2offset<std::uint32_t>;
		const auto table = (db->*v5)();
		return id < table.size() && table[id] ? base + table[id] : 0;
	}
};

const char* runtimeName() noexcept
{
	if (REX::FModule::IsRuntimeOG()) return "OG";
	if (REX::FModule::IsRuntimeNG()) return "NG";
	return "AE";
}

// The site holds a rel32 call/jmp into a trampoline stub "FF 25 00000000 <hook>".
bool redirected(std::uintptr_t site, std::uint8_t opcode, std::uintptr_t hook) noexcept
{
	std::uint8_t op{};
	std::int32_t disp{};
	if (!ReadMemory(site, &op, 1) || op != opcode || !ReadMemory(site + 1, &disp, 4)) return false;
	const auto stub = site + 5 + static_cast<std::intptr_t>(disp);
	std::array<std::uint8_t, 6> jmp{};
	std::uintptr_t target{};
	constexpr std::array<std::uint8_t, 6> expected{ 0xFF, 0x25, 0, 0, 0, 0 };
	return ReadMemory(stub, jmp.data(), jmp.size()) && jmp == expected && ReadMemory(stub + 6, &target, sizeof(target)) && target == hook;
}

bool install()
{
	const auto layout = REX::FModule::IsRuntimeOG() ? sites::Layout::OG : sites::Layout::NGAE;
	sites::Addresses a{};
	for (unsigned f = 0; f < sites::FieldCount; ++f)
		a[f] = IdTable::resolve(sites::ids[static_cast<unsigned>(layout)][f]);
	if (const char* failure = sites::verify(layout, a, ReadMemory)) {
		WriteLogLine(std::format("Runtime contract rejected ({}); zero hooks installed", failure).c_str());
		return false;
	}
	const auto h = sites::hooks(layout, a);
	ConfigureRuntime({ a[sites::PerShape], a[sites::SetComputeConstants], a[sites::RunComputeShader], a[sites::Renderer], h.callerReturn, a[sites::TlsIndex], a[sites::SegmentGetter] });

	const std::array<std::uintptr_t, 3> siteList{ h.shapeSite, h.uploadSite, h.tailSite };
	std::array<std::array<std::uint8_t, 5>, 3> saved{};
	for (unsigned i = 0; i < 3; ++i)
		std::memcpy(saved[i].data(), reinterpret_cast<const void*>(siteList[i]), 5);

	auto& trampoline = REL::GetTrampoline();
	const auto shapeHook = reinterpret_cast<std::uintptr_t>(&ShapeHook);
	const auto uploadHook = reinterpret_cast<std::uintptr_t>(&UploadHook);
	const auto runHook = reinterpret_cast<std::uintptr_t>(&RunHook);
	bool ok = trampoline.write_call<5>(h.shapeSite, shapeHook) == a[sites::PerShape];
	ok = trampoline.write_call<5>(h.uploadSite, uploadHook) == a[sites::SetComputeConstants] && ok;
	ok = trampoline.write_jmp<5>(h.tailSite, runHook) == a[sites::RunComputeShader] && ok;
	ok = ok && redirected(h.shapeSite, 0xE8, shapeHook) && redirected(h.uploadSite, 0xE8, uploadHook) && redirected(h.tailSite, 0xE9, runHook);
	if (!ok) {
		for (unsigned i = 0; i < 3; ++i)
			REL::WriteSafe(siteList[i], saved[i].data(), saved[i].size());
		WriteLogLine("Hook installation failed verification; original instructions restored");
		return false;
	}
	SetInstalled(true);
	WriteLogLine(std::format("Three correction hooks installed (shape {:X}, upload {:X}, tail {:X})", h.shapeSite, h.uploadSite, h.tailSite).c_str());
	return true;
}
}  // namespace
}  // namespace hpdf

// OG F4SE (0.6.x) loads plugins that answer this query; NG/AE F4SE reads F4SEPlugin_Version.
F4SE_PLUGIN_QUERY(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	const auto* data = F4SE::PluginVersionData::GetSingleton();
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = data ? data->GetPluginName().data() : "HighPolyDismembermentFix";
	a_info->version = data ? data->GetPluginVersion().pack() : 0;
	return a_f4se && !a_f4se->IsEditor();
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	if (!a_f4se || a_f4se->IsEditor()) return false;
	F4SE::Init(a_f4se, { .trampoline = true, .trampolineSize = 128 });
	try {
		const auto settings = hpdf::StartRuntime(std::format("{} {}", hpdf::runtimeName(), a_f4se->RuntimeVersion().string()).c_str());
		if (!settings.enabled) {
			hpdf::WriteLogLine("Disabled in configuration; zero hooks installed");
			return true;
		}
		if (!hpdf::install()) {
			hpdf::WriteLogLine("Correction disabled: startup validation or installation failed; native processing retained");
			return false;
		}
		return true;
	} catch (...) {
		hpdf::WriteLogLine("Startup exception; correction unavailable");
		return false;
	}
}
