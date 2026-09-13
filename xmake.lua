add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 移除 target_type 选项配置，直接强制 LeviLamina 为 client 端
add_requires("levilamina", {configs = {target_type = "client"}})

add_requires("levibuildscript")
add_requires("imgui", {configs = {shared = false, win32 = true, dx11 = true}})
add_requires("minhook", {configs = {shared = false}})
add_requires("nlohmann_json")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("ChiyanMap")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204",
        "/wd4100",   -- 允许未使用的函数参数
        "/wd4189"    -- 允许已初始化但未使用的局部变量
    )
    add_defines("NOMINMAX", "UNICODE")
    add_packages("levilamina", "imgui", "minhook", "nlohmann_json")
    add_syslinks("d3d11", "dxgi", "user32", "delayimp")
    add_ldflags("/DELAYLOAD:dwmapi.dll", "/DELAYLOAD:imm32.dll", "/DELAYLOAD:LeviLamina.dll")
    add_shflags("/DELAYLOAD:dwmapi.dll", "/DELAYLOAD:imm32.dll", "/DELAYLOAD:LeviLamina.dll")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    -- 完全移除服务端和客户端的 if-else 区分逻辑
