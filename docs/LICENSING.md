# Licensing and attribution

The project's original implementation and documentation are open-source software licensed under the [MIT License](../LICENSE), copyright (c) 2026 LZB7, with the owner's approval. This grant does not relicense incorporated upstream components or Bethesda assets. Their separate terms and mandatory attributions remain in force.

The plugin uses selected official F4SE SDK source at revision `6f6a7caa9aeaebc957a70d934b6ab6b80eb768b2`. The upstream README prohibits redistributing/modified versions of F4SE and recommends plugins. Its short License.txt supplies a warranty disclaimer rather than a broad source redistribution grant. To avoid assuming SDK redistribution rights for the GitHub source bundle, SDK files are not included there: `fetch_sdk.py` retrieves pinned files directly from upstream for a local build. No F4SE loader or runtime DLL is packaged. Any proposal to vendor/publish SDK source needs upstream permission or clarified terms; do not assume an MIT license covers it.

The incorporated GPUOpen/AMD DXBC checksum implementation is derived from the RSA Data Security, Inc. MD5 Message Digest Algorithm. Its attribution, derivative-work identification and full copyright/license notices remain in source and release notices. The code is not relabeled as solely project-owned. The LLVM-MinGW/libc++/libc++abi/libunwind and MinGW runtime notices accompany compiled releases.

Community Shaders is GPL-3.0-or-later. GFXBooster and its ShaderDB were examined as references; no code or shader assets were copied from any of these three projects. Where no clear license is supplied, no redistribution permission is assumed. No new GPL dependency is introduced merely by studying techniques.

No Bethesda shader, executable, game archive, raw game/third-party asset or test body is in the source bundle or Nexus packages. Runtime transformation operates on the user's own recognized shader. Check THIRD_PARTY_NOTICES.txt and vendor/dxbc-checksum/provenance.json for exact incorporated source identities.
