#include <stdio.h>

#ifndef LUA_IS_COMPILED_AS_CPP
extern "C" {
#endif
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#ifndef LUA_IS_COMPILED_AS_CPP
}
#endif

#include "win32com.h"

//#define DBG(x) (x)
#define DBG(x)

static void hr_to_lua_message(HRESULT hr,lua_State *L)
{
    LPVOID s;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM ,
                  NULL ,
                  hr ,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&s,
                  0,
                  NULL);
    lua_pushstring(L,(LPCTSTR)s);
    LocalFree(s);
}

static void set_nyaos_error(lua_State *L,const char *s)
{
    lua_getglobal(L,"nyaos");
    if( lua_istable(L,-1) ){
        if( s != NULL ){
            lua_pushstring(L,s);
        }else{
            lua_pushnil(L);
        }
        lua_setfield(L,-2,"error");
    }
    lua_pop(L,1);
}

static int destroy_object(lua_State *L)
{
    DBG( puts("[CALL] destroy_object") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    delete *u;
    return 0;
}

static int destroy_member(lua_State *L)
{
    DBG( puts("[CALL] destroy_member") );
    ActiveXMember **m=static_cast<ActiveXMember**>(
            luaL_checkudata(L,1,"ActiveXMember"));
    delete *m;
    return 0;
}

static int how_many_dim( lua_State *L )
{
    int rc=1;
    lua_pushinteger(L,1);
    lua_gettable(L,-2);
    if( lua_istable(L,-1) ){
        rc += how_many_dim(L);
    }
    lua_pop(L,1);
    return rc;
}

static int how_many_len( lua_State *L , SAFEARRAYBOUND *bound )
{
    if( ! lua_istable(L,-1) )
        return 1;
    bound->cElements = luaL_len(L,-1);
    bound->lLbound   = 0;
    ++bound;
    lua_pushinteger(L,1);
    lua_gettable(L,-2);
    int n=how_many_len(L,bound);
    lua_pop(L,1);
    return n+1;
}

static void reset_safearray(
        int depth,
        int maxdepth,
        long *counter,
        SAFEARRAYBOUND *bound ,
        SAFEARRAY *array )
{
    for( counter[depth]=0 ;
            counter[depth] < static_cast<long>(bound[depth].cElements) ;
            ++counter[depth] )
    {
        if( maxdepth == depth ){
            VARIANT var;
            VariantInit(&var);
            var.vt = VT_EMPTY;
            SafeArrayPutElement( array , counter , &var );
        }else{
            reset_safearray( depth+1 , maxdepth , counter , bound , array );
        }
    }
}

static void table2safearray(
        lua_State *L ,
        int depth ,
        int maxdepth ,
        long *counter ,
        SAFEARRAYBOUND *bound ,
        SAFEARRAY *array )
{
    for(counter[depth]=0 ;
            counter[depth] < static_cast<long>(bound[depth].cElements) ;
            ++counter[depth] )
    {
        DBG( printf("[%ld]",counter[depth]) );
        lua_pushinteger(L,1+counter[depth]);
        lua_gettable(L,-2);
        if( maxdepth == depth ){
            VARIANT var; VariantInit(&var);
            switch( lua_type(L,-1) ){
            case LUA_TSTRING:
                DBG( puts("<STR>") );
                var.vt      = VT_BSTR ;
                var.bstrVal = Unicode::c2b( lua_tostring(L,-1) );
                break;
            case LUA_TNUMBER:
                DBG( puts("<NUM>") );
                var.vt      = VT_R8 ;
                var.dblVal  = lua_tonumber(L,-1);
                break;
            default:
                DBG( puts("<EMP>") );
                var.vt      = VT_EMPTY ;
                break;
            }
            SafeArrayPutElement( array , counter , &var );
            VariantClear( &var );
        }else{
            if( lua_istable(L,-1) ){
                DBG( puts("<TBL>") );
                table2safearray(L,depth+1,maxdepth,counter,bound,array);
            }else{
                DBG( puts("<not TBL>") );
            }
        }
        lua_pop(L,1);
    }
}

static void lua2variants( lua_State *L , int i , Variants &args )
{
    switch( lua_type(L,i) ){
    case LUA_TTABLE:
        {
            int dim = how_many_dim(L);
            DBG( printf("max depth=%d\n",dim) );
            SAFEARRAYBOUND *bound = new SAFEARRAYBOUND[dim];
            how_many_len(L,bound);
            long *counter=new long[dim];

            SAFEARRAY *array=SafeArrayCreate(VT_VARIANT,dim,bound);

            reset_safearray(0,dim-1,counter,bound,array);
            table2safearray(L,0,dim-1,counter,bound,array);

            VARIANT *pvar=args.add_anything();
            VariantInit(pvar);
            pvar->vt = VT_VARIANT | VT_ARRAY;
            pvar->parray = array;
        }
        break;
    case LUA_TNUMBER:
        args.add_as_number( lua_tonumber(L,i) );
        break;
    case LUA_TBOOLEAN:
        args.add_as_boolean( lua_toboolean(L,i) );
        break;
    case LUA_TNIL:
        args.add_as_null();
        break;
    case LUA_TSTRING:
    default:
        {
            const char *s=lua_tostring(L,i);
            if( s != NULL )
                args.add_as_string( s );
            else
                args.add_as_null();
        }
        break;
    }
}

static int put_property(lua_State *L)
{
    DBG( puts("[CALL] put_property") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL ){
        DBG( puts("no ActiveXObject") );
        return 0;
    }
    const char *member_name = lua_tostring(L,2);
    if( member_name == NULL ){
        DBG( puts("no member name") );
        return 0;
    }

    Variants args;  lua2variants(L,3,args);
    VARIANT result; VariantInit(&result);

    char *error_info=0;
    (**u).invoke( member_name , DISPATCH_PROPERTYPUT , 
                  args , args.size() , result , NULL , &error_info );
    if( error_info != 0 ){
        lua_pushnil(L);
        lua_pushstring(L,error_info);
        DBG( puts("error_info=") );
        DBG( puts(error_info) );
        delete[]error_info;
        return 2;
    }else{
        lua_pushboolean(L,1);
        return 1;
    }
}

static int return_self(lua_State *L)
{
    DBG( printf("[CALL] return_self argc=%d\n",lua_gettop(L)) );
    DBG( printf("lua_type(argc[-1])==%d\n",lua_type(L,-1)) );
    lua_pushvalue(L,1);
    return 1;
}

static int find_member(lua_State *L);
static int make_iterator(lua_State *L);
static int make_iterator_from_member(lua_State *L);

static void push_activexobject(lua_State *L,ActiveXObject *obj)
{
    ActiveXObject **u=
        static_cast<ActiveXObject**>(
                lua_newuserdata( L, sizeof(ActiveXObject*) ));
    *u = obj;
    luaL_newmetatable(L,"ActiveXObject");
    lua_pushcfunction(L,destroy_object);
    lua_setfield(L,-2,"__gc");
    lua_pushcfunction(L,find_member);
    lua_setfield(L,-2,"__index");
    lua_pushcfunction(L,put_property);
    lua_setfield(L,-2,"__newindex");
    lua_pushcfunction(L,return_self);
    lua_setfield(L,-2,"__call");
    lua_pushcfunction(L,make_iterator);
    lua_setfield(L,-2,"__pairs");
    lua_setmetatable(L,-2);
}

static int variant2lua( VARIANT &v , lua_State *L );
static void safearray2table_sub(
        SAFEARRAY *safearray ,
        int depth ,
        int dim ,
        long *counter ,
        long *start ,
        long *end ,
        lua_State *L )
{
    DBG( putchar('{') );
    lua_newtable(L);
    for( counter[depth]=start[depth] ; counter[depth] <= end[depth] ; ++counter[depth] ){
        DBG( printf("[%ld]=",counter[depth]) );
        lua_pushinteger(L,counter[depth]);
        if( depth >= (dim-1) ){
            VARIANT *p=0;
            DBG( putchar('*') );
            HRESULT hr = SafeArrayPtrOfIndex(
                    safearray,
                    counter,
                    (void**)&p);
            if( FAILED(hr) ){
                lua_pushnil(L);
            }else{
                variant2lua(*p,L);
            }
        }else{
            safearray2table_sub(safearray,depth+1,dim,counter,start,end,L);
        }
        lua_settable(L,-3);
    }
    DBG( puts("}") );
}

static void safearray2table( VARIANT &v , lua_State *L )
{
    SAFEARRAY *safearray = ( (v.vt & VT_BYREF) ? *v.pparray : v.parray );
    int dim = SafeArrayGetDim( safearray );
    long *counter=new long[dim];
    long *start  =new long[dim];
    long *end    =new long[dim];
    for(int i=0;i<dim;++i){
        SafeArrayGetLBound(safearray,i+1,&start[i]);
        SafeArrayGetUBound(safearray,i+1,&end[i]);
        DBG( printf("bound[%d]=%ld..%ld\n",i,start[i],end[i]) );
    }
    HRESULT hr = SafeArrayLock(safearray);
    if( FAILED(hr) ){
        goto exit;
    }
    safearray2table_sub( safearray , 0 , dim , counter , start , end , L );
    SafeArrayUnlock(safearray);
exit:
    delete[]counter;
    delete[]start;
    delete[]end;
}

static int variant2lua( VARIANT &v , lua_State *L )
{
    if( v.vt == VT_BSTR ){
        char *p=Unicode::b2c( v.bstrVal );
        lua_pushstring(L,p);
        delete[]p;
    }else if( v.vt == (VT_BSTR|VT_BYREF) ){
        char *p=Unicode::b2c( *v.pbstrVal );
        lua_pushstring(L,p);
        delete[]p;
    }else if( v.vt == VT_DISPATCH ){
        ActiveXObject *o=new ActiveXObject( v.pdispVal );
        push_activexobject(L,o);
    }else if( v.vt == (VT_DISPATCH|VT_BYREF) ){
        ActiveXObject *o=new ActiveXObject( *v.ppdispVal );
        push_activexobject(L,o);
    }else if( v.vt == VT_BOOL ){
        lua_pushboolean(L,v.boolVal);
    }else if( v.vt == (VT_BOOL|VT_BYREF) ){
        lua_pushboolean(L,*v.pboolVal); 
    }else if( v.vt == VT_UI1 ){
        lua_pushinteger(L,v.bVal);
    }else if( v.vt == (VT_UI1|VT_BYREF) ){
        lua_pushinteger(L,*v.pbVal);
    }else if( v.vt == VT_I2 ){
        lua_pushinteger(L,v.iVal);
    }else if( v.vt == (VT_I2|VT_BYREF) ){
        lua_pushinteger(L,*v.piVal);
    }else if( v.vt == VT_I4 ){
        lua_pushinteger(L,v.lVal);
    }else if( v.vt == (VT_I4|VT_BYREF) ){
        lua_pushinteger(L,*v.plVal);
    }else if( v.vt == VT_R4 ){
        lua_pushnumber(L,v.fltVal);
    }else if( v.vt == (VT_R4|VT_BYREF) ){
        lua_pushnumber(L,*v.pfltVal);
    }else if( v.vt == VT_R8 ){
        lua_pushnumber(L,v.dblVal);
    }else if( v.vt == (VT_R8|VT_BYREF) ){
        lua_pushnumber(L,*v.pdblVal);
    }else if( (v.vt & VT_ARRAY) != 0 ){
        safearray2table( v , L );
    }else{
        DBG( printf("vt=[%d]\n",v.vt) );
        lua_pushnil( L );
    }
    return 1;
}

static int call_member(lua_State *L)
{
    DBG( puts("[CALL] call_member") );
    Variants args;
    VARIANT result; VariantInit(&result);

    ActiveXMember **m=static_cast<ActiveXMember**>(
            luaL_checkudata(L,1,"ActiveXMember")); 
    if( m == NULL || !(**m).ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXMethod");
        return 2;
    }
    ActiveXObject **o=static_cast<ActiveXObject**>(
            luaL_checkudata(L,2,"ActiveXObject"));
    if( o == NULL || !(**o).ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXObject");
        return 2;
    }
    for(int i=lua_gettop(L);i>=3;--i){
        lua2variants(L,i,args);
    }
    char *error_info=0;
    if( (**m).invoke( DISPATCH_METHOD | DISPATCH_PROPERTYGET ,
                args , args.size() , result , NULL , &error_info ) != 0 )
    {
        lua_pushnil(L);
        if( error_info != 0 ){
            lua_pushstring(L,error_info);
            delete[]error_info;
            set_nyaos_error(L,error_info);
        }else{
            lua_pushstring(L,"Something happend on COM");
            set_nyaos_error(L,"Something happend on COM");
        }
        return 2;
    }
    DBG( puts("invoke(DISPATCH_PROPERTYGET|DISPATCH_METHOD) succeded.") );
    delete[]error_info;
    return variant2lua(result,L);
}

static void push_activexmember(lua_State *L,ActiveXMember *member)
{
    ActiveXMember **m=static_cast<ActiveXMember**>(
            lua_newuserdata(L,sizeof(ActiveXMember*))
            );
    *m = member;
    luaL_newmetatable(L,"ActiveXMember");

    lua_pushcfunction(L,destroy_member);
    lua_setfield(L,-2,"__gc");

    lua_pushcfunction(L,call_member);
    lua_setfield(L,-2,"__call");

    lua_pushcfunction(L,make_iterator_from_member);
    lua_setfield(L,-2,"__pairs");

    lua_setmetatable(L,-2);
}

// 「self:Item("key") = val」 ができないため
// かわりに
// 「self:__put__("Item","key",val)」
// ==「self.__put__(self,"Item","key",val)」を提供
static int put__(lua_State *L)
{
    if( lua_gettop(L) < 4 ){
        lua_pushnil(L);
        lua_pushstring(L,"__put__ requires 4 arguments.");
        return 2;
    }
    DBG( puts("[CALL] __put__") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL || !(**u).ok() ){
        set_nyaos_error(L,"Invalid Object. Expected <ActiveXObject>");
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXObject>");
        return 2;
    }
    const char *name=lua_tostring(L,2);
    if( name == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"method name not found");
        return 2;
    }
    ActiveXMember member(**u,name);
    if( ! member.ok() ){
        lua_pushnil(L);
        lua_pushstring(L,"method not found");
        return 2;
    }

    Variants args;
    lua2variants(L,4,args);
    lua2variants(L,3,args);
    VARIANT result; VariantInit(&result);

    char *error_info=0;
    (**u).invoke( name , DISPATCH_PROPERTYPUT , 
                  args , args.size() , result , NULL , &error_info );
    if( error_info != 0 ){
        lua_pushnil(L);
        lua_pushstring(L,error_info);
        DBG( puts("error_info=") );
        DBG( puts(error_info) );
        delete[]error_info;
        return 2;
    }else{
        lua_pushboolean(L,1);
        return 1;
    }
}

static void const_setter(void *L_,const char *name,VARIANT &value);
static int find_member(lua_State *L)
{
    DBG( puts("[CALL] find_member") );
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL || !(**u).ok() ){
        set_nyaos_error(L,"Invalid Object. Expected <ActiveXObject>");
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXObject>");
        return 2;
    }
    const char *member_name=lua_tostring(L,2);
    if( strcmp(member_name,"__put__")==0 ){
        lua_pushcfunction(L,put__);
        return 1;
    }
    if( strcmp(member_name,"__iter__")==0 ){
        lua_pushcfunction(L,make_iterator);
        return 1;
    }
    if( strcmp(member_name,"__const__")==0 ){
        lua_newtable(L);
        (**u).const_load(L,const_setter);
        return 1;
    }
    ActiveXMember *member=new ActiveXMember(**u,member_name);
    if( member == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find method for ActiveXObject");
        return 2;
    }else if( !member->ok() ){
        lua_pushnil(L);
        hr_to_lua_message(member->construct_error(),L);
        set_nyaos_error(L,lua_tostring(L,-1));
        delete member;
        return 2;
    }

    Variants args;
    VARIANT result; VariantInit(&result);
    HRESULT hr;
    char *error_info=0;
    int rc=0;
    if( member->invoke( DISPATCH_PROPERTYGET ,
                args , 0 , result , &hr , &error_info ) != 0 )
    {
        DBG( printf("invoke('%s',DISPATCH_PROPERTYGET) failed. hr==%0lX\n",
                    member_name,hr) );

        if( hr == DISP_E_MEMBERNOTFOUND || hr == DISP_E_BADPARAMCOUNT ){
            DBG( puts("member is method") );
            push_activexmember(L,member);
            rc = 1;
        }else{
            delete member;
            if( error_info != 0 ){
                set_nyaos_error(L,error_info);
                lua_pushnil(L);
                lua_pushstring(L,error_info);
            }else{
                lua_pushnil(L);
                hr_to_lua_message(hr,L);
            }
            set_nyaos_error(L,lua_tostring(L,-1));
            rc = 2;
        }
    }else{
        DBG( printf("invoke('%s',DISPATCH_PROPERTYGET) success. hr==%0lX\n",
                    member_name,hr) );
        rc=variant2lua( result , L );
        if( rc != 0 ){
            // member is property
            delete member;
        }else{
            // member is method
            push_activexmember(L,member);
            rc = 1;
        }
    }
    delete[]error_info;
    if( result.vt != VT_DISPATCH ){
        VariantClear( &result );
    }
    return rc;
}

static int new_activex_object(lua_State *L,bool isNewObject)
{
    DBG( puts("[CALL] create_object") );
    const char *name=lua_tostring(L,1);
    ActiveXObject *obj=new ActiveXObject(name,isNewObject);
    if( obj == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Can not find ActiveXObject");
        return 2;
    }else if( !obj->ok() ){
        lua_pushnil(L);
        hr_to_lua_message(obj->construct_error(),L);
        delete obj;
        return 2;
    }else{
        push_activexobject(L,obj);
        return 1;
    }
}

int com_create_object(lua_State *L)
{
    return new_activex_object(L,true);
}

int com_get_active_object(lua_State *L)
{
    return new_activex_object(L,false);
}

static void const_setter(void *L_,const char *name,VARIANT &value)
{
    lua_State *L=static_cast<lua_State*>(L_);

    lua_pushstring( L , name );
    variant2lua( value , L );
    lua_settable( L , -3 );
}

int com_const_load(lua_State *L)
{
    ActiveXObject **u=static_cast<ActiveXObject**>(
            luaL_checkudata(L,1,"ActiveXObject") );
    if( u == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXObject");
        return 2;
    }
    lua_newtable(L);
    (**u).const_load(L,const_setter);
    return 1;
}

static int destroy_iterator(lua_State *L)
{
    ActiveXIterator **axi=static_cast<ActiveXIterator**>(
        luaL_checkudata(L,1,"ActiveXIterator") );
    if( axi == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXIterator");
        return 2;
    }
    delete (*axi);
    return 0;
}

static int next_iterator(lua_State *L)
{
    DBG( puts("[Enter] next_iterator") );
    ActiveXIterator **axi=static_cast<ActiveXIterator**>(
        luaL_checkudata(L,1,"ActiveXIterator") );
    if( axi == NULL ){
        lua_pushnil(L);
        lua_pushstring(L,"Invalid ActiveXIterator");
        DBG( puts("[leave] next_iterator (failure)") );
        return 2;
    }
    if( (**axi).nextObj() ){
        variant2lua( (**axi).var() , L );
        DBG( puts("[leave] next_iterator (continue)") );
        return 1;
    }else{
        DBG( puts("[leave] next_iterator (fisnish)") );
        return 0;
    }
}

static int make_iterator(lua_State *L)
{
    DBG( puts("[Enter] make_iterator") );
    ActiveXObject **axo=static_cast<ActiveXObject**>(
        luaL_checkudata(L,1,"ActiveXObject") );
    DBG( puts("[Step1] make_iterator") );
    if( axo == NULL || !(**axo).ok() ){
        set_nyaos_error(L,"Invalid Object. Expected <ActiveXObject>");
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXObject>");
        DBG( puts("[Leave] make_iterator (failure)") );
        return 2;
    }
    DBG( puts("[Step2] make_iterator") );

    // first return value : iterator-function
    lua_pushcfunction(L,next_iterator);

    DBG( puts("[Step3] make_iterator") );
    // second return value : object 
    ActiveXIterator **axi=
        static_cast<ActiveXIterator**>(
                lua_newuserdata( L , sizeof(ActiveXIterator*) ));
    DBG( puts("[Step4] make_iterator") );
    *axi = new ActiveXIterator(**axo);
    DBG( puts("[Step5] make_iterator") );
    luaL_newmetatable(L,"ActiveXIterator");
    lua_pushcfunction(L,destroy_iterator);
    lua_setfield(L,-2,"__gc");
    lua_setmetatable(L,-2);
    DBG( puts("[Leave] make_iterator (success)") );
    return 2;
}

static int make_iterator_from_member(lua_State *L)
{
    DBG( puts("[Enter] make_iterator_from_member") );
    ActiveXMember **axo=static_cast<ActiveXMember**>(
        luaL_checkudata(L,1,"ActiveXMember") );
    DBG( puts("[Step1] make_iterator_from_member") );
    if( axo == NULL || !(**axo).ok() ){
        set_nyaos_error(L,"Invalid Object. Expected <ActiveXMember>");
        lua_pushnil(L);
        lua_pushstring(L,"Invalid Object. Expected <ActiveXMember>");
        DBG( puts("[Leave] make_iterator_from_member (failure)") );
        return 2;
    }
    DBG( puts("[Step2] make_iterator_from_member") );

    // first return value : iterator-function
    lua_pushcfunction(L,next_iterator);

    DBG( puts("[Step3] make_iterator_from_member") );
    // second return value : object 
    ActiveXIterator **axi=
        static_cast<ActiveXIterator**>(
                lua_newuserdata( L , sizeof(ActiveXIterator*) ));
    DBG( puts("[Step4] make_iterator_from_member") );
    *axi = new ActiveXIterator((**axo).instance());
    DBG( puts("[Step5] make_iterator_from_member") );
    luaL_newmetatable(L,"ActiveXIterator");
    lua_pushcfunction(L,destroy_iterator);
    lua_setfield(L,-2,"__gc");
    lua_setmetatable(L,-2);
    DBG( puts("[Leave] make_iterator_from_member (success)") );
    return 2;
}
