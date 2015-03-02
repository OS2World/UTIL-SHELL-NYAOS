#include <stdlib.h>
#include "nnstring.h"
#include "shell.h"

#ifdef NYACUS
#  include <windows.h>
#  include <shellapi.h>
#
#  define OPEN_DEFAULT  ""
#  define OPEN_EXPLORE  "explore"
#  define OPEN_PROPERTY "properties"
#
#else
#  include <os2.h>
#
#  define OPEN_DEFAULT  "DEFAULT"
#  define OPEN_EXPLORE  "TREE"
#  define OPEN_PROPERTY "SETTINGS"
#
#endif

static void the_open( const char *fname , const char *action )
{
    char path[ FILENAME_MAX ];

    NnString fn( fname );
    fn.dequote();

#ifdef NYACUS
    fn.slash2yen();
    _fullpath( path , fn.chars() , sizeof(path) );

    int err=(int)(INT_PTR)ShellExecute(NULL,action,path,NULL,NULL,SW_SHOW );
    if( err < 32 ){
	conErr << fn << ": can't open the object as " << action 
	    << " (" << err << ").\n";
    }
#else /* NYAOS-II */
    NnString setup_string;
    setup_string << "OPEN=" << action;
    setup_string.upcase();

    // emx/gcc の _fullpath はbackslashをslashにかえてしまう.
    _fullpath( path , fn.chars() , sizeof(path) );
    fn = path;
    fn.slash2yen();

    HOBJECT hObject=WinQueryObject( (PSZ)fn.chars() );
    WinSetObjectData( hObject , (PCSZ) setup_string.chars() );
    // to active the object on the desktop.
    // WinSetObjectData( hObject , (PCSZ) setup_string.chars() );
#endif
}

int cmd_open( NyadosShell &shell , const NnString &argv )
{
    int count=0;
    NnString action(OPEN_DEFAULT);

    NnString arg1,left;
    for( argv.splitTo(arg1,left) ; !arg1.empty() ; left.splitTo(arg1,left) ){
	if( arg1.at(0) == '-' ){
	    switch( arg1.at(1) ){
	    case 'p':case 'P':
		action=OPEN_PROPERTY;
		break;
	    case 'o':case 'O':
		action=OPEN_DEFAULT;
		break;
	    case 'e':case 'E':
		action=OPEN_EXPLORE;
		break;
	    default:
		conErr << arg1 << ": unknown option.\n";
		return 0;
	    }
	}else if( arg1.at(0) == '+' ){
	    action = arg1.chars()+1;
	}else{
	    the_open( arg1.chars() , action.chars() );
	    ++count;
	}
    }
    if( count == 0 )
	the_open( "." , action.chars() );
    return 0;
}

 
/* 参考リンク：
 * http://www.sm.rim.or.jp/~shishido/shelle.html
 * http://www.microsoft.com/japan/msdn/library/default.asp?url=/japan/msdn/library/ja/jpshell/html/_win32_ShellExecute.asp

 * http://www.microsoft.com/japan/msdn/library/default.asp?url=/japan/msdn/library/ja/vclib/html/_crt__fullpath.2c_._wfullpath.asp
 */
