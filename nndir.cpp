#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <io.h>

#ifdef __EMX__
#  define INCL_DOSFILEMGR
#  include <os2.h>
#else
#  include <direct.h>
#  include <dir.h>
#endif

#include "nndir.h"

/* Default Special Folder 設定のため */
#ifdef NYACUS 
#    include <windows.h>
#    include <shlobj.h>
#    include <stdio.h>
#endif

int NnTimeStamp::compare( const NnTimeStamp &o ) const
{
    return year   != o.year   ? o.year    - year 
	:  month  != o.month  ? o.month   - month
	:  day    != o.day    ? o.day     - day
	:  hour   != o.hour   ? o.hour    - hour
	:  minute != o.minute ? o.minute  - minute
	                      :  o.second - second ;
}

static void stamp_conv( time_t time1 , NnTimeStamp &stamp_ )
{
    struct tm *tm1;

    tm1 = localtime( &time1 );
    if( tm1 != NULL ){
        stamp_.second = tm1->tm_sec  ;
        stamp_.minute = tm1->tm_min  ;
        stamp_.hour   = tm1->tm_hour ;
        stamp_.day    = tm1->tm_mday ;
        stamp_.month  = tm1->tm_mon + 1;
        stamp_.year   = tm1->tm_year + 1900 ;
    }else{
        stamp_.second = 0;
        stamp_.minute = 0;
        stamp_.hour   = 0;
        stamp_.day    = 0;
        stamp_.month  = 1;
        stamp_.year   = 1972 ;
    }
}

/** 「...\」→「..\..\」 DOS,OS/2 の為 */
void NnDir::extractDots( const char *&sp , NnString &dst )
{
    ++sp;
    for(;;){
	dst << "..";
	if( *++sp != '.' )
	    break;
	dst << '\\';
    }
}


NnDir::~NnDir()
{
    NnDir::findclose();
}
void NnDir::operator++()
{
    status = NnDir::findnext();
}
/* ファイルがあれば、ファイル名文字列オブジェクトへのポインタを返す。
 */
NnObject *NnDir::operator * ()
{
    return status ? 0 : &name_;
}

struct NnDir::Core {
#ifdef __EMX__
    _FILEFINDBUF4 findbuf;
    ULONG findcount;
    unsigned handle;

    Core() : findcount(1),handle(0xFFFFFFFF){}
    const char *name() const { return findbuf.achName; }
    unsigned long attr() const { return findbuf.attrFile; }
    filesize_t size() const { return findbuf.cbFile; }
#else
    WIN32_FIND_DATA wfd;
    HANDLE hfind;
    DWORD attributes;

    Core() : hfind(INVALID_HANDLE_VALUE){} 
    const char *name() const { return wfd.cFileName; }
    unsigned long attr() const { return wfd.dwFileAttributes; }
    filesize_t size() const 
    { return (((filesize_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow) ; }
#endif
    int findfirst( const char* path, unsigned attr);
    int findnext();
    int findclose();

    void stamp( NnTimeStamp &stamp );
};

#ifdef NYACUS
int NnDir::Core::findfirst( const char* path, unsigned attr)
{
    attributes = attr;
    hfind = ::FindFirstFile( path, &wfd);
    if( hfind==INVALID_HANDLE_VALUE){
	return -1;
    }
    return 0;
}
int NnDir::Core::findnext()
{
    if( ::FindNextFile( hfind, &wfd)==FALSE){
	return -1;
    }
    return 0;
}
int NnDir::Core::findclose()
{
    return ::FindClose( hfind );
}
void NnDir::Core::stamp(  NnTimeStamp &stamp_)
{
    FILETIME local;
    SYSTEMTIME s;
    const FILETIME *p = &wfd.ftLastWriteTime ;

    FileTimeToLocalFileTime( p , &local );
    FileTimeToSystemTime( &local , &s );
    stamp_.second = s.wSecond ;
    stamp_.minute = s.wMinute ;
    stamp_.hour   = s.wHour ;
    stamp_.day    = s.wDay ;
    stamp_.month  = s.wMonth ;
    stamp_.year   = s.wYear ;
}
#else
int NnDir::Core::findfirst( const char* path, unsigned attr)
{
    return DosFindFirst( (PUCHAR)path 
			  , (PULONG)&handle 
			  , attr 
			  , &findbuf
			  , sizeof(_FILEFINDBUF4) 
			  , &findcount 
			  , (ULONG)FIL_QUERYEASIZE );
}
int NnDir::Core::findnext()
{
    return DosFindNext(handle,&findbuf,sizeof(findbuf),&findcount);
}
int NnDir::Core::findclose()
{
    return DosFindClose( handle );
}

void NnDir::Core::stamp( NnTimeStamp &stamp_ )
{
    unsigned fdate = *(unsigned short*)&findbuf.fdateLastWrite;
    unsigned ftime = *(unsigned short*)&findbuf.ftimeLastWrite;

    /* 時刻 */
    stamp_.second = ( ftime & 0x1F) * 2 ;      /* 秒:5bit(0..31) */
    stamp_.minute = ( (ftime >> 5 ) & 0x3F );  /* 分:6bit(0..63) */
    stamp_.hour   =  ftime >> 11;              /* 時:5bit */

    /* 日付 */
    stamp_.day    = (fdate & 0x1F);            /* 日:5bit(0..31) */
    stamp_.month = ( (fdate >> 5 ) & 0x0F );   /* 月:4bit(0..16) */
    stamp_.year   =  (fdate >> 9 ) + 1980;     /* 年:7bit */
}

#endif

/* いわゆる _dos_findfirst
 *	p_path - パス (ワイルドカード部含む)
 *	attr   - アトリビュート
 * return
 *	0 ... 成功
 *	1 ... 失敗(最後のファイルだった等)
 *	-1 .. 失敗(LFNがサポートされていない)
 */
unsigned NnDir::findfirst(  const char *path , unsigned attr )
{
    NnString path2(path);
    return NnDir::findfirst( path2 , attr );
}

unsigned NnDir::findfirst(  const NnString &p_path , unsigned attr )
{
    unsigned result;
    NnString path;
    filter( p_path.chars() , path );

    core = new NnDir::Core();
    result = core->findfirst( path.chars() , attr );
    if( result == 0 ){
        name_ = core->name();
	attr_ = core->attr();
	size_ = core->size();
        core->stamp( stamp_ );
    }
    return result;
}

unsigned NnDir::findnext()
{
    int result = core->findnext();
    if( result==0){
	name_ = core->name();
	attr_ = core->attr();
        size_ = core->size();
        core->stamp( stamp_ );
    }
    return result;
}

void NnDir::findclose()
{
    if( core != NULL ){
	core->findclose();
        delete core;
        core = NULL;
    }
}

/* カレントディレクトリを得る.(LFN対応の場合のみ)
 *	pwd - カレントディレクトリ
 * return
 *	0 ... 成功
 *	1 ... 失敗
 */
int NnDir::getcwd( NnString &pwd )
{
    char buffer[256];
#ifdef __EMX__
    if( ::_getcwd2(buffer,sizeof(buffer)) != NULL ){
#else
    if( ::getcwd(buffer,sizeof(buffer)) != NULL ){
#endif
        pwd = buffer;
    }else{
        pwd.erase();
    }
    return 0;
}

/* スラッシュをバックスラッシュへ変換する
 * 重複した \ や / を一つにする.
 * 	src - 元文字列
 *	dst - 結果文字列の入れ物
 */
void NnDir::f2b( const char *sp , NnString &dst )
{
    int prevchar=0;
    while( *sp != '\0' ){
	if( isKanji(*sp & 255) ){
	    prevchar = *sp;
	    dst << *sp++;
	    if( *sp == 0 )
		break;
	    dst << *sp++;
	}else{
	    if( *sp == '/' || *sp == '\\' ){
		if( prevchar != '\\' )
		    dst << '\\';
		prevchar = '\\';
		++sp;
	    }else{
		prevchar = *sp;
		dst << *sp++;
	    }
	}
    }
    // dst = sp; dst.slash2yen();
}


/* パス名変換を行う。
 * ・スラッシュをバックスラッシュへ変換
 * ・チルダを環境変数HOMEの内容へ変換
 * 	src - 元文字列
 *	dst - 結果文字列の入れ物
 */
void NnDir::filter( const char *sp , NnString &dst_ )
{
    NnString dst;

    dst.erase();
    const char *home;
    if( *sp == '~'  && (isalnum( *(sp+1) & 255) || *(sp+1)=='_') ){
	NnString name;
	++sp;
	do{
	    name << *sp++;
	}while( isalnum(*sp & 255) || *sp == '_' );
	NnString *value=(NnString*)specialFolder.get(name);
	if( value != NULL ){
	    dst << *value;
	}else{
	    dst << '~' << name;
	}
    }else if( *sp == '~'  && 
             ( (home=getEnv("HOME",NULL))        != NULL  ||
               (home=getEnv("USERPROFILE",NULL)) != NULL ) )
    {
	dst << home;
	++sp;
    }else if( sp[0]=='.' &&  sp[1]=='.' && sp[2]=='.' ){
	extractDots( sp , dst );
    }
    dst << sp;

    f2b( dst.chars() , dst_ );
    // dst.slash2yen();
}

/* chdir.
 *	argv - ディレクトリ名
 * return
 *	 0 - 成功
 *	-1 - 失敗
 */
int NnDir::chdir( const char *argv )
{
    NnString newdir;
    filter( argv , newdir );
    if( isAlpha(newdir.at(0)) && newdir.at(1)==':' ){
#ifdef __EMX__
        _chdrive( newdir.at(0) );
#else
        _chdrive( newdir.at(0) & 0x1F );
#endif
    }
    if(    newdir.at( newdir.length()-1 ) == '\\'
        || newdir.at( newdir.length()-1 ) == '/' )
        newdir += '.';

#ifdef NYACUS
    if( newdir.iendsWith(".lnk") ){
        extern int read_shortcut(const char *src,char *buffer,int size);
	char buffer[ FILENAME_MAX ];
	if( read_shortcut( newdir.chars() , buffer , sizeof(buffer) )==0 ){
	    newdir = buffer;
	}
    }
#endif
    return ::chdir( newdir.chars() );
}

/* ファイルクローズ
 *  fd - ハンドル
 */
void NnDir::close( int fd )
{
    ::close(fd);
}
/* ファイル出力
 * (特にLFN対応というわけではないが、コンパイラ依存コードを
 *  避けるため作成)
 *      fd - ハンドル
 *      ptr - バッファのアドレス
 *      size - バッファのサイズ
 * return
 *      実出力サイズ (-1 : エラー)
 */
int NnDir::write( int fd , const void *ptr , size_t size )
{
    return ::write(fd,ptr,size);
}

/* LFN 対応 OPEN
 *      fname - ファイル名
 *      mode - "w","r","a" のいずれか
 * return
 *      ハンドル(エラー時:-1)
 */
int NnDir::open( const char *fname , const char *mode )
{
    switch( *mode ){
    case 'w':
        return ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_TRUNC , S_IWRITE|S_IREAD );
    case 'a':
        {
            int fd= ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_APPEND , S_IWRITE|S_IREAD );
            lseek(fd,0,SEEK_END);
            return fd;
        }
    case 'r':
        return ::open(fname , O_RDONLY|O_BINARY , S_IWRITE|S_IREAD );
    default:
        return -1;
    }
}

/* テンポラリファイル名を作る.  */
const NnString &NnDir::tempfn()
{
    static NnString result;

    /* ファイル名部分(非ディレクトリ部分)を作る */
    char tempname[ FILENAME_MAX ];
    tmpnam( tempname );
    
    /* ディレクトリ部分を作成 */
    const char *tmpdir;
    if(   ((tmpdir=getEnv("TEMP",NULL)) != NULL )
       || ((tmpdir=getEnv("TMP",NULL))  != NULL ) ){
        result  = tmpdir;
        result += '\\';
        result += tempname;
    }else{
        result  = tempname;
    }
    return result;
}

int NnDir::access( const char *path )
{
    NnDir dir(path); return *dir == NULL;
}

/* カレントドライブを変更する
 *    driveletter - ドライブ文字('A'..'Z')
 * return 0:成功,!0:失敗
 */
int  NnDir::chdrive( int driveletter )
{
    return 
#ifdef __EMX__
	_chdrive( driveletter );
#else
	_chdrive( driveletter & 0x1F );
#endif
}

/* カレントドライブを取得する。
 * return ドライブ文字 'A'...'Z'
 */
int NnDir::getcwdrive()
{
    return 
#ifdef __EMX__
	_getdrive();
#else
	'A'+ _getdrive() - 1;
#endif
}

int NnFileStat::compare( const NnSortable &another ) const
{
    return name_.icompare( ((NnFileStat&)another).name_ );
}
NnFileStat::~NnFileStat(){}

#ifdef NYACUS
/* My Document などを ~desktop などと登録する
 * 参考⇒http://www001.upp.so-net.ne.jp/yamashita/doc/shellfolder.htm
 */
void NnDir::set_default_special_folder()
{
    static struct list_s {
	const char *name ;
	int  key;
    } list[]={
	{ "desktop"           , CSIDL_DESKTOPDIRECTORY },
	{ "sendto"            , CSIDL_SENDTO },
	{ "startmenu"         , CSIDL_STARTMENU },
	{ "startup"           , CSIDL_STARTUP },
	{ "mydocuments"       , CSIDL_PERSONAL },
	{ "favorites"         , CSIDL_FAVORITES },
	{ "programs"          , CSIDL_PROGRAMS },
	{ "program_files"     , CSIDL_PROGRAM_FILES },
	{ "appdata"           , CSIDL_APPDATA },
	{ "allusersdesktop"   , CSIDL_COMMON_DESKTOPDIRECTORY },
	{ "allusersprograms"  , CSIDL_COMMON_PROGRAMS },
	{ "allusersstartmenu" , CSIDL_COMMON_STARTMENU },
	{ "allusersstartup"   , CSIDL_COMMON_STARTUP },
	{ NULL , 0 },
    };
    LPITEMIDLIST pidl;
    char path[ FILENAME_MAX ];

    for( struct list_s *p=list ; p->name != NULL ; p++ ){
	path[0] = '\0';

	::SHGetSpecialFolderLocation( NULL , p->key, &pidl );
	::SHGetPathFromIDList( pidl, path );

	if( path[0] != '\0' )
	    specialFolder.put( p->name , new NnString( path ) );
    }
}

#else
void NnDir::set_default_special_folder()
{
    /* do nothing */
}
#endif

NnHash NnDir::specialFolder;

NnFileStat *NnFileStat::stat(const NnString &name)
{
    NnTimeStamp stamp1;
#ifdef __MINGW32__
    struct _stati64 stat1;
#else
    struct stat stat1;
#endif
    unsigned attr=0 ;
    NnString name_(name);
    if( name_.endsWith(":") || name_.endsWith("\\") || name_.endsWith("/") )
	name_ << ".";

#ifdef __MINGW32__
    if( ::_stati64( name_.chars() , &stat1 ) != 0 )
#else
    if( ::stat( name_.chars() , &stat1 ) != 0 )
#endif
    {
	return NULL;
    }
    if( stat1.st_mode & S_IFDIR )
	attr |= ATTR_DIRECTORY ;
    if( (stat1.st_mode & S_IWUSR) == 0 )
	attr |= ATTR_READONLY ;
    
    stamp_conv( stat1.st_mtime , stamp1 );

    return new NnFileStat(
	name ,
	attr ,
	stat1.st_size ,
	stamp1 );
}
