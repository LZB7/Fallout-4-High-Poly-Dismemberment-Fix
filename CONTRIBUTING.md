# Contributing

Keep the primary 240 configuration isolated from experimental adaptations. Runtime metadata is an allowlist, not proof of compatibility. New profiles need clean-executable provenance, verified calling conventions/private offsets, exact signatures, correct relative branch targets, helper identities, TLS and renderer references, loader minima and shader evidence. Validate the complete profile before installing any hooks. Do not relax a failing safety predicate to force activation.

Run CPU, private mapped-PE hook, resource/device and GPU replay tests appropriate to changes. Preserve stock instructions and full output equivalence for representable operations. Generated counts alone are insufficient evidence; compare complete output buffers. Report static, host, GPU replay and gameplay evidence separately. Do not commit game executables, native/modified Bethesda shader bytecode, meshes, fixture buffers, large captures or personal paths.

Original/OG 1.10.163 support is outside this release. Add a separate profile/adapter and independently establish the OG shader transformation if investigating it; never infer shader identity from similar engine interfaces or break the AE configurations. Keep unrelated asset repair out of the runtime DLL. Consult docs/LICENSING.md before copying external code.
