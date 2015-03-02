#include <cstdlib>
#include "nnlua.h"

lua_State *NnLua::L=NULL;

void NnLua::shutdown()
{
    if( L != NULL ){
        lua_close(L);
        L = NULL;
    }
}

NnLua::NnLua()
{
    if( L == NULL ){
        if( (L = luaL_newstate()) == NULL )
            return;
        luaL_openlibs(L);
        atexit(shutdown);
    }
    initlevel = lua_gettop(L);
}

NnLua::~NnLua()
{
    lua_settop(L,initlevel);
}
