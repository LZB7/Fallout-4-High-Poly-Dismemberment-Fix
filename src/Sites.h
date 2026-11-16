#pragma once
// Interception sites for OG 1.10.163, NG 1.10.980/984 and AE 1.11.137-240.
// Pure C++: shared by the plugin (live process memory) and tests/SiteTests.cpp
// (mapped reference EXEs plus Address Library bins). Addresses come from
// Address Library IDs; every hook site, call target, RIP-relative global and
// engine layout constant the correction relies on is verified before any write.
// A build whose code differs anywhere in these windows fails closed.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace hpdf::sites {
// NG and AE share IDs and code layout; OG uses its own IDs and offsets.
enum class Layout : unsigned { OG = 0, NGAE = 1 };

enum Field : unsigned {
	DoCut,                // BGSDismembermentManager::DoCut
	PerShape,             // DoCut per-shape compute lambda (range packing, upload, dispatch)
	SetComputeConstants,  // BSGraphics::Renderer::SetComputeConstants(data, size)
	TechniqueRun,         // BSComputeShader::Run(technique, x, y, z)
	RunComputeShader,     // BSGraphics::Renderer::RunComputeShader(shader, x, y, z)
	Renderer,             // BSGraphics::Renderer singleton
	TlsIndex,             // game TLS index (thread render context at +0xB18/+0xB20)
	SegmentGetter,        // BSSubIndexTriShape virtual segment-data getter (+0x1D8)
	StartGetter,          // BSGeometrySegmentData::GetStartIndex
	CountGetter,          // BSGeometrySegmentData::GetNumPrimitives
	FieldCount
};

using Ids = std::array<std::uint64_t, FieldCount>;
inline constexpr std::array<Ids, 2> ids{ {
	{ 1477377, 339780, 446661, 260024, 1108829, 1378294, 842564, 865432, 561166, 467567 },
	{ 2196669, 2196736, 2276958, 2319681, 2276940, 2704525, 2787938, 2276005, 2270021, 2270022 },
} };

using Addresses = std::array<std::uintptr_t, FieldCount>;

struct Offsets
{
	std::uint32_t shapeCall;     // DoCut: call PerShape
	std::uint32_t uploadCall;    // PerShape: call SetComputeConstants(payload, 0x150)
	std::uint32_t callerReturn;  // PerShape: return address after the technique Run call
	std::uint32_t tailJump;      // TechniqueRun: jmp RunComputeShader
};
inline constexpr std::array<Offsets, 2> offsets{ {
	{ 0x3D7, 0x4EE, 0x5F6, 0x5F },
	{ 0x317, 0x7C5, 0x8C3, 0x5A },
} };

struct Hooks
{
	std::uintptr_t shapeSite, uploadSite, tailSite, callerReturn;
};
inline Hooks hooks(Layout layout, const Addresses& a) noexcept
{
	const auto& o = offsets[static_cast<unsigned>(layout)];
	return { a[DoCut] + o.shapeCall, a[PerShape] + o.uploadCall, a[TechniqueRun] + o.tailJump, a[PerShape] + o.callerReturn };
}

// Hex bytes separated by spaces; "??" is a wildcard.
struct Exact
{
	Field anchor;
	std::uint32_t offset;
	std::string_view pattern;
	const char* what;
};
// The pattern must occur somewhere in [anchor, anchor + span).
struct Contains
{
	Field anchor;
	std::uint32_t begin, span;
	std::string_view pattern;
	const char* what;
};
// rel32 call (E8), jmp (E9) or RIP-relative operand resolving to another field.
struct Ref
{
	Field anchor;
	std::uint32_t offset;   // instruction start
	std::uint8_t disp;      // rel32 offset within instruction
	std::uint8_t length;    // instruction length
	Field target;
	const char* what;
};
// Byte-identical engine helpers whose layout the plugin reimplements read-only.
struct Hash
{
	Field anchor;
	std::uint32_t size;
	std::uint64_t fnv1a;
	const char* what;
};

// Identical in every layout.
inline constexpr std::array<Contains, 14> sharedContains{ {
	{ SetComputeConstants, 0, 0xB4, "65 48 8B 04 25 58 00 00 00", "SetComputeConstants reads game TLS" },
	{ SetComputeConstants, 0, 0xB4, "BF 18 0B 00 00", "SetComputeConstants uses TLS context +0xB18" },
	{ SetComputeConstants, 0, 0xB4, "BB 20 0B 00 00", "SetComputeConstants uses TLS state +0xB20" },
	{ SetComputeConstants, 0, 0xB4, "48 8B 92 40 1B 00 00", "SetComputeConstants maps CB0 at state +0x1B40" },
	{ SetComputeConstants, 0, 0xB4, "FF 50 70", "SetComputeConstants Map" },
	{ SetComputeConstants, 0, 0xB4, "FF 50 78", "SetComputeConstants Unmap" },
	{ RunComputeShader, 0, 0xB2, "48 8B 57 08", "RunComputeShader binds wrapper +0x08 shader" },
	{ RunComputeShader, 0, 0xB2, "41 BE 18 0B 00 00", "RunComputeShader uses TLS context +0xB18" },
	{ RunComputeShader, 0, 0xB2, "FF 90 28 02 00 00", "RunComputeShader CSSetShader" },
	{ RunComputeShader, 0, 0xB2, "41 B9 20 0B 00 00", "RunComputeShader uses TLS state +0xB20" },
	{ RunComputeShader, 0, 0xB2, "49 81 C1 40 1B 00 00", "RunComputeShader binds CB0 at state +0x1B40" },
	{ RunComputeShader, 0, 0xB2, "FF 90 38 02 00 00", "RunComputeShader CSSetConstantBuffers" },
	{ RunComputeShader, 0, 0xB2, "48 FF A0 48 01 00 00", "RunComputeShader Dispatch tail" },
	{ PerShape, 0x50, 0x70, "41 8B 88 94 00 00 00", "PerShape reads state +0x94 segment" },
} };
inline constexpr std::array<Hash, 2> sharedHashes{ {
	{ StartGetter, 0x65, 0x62aadd2410f824c7ull, "Segment range start getter" },
	{ CountGetter, 0x6A, 0x3eb6b66b1bcda404ull, "Segment triangle count getter" },
} };
inline constexpr std::string_view segmentGetterCode = "48 8B 81 70 01 00 00 C3";

inline constexpr std::array<Exact, 7> exactOG{ {
	{ DoCut, 0x3D0, "48 8D 4D A8 48 8B D3 E8 ?? ?? ?? ?? 83 F8 01 75 0E", "DoCut per-shape call and loop" },
	{ PerShape, 0x60, "4C 8B 46 18", "PerShape closure +0x18 state" },
	{ PerShape, 0xBC, "FF 90 D8 01 00 00", "PerShape segment getter vcall" },
	{ PerShape, 0x4DD, "48 8D 55 60 48 8D 0D ?? ?? ?? ?? 41 B8 50 01 00 00 E8 ?? ?? ?? ??", "PerShape 0x150 constant upload" },
	{ PerShape, 0x5D6, "48 8B 0D ?? ?? ?? ?? 41 B9 01 00 00 00 48 8B 01 33 D2 45 8B C1 C7 44 24 20 01 00 00 00 FF 50 10 48 8B 97 00 01 00 00", "PerShape technique Run call and return" },
	{ TechniqueRun, 0x0, "40 53 48 83 EC 30 4C 8B D9 48 8B 49 48", "Technique lookup prologue" },
	{ TechniqueRun, 0x50, "48 8D 0D ?? ?? ?? ?? 48 8B D3 48 83 C4 30 5B E9 ?? ?? ?? ??", "Technique Run tail" },
} };
inline constexpr std::array<Ref, 8> refsOG{ {
	{ DoCut, 0x3D7, 1, 5, PerShape, "DoCut calls PerShape" },
	{ PerShape, 0x2C0, 1, 5, StartGetter, "PerShape calls start getter" },
	{ PerShape, 0x2D7, 1, 5, CountGetter, "PerShape calls count getter" },
	{ PerShape, 0x4E1, 3, 7, Renderer, "Upload renderer" },
	{ PerShape, 0x4EE, 1, 5, SetComputeConstants, "PerShape calls SetComputeConstants" },
	{ TechniqueRun, 0x50, 3, 7, Renderer, "Technique Run renderer" },
	{ TechniqueRun, 0x5F, 1, 5, RunComputeShader, "Technique Run tail target" },
	{ SetComputeConstants, 0x13, 2, 6, TlsIndex, "SetComputeConstants TLS index" },
} };

inline constexpr std::array<Exact, 7> exactNGAE{ {
	{ DoCut, 0x310, "48 8B D3 48 8D 4D D0 E8 ?? ?? ?? ?? 83 F8 01 75 09", "DoCut per-shape call and loop" },
	{ PerShape, 0x60, "4D 8B 44 24 18", "PerShape closure +0x18 state" },
	{ PerShape, 0xBD, "FF 90 D8 01 00 00", "PerShape segment getter vcall" },
	{ PerShape, 0x7B4, "41 B8 50 01 00 00 48 8D 55 70 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ??", "PerShape 0x150 constant upload" },
	{ PerShape, 0x8A9, "48 8B 0D ?? ?? ?? ?? 45 8B CF 45 8B C7 44 89 7C 24 20 33 D2 48 8B 01 FF 50 10 48 8B 97 00 01 00 00", "PerShape technique Run call and return" },
	{ TechniqueRun, 0x0, "40 53 4C 8B D9 33 DB 48 8B 49 48", "Technique lookup prologue" },
	{ TechniqueRun, 0x4F, "48 8D 0D ?? ?? ?? ?? 48 8B D3 5B E9 ?? ?? ?? ??", "Technique Run tail" },
} };
inline constexpr std::array<Ref, 8> refsNGAE{ {
	{ DoCut, 0x317, 1, 5, PerShape, "DoCut calls PerShape" },
	{ PerShape, 0x2E1, 1, 5, StartGetter, "PerShape calls start getter" },
	{ PerShape, 0x2F9, 1, 5, CountGetter, "PerShape calls count getter" },
	{ PerShape, 0x7BE, 3, 7, Renderer, "Upload renderer" },
	{ PerShape, 0x7C5, 1, 5, SetComputeConstants, "PerShape calls SetComputeConstants" },
	{ TechniqueRun, 0x4F, 3, 7, Renderer, "Technique Run renderer" },
	{ TechniqueRun, 0x5A, 1, 5, RunComputeShader, "Technique Run tail target" },
	{ SetComputeConstants, 0x13, 2, 6, TlsIndex, "SetComputeConstants TLS index" },
} };
inline constexpr Ref runTlsRef{ RunComputeShader, 0x2C, 2, 6, TlsIndex, "RunComputeShader TLS index" };

inline std::uint64_t fnv1a(const std::uint8_t* p, std::size_t n) noexcept
{
	std::uint64_t h = 14695981039346656037ull;
	for (std::size_t i = 0; i < n; ++i) {
		h ^= p[i];
		h *= 1099511628211ull;
	}
	return h;
}

// Parsed pattern: value per byte, -1 for wildcard.
struct Pattern
{
	std::array<std::int16_t, 64> bytes{};
	std::size_t size{};
	bool valid{};
};
inline Pattern parse(std::string_view text) noexcept
{
	Pattern p{};
	auto hex = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
	};
	std::size_t i = 0;
	while (i < text.size()) {
		if (text[i] == ' ') {
			++i;
			continue;
		}
		if (i + 1 >= text.size() || p.size >= p.bytes.size()) return {};
		if (text[i] == '?' && text[i + 1] == '?') {
			p.bytes[p.size++] = -1;
		} else {
			const int hi = hex(text[i]), lo = hex(text[i + 1]);
			if (hi < 0 || lo < 0) return {};
			p.bytes[p.size++] = static_cast<std::int16_t>(hi * 16 + lo);
		}
		i += 2;
	}
	p.valid = p.size != 0;
	return p;
}
inline bool matches(const Pattern& p, const std::uint8_t* data) noexcept
{
	for (std::size_t i = 0; i < p.size; ++i)
		if (p.bytes[i] >= 0 && data[i] != p.bytes[i]) return false;
	return true;
}

// Read is callable as bool(std::uintptr_t address, void* out, std::size_t size).
// Returns nullptr on success, otherwise the failed contract.
template <class Read>
const char* verify(Layout layout, const Addresses& a, Read&& read)
{
	for (auto value : a)
		if (!value) return "unresolved Address Library ID";

	auto checkExact = [&](const Exact& e) -> bool {
		const auto p = parse(e.pattern);
		std::array<std::uint8_t, 64> buffer{};
		return p.valid && read(a[e.anchor] + e.offset, buffer.data(), p.size) && matches(p, buffer.data());
	};
	auto checkContains = [&](const Contains& c) -> bool {
		const auto p = parse(c.pattern);
		std::array<std::uint8_t, 256> buffer{};
		if (!p.valid || c.span > buffer.size() || p.size > c.span || !read(a[c.anchor] + c.begin, buffer.data(), c.span)) return false;
		for (std::size_t i = 0; i + p.size <= c.span; ++i)
			if (matches(p, buffer.data() + i)) return true;
		return false;
	};
	auto checkRef = [&](const Ref& r) -> bool {
		std::int32_t disp{};
		const auto at = a[r.anchor] + r.offset;
		if (!read(at + r.disp, &disp, sizeof(disp))) return false;
		if (r.disp == 1) {
			std::uint8_t op{};
			if (!read(at, &op, 1) || (op != 0xE8 && op != 0xE9)) return false;
		}
		return at + r.length + static_cast<std::intptr_t>(disp) == a[r.target];
	};

	const bool og = layout == Layout::OG;
	const Exact* exact = og ? exactOG.data() : exactNGAE.data();
	const std::size_t exactCount = og ? exactOG.size() : exactNGAE.size();
	for (std::size_t i = 0; i < exactCount; ++i)
		if (!checkExact(exact[i])) return exact[i].what;
	for (const auto& c : sharedContains)
		if (!checkContains(c)) return c.what;
	const Ref* refs = og ? refsOG.data() : refsNGAE.data();
	const std::size_t refCount = og ? refsOG.size() : refsNGAE.size();
	for (std::size_t i = 0; i < refCount; ++i)
		if (!checkRef(refs[i])) return refs[i].what;
	if (!checkRef(runTlsRef)) return runTlsRef.what;

	// The plugin reimplements these read-only; require the exact verified bodies.
	for (const auto& h : sharedHashes) {
		std::array<std::uint8_t, 0x80> buffer{};
		if (h.size > buffer.size() || !read(a[h.anchor], buffer.data(), h.size) || fnv1a(buffer.data(), h.size) != h.fnv1a) return h.what;
	}
	{
		const auto p = parse(segmentGetterCode);
		std::array<std::uint8_t, 8> buffer{};
		if (!read(a[SegmentGetter], buffer.data(), buffer.size()) || !matches(p, buffer.data())) return "Segment getter body";
	}

	// The three sites must currently hold the original rel32 instructions.
	const auto h = hooks(layout, a);
	auto target = [&](std::uintptr_t site, std::uint8_t opcode, std::uintptr_t& out) {
		std::uint8_t op{};
		std::int32_t disp{};
		if (!read(site, &op, 1) || op != opcode || !read(site + 1, &disp, 4)) return false;
		out = site + 5 + static_cast<std::intptr_t>(disp);
		return true;
	};
	std::uintptr_t t{};
	if (!target(h.shapeSite, 0xE8, t) || t != a[PerShape]) return "Shape site is not the original call";
	if (!target(h.uploadSite, 0xE8, t) || t != a[SetComputeConstants]) return "Upload site is not the original call";
	if (!target(h.tailSite, 0xE9, t) || t != a[RunComputeShader]) return "Tail site is not the original jump";
	{
		std::array<std::uint8_t, 3> call{};
		if (!read(h.callerReturn - 3, call.data(), 3) || call != std::array<std::uint8_t, 3>{ 0xFF, 0x50, 0x10 }) return "Caller return is not after the technique vcall";
	}
	return nullptr;
}
}  // namespace hpdf::sites
