#ifndef NNLUA_HEADER
#define NNLUA_HEADER

#ifndef LUA_IS_COMPILED_AS_CPP
extern "C" {
#endif
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#ifndef LUA_IS_COMPILED_AS_CPP
}
#endif

class NnLua{
    private:
        static void shutdown();
        int initlevel;
    protected:
        static lua_State *L;
    public:
        NnLua();
        ~NnLua();
        operator lua_State* () { return L; }
};

#endif
/* vim:set ft=cpp: */
