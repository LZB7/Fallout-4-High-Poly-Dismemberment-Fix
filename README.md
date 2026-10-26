# Fallout 4 High-Poly Dismemberment Fix

Open-source software licensed under the [MIT License](LICENSE). Copyright (c) 2026 LZB7. Incorporated third-party components retain their own notices and terms; see [licensing and attribution](docs/LICENSING.md).

A small Windows x64 F4SE plugin that corrects truncated mesh-range starts in Fallout 4's dismemberment compute pipeline. Meshes still need valid segments, cut offsets and SSF definitions. The plugin does not edit assets, split meshes, reorder triangles or redistribute Bethesda shaders.

| Configuration | Plugin version | Runtime / F4SE | Status |
|---|---|---|---|
| `primary` | 1.0.0 | 1.11.240 / 0.7.9 | Based on extensively user-tested correction; production cleanup regression-tested |
| `experimental` | 1.0.1-experimental.1 | 1.11.221 / 0.7.8 and 1.11.240 / 0.7.9 | 221 statically validated, not gameplay-verified |

Install only one variant: both produce `HighPolyDismembermentFix.dll`. See [primary installation instructions](docs/Nexus-Primary.txt) and [experimental instructions](docs/Nexus-Experimental.txt). Do not install the former development listener beside the release DLL.

## Build

Requires Windows x64, Python 3, PowerShell and LLVM-MinGW (validated with release 20261020, clang 23.1.4, UCRT x86_64). System D3D11, D3DCompiler 47, BCrypt and UCRT are used; no CommonLib or Address Library runtime dependency is added. Compiler download information and pinned dependency hashes are in `dependencies.json`.

Fetch only the required official SDK source files, then build:

```powershell
python scripts/fetch_sdk.py
./Build.ps1 -Compiler '<llvm-mingw>/bin/clang++.exe' -Variant primary
./Build.ps1 -Compiler '<llvm-mingw>/bin/clang++.exe' -Variant experimental
```

Build outputs are under `build/primary` and `build/experimental`; these are not source-control contents. `Build.ps1` verifies architecture, exports, import dependencies, runtime metadata and rejected loader paths. It does not install or launch the game.

For the full automated suite, add `-RunTests -Reference240 '<240 Fallout4.exe>' -Reference221 '<221 Fallout4.exe>' -FixtureDirectory '<private fixtures>'`. Private replay fixtures are `native.dxbc`, `indices.bin`, `vertices.bin`, `arm-constants.bin`, `thigh-constants.bin` and `right-thigh-constants.bin`. They are deliberately excluded from this repository: they contain game/third-party data. The original research workspace preserves them. No historical 221 shader archive is required. GPU replay exercises the known shader on D3D11 WARP, not gameplay on 221.

`scripts/verify_profiles.py` checks the exact signatures and interception targets against user-supplied clean reference executables. The synthetic production harness maps private copies of both EXEs, verifies both profiles, rejects changed sites, installs all three hooks into the copies and checks synchronous scope/payload forwarding. It never executes game code. The correction suite covers native/zero-sideband arm equivalence across all eight outputs, high-start left/right thighs, shared/nonzero-offset/minimum-sized input views, arithmetic/format/alias/device negatives and cache lifetime. Experimental identity tests accept container-only changes and reject changed executable/signature chunks, corrupt checksums, invalid bounds and overlapping chunks.

## Implementation

The engine packs range starts into the high 16 bits of constant records. The fix preserves the original records and supplies two additional start bits per record in unused constant space. Five integer instructions restore each 18-bit start before the stock shader computes its range end. It does not widen triangle counts or generated vertex IDs. Unsupported ranges retain native processing and are not reported as fixed.

Only three synchronous interception points remain: per-shape scope, constant upload and the compute Run tail. Native asynchronous geometry ownership is unchanged; no observer job map, post/readback hook, actor reference tracking, GPU wait, geometry copy or global D3D interception is introduced. The stack-only resource descriptor snapshot remains part of safety validation, with no descriptor serialization. Minimal messages go through a small asynchronous queue, with repeated default fallback messages suppressed.

`src/RuntimeProfiles.h` selects immutable addresses, exact code hashes, image size, helper identity, TLS/renderer globals, caller guard and F4SE minimum before patching. Both profiles retain conservative startup contracts, including downstream layout/output assumptions. There is no blanket address delta or unsupported-runtime override. All three hooks install transactionally with original-call/branch verification; the tail remains E9. The module and shared-pool trampoline wrapper remain pinned for process lifetime.

The primary configuration recognizes the exact validated shader hash. Experimental builds additionally accept checksum-valid containers whose complete executable and input/output signature chunks reconstruct to that exact program; only known non-executable metadata chunks may differ. This is stronger than matching a shader family, reflection interface or a short opcode pattern. Different executable instructions are not accepted without a separately investigated transformation. Pool/view bounds, raw alignment, output alias rejection, immediate-context/device checks and per-dispatch COM ownership are unchanged.

## Project status and licensing

The original diagnostic/corrective development project is preserved outside this maintained source tree. No new gameplay verification on 221 is claimed.

See [licensing and attribution](docs/LICENSING.md), [contribution guidance](CONTRIBUTING.md), [issue reporting](SUPPORT.md) and [future work](docs/FUTURE.md). The project's original code is MIT-licensed. The SDK is fetched into ignored storage rather than redistributed in the repository. Retain all incorporated third-party notices.
