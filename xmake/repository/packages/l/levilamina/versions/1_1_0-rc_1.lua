local deps_common = {
    "entt v3.14.0",
    "expected-lite v0.8.0",
    "fmt 10.2.1",
    "gsl v4.0.0",
    "glm 1.0.1",
    "leveldb 1.23",
    "magic_enum v0.9.7",
    "nlohmann_json v3.11.3",
    "rapidjson v1.1.0",
    "type_safe v0.2.4",
    "pcg_cpp v1.0.0",
    "pfr 2.1.1",
    "symbolprovider v1.2.0",
    "parallel-hashmap v1.3.12",
    "concurrentqueue v1.0.4"
}
local deps_server = {
    "bedrockdata 1.21.60.10-server"
}
local deps_client = {
    "bedrockdata 1.21.0.3-client",
    "imgui[dx11=y,dx12=y] v1.91.0-docking"
}

function load(package)
    for _, dep in ipairs(deps_common) do
        package:add("deps", dep)
    end
    if package:config("target_type") == "server" then
        for _, dep in ipairs(deps_server) do
            package:add("deps", dep)
        end
    else
        for _, dep in ipairs(deps_client) do
            package:add("deps", dep)
        end
    end
end
