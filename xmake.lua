add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- 基于 levimc-repo 官方 levilamina 包定义派生补丁版本（不指定版本号即为最新版，强制 client 端）
-- 仅覆写 on_install，为 VS2026 工具链补齐上游缺失的头文件，其余定义（urls/versions/依赖）全部继承官方包
package("levilamina-patched")
    set_base("levilamina")
    on_install(function (package)
        cprint("${bright green}>>> [levilamina-patched] Applying VS2026 compatibility patches...")

        -- 1. Patch Alias.h for incomplete types in TypedStorage<..., std::unique_ptr<T>>
        local alias_files = os.files("**/Alias.h")
        for _, file in ipairs(alias_files) do
            local content = io.readfile(file)
            if not content:find("IncompletePtrStorage", 1, true) then
                local norm = content:gsub("\r\n", "\n")
                local target = "template <size_t A, size_t S, class T>\nstruct TypedStorageType<A, S, std::unique_ptr<T>> {\n    using Type = std::unique_ptr<T>;\n};"
                local pos = norm:find(target, 1, true)
                if pos then
                    local replacement = [==[template <size_t A, size_t S, class T>
struct TypedStorageType<A, S, std::unique_ptr<T>> {
    using Type = std::unique_ptr<T>;
};

template <size_t Align, size_t Size, class Ptr>
struct IncompletePtrStorage {
    alignas(Align) std::byte data[Size];

    using element_type = typename Ptr::element_type;

    [[nodiscard]] Ptr&       ptr() { return *reinterpret_cast<Ptr*>(data); }
    [[nodiscard]] Ptr const& ptr() const { return *reinterpret_cast<Ptr const*>(data); }

    [[nodiscard]] element_type* get() const { return ptr().get(); }

    [[nodiscard]] element_type* operator->() const { return get(); }
    [[nodiscard]] element_type& operator*() const { return *get(); }

    [[nodiscard]] operator Ptr&() { return ptr(); }
    [[nodiscard]] operator Ptr const&() const { return ptr(); }

    [[nodiscard]] explicit operator bool() const { return get() != nullptr; }

    template <class U>
        requires(!std::is_same_v<std::remove_cvref_t<U>, IncompletePtrStorage>)
    IncompletePtrStorage& operator=(U&& u) {
        ptr() = std::forward<U>(u);
        return *this;
    }
};

template <size_t A, size_t S, class T>
    requires(!requires { sizeof(T); })
struct TypedStorageType<A, S, std::unique_ptr<T>> {
    using Type = IncompletePtrStorage<A, S, std::unique_ptr<T>>;
};]==]
                    local patched = norm:sub(1, pos - 1) .. replacement .. norm:sub(pos + #target)
                    io.writefile(file, patched)
                    cprint("${bright green}>>> Patched " .. file)
                else
                    cprint("${bright yellow}>>> Warning: target snippet not found in " .. file)
                end
            end
        end

        -- 2. Patch MinecraftCommands.h for missing CommandRegistry.h (unique_ptr<CommandRegistry> with incomplete type)
        local mc_files = os.files("**/MinecraftCommands.h")
        for _, file in ipairs(mc_files) do
            local content = io.readfile(file)
            if not content:find("CommandRegistry.h", 1, true) then
                local norm = content:gsub("\r\n", "\n")
                local target = "class CommandRegistry;"
                local pos = norm:find(target, 1, true)
                if pos then
                    local replacement = "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/CommandOutputSender.h\"\n#include \"mc/server/commands/Command.h\"\n#include \"mc/server/commands/DeferredCommandBase.h\"\nclass CommandRegistry;"
                    local patched = norm:sub(1, pos - 1) .. replacement .. norm:sub(pos + #target)
                    io.writefile(file, patched)
                    cprint("${bright green}>>> Patched " .. file)
                else
                    cprint("${bright yellow}>>> Warning: forward declaration not found in " .. file)
                end
            end
        end

        -- 3. Patch ExecuteCommandEvent.h for missing CommandRegistry.h
        local exec_events = os.files("**/ExecuteCommandEvent.h")
        for _, file in ipairs(exec_events) do
            local content = io.readfile(file)
            if not content:find("CommandRegistry.h", 1, true) then
                local norm = content:gsub("\r\n", "\n")
                local target = "#include \"mc/server/commands/MinecraftCommands.h\""
                local pos = norm:find(target, 1, true)
                if pos then
                    local replacement = "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/MinecraftCommands.h\""
                    local patched = norm:sub(1, pos - 1) .. replacement .. norm:sub(pos + #target)
                    io.writefile(file, patched)
                    cprint("${bright green}>>> Patched " .. file)
                end
            end
        end

        cprint("${bright green}>>> [levilamina-patched] Patches applied, building LeviLamina...")
        if package:config("target_type") == "server" then
            import("package.tools.xmake").install(package)
        else
            import("package.tools.xmake").install(package, {"--target_type=client"})
        end
    end)
package_end()

add_requires("levilamina-patched", {alias = "levilamina", configs = {target_type = "client"}})

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
