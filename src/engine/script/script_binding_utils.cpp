#include "script_binding_utils.h"

#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace engine::script {

namespace {

int readOnlyNewIndex(lua_State* lua_state) {
    const char* table_name = lua_tostring(lua_state, lua_upvalueindex(1));
    if (!table_name) {
        table_name = "table";
    }

    const char* raw_key = lua_tostring(lua_state, 2);
    if (raw_key) {
        return luaL_error(lua_state, "%s is read-only (key='%s')", table_name, raw_key);
    }
    return luaL_error(lua_state, "%s is read-only", table_name);
}

void setReadOnlyNewIndex(sol::table& metatable, std::string_view name) {
    lua_State* lua_state = metatable.lua_state();
    const int stack_top = lua_gettop(lua_state);

    metatable.push();
    lua_pushlstring(lua_state, name.data(), name.size());
    lua_pushcclosure(lua_state, &readOnlyNewIndex, 1);
    lua_setfield(lua_state, -2, "__newindex");

    lua_settop(lua_state, stack_top);
}

} // namespace

sol::table createReadOnlyProxy(sol::state& lua, sol::table source, std::string_view name) {
    sol::table proxy = lua.create_table();
    sol::table metatable = lua.create_table();

    metatable[sol::meta_function::index] = source;
    setReadOnlyNewIndex(metatable, name);
    metatable[sol::meta_function::metatable] = "locked";

    proxy[sol::metatable_key] = metatable;
    return proxy;
}

} // namespace engine::script
