package("levilamina")
    add_urls("https://github.com/LiteLDev/LeviLamina.git")
    add_versionfiles("versions/versions.txt")

    add_defines("ENTT_PACKED_PAGE=128", "ENTT_SPARSE_PAGE=2048", "ENTT_NO_MIXIN")

    add_configs("target_type", {default = "server", values = {"server", "client"}})

    on_load(function(package)
        import("core.base.semver")
        local version = package:version_str()
        local sem = semver.try_parse(version)
        if sem and sem:le("0.12.4") then
            version = "old"
        end
        version = string.gsub(version, "%.", "_")
        try { function()
            import("versions." .. version).load(package)
        end, catch { function(e)
            cprint(
                "${bright yellow}warning: ${clear}Unknown version: ${bright cyan}"
                .. version .. "${clear}, will use main branch dependencies."
            )
            import("versions.main").load(package)
        end } }
        if package:config("target_type") == "server" then
            package:add("defines", "LL_PLAT_S")
        else
            package:add("defines", "LL_PLAT_C")
        end
    end)

    on_install(function(package)
        cprint("${bright green}>>> [local-repo] Starting patched on_install for LeviLamina...")
        cprint("${bright cyan}>>> Current directory: " .. os.curdir())

        -- 1. Patch Alias.h for incomplete types in TypedStorage<..., std::unique_ptr<T>>
        local alias_files = os.files("**/Alias.h")
        for _, file in ipairs(alias_files) do
            local content = io.readfile(file)
            if not content:find("IncompletePtrStorage", 1, true) then
                cprint("${bright green}>>> Patching " .. file .. " for IncompletePtrStorage...")
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
                    cprint("${bright green}>>> Successfully patched Alias.h!")
                else
                    cprint("${bright red}>>> Warning: could not find target snippet in Alias.h!")
                end
            else
                cprint("${bright blue}>>> Already contains IncompletePtrStorage: " .. file)
            end
        end

        -- 2. Patch MinecraftCommands.h for missing headers
        local mc_files = os.files("**/MinecraftCommands.h")
        for _, file in ipairs(mc_files) do
            local content = io.readfile(file)
            if not content:find("CommandRegistry.h", 1, true) then
                cprint("${bright green}>>> Patching " .. file .. " for missing CommandRegistry header...")
                local norm = content:gsub("\r\n", "\n")
                local target = "class CommandRegistry;"
                local pos = norm:find(target, 1, true)
                if pos then
                    local replacement = "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/CommandOutputSender.h\"\n#include \"mc/server/commands/Command.h\"\n#include \"mc/server/commands/DeferredCommandBase.h\"\nclass CommandRegistry;"
                    local patched = norm:sub(1, pos - 1) .. replacement .. norm:sub(pos + #target)
                    io.writefile(file, patched)
                    cprint("${bright green}>>> Successfully patched MinecraftCommands.h!")
                end
            else
                cprint("${bright blue}>>> Already contains CommandRegistry.h: " .. file)
            end
        end

        -- 3. Patch ExecuteCommandEvent.h for missing CommandRegistry.h
        local exec_events = os.files("**/ExecuteCommandEvent.h")
        for _, file in ipairs(exec_events) do
            local content = io.readfile(file)
            if not content:find("CommandRegistry.h", 1, true) then
                cprint("${bright green}>>> Patching " .. file .. " for CommandRegistry.h...")
                local norm = content:gsub("\r\n", "\n")
                local target = "#include \"mc/server/commands/MinecraftCommands.h\""
                local pos = norm:find(target, 1, true)
                if pos then
                    local replacement = "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/MinecraftCommands.h\""
                    local patched = norm:sub(1, pos - 1) .. replacement .. norm:sub(pos + #target)
                    io.writefile(file, patched)
                    cprint("${bright green}>>> Successfully patched ExecuteCommandEvent.h!")
                end
            end
        end

        if package:config("target_type") == "server" then
            import("package.tools.xmake").install(package)
        else
            import("package.tools.xmake").install(package, {"--target_type=client"})
        end
    end)


