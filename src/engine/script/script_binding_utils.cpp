#include "script_binding_utils.h"

#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace engine::script {

sol::table createReadOnlyProxy(sol::state& lua, sol::table source, std::string_view name) {
    sol::table proxy = lua.create_table();
    sol::table metatable = lua.create_table();
    const std::string table_name(name);

    metatable[sol::meta_function::index] = source;
    metatable[sol::meta_function::new_index] = [table_name](lua_State* lua_state) -> int {
        const char* raw_key = lua_tostring(lua_state, 2);
        if (raw_key) {
            return luaL_error(lua_state, "%s is read-only (key='%s')", table_name.c_str(), raw_key);
        }
        return luaL_error(lua_state, "%s is read-only", table_name.c_str());
    };
    metatable[sol::meta_function::metatable] = "locked";

    proxy[sol::metatable_key] = metatable;
    return proxy;
}

} // namespace engine::script
