#include <ctype.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMX__
#  include <fcntl.h>
#  define INCL_DOSSESMGR
#  define INCL_DOSERRORS
#  define INCL_DOSFILEMGR
#  define INCL_DOSPROCESS
#  include <os2.h>
#else
#  include <fcntl.h>
#  include "windows.h"
#endif

#include "nndir.h"
#include "nnhash.h"
#include "nnstring.h"
#include "nnvector.h"
#include "shell.h"
#include "writer.h"
#include "mysystem.h"
#include "nua.h"

//#define DBG(s) s
#define DBG(s)

typedef enum mysystem_process_e {
    MYP_WAIT ,
    MYP_NOWAIT , 
    MYP_PIPE ,
} mysystem_process_t;

typedef union mysystem_result_u {
#ifdef __EMX__
    int rc;
    int phandle;
    int pid;
#else
    DWORD     rc;
    phandle_t phandle;
    DWORD     pid;
#endif
} mysystem_result_t ; 

extern int which( const char *nm, NnString &which );

int mkpipeline( int pipefd[] )
{
#ifdef __EMX__
    if( _pipe(pipefd) != 0 ){
#else
    if( _pipe(pipefd,1024,_O_TEXT | _O_NOINHERIT ) != 0 ){
#endif
        return -1;
    }
#ifdef __EMX__
    fcntl( pipefd[0], F_SETFD, FD_CLOEXEC );
    fcntl( pipefd[1], F_SETFD, FD_CLOEXEC );
#endif 
    return 0;
}

static void lua_filter( NnString &cmdname , NnString &cmdline )
{
    LuaHook L("filter3");
    while( L.next() ){
        lua_pushstring( L,cmdname.chars() );
        lua_pushstring( L,cmdline.chars() );
        if( lua_pcall(L,2,2,0) != 0 ){
            return;
        }
        if( lua_isstring(L,-1) && lua_isstring(L,-2) ){
            cmdname = lua_tostring(L,-2);
            cmdline = lua_tostring(L,-1);
        }
        lua_pop(L,2);
    }
}

static int remove_quote_and_caret(int c)
{
    return (c=='^' || c=='"') ? '\0' : c;
}

/* 代替spawn。spawnのインターフェイスを NNライブラリに適した形で提供する。
 *      args - パラメータ
 *      wait - MYP_WAIT   : プロセス終了を待つ
 *             MYP_NOWAIT or MYP_PIPE : プロセス終了を待たない
 *      result - プロセスの戻り値 or ID or ハンドルが格納される
 * return
 *      0  - 成功
 *      -1 - 失敗
 */
static int mySpawn( 
        const NnVector     &args ,
        mysystem_process_t wait  ,
        mysystem_result_t  &result )
{
    /* コマンド名から、ダブルクォートとキャレットを除く */
    NnString cmdname(args.const_at(0)->repr());
    cmdname.filter(remove_quote_and_caret);

    NnString fullpath_cmdname;
    if( which( cmdname.chars() , fullpath_cmdname ) != 0 ){
        errno = ENOENT;
        return -1;
    }

    NnString cmdline;
    const char *comspec = getenv("COMSPEC");
    const char *tailchar="";

    if( comspec != NULL && (
                fullpath_cmdname.iendsWith(".cmd") ||
                fullpath_cmdname.iendsWith(".bat") ) )
    {
#ifdef NYACUS
        cmdline << comspec << " /S /C \"";
#else
        cmdline << comspec << " /C \"";
#endif
        if( fullpath_cmdname.findOf(" ",0) >= 0 )
            cmdline << '\"' << fullpath_cmdname << "\"";
        else
            cmdline << fullpath_cmdname;
        fullpath_cmdname = cmdname = comspec;
        tailchar = "\"";
    }else{
        cmdline << args.const_at(0)->repr();
    }

    for(int i=1 ; i < args.size() ; ++i ){
        cmdline << ' ' <<args.const_at(i)->repr();
    }
    cmdline.trim();
    cmdline << tailchar;

    DBG( printf("mySpawn('%s')\n",cmdline.chars()) );

    lua_filter( fullpath_cmdname , cmdline );

#ifdef __EMX__
    unsigned long type=0;
    int rc=DosQueryAppType( (unsigned char *)cmdname.chars() , &type );
    if( wait == MYP_WAIT && rc ==0  &&  (type & 7 )== FAPPTYP_WINDOWAPI){
        const char **argv
            =(const char**)malloc( sizeof(const char *)*(args.size() + 1) );
        for(int i=0 ; i<args.size() ; i++)
            argv[i] = ((NnString*)args.const_at(i))->chars();
        argv[ args.size() ] = NULL;

        result.rc = result.pid = spawnvp( P_PM ,
                (char*)NnDir::long2short(cmdname.chars()) , (char**)argv );
        free( argv );
    }else{
        char errbuffer[ 256 ];

        char *zero=strchr(cmdline.chars(),' ');
        if( zero != NULL )
            *zero = '\0';

        RESULTCODES resultcodes;

        if( DosExecPgm(
                    errbuffer ,
                    sizeof(errbuffer) ,
                    wait == MYP_WAIT ? EXEC_SYNC : wait == MYP_NOWAIT ? EXEC_ASYNC : EXEC_ASYNCRESULT ,
                    (const unsigned char *)cmdline.chars() ,
                    0 ,
                    &resultcodes ,
                    (const unsigned char *)fullpath_cmdname.chars() ) )
        {
            errno = ENOEXEC;
            return -1;
        }
        if( wait == MYP_WAIT ){
            result.rc = resultcodes.codeResult ;
            return resultcodes.codeTerminate != 0 ;
        }else{
            result.pid = resultcodes.codeTerminate;
            return 0;
        }
    }
#else
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset(&si,0,sizeof(si));
    memset(&pi,0,sizeof(pi));

    if( ! CreateProcess(const_cast<CHAR*>(fullpath_cmdname.chars()) ,
                        const_cast<CHAR*>(cmdline.chars()) ,
                        NULL, 
                        NULL,
                        TRUE, /* 親プロセスの情報を継承するか？ */
                        0,    /* 作成フラグ？ */
                        NULL, /* 環境変数のポインタ */
                        NULL, /* カレントディレクトリ */
                        &si,
                        &pi) )
    {
        errno = ENOEXEC;
        return -1;
    }
    switch( wait ){
    case MYP_WAIT: /* プロセス終了を待つ */
        WaitForSingleObject(pi.hProcess,INFINITE); 
        GetExitCodeProcess(pi.hProcess,&result.rc);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        break;
    case MYP_NOWAIT: /* プロセス終了を待たない */
        result.pid = pi.dwProcessId ;
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        break;
    case MYP_PIPE: /* ここでは待たないが、呼び出し元で待つ */
        result.phandle = pi.hProcess ;
        CloseHandle(pi.hThread);
        break;
    default:
        abort();
        break;
    }
#endif
    return 0;
}

/* リダイレクト部分を解析する。
 *    - >> であれば、アペンドモードであることを検出する
 * 
 *  cmdline - コマンドラインへのポインタ
 *            最初の「>」の「上」にあることを想定
 *  redirect - 読み取り結果を格納するオブジェクト
 *
 * return
 *    0  : 成功
 *    -1 : 失敗
 *  */
static int parseRedirect( const char *&cmdline , Redirect &redirect )
{
    const char *mode="w";
    if( *cmdline == '>' ){
	mode = "a";
	++cmdline;
    }
    NnString path;
    NyadosShell::readNextWord( cmdline , path );
    if( redirect.switchTo( path , mode ) != 0 ){
        return -1;
    }
    return 0;
}

/* リダイレクトの > の右側移行の処理を行う.
 *	sp : > の次の位置
 *	redirect : リダイレクトオブジェクト
 * return
 *      0  : 成功
 *      -1 : 失敗
 */
static int after_lessthan( const char *&sp , Redirect &redirect )
{
    if( sp[0] == '&'  &&  isdigit(sp[1]) ){
	/* n>&m */
        if( redirect.set( sp[1]-'0' ) != 0 )
            return -1;
	sp += 2;
    }else if( sp[0] == '&' && sp[1] == '-' ){
	/* n>&- */
        if( redirect.switchTo( "nul", "w" ) != 0 )
            return -1;
    }else{
	/* n> ファイル名 */
        if( parseRedirect( sp , redirect ) != 0 )
            return -1;
    }
    return 0;
}

/* コマンドラインを | で分割して、ベクターに各コマンドを代入する
 *	cmdline - コマンドライン
 *	vector - 各要素が入る配列
 */
static void devidePipes( const char *cmdline , NnVector &vector )
{
    NnString one,left(cmdline);
    while( left.splitTo(one,left,"|") , !one.empty() )
	vector.append( one.clone() );
}

/* リダイレクト処理付き１コマンド処理ルーチン
 *	cmdline - コマンド
 *	wait - MYP_WAIT / MYP_NOWAIT / MYP_PIPE
 *	result - コマンドの実行結果を格納する先
 *	error_fname - エラーになった時に表示するコマンド名を入れる器
 * return
 *        0 : 成功
 *       -1 : リダイレクト失敗.
 */
static int do_one_command( 
        const char *cmdline ,
        mysystem_process_t wait ,
        mysystem_result_t  &result ,
        NnString &error_fname )
{
    DBG( printf("do_one_command('%s',...)\n",cmdline) );

    Redirect redirect0(0);
    Redirect redirect1(1);
    Redirect redirect2(2);
    NnString execstr;

    int quote=0;
    bool escape=false;
    while( *cmdline != '\0' ){
	if( *cmdline == '"' )
	    quote ^= 1;

        if( quote==0 && cmdline[0]=='<' && !escape ){
            ++cmdline;
            NnString path;
            NyadosShell::readNextWord( cmdline , path );
            if( redirect0.switchTo( path , "r" ) != 0 ){
                return -1;
            }
        }else if( quote==0 && cmdline[0]=='>' && !escape ){
	    ++cmdline;
            if( parseRedirect( cmdline , redirect1 ) != 0 )
                return -1;
	}else if( quote==0 && cmdline[0]=='1' && cmdline[1]=='>' && !escape ){
	    cmdline += 2;
	    if( after_lessthan( cmdline , redirect1 ) != 0 )
                return -1;
	}else if( quote==0 && cmdline[0]=='2' && cmdline[1]=='>' && !escape ){
	    cmdline += 2;
            if( after_lessthan( cmdline , redirect2 ) != 0 )
                return -1;
        }else if( !escape && quote==0 && *cmdline == '^' ){
            escape = true;
            ++cmdline;
	}else{
            escape = false;
            if( isKanji(*cmdline) )
                execstr << *cmdline++;
	    execstr << *cmdline++;
	}
    }
    NnVector args;

    execstr.splitTo( args );
    int rc=mySpawn( args , wait , result );
    if( rc != 0 ){
        error_fname = args.const_at(0)->repr();
    }
    return rc;
}

/* system代替関数（パイプライン処理）
 *	cmdline - コマンド文字列	
 */
static int do_pipeline(
        const char *cmdline ,
        mysystem_process_t wait ,
        mysystem_result_t &result )
{
    if( cmdline[0] == '\0' )
	return 0;
    int pipefd0 = -1;
    int save0 = -1;
    NnVector pipeSet;
    int rc=0;

    devidePipes( cmdline , pipeSet );
    for( int i=0 ; i < pipeSet.size() ; ++i ){
	errno = 0;
        NnString error_fname;

        if( pipefd0 != -1 ){
	    // パイプラインが既に作られている場合、
	    // 入力側を標準入力へ張る必要がある
            if( save0 == -1  )
		save0 = dup( 0 );
            dup2( pipefd0 , 0 );
	    ::close( pipefd0 );
            pipefd0 = -1;
        }
        if( i < pipeSet.size() - 1 ){
	    /* パイプラインの末尾でないコマンドの場合、
	     * 標準出力の先を、パイプの一方にする必要がある
	     */
            int handles[2];

            int save1=dup(1);
            if( mkpipeline( handles ) != 0 )
                return -1;
            dup2( handles[1] , 1 );
	    ::close( handles[1] );
            pipefd0 = handles[0];

            rc = do_one_command( ((NnString*)pipeSet.at(i))->chars(),
                            MYP_NOWAIT , result , error_fname );
            dup2( save1 , 1 );
            ::close( save1 );
        }else{
            rc = do_one_command( ((NnString*)pipeSet.at(i))->chars(),
                            wait , result , error_fname );
        }
	if( rc != 0 ){
	    if( errno != 0 ){
                int save_errno=errno;
                if( ! error_fname.empty() ){
                    conErr << error_fname << ": ";
                }
                conErr << strerror( save_errno ) << '\n';
	    }else{
                conErr << "unexpected error.\n";
            }
            goto exit;
	}
    }
  exit:
    if( save0 >= 0 ){
        dup2( save0 , 0 );
    }
    return rc;
}

int mySystem( const char *cmdline , int wait )
{
    mysystem_result_t result;
    DBG( printf("mySystem('%s')\n",cmdline) );
    if( wait ){
        int rc = do_pipeline( cmdline , MYP_WAIT , result );
        return rc ? rc : result.rc ;
    }else{
        int rc = do_pipeline( cmdline , MYP_NOWAIT , result );
        return rc ? rc : result.pid ;
    }

}

/* CMD.EXE を使わない popen 関数相当
 *    cmdname : コマンド名
 *    mode : "r" or "w"
 *    phandle : プロセスハンドル
 * return
 *    -1 : パイプ生成失敗
 *    -2 : spawn 失敗
 *    others : ファイルハンドル
 */
int myPopen(const char *cmdline , const char *mode , phandle_t *phandle )
{
    int pipefd[2];
    int backfd;
    int d=(mode[0]=='r' ? 1 : 0 );
    mysystem_result_t result;

    if( mkpipeline( pipefd ) != 0 )
        return -1;

    backfd = dup(d);
    ::_dup2( pipefd[d] , d );
    ::_close( pipefd[d] );
    int rc=do_pipeline( cmdline , MYP_PIPE , result );
    ::_dup2( backfd , d );
    ::_close( backfd );
    if( rc != 0 ){
        return -1;
    }
    if( phandle != NULL )
        *phandle = result.phandle ;

    return pipefd[ d ? 0 : 1 ];
}

void myPclose(int fd, phandle_t phandle )
{
    if( fd >= 0 ){
        ::_close(fd);
        if( phandle ){
#ifdef __EMX__
            // ::waitpid(phandle,NULL,0);
            RESULTCODES resultcodes;
            PID pidChild;

            int rc=DosWaitChild(
                    DCWA_PROCESSTREE , 
                    DCWW_WAIT ,
                    &resultcodes ,
                    &pidChild ,
                    phandle  );
            if( rc ) {
                // printf("DosKillProcess(%d)\n",rc);
                DosKillProcess( 0 , phandle );
            }
#else
            WaitForSingleObject( phandle , INFINITE); 
            CloseHandle( phandle );
#endif
            phandle = 0;
        }
    }
}
