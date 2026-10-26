#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace diag {
constexpr std::uint32_t runtime240 = 0x010B0F00;
constexpr bool supported(std::uint32_t runtime) { return runtime == runtime240; }
struct Range { std::uint32_t start{}, triangles{}; bool valid{}; };
struct Decoded {
    std::uint32_t retainedStart{}, retainedTriangles{}, lostStartBits{}, lostCountBits{};
    std::uint64_t fullEnd{}, consumedEnd{};
    bool startAligned{}, consumedAligned{}, inBounds{}, consumedInBounds{}, matches{}, lossy{};
};
inline Decoded decode(Range r, std::uint32_t word, std::uint32_t entries) {
    Decoded d{}; d.retainedStart=word>>16; d.retainedTriangles=word&0xffff;
    d.lostStartBits=r.start&0xffff0000u; d.lostCountBits=r.triangles&0xffff0000u;
    d.fullEnd=std::uint64_t(r.start)+3ull*r.triangles;
    d.consumedEnd=std::uint64_t(d.retainedStart)+3ull*d.retainedTriangles;
    d.startAligned=r.start%3==0; d.consumedAligned=d.retainedStart%3==0;
    d.inBounds=d.fullEnd<=entries; d.consumedInBounds=d.consumedEnd<=entries;
    d.matches=r.valid && word==((r.start<<16)|(r.triangles&0xffff));
    d.lossy=d.matches && (r.start!=d.retainedStart || r.triangles!=d.retainedTriangles);
    return d;
}
// JSON strings must escape every control character, including embedded NUL.
inline std::string quote(const std::string& s) {
    constexpr char hex[]="0123456789abcdef"; std::string out="\"";
    for (unsigned char c:s) {
        if(c=='"'||c=='\\') {out+='\\';out+=char(c);}
        else if(c<32 || c>=127) {out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}
        else out+=char(c);
    }
    return out+'"';
}
struct Context {
    std::uint64_t id{}, epoch{}, state{}, shape{}, root{}, bone{}, created{};
    std::uint32_t actor{}, form{}, limb{}, segment{}, triangles{}, entries{}, vertices{}, childCount{};
    float cut{};
    std::array<std::uint32_t,4> cutOriginBits{},cutNormalBits{};
    bool cutPlaneReadable{};
    std::uint8_t flags[8]{};
    bool valid{}, rangesValid{};
    bool correctionApplied{};std::uint32_t correctionSideband{};
    bool shapeNameReadable{}, boneNameReadable{};
    char shapeName[96]{}, boneName[96]{};
    std::array<Range,12> ranges{};
};
struct Job { bool occupied{}; std::uint64_t cs{}, updated{}; Context context{}; };
// Caller supplies synchronization. Fixed memory, exact CS/state/epoch match, TTL.
template<std::size_t N> class Correlator {
public:
    std::array<Job,N> slots{};
    std::uint64_t evictions{}, replacements{}, expired{};
    void clear() { for(auto& j:slots) j.occupied=false; }
    void expire(std::uint64_t now) {
        for(auto& j:slots) if(j.occupied && now-j.updated>120000) {j.occupied=false;++expired;}
    }
    void put(std::uint64_t cs, const Context& c, std::uint64_t now) {
        expire(now); Job* empty=nullptr; Job* oldest=&slots[0];
        for(auto& j:slots) {
            if(j.occupied && j.cs==cs) {empty=&j;++replacements;break;}
            if(!j.occupied && !empty) empty=&j;
            if(j.updated<oldest->updated) oldest=&j;
        }
        if(!empty) {empty=oldest;++evictions;}
        *empty={true,cs,now,c};
    }
    bool get(std::uint64_t state, std::uint64_t cs, std::uint64_t epoch,
             std::uint64_t now, Context& out) {
        expire(now);
        for(const auto& j:slots) if(j.occupied && j.cs==cs && j.context.state==state && j.context.epoch==epoch) {
            out=j.context;return true;
        }
        return false;
    }
    void retire(std::uint64_t state, std::uint64_t epoch) {
        for(auto& j:slots) if(j.occupied && j.context.state==state && j.context.epoch==epoch) j.occupied=false;
    }
};
}
