#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef __EMX__
#  include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include "nndir.h"
#include "writer.h"
#include "reader.h"
#include "ntcons.h"

#include "mysystem.h"

#ifdef NYACUS

int AnsiConsoleWriter::default_color=-1;

Writer &AnsiConsoleWriter::write( char c )
{
    enum{ EMPTY = -1 };

    if( mode == PRINTING ){
	if( c == '\x1B' ){
            _commit( 1 );
            _commit( 2 );
	    // fflush( stdout );
	    // fflush( stderr );
	    mode = STACKING;
	    param[ n = 0 ] = 0;
	}else if( c == '\b' ){
            _commit( 1 );
            _commit( 2 );
	    // fflush( stdout );
	    // fflush( stderr );
	    Console::backspace();
	}else{
            if( this->prevchar ){
                char buffer[3] = { this->prevchar , c , '\0' };
                ::_write( this->fd() , buffer , sizeof(char)*2 );
                this->prevchar = 0;
            }else if( isKanji(c) ){
                this->prevchar = c ;
            }else{
                ::_write( this->fd() , &c , sizeof(char) );
            }
	}
    }else{
	if( c == '[' ){
	    flag |= BAREN ;
	}else if( c == '>' ){
	    flag |= GREATER ;
	}else if( isdigit(c) ){
	    if( param[ n ] == EMPTY ){
		param[ n ] = c - '0' ;
	    }else{
		param[ n ] = param[ n ] * 10 + (c-'0');
	    }
	}else if( c == ';' ){
	    if( n < numof(param)-1 )
		param[ ++n ] = EMPTY ;
	}else if( isalpha(c) ){
	    if( c == 'm' ){
		int lastcolor = Console::color();
		int forecolor =   lastcolor       & 7;
		int backcolor = ((lastcolor >> 4) & 7);
		int high      = ((lastcolor >> 3 )& 1);

		if( default_color == -1 )
		    default_color = lastcolor ;

		for(size_t i=0 ; i <= n ; ++i ){
		    /* ANSI-ESC : BGR ⇒ * NT-CODE  : RGB の変換テーブル */
		    static int cnv[]={ 0 , 4 , 2 , 6 , 1 , 5 , 3 , 7 };

		    if( 30 <= param[i] && param[i] <= 37 ){
			forecolor = cnv[ param[i]-30 ];
		    }else if( param[i] == 39 ){
			forecolor = Console::foremask(default_color);
		    }else if( 40 <= param[i] && param[i] <= 47 ){
			backcolor = cnv[ param[i]-40 ];
		    }else if( param[i] == 49 ){
			backcolor = Console::backmask(default_color);
		    }else if( param[i] == 1 ){
			high = 1;
		    }else if( param[i] == 0 ){
			forecolor = 7;
			backcolor = 0;
			high      = 0;
		    }
		}
		Console::color( (backcolor<<4) | (high<<3) | forecolor );
	    }else if( c == 'J' ){
		Console::clear();
	    }else if( c == 'l' && (flag & GREATER) != 0 ){
		int x,y;
		Console::getLocate(x,y);
		Console::locate(x,y);
	    }
	    n = 0;
	    mode = PRINTING;
	}else{
	    n = 0;
	    mode = PRINTING;
	}
    }
    return *this;
}

Writer &AnsiConsoleWriter::write( const char *s )
{
    while( *s != '\0' )
	this->write( *s++ );
    _commit( fd() );
    // fflush( fp() );
    return *this;
}

#endif

Writer *conOut_ = new AnsiConsoleWriter(1) ;
Writer *conErr_ = new AnsiConsoleWriter(2) ; 

int Writer::isatty() const
{ 
    return 0;
}

Writer::~Writer(){}

Writer &Writer::operator << (int n)
{
    NnString tmp;
    tmp.addValueOf( n );
    return *this << tmp.chars();
#if 0
    if( n < 0 ){
        *this << '-';
        return *this << -n;
    }
    if( n >= 10 ){
        *this << (n/10) ;
        n %= 10;
    }
    return *this << "0123456789"[n];
#endif
}

Writer &Writer::write( const char *s )
{
    while( *s != '\0' )
        *this << *s++;
    return *this;
}

Writer &StreamWriter::write( char c )
{
    fputc( c , fp_ );
    return *this;
}
Writer &StreamWriter::write( const char *s )
{
    fputs( s , fp_ );
    return *this;
}

void PipeWriter::open( const NnString &cmds_ )
{
    int fd=myPopen( cmds_.chars() , "w" , &pid );
    if( fd < 0 )
        return;
    cmds = cmds_;
    cmds.trim();
    setFd( fd );
}

PipeWriter::PipeWriter( const char *s )
{
    NnString cmds_(s);
    pid = 0;
    open( cmds_ );
}

PipeWriter::~PipeWriter()
{
    myPclose( fd() , pid );
}

FileWriter::FileWriter( const char *fn , const char *mode )
    : StreamWriter( fopen(fn,mode) )
{
    /* Borland C++ では、最初に書きこむまで、ファイルポインタが
     * 移動しないので、dup2 を使う場合は、
     * 明示的に lseek で移動させてやる必要がある。
     * (stdio と io を混在させて使うのは本来はダメなので、文句は言えない…)
     */
    if( this->ok() && *mode == 'a' )
	fseek( fp() , 0 , SEEK_END );
}

FileWriter::~FileWriter()
{
    if( ok() )
        fclose( this->fp() );
}

/* 標準出力・入力をリダイレクトする
 *      x - ファイルハンドル
 * return
 *      0  : 成功
 *      -1 : 失敗
 */
int Redirect::set(int x)
{
    if( original_fd == -1 ){
        original_fd = dup(fd());
        if( original_fd == -1 )
            return -1;
    }
    if( dup2( x , fd() ) == -1 )
        return -1;
    isOpenFlag = 1;
    return 0;
}

void Redirect::close()
{
    if( original_fd != -1  &&  isOpenFlag ){ 
        ::close( fd() );
        isOpenFlag = 0;
    }
}

/* リダイレクトを元に戻す */
void Redirect::reset()
{
    if( original_fd != -1 ){
        dup2( original_fd , fd() );
        ::close( original_fd );
        original_fd = -1;
    }
}

/* ファイル名を指定して、リダイレクトする。
 *      fname - ファイル名
 *      mode  - モード
 * return
 *      0 - 成功
 *      -1 - 失敗
 */
int Redirect::switchTo( const NnString &fname , const char *mode  )
{
    if( *mode == 'r' ){
	FileReader fr(fname.chars());
	if( fr.eof() )
            return -1;
	this->set( fr.fd() );
    }else{
	FileWriter fw( fname.chars() , mode );
	if( ! fw.ok() )
            return -1;
	this->set( fw.fd() );

    }
    return 0;
}

int StreamWriter::isatty() const
{
    return ::isatty( fd() ); 
}

RawWriter::~RawWriter(){}

int RawWriter::ok() const
{
    return fd_ >= 0 && failed==0 ;
}

Writer &RawWriter::write( char c )
{
    if( ::write( fd_ , &c , 1 ) < 1 )
        ++failed;
    return *this;
}

Writer &RawWriter::write( const char *s )
{
    int len=strlen(s);
    if( ::write( fd_ , s , len  ) < len )
        ++failed;
    return *this;
}

int RawWriter::isatty() const
{
    return ::isatty(fd_);
}
