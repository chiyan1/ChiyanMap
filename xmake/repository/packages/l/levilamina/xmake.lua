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
        local files = os.files("**/MinecraftCommands.h")
        cprint("${bright cyan}>>> Found " .. tostring(#files) .. " MinecraftCommands.h file(s)")
        for _, file in ipairs(files) do
            cprint("${bright yellow}>>> Inspecting: " .. file)
            local content = io.readfile(file)
            if not content:find("CommandRegistry.h", 1, true) then
                cprint("${bright green}>>> Patching " .. file .. " for missing CommandRegistry header...")
                io.replace(
                    file,
                    "class CommandRegistry;",
                    "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/CommandOutputSender.h\"\n#include \"mc/server/commands/Command.h\"\n#include \"mc/server/commands/DeferredCommandBase.h\"\nclass CommandRegistry;",
                    {plain = true}
                )
            else
                cprint("${bright blue}>>> Already contains CommandRegistry.h: " .. file)
            end
        end

        local spawner_files = os.files("**/BedrockSpawner.h")
        for _, file in ipairs(spawner_files) do
            local content = io.readfile(file)
            if not content:find("SpawnGroupRegistry.h", 1, true) then
                cprint("${bright green}>>> Patching " .. file .. " for missing SpawnGroupRegistry / EntityTypeCache headers...")
                io.replace(
                    file,
                    "#include \"mc/world/level/Spawner.h\"",
                    "#include \"mc/world/level/Spawner.h\"\n#include \"mc/world/actor/ActorSpawnRuleGroup.h\"\n#include \"mc/world/actor/registry/SpawnGroupRegistry.h\"\n#include \"mc/world/level/chunk/EntityTypeCache.h\"",
                    {plain = true}
                )
            end
        end

        -- General pass: in all src/mc headers, remove "= default;" on virtual destructors that hold unique_ptr
        -- to prevent Clang-CL from inline-instantiating destructors that delete incomplete types
        local mc_headers = os.files("**/src/mc/**.h")
        if #mc_headers == 0 then
            mc_headers = os.files("src/mc/**.h")
        end
        local patched_count = 0
        for _, file in ipairs(mc_headers) do
            local content = io.readfile(file)
            if content:find("unique_ptr", 1, true) and content:find("= default;", 1, true) then
                local new_content, count = content:gsub("(virtual%s+~[%w_]+%s*%b()[^;=]*)=%s*default%s*;", "%1;")
                if count > 0 then
                    io.writefile(file, new_content)
                    patched_count = patched_count + count
                end
            end
        end
        cprint("${bright green}>>> Patched " .. tostring(patched_count) .. " virtual destructor(s) across mc headers")

        if package:config("target_type") == "server" then
            import("package.tools.xmake").install(package)
        else
            import("package.tools.xmake").install(package, {"--target_type=client"})
        end
    end)

