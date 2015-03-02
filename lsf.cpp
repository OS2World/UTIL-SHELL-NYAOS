#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "shell.h"
#include "nnvector.h"
#include "nndir.h"
#include "getline.h" /* DosShell.executableSuffix のためのみ */
#include "nnhash.h"
#ifdef NYACUS
#  include "ntcons.h" 
#  include <windows.h>
#  include <wincon.h>
#endif

#include "writer.h" /* for test  */

enum {
    OPT_LONG  = 0x1 ,
    OPT_ALL   = 0x2 ,
    OPT_ONE   = 0x4 ,
    OPT_REC   = 0x8 ,
    OPT_COLOR = 0x10 , 
    OPT_ALL2  = 0x20 , // -A
    OPT_HUMAN = 0x40 ,
    OPT_SI    = 0x80 ,
};

# define LS_LEFT  "\033["
# define LS_RIGHT "m"

static NnString ls_normal_file ;
static NnString ls_directory ;
static NnString ls_system_file ;
static NnString ls_hidden_file ;
static NnString ls_executable_file ;
static NnString ls_read_only_file ;
static NnString ls_end_code ;

#define C2(x,y) (x<<8 | y)

volatile int ctrl_c=0;

/* CTRL-C が押された時のハンドル */
#ifdef __EMX__
static void handle_ctrl_c(int sig)
{
    ctrl_c = 1;
    signal(sig,SIG_ACK);
}
#else /* NYACUS */
static BOOL WINAPI handle_ctrl_c(DWORD ctrlChar)
{
    if( CTRL_C_EVENT == ctrlChar ){
        ctrl_c = 1;
        return TRUE;
    }else{
        return FALSE;
    }
}
#endif


/* 環境変数 LS_COLORS の内容を読み取って、ls の色設定に反映させる.
 *	err : エラーの出力先
 * return
 *	0 : 成功
 *	1 : エラー
 */
static int env_to_color( Writer &err )
{
    ls_normal_file      = LS_LEFT "1;37" LS_RIGHT ; /* 白 */
    ls_directory        = LS_LEFT "1;32" LS_RIGHT ; /* 緑 */
    ls_system_file      = LS_LEFT "0;31" LS_RIGHT ; /* 暗い赤 */
    ls_hidden_file      = LS_LEFT "0;34" LS_RIGHT ; /* 暗い青 */
    ls_executable_file  = LS_LEFT "1;35" LS_RIGHT ; /* 紫 */
    ls_read_only_file   = LS_LEFT "1;33" LS_RIGHT ; /* 黄 */
    ls_end_code         = LS_LEFT "0"    LS_RIGHT ; /* 灰色 */

    NnString env,one;

    NnString *opt_ = dynamic_cast<NnString*>( properties.get("ls_colors") );
    if( opt_ != NULL ){
        env = *opt_;
    }else{
        const char *env_=getenv("LS_COLORS");
        if( env_ == NULL ) return 0;
        env = env_;
    }

    env.dequote();
    NnString left,right;
    while( env.splitTo(one,env,":") , one.length() > 0 ){
	one.splitTo(left,right,"=");
	if( left.length() != 2 || right.length() <= 0 )
	    goto errpt;
	
	NnString value;
	value << LS_LEFT << right << LS_RIGHT ;
	
	switch( C2(left.at(0),left.at(1) ) ){
	    case C2('f','i'): ls_normal_file     = value ; break ;
	    case C2('d','i'): ls_directory       = value ; break ;
	    case C2('s','y'): ls_system_file     = value ; break ;
	    case C2('r','o'): ls_read_only_file  = value ; break ;
	    case C2('h','i'): ls_hidden_file     = value ; break ;
	    case C2('e','x'): ls_executable_file = value ; break ;
	    case C2('e','c'): ls_end_code        = value ; break ;
	    default:       goto errpt;
	}
    }
    return 0;

  errpt:
    err << "LS_COLORS: syntax error " << left << '=' << right << '\n';
    return 1;
}


static int isExecutable( const NnString &path )
{
    const char *env=getenv("PATHEXT");
    if( env != NULL ){
	/* 環境変数 PATHENV が定義されているとき */
	NnString env1(env);
	NnString suffix;

	while( env1.splitTo(suffix,env1,";") , !suffix.empty() ){
	    if( path.iendsWith(suffix) )
		return 1;
	}
    }else{
	/* 未定義のとき */
	if( path.iendsWith(".EXE") ||
	    path.iendsWith(".COM") ||
	    path.iendsWith(".BAT") )
	{
	    return 1;
	}
    }
    /* suffix 文で定義された拡張子を検索 */
    int lastdot=path.findLastOf("/\\.");
    if( lastdot == -1  ||  path.at(lastdot) != '.' )
	return 0; /* 拡張子なし */

    if( DosShell::executableSuffix.get(path.chars()+lastdot+1) != NULL )
	return 1;
    else
	return 0;
}

static const char *attr2color( const NnFileStat &stat )
{
    if( stat.isHidden() ){
	return ls_hidden_file.chars();
    }else if ( stat.isSystem() ){
	return ls_system_file.chars() ;
    }else if( stat.isDir() ){
	return ls_directory.chars() ;
    }else if ( stat.isReadOnly() ){
	return ls_read_only_file.chars() ;
    }else if( isExecutable( stat.name() ) ){
	return ls_executable_file.chars() ;
    }else{
	return ls_normal_file.chars() ;
    }
}


static int maxlength( const NnVector &list )
{
    int max=1;
    for(int i=0;i<list.size();++i){
	const NnFileStat *file1=(const NnFileStat*)list.const_at(i);
	int len=file1->name().length();

	if( file1->isDir() || isExecutable(file1->name()) )
	    ++len;

	if( len > max )
	    max = len;
    }
    return max;
}

/* sprintf が long long 型をサポートするのは、C99 以降であるため、
 * 自前で、文字列化関数を作らねばならなかった
 */
void filesize2str( filesize_t n , NnString &buffer )
{
    if( n > 9u ){
        filesize2str( n/10u , buffer );
        n %= 10;
    }
    buffer << "0123456789"[ n ];
}

/* ファイルの一覧を格子状に出力する.
 *	list   (i) NnFileStat の配列
 *	option (i) オプション
 *	out    (i) 出力先
 */
static void dir_files( const NnVector &list , int option , Writer &out )
{
    static const char unit_list[]={
        0, 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'
    };
    if( option & OPT_LONG ){
	/*** ロングフォーマット ***/
	if( option & OPT_COLOR )
	    out << ls_end_code ;
	
	NnString *filesize_list=new NnString[ list.size() ];
	int filesize_max=1;
	for(int i=0 ; i<list.size() ; ++i ){
	    const NnFileStat *st=(const NnFileStat*)list.const_at(i);

	    NnString &filesize_str = filesize_list[ i ];
	    if( (option & OPT_HUMAN) || (option & OPT_SI) ){
		char buffer[ 4 ];
		int base = ( (option & OPT_SI) ? 1000 : 1024 );
		int u;
		double size=st->size();
		for( u=0 ; size>=1000 ; ++u )
		    size /= base;
		if( size == 0 || size >= 10 ){
		    sprintf( buffer , "%3.f" , size );
		}else{
		    sprintf( buffer , "%.1f" , size );
		}
		filesize_str << buffer << unit_list[ u ];
	    }else {
		filesize2str( st->size() , filesize_str );
	    }

	    int len=filesize_str.length();
	    if( len > filesize_max )
		filesize_max = len;
	}

	for(int i=0 ; i<list.size() ; ++i ){
	    char buffer[ 1024 ];
	    const NnFileStat *st=(const NnFileStat*)list.const_at(i);
	    int canExe=isExecutable( st->name() ) || st->isDir() ;

	    out << (st->isDir() ? 'd' : '-' )
		<< 'r'
		<< (st->isReadOnly() ? '-' : 'w' )
		<< (canExe ? 'x' : '-' )
		<< ' ';

	    sprintf( buffer , "%*s %04d-%02d-%02d %02d:%02d ",
		    filesize_max ,
		    filesize_list[ i ].chars() ,
		    st->stamp().year ,
		    st->stamp().month ,
		    st->stamp().day ,
		    st->stamp().hour ,
		    st->stamp().minute );
	    out << buffer ;

	    if( option & OPT_COLOR ){
		out << attr2color(*st);
	    }
	    out << st->name();
	    if( option & OPT_COLOR ){
		out << ls_end_code ;
	    }

	    if( st->isDir() ){
		out << '/';
	    } else if( canExe ){
		out << '*';
	    }
	    out << '\n';
	}
	delete[]filesize_list;
    }else if( option & OPT_ONE ){
	for(int i=0 ; i<list.size() ; ++i ){
	    out << ((NnFileStat*)list.const_at(i))->name() << '\n';
	}
    }else{
	/*** 格子状フォーマット ***/
	int max = maxlength(list);
        NnString *env_width=(NnString*)properties.get("width");
        int screen_width=80;
        if( env_width != NULL ){
            screen_width = atoi(env_width->chars());
#ifdef NYACUS
        }else{
            screen_width = Console::getWidth();
#endif
        }
        if( screen_width < 20 || screen_width > 255 ){
            screen_width = 80;
        }
        int n_per_line = (screen_width-1)/(max+1);

	int nlines;
	if( n_per_line <= 0 ){
	    nlines = list.size();
	}else{
	    nlines = (list.size() + n_per_line - 1) / n_per_line ;
	}

	NnString *printline=new NnString[ nlines ];
	for(int i=0 ; i<list.size() ; ++i ){
	    const NnFileStat *st=(const NnFileStat *)list.const_at(i) ;
	    int left = max+1 ;
	    NnString &line = printline[ i % nlines ] ;

	    if( option & OPT_COLOR ){
		 line << attr2color( *st );
	    }
	    line << st->name();
	    left -= st->name().length();
	    if( option & OPT_COLOR ){
		line << ls_end_code ;
	    }
	    if( st->isDir() ){
		line << '/';
		--left;
	    }else if( isExecutable( st->name() ) ){
		line << '*';
		--left;
	    }
	    while( left > 0 ){
		line << ' ';
		--left;
	    }
	}
	for(int j=0 ; j<nlines ; ++j ){
	    out << printline[j].trim() << '\n';
	}
	delete[]printline;
    }
}

/* ソートのためにファイルを比較する関数オブジェクト */
class NnFileComparer : public NnComparer {
public:
    enum NnCompareType {
	COMPARE_WITH_NAME ,
	COMPARE_WITH_TIME ,
	COMPARE_WITH_SIZE ,
    } type ;
private:
    int reverse;
public:
    NnFileComparer() : type(COMPARE_WITH_NAME) , reverse(0){}
    NnFileComparer( int reverse_ , NnCompareType type_ )
	: type(type_),reverse(reverse_){}

    void set_type   ( NnCompareType type_ ){ type = type_ ; }
    void set_reverse( int reverse_ ){ reverse = reverse_ ; }
    int operator()( const NnObject *left , const NnObject *right );
};

/* ファイルのソート順を決める比較関数 */
int NnFileComparer::operator()( const NnObject *left_ ,
			        const NnObject *right_ )
{
    const NnFileStat *left = (const NnFileStat*)left_;
    const NnFileStat *right= (const NnFileStat*)right_;

    if( right == NULL ) return -1;
    if( left  == NULL ) return +1;

    int diff;
    switch( type ){
    default: /* COMPARE_WITH_NAME 含む */
	diff =  left->name().compare( right->name() );
	break;
    case COMPARE_WITH_TIME:
	diff = left->stamp().compare( right->stamp() );
	break;
    case COMPARE_WITH_SIZE:
	diff = right->size() - left->size();
	break;
    }
    return reverse ? -diff : diff ;
}

/* １ディレクトリをリスト表示する.
 *	dirname (i) ディレクトリ名
 *	option  (i) OPT_xxxx の OR からなるオプション
 *      filecmpr(i) ソートでファイルを比較する関数オブジェクト
 *	dirs    (o) (option & OPT_REC)!=0 の時、本配列末尾に
 *                  下位ディレクトリが追記される
 *	out     (o) 出力先
 */
static void dir1( const char *dirname ,
		  int option ,
		  NnFileComparer &filecmpr ,
		  NnVector &dirs ,
		  Writer &out )
{
    NnVector list;

    NnString wildcard;
    if( dirname == NULL  ||  dirname[0] == '\0' ){
	wildcard = "*";
    }else{
	wildcard << dirname << "\\*";
    }

    for( NnDir dir(wildcard) ; dir.more() ; dir.next() ){
	NnFileStat *st=dir.stat();

	/* -R オプションがついているときは、下位フォルダーをリストに加える.*/
	if( (option & OPT_REC) != 0 &&
	    st->isDir() && 
	    !st->name().startsWith(".") )
	{

	    NnString fullpath;
	    if( dirname != NULL && dirname[0] != '\0' )
		fullpath << dirname << '/';
	    fullpath << st->name();
	    dirs.append( new NnFileStat(
			    fullpath ,
			    st->attr() ,
			    st->size() ,
			    st->stamp() )
			);
	}
        if( ( st->name().compare(".") == 0 ||
              st->name().compare("..") == 0 ) && 
             (option & OPT_ALL2 ) != 0 ) 
        {
            delete st;
        }else if( ( st->name().startsWith(".") ||
              st->name().startsWith("_") ||
              st->isHidden() ) && 
            (option & OPT_ALL)==0 )
        {
	    delete st;
	}else{
	    list.append( st );
	}
    }
    list.sort( filecmpr );
    dir_files(list,option,out);
}


int cmd_ls( NyadosShell &shell , const NnString &argv )
{
    NnVector args;
    NnVector files,dirs;
    NnFileComparer filecmpr;
    int option = 0;
    int i;

    env_to_color( conErr );
    ctrl_c = 0;
#ifdef NYACUS
    Console::enable_ctrl_c();
    SetConsoleCtrlHandler( handle_ctrl_c , TRUE );
#else
    signal( SIGINT , handle_ctrl_c );
    signal( SIGPIPE , handle_ctrl_c );
#endif

    /* 出力先が端末の時はカラーをデフォルトとする */
    if( conOut.isatty() ){
        option |= OPT_COLOR;
    }else{
        option |= OPT_ONE;
    }
    
    argv.splitTo( args );

    for(i=0 ; i<args.size() ; ++i ){
	NnString path( *(NnString*)args.at(i) );
	path.dequote();

	if( path.chars()[0] == '-' ){
            if( path.chars()[1] == '-' ){
                const char *p=path.chars()+2;
                if( strcmp(p,"color=always")==0 ||
                    strcmp(p,"color"       )==0 )
                {
                    /* 端末以外に対しても、常にカラー出力する */
                    option |= OPT_COLOR;
                }else if( strcmp(p,"color=auto")==0 ||
                          strcmp(p,"color=tty" )==0 ){
                    /* オプション無しと等価: 何もしない */
                    ;
                }else if( strcmp(p,"color=never")==0 ){
                    /* カラーを抑制する */
                    option &= ~OPT_COLOR;
                }else if( strcmp(p,"si")==0 ){
                    option &=~OPT_HUMAN;
                    option |= OPT_SI;
                }else{
                    conErr << path.chars() << ": not such option.\n";
                    return 0;
                }
            }else{
                for(const char *p=path.chars()+1 ; *p != '\0' ; ++p ){
                    switch( *p ){
                    case 'l': option |= OPT_LONG ; break;
                    case 'A': option |= OPT_ALL2 ;
                    case 'a': option |= OPT_ALL  ; break;
                    case '1': option |= OPT_ONE  ; break;
                    case 'x': option &=~OPT_ONE  ; break;
                    case 'R': option |= OPT_REC  ; break;
                    case 'h':
                        option &=~OPT_SI;
                        option |= OPT_HUMAN;
                        break;
                    case 'r':
                        filecmpr.set_reverse(1) ;
                        break;
                    case 't':
                        filecmpr.set_type( NnFileComparer::COMPARE_WITH_TIME );
                        break;
                    case 'S':
                        filecmpr.set_type( NnFileComparer::COMPARE_WITH_SIZE );
                        break;
                    default :
                        conErr << *p << ": not such option.\n";
                        return 0;
                    }
                }
            }
	}else{
	    NnVector temp;
	    fnexplode( path.chars() , temp );
	    if( temp.size() <= 0 ){
		NnFileStat *st=NnFileStat::stat( path );
		if( st == NULL ){
		    conErr << path << ": not found.\n";
		    return 0;
		}else if( st->isDir() ){
		    dirs.append( st );
		}else{
		    files.append( st );
		}
	    }else{
		for(int i=temp.size() ; i>0 ; --i ){
		    NnFileStat *file1=(NnFileStat*)temp.shift();
		    if( file1->isDir() ){
			dirs.append( file1 );
		    }else{
			files.append( file1 );
		    }
		}
	    }
	}
    }
    if( files.size() <= 0 && dirs.size() <= 0 ){
	dir1(NULL,option,filecmpr,dirs,conOut );
    }else{
	files.sort( filecmpr );
	dir_files( files , option , conOut );
    }

    if( files.size() > 0 && dirs.size() > 0 )
	conOut << '\n';
    
    /* 注意：dirs.size は動的に変わる(関数内部で増える)場合がある */
    for( i=0 ; i < dirs.size() ; ++i ){
        if( ! conOut.ok() ){
            break;
        }
	if( ctrl_c ){
	    conErr << "^C\n";
	    break;
	}
	NnFileStat *st=(NnFileStat*)dirs.at(i);
	NnString name( st->name() );

	if( dirs.size() >= 2 || files.size() > 0 ){
	    conOut << st->name() << ":\n";
	}
	dir1( st->name().chars() , option , filecmpr , dirs , conOut );
	if( i+1 < dirs.size() ){
	    conOut << '\n';
	}
    }
    ctrl_c = 0;
#ifdef NYACUS
    Console::disable_ctrl_c();
    SetConsoleCtrlHandler( handle_ctrl_c , FALSE );
#else
    signal( SIGINT , SIG_IGN );
    signal( SIGPIPE , SIG_IGN );
#endif
    return 0;
}
