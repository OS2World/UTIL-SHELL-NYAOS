#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef NYACUS
#  include <new>
#  include <windows.h>
#else
#  include <new.h>
#endif

#include "nnstring.h"
#include "getline.h"
#include "ishell.h"
#include "writer.h"
#include "nua.h"
#include "ntcons.h"
#include "source.h"
#include "version.h"

#ifdef NYACUS
int  nya_new_handler(size_t size)
#else
void nya_new_handler()
#endif
{
    fputs("memory allocation error.\n",stderr);
    abort();
#ifdef NYACUS
    return 0;
#endif
}

/* -d デバッグオプション
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	常に 0 (継続)
 */
static int opt_d( int , char **argv , int &i )
{
    if( argv[i][2] == '\0' ){
	properties.put("debug",new NnString() );
    }else{
	size_t len=strcspn(argv[i]+2,"=");
	NnString var(argv[i]+2,len);
	if( argv[i][2+len] == '\0' ){
	    properties.put( var , new NnString() );
	}else{
	    properties.put( var , new NnString(argv[i]+2+len+1));
	}
    }
    return 0;
}
/* -r _nya 指定オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_r( int argc , char **argv, int &i )
{
    if( i+1 >= argc ){
	conErr << "-r : needs a file name.\n";
	return 2;
    }
    if( access( argv[i+1] , 0 ) != 0 ){
	conErr << argv[++i] << ": no such file.\n";
	return 2;
    }
    rcfname = argv[++i];
    return 0;
}
/* -e 1行コマンド用オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_e( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << "-e needs commandline.\n";
	return 2;
    }
    OneLineShell sh( argv[++i] );
    sh.mainloop();
    return 1;
}

/* -E 1行Luaコマンド用オプション処理
 * -F ファイル読み込みオプション処理
 */
static int opt_EF( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << "option needs commandline.\n";
	return 2;
    }
    NyaosLua L;
    int n=0;

    if( argv[i][0] == '-' && argv[i][1] == 'E' ){
        if( luaL_loadstring( L , argv[++i] ) )
            goto errpt;
    }else{
        if( luaL_loadfile( L , argv[++i] ) )
            goto errpt;
    }

    lua_newtable(L);
    while( ++i < argc ){
        lua_pushinteger(L,++n);
        lua_pushstring(L,argv[i]);
        lua_settable(L,-3);
    }
    lua_setglobal(L,"arg");

    if( lua_pcall( L , 0 , 0 , 0 ) )
        goto errpt;

    lua_settop(L,0);
    return 1;
  errpt:
    const char *msg = lua_tostring( L , -1 );
    fputs(msg,stderr);
    putc('\n',stderr);
    return 2;
}

/* -f スクリプト実行用オプション処理
 *	argc , argv : main の引数
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_f( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << argv[0] << " -f : no script filename.\n";
	return 2;
    }
    if( access(argv[i+1],0 ) != 0 ){
	conErr << argv[i+1] << ": no such file or command.\n";
	return 2;
    }
    NnString path(argv[i+1]);
    if( path.endsWith( ".lua" ) || path.endsWith(".luac") || path.endsWith(".nua") ){
        return opt_EF(argc,argv,i);
    }

    ScriptShell scrShell( path.chars() );

    for( int j=i+1 ; j < argc ; j++ )
	scrShell.addArgv( *new NnString(argv[j]) );
    int rc=1;
    if( argv[i][2] == 'x' ){
	NnString line;
	while(   (rc=scrShell.readline(line)) >= 0
	      && (strstr(line.chars(),"#!")  == NULL
	      || strstr(line.chars(),"nya") == NULL ) ){
	    // no operation.
	}
    }
    if( rc >= 0 )
	scrShell.mainloop();
    return 1;
}

#ifdef NYACUS
/* -t コンソールの直接のコントロールを抑制し、
 *    ANSIエスケープシーケンスをそのまま出力する
 *	i : 現在読み取り中の argv の添字
 * return
 *	0 : 継続 
 *	not 0 : NYA*S を終了させる(mainの戻り値+1を返す)
 */
static int opt_t( int , char ** , int & )
{
    delete conOut_; conOut_ = new RawWriter(1);
    delete conErr_; conErr_ = new RawWriter(2);

    return 0;
}
#endif

static void goodbye()
{
    NyaosLua L("goodbye");
    if( L != NULL ){
        if( lua_isfunction(L,-1) ){
            if( lua_pcall(L,0,0,0) != 0 )
                goto errpt;
        }else if( lua_istable(L,-1) ){
            NnVector list;
            lua_pushnil(L);
            while( lua_next(L,-2) ){
                lua_pushvalue(L,-2);
                list.append( new NnString( lua_tostring(L,-1) ) );
                lua_pop(L,2);
            }
            list.sort();

            for(int i=0;i<list.size();++i){
                lua_getfield(L,-1,list.const_at(i)->repr() );
                if( lua_pcall(L,0,0,0) != 0 )
                    goto errpt;
            }
        }
    }
    return;
errpt:
    const char *msg = lua_tostring(L,-1);
    conErr << "logoff code nyaos.goodbye() raise error.\n"
           << msg << "\n[Hit any key]\n";
    (void)Console::getkey();
}

void set_nyaos_argv_version( int argc , char **argv )
{
    NyaosLua L(NULL);
    assert( L.ok() );
    lua_newtable(L);

    /* nyaos.argv[0] */
    lua_pushinteger(L,0);
    char me[ FILENAME_MAX ];
#ifdef __EMX__
    _execname( me , sizeof(me) );
#else
    GetModuleFileName( NULL , me , sizeof(me) );
#endif
    lua_pushstring(L,me);
    lua_settable(L,-3);
    for(int i=1;i<argc;i++){
        /* nyaos.argv[1]... */
        lua_pushinteger(L,i); /* [3] */
        lua_pushstring(L,argv[i]); /* [4] */
        lua_settable(L,-3);
    }
    /* argv を nyaos のフィールドに登録する */
    lua_setfield(L,-2,"argv");
    lua_pushstring(L,VER);
    lua_setfield(L,-2,"version");
}

static void load_history( History *hisObj )
{
    NnString *histfn=(NnString*)properties.get("savehist");
    if( histfn != NULL && hisObj != NULL ){
	FileReader fr( histfn->chars() );
	if( !fr.eof() ){
            hisObj->read( fr );
	}
    }
}

static void save_history( History *hisObj )
{
    NnString *histfn=(NnString*)properties.get("savehist");
    if( histfn != NULL && hisObj != NULL ){
        int histfrom=0;
        NnString *histsizestr=(NnString*)properties.get("histfilesize");
        if( histsizestr != NULL ){
	    int histsize=atoi(histsizestr->chars());
	    if( histsize !=0 && (histfrom=hisObj->size() - histsize)<0 )
		histfrom = 0;
        }
        /* 強制終了時、スタックトップに空ヒストリが入っているので
         * 削除しておく */
        History1 *top=hisObj->top();
        if( top != NULL && top->body().empty() ){
            hisObj->pop();
        }

        {
            FileReader fr( histfn->chars() );
            if( ! fr.eof() ){
                History hisObj2;
                History newHisObj;
                hisObj2.read( fr );

                int i=0, j=0;
                int size=hisObj->size(), size2=hisObj2.size();

                /* 先頭の一致部分を読み飛ばす */
                while(i<size && j<size2){
                    if((*hisObj)[i]->compare(*hisObj2[j]) == 0){
                        i++; j++;
                    }else{
                        break;
                    }
                }
                int startIndex=i;

                /* マージする */
                while(i<size && j<size2){
                    History1 *history=(*hisObj)[i];
                    History1 *history2=hisObj2[j];

                    if(history->compare(*history2) == 0){
                        newHisObj.append(new History1(*history));
                        i++; j++;
                    }else if(history->stamp().compare(history2->stamp()) >= 0){
                        newHisObj.append(new History1(*history2));
                        j++;
                    }else{
                        newHisObj.append(new History1(*history));
                        i++;
                    }
                }
                for( ; i<size ; i++ )
                    newHisObj.append(new History1(*(*hisObj)[i]));
                for( ; j<size2 ; j++ )
                    newHisObj.append(new History1(*hisObj2[j]));

                /* newHisObjからhisObjにコピーする */
                for(int k=startIndex ; k<size ; k++ )
                    hisObj->drop();
                for(int k=0 ; k < newHisObj.size() ; k++ )
                    hisObj->append(newHisObj[k]);
                while(newHisObj.size()>0)
                    newHisObj.pop(); // deleteはしない
            }
        }

        FileWriter fw( histfn->chars() , "w" );
        if( fw.ok() ){
            History1 his1;
            for(int i=histfrom ; i<hisObj->size() ; i++ ){
                if( hisObj->get(i,his1) == 0 )
                    fw << his1.stamp() << ' ' << his1.body() << '\n';
            }
        }
    }
}

static History *globalHistoryObject=NULL;

#ifdef NYACUS

static BOOL WINAPI HandleRoutine( DWORD dwCtrlType )
{
    switch( dwCtrlType ){
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if( globalHistoryObject != NULL )
            save_history( globalHistoryObject );
        goodbye();
        ExitProcess(0);
        break;
    default:
        break;
    }
    return FALSE;
}
#endif

int main( int argc, char **argv )
{
    init_dbcs_table();
    // set_new_handler(nya_new_handler);
    properties.put("nyatype",new NnString("NYAOS3K") );
    setvbuf( stdout , NULL , _IONBF , 0 );
    setvbuf( stderr , NULL , _IONBF , 0 );

    set_nyaos_argv_version(argc,argv);

    NnVector nnargv;
    for(int i=1;i<argc;i++){
        if( argv[i][0] == '-' ){
            int rv;
            switch( argv[i][1] ){
            case 'D': rv=opt_d(argc,argv,i); break;
            case 'r': rv=opt_r(argc,argv,i); break;
            case 'f': rv=opt_f(argc,argv,i); break;
            case 'e': rv=opt_e(argc,argv,i); break;
            case 'F':
            case 'E': rv=opt_EF(argc,argv,i); break;
#ifdef NYACUS
            case 't': rv=opt_t(argc,argv,i); break;
#endif
            default:
                rv = 2;
                conErr << argv[0] << " : -" 
                    << (char)argv[1][1] << ": No such option\n";
                break;
            }
            if( rv != 0 )
                return rv-1;
        }else{
            NnString *S0=new NnString(argv[i]);
            if( S0->endsWith(".lua") || S0->endsWith(".luac") ||
                    S0->endsWith(".nua") || S0->endsWith(".ny") )
            {
                delete S0;
                --i;
                return opt_f(argc,argv,i);
            }
            nnargv.append( S0 );
        }
    }

    if( isatty(fileno(stdin)) ){
        conOut << 
            "Nihongo Yet Another Open Shell "VER
            " (c) 2001-13 by HAYAMA,Kaoru\n";
        if( properties.get("debug") != NULL ){
            conOut << "This version is built on " __DATE__ " " __TIME__ "\n";
        }
    }

    NnDir::set_default_special_folder();

    /* _nya を実行する */
    if( rcfname.empty() ){
#ifdef NYACUS
        char execname[ FILENAME_MAX ];

        if( GetModuleFileName( NULL , execname , sizeof(execname)-1 ) > 0 ){
            seek_and_call_rc(execname,nnargv);
        }else{
#endif
            seek_and_call_rc(argv[0],nnargv);
#ifdef NYACUS
        }
#endif
    }else{
        callrc(rcfname,nnargv);
    }

    signal( SIGINT , SIG_IGN );
#ifdef __EMX__
    signal( SIGPIPE , SIG_IGN );
#endif

    if( isatty(fileno(stdin)) ){
        /* DOS窓からの入力に従って実行する */
        InteractiveShell shell;

#ifdef NYACUS
        SetConsoleCtrlHandler( HandleRoutine , TRUE );
#endif
        globalHistoryObject = shell.getHistoryObject();
        load_history( globalHistoryObject );
        shell.mainloop();
        save_history( globalHistoryObject );
        globalHistoryObject = NULL;
    }else{
        ScriptShell shell(new StreamReader(stdin));
        shell.mainloop();
    }
    goodbye();
    return 0;
}
