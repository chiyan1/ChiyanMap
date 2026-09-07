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
        local cmd_h = "src/mc/server/commands/MinecraftCommands.h"
        if os.isfile(cmd_h) then
            local content = io.readfile(cmd_h)
            if not content:find("CommandRegistry.h", 1, true) then
                io.replace(
                    cmd_h,
                    "class CommandRegistry;",
                    "#include \"mc/server/commands/CommandRegistry.h\"\n#include \"mc/server/commands/CommandOutputSender.h\"\n#include \"mc/server/commands/Command.h\"\n#include \"mc/server/commands/DeferredCommandBase.h\"\nclass CommandRegistry;",
                    {plain = true}
                )
            end
        end
        if package:config("target_type") == "server" then
            import("package.tools.xmake").install(package)
        else
            import("package.tools.xmake").install(package, {"--target_type=client"})
        end
    end)
