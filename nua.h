#ifndef NUA_H
#define NUA_H

#include "nnlua.h"
#include "nnvector.h"

class NyaosLua : public NnLua {
    private:
        static int initialized;
        int init();
        int ready;
    public:
        NyaosLua() : NnLua() , ready(1) { init(); }
        NyaosLua( const char *field );
        int ok() const { return L!=NULL && ready;  }
        int ng() const { return L==NULL || !ready; }
        operator lua_State*() { return L; }
};

void redirect_emu_to_real(int &back_in, int &back_out,int &back_err);
void redirect_rewind(int back_in, int back_out,int back_err);
void call_luahooks( const char *hookname, int (*pushfunc)(lua_State *L,void *), void *arg);

class LuaHook {
    NyaosLua L;
    enum {
        SINGLE_HOOK ,
        MULTI_HOOK ,
        ERROR_HOOK
    } status;
    int count;
    NnVector funclist;
public:
    LuaHook(const char *hookname);
    ~LuaHook(){}
    int next();
    operator lua_State* (){ return L; }
};

#endif
/* vim:set ft=cpp textmode: */
