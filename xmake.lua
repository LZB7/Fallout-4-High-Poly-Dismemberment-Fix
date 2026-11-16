set_xmakever("3.0.0")

includes("lib/commonlibf4")

-- Primary 1.1.0; experimental 1.1.1 so the two DLLs are distinguishable.
local version = has_config("experimental") and "1.1.1" or "1.1.0"

set_project("HighPolyDismembermentFix")
set_version(version)
set_license("MIT")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")
set_defaultmode("release")

add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- Experimental (xmake f --experimental=y): also accept checksum-valid DXBC containers whose complete
-- executable/signature chunks reconstruct to the validated DismemberCS.
option("experimental", function()
    set_default(false)
    set_showmenu(true)
    set_description("Build the experimental shader-container variant")
    add_defines("HPDF_EXPERIMENTAL=1")
end)

target("HighPolyDismembermentFix", function()
    add_rules("commonlibf4.plugin", {
        name = "HighPolyDismembermentFix",
        author = "High-Poly Dismemberment Fix contributors",
        description = "Corrects truncated mesh-range starts in Fallout 4's dismemberment compute shader"
    })

    add_options("experimental")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "UNICODE", "_UNICODE")
    add_defines(format('HPDF_VERSION_LABEL="%s"', version))

    -- Plugin.cpp uses CommonLibF4; Runtime.cpp uses the Windows SDK D3D11
    -- headers and never includes CommonLibF4. No shared precompiled header.
    add_files("src/Plugin.cpp", "src/Runtime.cpp", "vendor/dxbc-checksum/DXBCChecksum.cpp")
    add_headerfiles("src/**.h", "vendor/dxbc-checksum/*.h")
    add_includedirs("src")
    add_syslinks("d3d11", "d3dcompiler", "bcrypt", "dxguid")

    add_installfiles("config/HighPolyDismembermentFix.ini", { prefixdir = "F4SE/Plugins" })
end)

-- Offline site-table verification against reference EXEs (see README).
target("SiteTests", function()
    set_kind("binary")
    set_default(false)
    add_files("tests/SiteTests.cpp")
end)

-- Host/GPU regression tests from the original suite (CorrectionTests needs the
-- private replay fixtures under validation/shader; see README).
for _, name in ipairs({ "CoreTests", "CorrectionTests", "ShaderIdentityTests" }) do
    target(name, function()
        set_kind("binary")
        set_default(false)
        add_options("experimental")
        add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
        add_files("tests/" .. name .. ".cpp")
        if name ~= "CoreTests" then
            add_files("vendor/dxbc-checksum/DXBCChecksum.cpp")
        end
        add_includedirs("src")
        add_syslinks("d3d11", "d3dcompiler", "bcrypt", "dxguid")
    end)
end
