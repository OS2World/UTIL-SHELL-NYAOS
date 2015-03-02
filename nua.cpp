#include <assert.h>
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/types.h>
#include <unistd.h>
#include "nnhash.h"
#include "getline.h"
#include "nua.h"
#include "ntcons.h"
#include "history.h"

#ifdef NYACUS
#  include <windows.h>
#  include <wincon.h>
#else
#  include <signal.h>
#endif

//#define TRACE(X) ((X),fflush(stdout))
#define TRACE(X)

const static char 
    NYAOS_NNHASH[]      = "nyaos_nnhash" ,
    NYAOS_NNHASH_EACH[] = "nyaos_nnhash_each" ,
    NYAOS_NNVECTOR[]    = "nyaos_nnvector" ;

extern int open_luautil( lua_State *L );
#ifdef NYACUS
extern int com_create_object( lua_State *L );
extern int com_get_active_object( lua_State *L );
extern int com_const_load( lua_State *L );
#endif

static void lstop (lua_State *L, lua_Debug *ar) {
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  luaL_error(L, "interrupted!");
}

#ifdef __EMX__
static void handle_ctrl_c(int sig)
{
    NnLua L;
    lua_sethook(L, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
    signal(sig,SIG_ACK);
}
#else
static BOOL WINAPI handle_ctrl_c(DWORD ctrlChar)
{
    if( CTRL_C_EVENT == ctrlChar){
        NnLua L;
        lua_sethook(L, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
        return TRUE;
    }else{
        return FALSE;
    }
}
#endif

/* Lua を(必要であれば)初期化すると同時に、
 * NYAOS テーブル上のオブジェクトをスタックに積む
 *    field - nyaos 上のフィールド名。NULL の時は nyaos 自体を積む
 *    L     - luaState オブジェクト。NULL の時は、当関数で取得する。
 * return
 *    not NULL - luaState オブジェクト
 *    NULL     - 初期化失敗 or nyaos がテーブルでなかった
 */
NyaosLua::NyaosLua( const char *field ) : NnLua()
{
    if( init() ){
        ready = 0;
        return;
    }
    lua_getglobal(L,"nyaos"); /* +1 */
    assert( lua_istable(L,-1) ); 

    if( field != NULL && field[0] != '\0' ){
        lua_getfield(L,-1,field);
        lua_remove(L,-2); /* drop 'nyaos' */
    }
    ready = 1;
}

int nua_get(lua_State *L)
{
    NnHash **dict=(NnHash**)luaL_checkudata(L,-2,NYAOS_NNHASH);
    const char *key=luaL_checkstring(L,-1);

    NnObject *result=(*dict)->get( NnString(key) );
    if( result != NULL ){
        lua_pushstring( L , result->repr() );
        return 1;
    }else{
        return 0;
    }
}

int nua_put(lua_State *L)
{
    NnHash **dict=(NnHash**)luaL_checkudata(L,-3,NYAOS_NNHASH);
    const char *key=luaL_checkstring(L,-2);
    const char *val=lua_tostring(L,-1);

    if( val ){
        (*dict)->put( NnString(key) , new NnString(val) );
    }else{
        (*dict)->remove( NnString(key) );
    }
    return 0;
}

int nua_vector_set(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-3,NYAOS_NNVECTOR);
    int key=luaL_checkint(L,-2);
    const char *val=lua_tostring(L,-1);
    if( key < 1 || (*vec)->size() < key ){
        lua_pushstring(L,"Index Error on nyaos_vector_set");
        return lua_error(L);
    }

    NnString rightvalue(val != NULL ? val : "" );
    NnString *leftvalue = dynamic_cast<NnString*>( (*vec)->at( key-1 ) );
    if(leftvalue == NULL ){
        lua_pushstring(L,"Internal Error on nua_vector_set");
        return lua_error(L);
    }
    *leftvalue = rightvalue ;
    return 0;
}

int nua_iter(lua_State *L)
{
    TRACE(puts("Enter: nua_iter") );
    NnHash::Each **ptr=(NnHash::Each**)luaL_checkudata(L,1,NYAOS_NNHASH_EACH);

    if( ptr != NULL && ***ptr != NULL ){
        lua_pushstring(L,(**ptr)->key().chars());
        lua_pushstring(L,(**ptr)->value()->repr());
        ++(**ptr);
        TRACE(puts("Leave: nua_iter: next") );
        return 2;
    }else{
        TRACE(puts("Leave: nua_iter: last") );
        return 0;
    }
}

int nua_iter_gc(lua_State *L)
{
    TRACE(puts("Enter: nua_iter_gc") );
    NnHash::Each **ptr=(NnHash::Each**)luaL_checkudata(L,1,NYAOS_NNHASH_EACH);
    if( ptr != NULL && *ptr !=NULL ){
        delete *ptr;
        *ptr = NULL;
    }
    TRACE(puts("Leave: nua_iter_gc"));
    return 0;
}

int nua_iter_factory(lua_State *L)
{
    TRACE(puts("Enter: nua_iter_factory"));
    NnHash **dict=(NnHash**)luaL_checkudata(L,1,NYAOS_NNHASH);
    NnHash::Each *ptr=new NnHash::Each(**dict);
    if( ptr == NULL ){
        TRACE(puts("Leave: nua_iter_factory: ptr==NULL"));
        return 0;
    }

    lua_pushcfunction(L,nua_iter);
    void *state = lua_newuserdata(L,sizeof(NnHash::Each*));
    memcpy(state,&ptr,sizeof(NnHash::Each*));

    luaL_newmetatable(L,NYAOS_NNHASH_EACH);
    lua_pushstring(L,"__gc");
    lua_pushcfunction(L,nua_iter_gc);
    lua_settable(L,-3);
    lua_setmetatable(L,-2);

    TRACE(puts("Leave: nua_iter_factory: success") );
    return 2;
}

/* #演算子に限っては、何故か、スタックに
 *   [ユーザオジェクト] [NIL]
 * と入ってくるので、スタック位置に注意が必要
 * */
int nua_vector_len(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,1,NYAOS_NNVECTOR);
    lua_pushinteger( L , (*vec)->size() );
    return 1;
}

int nua_vector_add(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-2,NYAOS_NNVECTOR);
    const char *str=luaL_checkstring(L,-1);
    History *history=dynamic_cast<History*>( *vec );
    if( history != NULL ){
        history->append( new History1(str) );
    }else{
        (*vec)->append( new NnString(str) );
    }
    return 0;
}

int nua_vector_pop(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-1,NYAOS_NNVECTOR);
    NnObject *object=(*vec)->pop();
    lua_pushstring( L , object->repr() );
    delete object;
    return 1;
}

int nua_vector_shift(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-1,NYAOS_NNVECTOR);
    NnObject *object=(*vec)->shift();
    lua_pushstring( L , object->repr() );
    delete object;
    return 1;
}

int nua_vector_get(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-2,NYAOS_NNVECTOR);
    if( lua_isnumber(L,-1) ){
        int n=luaL_checkint(L,-1);
        if( n < 1 || (*vec)->size() < n ){
            lua_pushnil( L );
        }else{
            lua_pushstring( L , (*vec)->at(n-1)->repr() );
        }
        return 1;
    }else{
        const char *method=luaL_checkstring(L,-1);
        if( strcmp(method,"add")==0  || strcmp(method,"push")==0 ){
            lua_pushcfunction( L , nua_vector_add );
        }else if( strcmp(method,"len")==0 ){
            lua_pushcfunction( L , nua_vector_len );
        }else if( strcmp(method,"pop")==0 || strcmp(method,"drop")==0 ){
            lua_pushcfunction( L , nua_vector_pop );
        }else if( strcmp(method,"shift")==0 ){
            lua_pushcfunction( L , nua_vector_shift );
        }else{
            lua_pushnil(L);
        }
        return 1;
    }
}

int nua_vector_iter(lua_State *L)
{
    NnVector **vec=(NnVector**)luaL_checkudata(L,-2,NYAOS_NNVECTOR);
    int n=luaL_checkint(L,-1);
    if( n >= (*vec)->size() )
        return 0;
    lua_pushinteger( L , n+1 );
    lua_pushstring( L , (*vec)->at(n)->repr() );
    return 2;
}

int nua_vector_iter_factory(lua_State *L)
{
    (void)luaL_checkudata(L,-1,NYAOS_NNVECTOR);
    lua_pushcfunction(L,nua_vector_iter);
    lua_insert(L,-2);
    lua_pushinteger(L,0);
    return 3;
}

int nua_exec(lua_State *L)
{
    const char *statement = lua_tostring(L,-1);
    if( statement == NULL )
        return luaL_argerror(L,1,"invalid nyaos-statement");
    
    OneLineShell shell( statement );

    shell.mainloop();

    lua_pushinteger(L,shell.exitStatus());
    return 1;
}

int nua_eval(lua_State *L)
{
    extern int eval_cmdline( const char *cmdline, NnString &dst, int max , bool shrink );

    const char *statement = lua_tostring(L,-1);
    if( statement == NULL )
        return luaL_argerror(L,1,"invalid nyaos-statement");

    NnString buffer;
    if( eval_cmdline( statement , buffer , 0 , false ) == 0 ){
        lua_pushstring( L , buffer.chars() );
        return 1;
    }else{
        lua_pushnil( L );
        lua_pushstring( L , strerror(errno) );
        return 2;
    }
}

int nua_access(lua_State *L)
{
    const char *fname = lua_tostring(L,1);
    int amode = lua_tointeger(L,2);

    if( fname == NULL )
        return luaL_argerror(L,2,"invalid filename");
    
    lua_pushboolean(L, access(fname,amode)==0 );
    return 1;
}

int nua_getkey(lua_State *L)
{
    char key=Console::getkey();
    lua_pushlstring(L,&key,1);
    return 1;
}

int nua_write(lua_State *L)
{
    int n=lua_gettop(L);
    for(int i=1;i<=n;i++){
        const char *s=luaL_checkstring(L,i);
        conOut << s;
    }
    return 0;
}

int nua_len(lua_State *L)
{
    const char *s=lua_tostring(L,1);
    if( s == NULL )
        return 0;

    int count=0;
    while( *s != '\0' ){
        ++count;
        if( isKanji(*s) )
            if( *++s == '\0' )
                break;
        ++s;
    }
    lua_pushinteger(L,count);
    return 1;
}

static int nua_getcwd(lua_State *L)
{
    NnString cwd;
    NnDir::getcwd(cwd);
    lua_pushstring( L , cwd.chars() );
    return 1;
}

int nua_substring(lua_State *L)
{
    const char *s=lua_tostring(L,1);
    if( s == NULL )
        return 0;

    int start=lua_tointeger(L,2)-1;
    int end  =lua_isnumber(L,3) ? lua_tointeger(L,3)-1 : -1;

    int len=0;
    for(const char *p=s ; *p != '\0' ; ){
        len++;
        if( isKanji(*p) )
            ++p;
        if( *p == '\0' )
            break;
        ++p;
    }
    if( start < 0 ){
        start = len + start + 1;
        if( start < 0 )
            start = 0;
    }
    if( end < 0 ){
        end = len + end + 1;
        if( end < 0 )
            end = 0;
    }
    // printf("len=%d start=%d end=%d\n",len,start,end);
    int count=0;
    while( count < start ){
        if( *s == '\0' ){
            goto empty;
        }
        if( isKanji(*s) ){
            if( *++s == '\0' )
                goto empty;
        }
        ++s;
        count++;
    }
    {
        NnString buf;
        while( count <= end && *s != '\0' ){
            if( isKanji(*s) )
                buf << *s++;
            if( *s == '\0' )
                break;
            buf << *s++;
            count++;
        }
        lua_pushstring(L,buf.chars());
    }
    return 1;
empty:
    lua_pushstring(L,"");
    return 1;
}


int nua_default_complete(lua_State *L)
{
    const char *basestring = lua_tostring(L,1);
    int pos=lua_tointeger(L,2);
    if( basestring == NULL ){
        return 0;
    }

    NnString basestring1(basestring);
    NnVector list;
    if( pos > 0 ){
        GetLine::makeCompletionListCore( basestring1 , list);
    }else{
        DosShell::makeTopCompletionListCore( basestring1 , list );
    }
    int count=0;
    lua_newtable(L);
    for(int i=0;i<list.size();i++){
        NnPair *pair=dynamic_cast<NnPair*>( list.at(i) );
        if( pair == NULL ) continue;

        lua_pushinteger( L , ++count );

        lua_newtable( L );
        /* フルパス */
        lua_pushinteger( L , 1 );
        lua_pushstring( L, pair->first()->repr() );
        lua_settable( L , -3 ) ;

        /* ファイル名部分のみ */
        lua_pushinteger( L , 2 );
        lua_pushstring( L, pair->second_or_first()->repr() );
        lua_settable( L , -3 ) ;

        lua_settable( L , -3 );
    }
    return 1;
}

int nua_putenv(lua_State *L )
{
    extern void putenv_(const NnString &,const NnString &);
    NnString name( lua_tostring(L,1) );
    NnString value( lua_tostring(L,2) );

    putenv_( name , value );
    return 0;
}

int NyaosLua::initialized=0;

/* NYAOS 向け Lua 環境初期化
 * return
 *    0  : 成功 
 *    !0 : 失敗
 */
int NyaosLua::init()
{
    if( ! initialized ){
        extern NnHash aliases;
        extern NnHash properties;
        extern NnHash functions;
        extern NnVector dirstack;

        static struct {
            const char *name;
            NnHash *dict;
            int (*index)(lua_State *);
            int (*newindex)(lua_State *);
            int (*call)(lua_State *);
        } list[]={
            { "alias"  , &aliases   , nua_get , nua_put , nua_iter_factory },
            { "suffix" , &DosShell::executableSuffix , nua_get , nua_put , nua_iter_factory },
            { "option" , &properties , nua_get , nua_put , nua_iter_factory } ,
            { "folder" , &NnDir::specialFolder , nua_get , nua_put , nua_iter_factory } ,
            { "functions" , &functions , nua_get , NULL , nua_iter_factory },
            { NULL , NULL , 0 } ,
        }, *p = list;

        open_luautil(L); 

        /* table-like objects */
        while( p->name != NULL ){
            NnHash **u=(NnHash**)lua_newuserdata(L,sizeof(NnHash *));
            *u = p->dict;

            luaL_newmetatable(L,NYAOS_NNHASH);
            if( p->index != NULL ){
                lua_pushcfunction(L,p->index);
                lua_setfield(L,-2,"__index");
            }
            if( p->newindex != NULL ){
                lua_pushcfunction(L,p->newindex);
                lua_setfield(L,-2,"__newindex");
            }
            if( p->call != NULL ){
                lua_pushcfunction(L,p->call);
                lua_setfield(L,-2,"__pairs");
            }
            lua_setmetatable(L,-2);

            lua_setfield(L,-2,p->name);
            p++;
        }

        static struct {
            const char *name;
            NnVector *vec;
            int (*index)(lua_State *);
            int (*newindex)(lua_State *);
            int (*ipair)(lua_State *);
            int (*len)(lua_State *);
        } list2[]={
            { "history"  , &GetLine::history ,
                nua_vector_get          , nua_vector_set ,
                nua_vector_iter_factory , nua_vector_len },
            { "dirstack" , &dirstack ,
                nua_vector_get          , nua_vector_set ,
                nua_vector_iter_factory , nua_vector_len },
            { NULL } ,
        }, *q = list2;

        while( q->name != NULL ){
            /* history object */
            NnVector **h=(NnVector**)lua_newuserdata(L,sizeof(NnVector*));
            *h = q->vec;

            luaL_newmetatable(L,NYAOS_NNVECTOR);
            if( q->index != NULL ){
                lua_pushcfunction(L,q->index);
                lua_setfield(L,-2,"__index");
            }
            if( q->newindex != NULL ){
                lua_pushcfunction(L,q->newindex);
                lua_setfield(L,-2,"__newindex");
            }
            if( q->ipair != NULL ){
                lua_pushcfunction(L,q->ipair);
                lua_setfield(L,-2,"__pairs");
                lua_pushcfunction(L,q->ipair);
                lua_setfield(L,-2,"__ipairs");
            }
            if( q->len != NULL ){
                lua_pushcfunction(L,q->len);
                lua_setfield(L,-2,"__len");
            }
            lua_setmetatable(L,-2);
            lua_setfield(L,-2,q->name);

            q++;
        }

        /* tool functions */
#ifdef NYACUS
        lua_pushcfunction(L,com_create_object);
        lua_setfield(L,-2,"create_object");
        lua_pushcfunction(L,com_get_active_object);
        lua_setfield(L,-2,"get_active_object");
        lua_pushcfunction(L,com_const_load);
        lua_setfield(L,-2,"const_load");
#endif
        lua_pushcfunction(L,nua_exec);
        lua_setfield(L,-2,"exec");
        lua_pushcfunction(L,nua_eval);
        lua_setfield(L,-2,"eval");
        lua_pushcfunction(L,nua_access);
        lua_setfield(L,-2,"access");
        lua_pushcfunction(L,nua_getkey);
        lua_setfield(L,-2,"getkey");
        lua_pushcfunction(L,nua_write);
        lua_setfield(L,-2,"write");
        lua_pushcfunction(L,nua_default_complete);
        lua_setfield(L,-2,"default_complete");
        lua_pushcfunction(L,nua_putenv);
        lua_setfield(L,-2,"putenv");
        lua_pushcfunction(L,nua_substring);
        lua_setfield(L,-2,"sub");
        lua_pushcfunction(L,nua_len);
        lua_setfield(L,-2,"len");
        lua_pushcfunction(L,nua_getcwd);
        lua_setfield(L,-2,"currentdir");

        lua_pushinteger(L, getpid() );
        lua_setfield(L,-2,"pid");

        /* nyaos.command[] */
        lua_newtable(L);
        lua_setfield(L,-2,"command");
        lua_newtable(L);
        lua_setfield(L,-2,"command2");
        lua_newtable(L);
        lua_setfield(L,-2,"filter");
        lua_newtable(L);
        lua_setfield(L,-2,"filter2");
        lua_newtable(L);
        lua_setfield(L,-2,"filter3");
        lua_newtable(L);
        lua_setfield(L,-2,"keyhook");
        lua_newtable(L);
        lua_setfield(L,-2,"goodbye");

        /* close nyaos table */
        lua_setglobal(L,"nyaos");

        initialized = 1;
    }
    return initialized ? 0 : -1;
}

void redirect_emu_to_real(int &back_in, int &back_out,int &back_err)
{
    /* 標準入力のリダイレクトに対応 */
    int cur_in=0;
    StreamReader *sr=dynamic_cast<StreamReader*>( conIn_ );
    if( sr != NULL ){
        cur_in = sr->fd();
    }else{
        conErr << "CAN NOT GET HANDLE(STDIN)\n";
    }
    back_in = -1;
    if( cur_in != 0 ){
        back_in = ::dup(0);
        ::dup2( cur_in , 0 );
    }
    /* 標準出力のリダイレクト・パイプ出力に対応 */
    int cur_out=+1;
    StreamWriter *sw=dynamic_cast<StreamWriter*>( conOut_  );
    if( sw != NULL ){
        cur_out = sw->fd();
    }else{
        RawWriter *rw=dynamic_cast<RawWriter*>( conOut_ );
        if( rw != NULL ){
            cur_out = rw->fd();
        }else{
            conErr << "CAN NOT GET HANDLE(STDOUT)\n";
        }
    }
    fflush(stdout);
    back_out=-1;
    if( cur_out != 1 ){
        back_out = ::dup(1);
        ::dup2( cur_out , 1 );
    }
    /* 標準エラー出力のリダイレクト・パイプ出力に対応 */
    int cur_err=+2;
    sw=dynamic_cast<StreamWriter*>( &conErr );
    if( sw != NULL ){
        cur_err = sw->fd();
    }else{
        RawWriter *rw=dynamic_cast<RawWriter*>( &conErr );
        if( rw != NULL ){
            cur_err = rw->fd();
        }else{
            conErr << "Redirect and pipeline for stderr are not supported yet.\n";
        }
    }
    fflush(stderr);
    back_err=-1;
    if( cur_err != 2 ){
        back_err = ::dup(2);
        ::dup2( cur_err , 2 );
    }
}

void redirect_rewind(int back_in, int back_out,int back_err)
{
    /* 標準エラー出力を元に戻す */
    if( back_err >= 0 ){
        ::dup2( back_err , 2 );
        ::close( back_err );
    }
    /* 標準出力を元に戻す */
    if( back_out >= 0 ){
        ::dup2( back_out , 1 );
        ::close( back_out );
    }
    /* 標準入力を元に戻す */
    if( back_in >= 0 ){
        ::dup2( back_in , 0 );
        ::close( back_in );
    }
}


int cmd_lua_e( NyadosShell &shell , const NnString &argv )
{
    int back_in, back_out , back_err;

    NnString arg1,left;
    argv.splitTo( arg1 , left );
    NyadosShell::dequote( arg1 );
    arg1.replace( "$T" , "\n" , arg1 );

    if( arg1.empty() ){
        conErr << "lua_e \"lua-code\"\n" ;
        return 0;
    }
    NyaosLua L;

    redirect_emu_to_real( back_in , back_out , back_err );
#ifdef __EMX__
    signal( SIGINT , handle_ctrl_c );
#else
    Console::enable_ctrl_c();
    SetConsoleCtrlHandler( handle_ctrl_c , TRUE );
#endif

    /* Lua インタプリタコール */
    if( luaL_loadstring(L,arg1.chars() ) || lua_pcall( L , 0 , 0 , 0 ) ){
        const char *msg = lua_tostring( L , -1 );
        conErr << msg << '\n';
    }
    lua_settop(L,0);

#ifdef __EMX__
    signal( SIGINT , SIG_IGN );
#else
    SetConsoleCtrlHandler( handle_ctrl_c , FALSE );
    Console::disable_ctrl_c();
#endif
    redirect_rewind( back_in , back_out , back_err );
    return 0;
}

LuaHook::LuaHook(const char *hookname) : L(hookname) , count(0)
{
    if( L == NULL ){
        status = ERROR_HOOK;
    }else if( lua_isfunction(L,-1) ){
        status = SINGLE_HOOK;
    }else if( lua_istable(L,-1) ){
        status = MULTI_HOOK;

        lua_pushnil(L); /* start key */
        while( lua_next(L,-2) != 0 ){
            if( lua_isfunction(L,-1) ){
                lua_pushvalue(L,-2);
                funclist.append( new NnString(luaL_checkstring(L,-1)) );
                lua_pop(L,2);
            }else{
                lua_pop(L,1); /* drop value */
            }
        }
        funclist.sort();
    }else{
        status = ERROR_HOOK;
    }
}

int LuaHook::next()
{
    switch( status ){
    default:
        return 0;
    case SINGLE_HOOK:
        return count++ == 0;
    case MULTI_HOOK:
        if( count < funclist.size() ){
            lua_getfield(L,-1,funclist.const_at(count)->repr());
            count++;
            return 1;
        }else{
            if( count == funclist.size() ){
                lua_pop(L,1);
            }
            return 0;
        }
    };
}
