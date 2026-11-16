// Verifies the shared interception table against privately supplied clean EXEs
// and their Address Library bins. Portable C++: never executes game code.
// Usage: SiteTests <og|ngae> <Fallout4.exe> <version-x-y-z-0.bin> [...]
#include "../src/Sites.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace hpdf::sites;
unsigned checks{};
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

template <class T> T get(const std::vector<std::uint8_t>& d, std::size_t o) { T v{}; CHECK(o + sizeof(T) <= d.size()); std::memcpy(&v, d.data() + o, sizeof(T)); return v; }

std::vector<std::uint8_t> load(const char* path)
{
	std::ifstream f(path, std::ios::binary);
	CHECK(f.good());
	return { std::istreambuf_iterator<char>(f), {} };
}

// Section-mapped image at preferred base 0x140000000 (no relocation needed for reads).
struct Image
{
	std::vector<std::uint8_t> bytes;
	static constexpr std::uintptr_t base = 0x140000000;
	bool read(std::uintptr_t address, void* out, std::size_t size) const
	{
		if (address < base || address - base > bytes.size() || bytes.size() - (address - base) < size) return false;
		std::memcpy(out, bytes.data() + (address - base), size);
		return true;
	}
};

Image map(const std::vector<std::uint8_t>& raw)
{
	const auto pe = get<std::uint32_t>(raw, 0x3C);
	CHECK(get<std::uint32_t>(raw, pe) == 0x4550 && get<std::uint16_t>(raw, pe + 4) == 0x8664);
	const auto sections = get<std::uint16_t>(raw, pe + 6), optional = get<std::uint16_t>(raw, pe + 20);
	CHECK(get<std::uint64_t>(raw, pe + 24 + 24) == Image::base);
	Image image{ std::vector<std::uint8_t>(get<std::uint32_t>(raw, pe + 24 + 56)) };
	const auto headers = get<std::uint32_t>(raw, pe + 24 + 60);
	std::memcpy(image.bytes.data(), raw.data(), headers);
	for (unsigned i = 0; i < sections; ++i) {
		const auto s = pe + 24 + optional + 40 * i;
		const auto vsize = get<std::uint32_t>(raw, s + 8), va = get<std::uint32_t>(raw, s + 12);
		const auto rsize = get<std::uint32_t>(raw, s + 16), ro = get<std::uint32_t>(raw, s + 20);
		const auto n = std::min(rsize, vsize ? vsize : rsize);
		CHECK(std::size_t(va) + n <= image.bytes.size() && std::size_t(ro) + n <= raw.size());
		std::memcpy(image.bytes.data() + va, raw.data() + ro, n);
	}
	return image;
}

std::uintptr_t resolve(const std::vector<std::uint8_t>& bin, std::uint64_t id)
{
	const auto count = get<std::uint64_t>(bin, 0);
	CHECK(8 + count * 16 <= bin.size());
	std::size_t lo = 0, hi = count;
	while (lo < hi) {
		const auto mid = (lo + hi) / 2;
		const auto key = get<std::uint64_t>(bin, 8 + mid * 16);
		if (key == id) return Image::base + get<std::uint64_t>(bin, 16 + mid * 16);
		key < id ? lo = mid + 1 : hi = mid;
	}
	return 0;
}

int main(int argc, char** argv)
{
	CHECK(argc >= 4 && (argc - 1) % 3 == 0);
	// Pattern parser sanity.
	CHECK(parse("48 8B ?? C3").valid && parse("48 8B ?? C3").size == 4 && parse("48 8B ?? C3").bytes[2] == -1);
	CHECK(!parse("4").valid && !parse("ZZ").valid && !parse("").valid);
	for (int arg = 1; arg < argc; arg += 3) {
		const std::string kind = argv[arg];
		CHECK(kind == "og" || kind == "ngae");
		const auto layout = kind == "og" ? Layout::OG : Layout::NGAE;
		const auto other = kind == "og" ? Layout::NGAE : Layout::OG;
		auto image = map(load(argv[arg + 1]));
		const auto bin = load(argv[arg + 2]);
		Addresses a{};
		for (unsigned f = 0; f < FieldCount; ++f) {
			a[f] = resolve(bin, ids[static_cast<unsigned>(layout)][f]);
			CHECK(a[f] != 0);
		}
		auto reader = [&](std::uintptr_t p, void* out, std::size_t n) { return image.read(p, out, n); };
		const char* failure = verify(layout, a, reader);
		if (failure) std::fprintf(stderr, "%s: %s\n", argv[arg + 1], failure);
		CHECK(failure == nullptr);

		// The other layout's offsets must not validate against this build.
		Addresses wrong{};
		for (unsigned f = 0; f < FieldCount; ++f) wrong[f] = resolve(bin, ids[static_cast<unsigned>(other)][f]);
		bool anyResolved = false;
		for (auto v : wrong) anyResolved |= v != 0;
		if (anyResolved) CHECK(verify(other, wrong, reader) != nullptr);

		// Every hook site, layout constant and RIP global must be load-bearing.
		const auto h = hooks(layout, a);
		auto mutate = [&](std::uintptr_t address, const char* label) {
			auto& byte = image.bytes[address - Image::base];
			byte ^= 0x01;
			const char* r = verify(layout, a, reader);
			byte ^= 0x01;
			if (!r) std::fprintf(stderr, "mutation not detected: %s\n", label);
			CHECK(r != nullptr);
		};
		mutate(h.shapeSite, "shape opcode");
		mutate(h.shapeSite + 1, "shape rel32");
		mutate(h.uploadSite, "upload opcode");
		mutate(h.uploadSite + 2, "upload rel32");
		mutate(h.tailSite, "tail opcode");
		mutate(h.tailSite + 4, "tail rel32");
		mutate(h.callerReturn - 1, "technique vcall slot");
		mutate(h.uploadSite - 4, "payload size");
		mutate(a[SegmentGetter] + 3, "segment getter offset");
		mutate(a[StartGetter] + 0x20, "start getter body");
		mutate(a[CountGetter] + 0x20, "count getter body");
		mutate(a[TechniqueRun] + 1, "technique prologue");
		// Patched instructions (as after a hook install) are rejected.
		auto& op = image.bytes[h.tailSite - Image::base];
		op = 0xE8;
		CHECK(verify(layout, a, reader) != nullptr);
		op = 0xE9;
		CHECK(verify(layout, a, reader) == nullptr);
		// A missing ID fails closed.
		auto missing = a;
		missing[TlsIndex] = 0;
		CHECK(verify(layout, missing, reader) != nullptr);
		// A shifted global (e.g. a different build reusing an ID) fails closed.
		auto shifted = a;
		shifted[Renderer] += 8;
		CHECK(verify(layout, shifted, reader) != nullptr);
		shifted = a;
		shifted[TlsIndex] += 4;
		CHECK(verify(layout, shifted, reader) != nullptr);
		std::printf("PASS %s %s: shape %llx upload %llx tail %llx return %llx\n", kind.c_str(), argv[arg + 1],
			(unsigned long long)h.shapeSite, (unsigned long long)h.uploadSite, (unsigned long long)h.tailSite, (unsigned long long)h.callerReturn);
	}
	std::printf("PASS %u checks\n", checks);
}
