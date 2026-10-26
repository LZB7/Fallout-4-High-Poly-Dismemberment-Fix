#pragma once
#include <cstdint>
namespace release {
struct Contract {const char* name;std::uint32_t rva,size;std::uint64_t hash;};
struct Call {std::uint32_t site,target;};
struct Profile {std::uint32_t runtime,imageSize,tlsIndex,renderer,callerReturn,tail,run,getter,minF4se;const char* name;const Contract* contracts;unsigned count;Call shape,upload;};
inline constexpr Contract contracts240[]={
{"DoCut closure and call",0x4091e0,0x3d0,0xac0658a68a52a1ceull},
{"Per-shape compute including range packing/upload",0x40dc10,0xa40,0xab60acda40c5d12full},
{"Postprocess readback and output use",0x4095b0,0x940,0xd7f9e39eb3f6894full},
{"Post queue call context",0x405790,0x170,0x278867d33052fef2ull},
{"ApplyPost actor lifetime, state and cleanup",0x4086c0,0x1a0,0x1ec506da84b41db9ull},
{"Constant upload void ABI",0x181eb50,0xb4,0x53da1cac543c6f02ull},
{"Structured readback void ABI and element count",0x181b440,0x92,0xacb51eaf272c5c20ull},
{"Segment range start getter",0x16c36d0,0x65,0x62aadd2410f824c7ull},
{"Segment triangle count getter",0x16c3740,0x6a,0x3eb6b66b1bcda404ull},
{"Segment child-count getter",0x16c37b0,0x41,0x3226a99591ddf879ull},
{"CSData constructor capacities and layout",0x404450,0x5c0,0xa2d013fd8ac90f25ull},
{"Actor handle ownership",0x2314a0,0xb5,0x901322e1a57102acull},
{"Persistent limb getter",0x5119a0,0x47,0x1de5eaa08a21ba0eull},
{"Applied-to-3D getter",0x511a30,0x47,0x71739f71811a99bull},
{"Dismemberment extra getter",0x28d7e0,0x80,0xf076588a5a0a3f44ull},
{"BSSubIndexTriShape segment getter",0x17e6e30,0x8,0xa00a83937a1d6a25ull},
{"BSSubIndexTriShape constructor segment pointer",0x17e6500,0xa0,0x91c31754732fa118ull},
{"BSTriShape vertex and triangle fields",0x16da5c0,0x45,0xafacd1b43c89bc0bull},
{"BSFixedString QString cache entry layout",0x167c490,0x23,0x26aa110b0bb8ae96ull},
{"BSFixedString redirected cache entry text",0x167ec00,0x18,0x1bb069995cce898cull},
{"Technique lookup and tail ABI",0x22725c0,0x5f,0xb942022629a6221eull},
{"Uncached compute bind, CB0 and dispatch",0x181d170,0xb2,0xb6f9e9dee186ea73ull},
{"Compute wrapper retained bytecode and D3D creation",0x181e060,0x121,0xcb989efcca1d9194ull},
{"Dismemberment resource allocation capacities",0x404450,0x6da,0xd6833e1510ffd809ull},
{"Raw SRV immediate bind",0x1826170,0x79,0xb2ffa3915f6abad7ull},
{"Raw UAV immediate bind",0x18260e0,0x82,0x3766564fc42dde8cull},
};
inline constexpr Profile profile240={0x10b0f00,0x425a000,0x3e73018,0x3a25d00,0x40e4d3,0x227261a,0x181d170,0x17e6e30,0x70090,"1.11.240.0",contracts240,26,{0x4094f7,0x40dc10},{0x40e3d5,0x181eb50}};

inline constexpr Contract contracts221[]={
{"DoCut closure and call",0x408ec0,0x3d0,0x6420496c9f7be53cull},
{"Per-shape compute including range packing/upload",0x40db00,0xa40,0xf2c8efb1c9127325ull},
{"Postprocess readback and output use",0x409290,0x940,0x2feb8ef585876768ull},
{"Post queue call context",0x405470,0x170,0x6415e8129abf8ce7ull},
{"ApplyPost actor lifetime, state and cleanup",0x4083a0,0x1a0,0xd66b9a62acdf0cb3ull},
{"Constant upload void ABI",0x181e690,0xb4,0xfd79ec39da63fb60ull},
{"Structured readback void ABI and element count",0x181af70,0x92,0x8d9d387868b3c223ull},
{"Segment range start getter",0x16c32d0,0x65,0x62aadd2410f824c7ull},
{"Segment triangle count getter",0x16c3340,0x6a,0x3eb6b66b1bcda404ull},
{"Segment child-count getter",0x16c33b0,0x41,0x3226a99591ddf879ull},
{"CSData constructor capacities and layout",0x404130,0x5c0,0x47192163faafa390ull},
{"Actor handle ownership",0x231180,0xb5,0xa97c65e1d0ca2476ull},
{"Persistent limb getter",0x511680,0x47,0x1de5eaa08a21ba0eull},
{"Applied-to-3D getter",0x511710,0x47,0x71739f71811a99bull},
{"Dismemberment extra getter",0x28d4c0,0x80,0x23ccad6d1da71110ull},
{"BSSubIndexTriShape segment getter",0x17e6970,0x8,0xa00a83937a1d6a25ull},
{"BSSubIndexTriShape constructor segment pointer",0x17e6040,0xa0,0x566fde054406d326ull},
{"BSTriShape vertex and triangle fields",0x16da1c0,0x45,0x8ced19d7c0c6cbd0ull},
{"BSFixedString QString cache entry layout",0x167c170,0x23,0x26aa110b0bb8ae96ull},
{"BSFixedString redirected cache entry text",0x167e8e0,0x18,0x1bb069995cce898cull},
{"Technique lookup and tail ABI",0x226ab90,0x5f,0x8ece7acb49090983ull},
{"Uncached compute bind, CB0 and dispatch",0x181ccb0,0xb2,0x96e4ef0b3d4ae24ull},
{"Compute wrapper retained bytecode and D3D creation",0x181dba0,0x121,0x4acecc98099bc801ull},
{"Dismemberment resource allocation capacities",0x404130,0x6da,0xa608757e942302a0ull},
{"Raw SRV immediate bind",0x1825cb0,0x79,0x8cbc141a9734a1b8ull},
{"Raw UAV immediate bind",0x1825c20,0x82,0x70e700d938f3f3d1ull},
};
inline constexpr Profile profile221={0x10b0dd0,0x4244000,0x3e5c6d8,0x3a0f480,0x40e3c3,0x226abea,0x181ccb0,0x17e6970,0x70080,"1.11.221.0",contracts221,26,{0x4091d7,0x40db00},{0x40e2c5,0x181e690}};

inline const Profile* select(std::uint32_t r) {if(r==profile240.runtime)return &profile240;
#ifdef HPDF_EXPERIMENTAL
if(r==profile221.runtime)return &profile221;
#endif
return nullptr;}
}
